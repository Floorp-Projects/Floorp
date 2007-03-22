/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsEmbedChromeRegistry.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "plstr.h"

#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIProperties.h"
#include "nsILocalFile.h"
#include "nsIURI.h"

#define CHROME_TYPE_CONTENT 0
#define CHROME_TYPE_LOCALE 1
#define CHROME_TYPE_SKIN 2

const char kChromePrefix[] = "chrome://";

static nsresult
SplitURL(nsIURI *aChromeURI, nsCString& aPackage, nsCString& aProvider, nsCString& aFile,
         PRBool *aModified = nsnull)
{
  // Splits a "chrome:" URL into its package, provider, and file parts.
  // Here are the current portions of a
  // chrome: url that make up the chrome-
  //
  //     chrome://global/skin/foo?bar
  //     \------/ \----/\---/ \-----/
  //         |       |     |     |
  //         |       |     |     `-- RemainingPortion
  //         |       |     |
  //         |       |     `-- Provider
  //         |       |
  //         |       `-- Package
  //         |
  //         `-- Always "chrome://"
  //
  //

  nsresult rv;

  nsCAutoString str;
  rv = aChromeURI->GetSpec(str);
  if (NS_FAILED(rv)) return rv;

  // We only want to deal with "chrome:" URLs here. We could return
  // an error code if the URL isn't properly prefixed here...
  if (PL_strncmp(str.get(), kChromePrefix, sizeof(kChromePrefix) - 1) != 0)
    return NS_ERROR_INVALID_ARG;

  // Cull out the "package" string; e.g., "navigator"
  aPackage = str.get() + sizeof(kChromePrefix) - 1;

  PRInt32 idx;
  idx = aPackage.FindChar('/');
  if (idx < 0)
    return NS_OK;

  // Cull out the "provider" string; e.g., "content"
  aPackage.Right(aProvider, aPackage.Length() - (idx + 1));
  aPackage.Truncate(idx);

  idx = aProvider.FindChar('/');
  if (idx < 0) {
    // Force the provider to end with a '/'
    idx = aProvider.Length();
    aProvider.Append('/');
  }

  // Cull out the "file"; e.g., "navigator.xul"
  aProvider.Right(aFile, aProvider.Length() - (idx + 1));
  aProvider.Truncate(idx);

  PRBool nofile = aFile.IsEmpty();
  if (nofile) {
    // If there is no file, then construct the default file
    aFile = aPackage;

    if (aProvider.Equals("content")) {
      aFile += ".xul";
    }
    else if (aProvider.Equals("skin")) {
      aFile += ".css";
    }
    else if (aProvider.Equals("locale")) {
      aFile += ".dtd";
    }
    else {
      NS_ERROR("unknown provider");
      return NS_ERROR_FAILURE;
    }
  } else {
    // Protect against URIs containing .. that reach up out of the
    // chrome directory to grant chrome privileges to non-chrome files.
    int depth = 0;
    PRBool sawSlash = PR_TRUE;  // .. at the beginning is suspect as well as /..
    for (const char* p=aFile.get(); *p; p++) {
      if (sawSlash) {
        if (p[0] == '.' && p[1] == '.'){
          depth--;    // we have /.., decrement depth.
        } else {
          static const char escape[] = "%2E%2E";
          if (PL_strncasecmp(p, escape, sizeof(escape)-1) == 0)
            depth--;   // we have the HTML-escaped form of /.., decrement depth.
        }
      } else if (p[0] != '/') {
        depth++;        // we have /x for some x that is not /
      }
      sawSlash = (p[0] == '/');

      if (depth < 0) {
        return NS_ERROR_FAILURE;
      }
    }
  }
  if (aModified)
    *aModified = nofile;
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsEmbedChromeRegistry, nsIChromeRegistry)

nsEmbedChromeRegistry::nsEmbedChromeRegistry()
{
}

nsresult
nsEmbedChromeRegistry::Init()
{
    NS_ASSERTION(0, "Creating embedding chrome registry\n");
    nsresult rv;
    
    rv = NS_NewISupportsArray(getter_AddRefs(mEmptyArray));
    if (NS_FAILED(rv)) return rv;

    rv = ReadChromeRegistry();
    if (NS_FAILED(rv)) return rv;
    
    return NS_OK;
}

nsresult
nsEmbedChromeRegistry::ReadChromeRegistry()
{
    nsresult rv;
    nsCOMPtr<nsIProperties> directoryService =
        do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsILocalFile> listFile;
    rv = directoryService->Get(NS_APP_CHROME_DIR, NS_GET_IID(nsILocalFile),
                               getter_AddRefs(listFile));
    if (NS_FAILED(rv)) return rv;

    rv = listFile->AppendRelativeNativePath(NS_LITERAL_CSTRING("installed-chrome.txt"));
    if (NS_FAILED(rv)) return rv;

    PRFileDesc *file;
    rv = listFile->OpenNSPRFileDesc(PR_RDONLY, 0, &file);
    if (NS_FAILED(rv)) return rv;

    PRFileInfo finfo;

    if (PR_GetOpenFileInfo(file, &finfo) == PR_SUCCESS) {
        char *dataBuffer = new char[finfo.size+1];
        if (dataBuffer) {
            PRInt32 bufferSize = PR_Read(file, dataBuffer, finfo.size);
            if (bufferSize > 0) {
                dataBuffer[bufferSize] = '\r';
                rv = ProcessNewChromeBuffer(dataBuffer, bufferSize);
            }
            delete [] dataBuffer;
        }
    }
    PR_Close(file);

    return rv;
}

nsresult
nsEmbedChromeRegistry::ProcessNewChromeBuffer(char* aBuffer, PRInt32 aLength)
{
    while (aLength > 0) {
        PRInt32 processedBytes = ProcessChromeLine(aBuffer, aLength);
        aBuffer += processedBytes;
        aLength -= processedBytes;
    }
    return NS_OK;
}

#define MAX_TOKENS 5
struct chromeToken {
    const char *tokenStart;
    const char *tokenEnd;
};

PRInt32
nsEmbedChromeRegistry::ProcessChromeLine(const char* aBuffer, PRInt32 aLength)
{
    PRInt32 bytesProcessed = 0;
    chromeToken tokens[MAX_TOKENS];
    PRInt32 tokenCount = 0;
    PRBool expectingToken = PR_TRUE;
    
    while (bytesProcessed <= aLength &&
           *aBuffer != '\n' && *aBuffer != '\r' &&
           tokenCount < MAX_TOKENS) {

        if (*aBuffer == ',') {
            tokenCount++;
            expectingToken = PR_TRUE;
        }
        else if (expectingToken)
            tokens[tokenCount].tokenStart = aBuffer;
        else
            tokens[tokenCount].tokenEnd = aBuffer;


        aBuffer++;
        bytesProcessed++;
    }
    NS_ASSERTION(tokenCount == 4, "Unexpected tokens in line");

    nsDependentCSubstring
        chromeType(tokens[0].tokenStart, tokens[0].tokenEnd);
    nsDependentCSubstring
        chromeProfile(tokens[1].tokenStart, tokens[1].tokenEnd);
    nsDependentCSubstring
        chromeLocType(tokens[2].tokenStart, tokens[2].tokenEnd);
    nsDependentCSubstring
        chromeLocation(tokens[3].tokenStart, tokens[3].tokenEnd);
    
    RegisterChrome(chromeType, chromeProfile, chromeLocType, chromeLocation);
    return bytesProcessed;
}

nsresult
nsEmbedChromeRegistry::RegisterChrome(const nsACString& aChromeType,
                                      const nsACString& aChromeProfile,
                                      const nsACString& aChromeLocType,
                                      const nsACString& aChromeLocation)
{
    PRInt32 chromeType;
    if (aChromeType.EqualsLiteral("skin"))
        chromeType = CHROME_TYPE_SKIN;
    else if (aChromeType.EqualsLiteral("locale"))
        chromeType = CHROME_TYPE_LOCALE;
    else
        chromeType = CHROME_TYPE_CONTENT;

    PRBool chromeIsProfile =
        aChromeProfile.EqualsLiteral("profile");

    PRBool chromeIsURL =
        aChromeProfile.EqualsLiteral("url");

    return RegisterChrome(chromeType, chromeIsProfile, chromeIsURL,
                          aChromeLocation);
}

nsresult
nsEmbedChromeRegistry::RegisterChrome(PRInt32 aChromeType, // CHROME_TYPE_CONTENT, etc
                                      PRBool aChromeIsProfile, // per-profile?
                                      PRBool aChromeIsURL, // is it a url? (else path)
                                      const nsACString& aChromeLocation)
{


    return NS_OK;
}

NS_IMETHODIMP
nsEmbedChromeRegistry::CheckForNewChrome()
{
    return NS_OK;
}

NS_IMETHODIMP
nsEmbedChromeRegistry::Canonify(nsIURI* aChromeURI)
{
#if 1
  // Canonicalize 'chrome:' URLs. We'll take any 'chrome:' URL
  // without a filename, and change it to a URL -with- a filename;
  // e.g., "chrome://navigator/content" to
  // "chrome://navigator/content/navigator.xul".
  if (! aChromeURI)
      return NS_ERROR_NULL_POINTER;

  PRBool modified = PR_TRUE; // default is we do canonification
  nsCAutoString package, provider, file;
  nsresult rv;
  rv = SplitURL(aChromeURI, package, provider, file, &modified);
  if (NS_FAILED(rv))
    return rv;

  if (!modified)
    return NS_OK;

  nsCAutoString canonical( kChromePrefix );
  canonical += package;
  canonical += "/";
  canonical += provider;
  canonical += "/";
  canonical += file;

  return aChromeURI->SetSpec(canonical);
#else
  return NS_OK;
#endif
}

NS_IMETHODIMP
nsEmbedChromeRegistry::ConvertChromeURL(nsIURI* aChromeURL, nsACString& aResult)
{
    nsresult rv;
    
    rv = aChromeURL->GetSpec(aResult);
    if (NS_FAILED(rv)) return rv;
    
    return NS_OK;
}
