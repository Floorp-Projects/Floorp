/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 */

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsEscape.h"

#include "nsICharsetConverterManager.h"
#include "nsIPlatformCharset.h"
#include "nsILocalFile.h"

#include "nsIURIFixup.h"
#include "nsDefaultURIFixup.h"

static NS_DEFINE_CID(kPlatformCharsetCID, NS_PLATFORMCHARSET_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsDefaultURIFixup, nsIURIFixup)

nsDefaultURIFixup::nsDefaultURIFixup()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}


nsDefaultURIFixup::~nsDefaultURIFixup()
{
  /* destructor code */
}


/* nsIURI createFixupURI (in string aURIText); */
NS_IMETHODIMP nsDefaultURIFixup::CreateFixupURI(const PRUnichar *aStringURI, nsIURI **aURI)
{
    *aURI = nsnull;

    // Try and get the prefs service
    if (!mPrefs)
    {
        mPrefs = do_GetService(NS_PREF_CONTRACTID);
    }

    nsAutoString uriString(aStringURI);
    uriString.Trim(" ");  // Cleanup the empty spaces that might be on each end.

    // Eliminate embedded newlines, which single-line text fields now allow:
    uriString.StripChars("\r\n");

    // Check for if it is a file URL
    FileURIFixup(uriString.get(), aURI);
    if(*aURI)
        return NS_OK;

#ifdef XP_PC
    // Not a file URL, so translate '\' to '/' for convenience in the common protocols
    if (uriString.FindChar(':') == -1 ||
	uriString.EqualsIgnoreCase("http:", 5) ||
        uriString.EqualsIgnoreCase("https:", 6) ||
        uriString.EqualsIgnoreCase("ftp:", 4))
    {
        uriString.ReplaceChar(PRUnichar('\\'), PRUnichar('/'));
    }
#endif

    // Just try to create an URL out of it
    NS_NewURI(aURI, uriString, nsnull);
    if(*aURI)
        return NS_OK;

    // See if it is a keyword
    // Test whether keywords need to be fixed up
    PRBool fixupKeywords = PR_FALSE;
    if (mPrefs)
    {
        NS_ENSURE_SUCCESS(mPrefs->GetBoolPref("keyword.enabled", &fixupKeywords), NS_ERROR_FAILURE);
    }
    if (fixupKeywords)
    {
        KeywordURIFixup(uriString.get(), aURI);
        if(*aURI)
            return NS_OK;
    }

    // See if a protocol needs to be added
    PRInt32 checkprotocol = uriString.Find("://",0);
    // if no scheme (protocol) is found, assume http or ftp.
    if (checkprotocol == -1) {
        // find host name
        PRInt32 hostPos = uriString.FindCharInSet("./:");
        if (hostPos == -1) 
            hostPos = uriString.Length();

        // extract host name
        nsAutoString hostSpec;
        uriString.Left(hostSpec, hostPos);

        // insert url spec corresponding to host name
        if (hostSpec.EqualsIgnoreCase("ftp")) 
            uriString.InsertWithConversion("ftp://", 0, 6);
        else 
            uriString.InsertWithConversion("http://", 0, 7);
    } // end if checkprotocol

    return NS_NewURI(aURI, uriString, nsnull);
}


nsresult nsDefaultURIFixup::FileURIFixup(const PRUnichar* aStringURI, 
                                         nsIURI** aURI)
{
    nsAutoString uriSpecIn(aStringURI);
    nsCAutoString uriSpecOut;

    nsresult rv = ConvertFileToStringURI(uriSpecIn, uriSpecOut);
    if (NS_SUCCEEDED(rv))
    {
        // if this is file url, uriSpecOut is already in FS charset
        if(NS_SUCCEEDED(NS_NewURI(aURI, uriSpecOut.get(), nsnull)))
            return NS_OK;
    } 
    return NS_ERROR_FAILURE;
}

nsresult nsDefaultURIFixup::ConvertFileToStringURI(nsString& aIn,
                                                   nsCString& aOut)
{
    PRBool attemptFixup = PR_FALSE;

#ifdef XP_PC
    // Check for \ in the url-string or just a drive (PC)
    if(kNotFound != aIn.FindChar(PRUnichar('\\')) || ((aIn.Length() == 2 ) && (aIn.Last() == PRUnichar(':') || aIn.Last() == PRUnichar('|'))))
    {
        attemptFixup = PR_TRUE;
    }
#elif XP_UNIX
    // Check if it starts with / or \ (UNIX)
    const PRUnichar * up = aIn.get();
    if((PRUnichar('/') == *up) || (PRUnichar('\\') == *up))
    {
        attemptFixup = PR_TRUE;
    }
#else
    // Do nothing (All others for now) 
#endif

    if (attemptFixup)
    {
        // Test if this is a valid path by trying to create a local file
        // object. The URL of that is returned if successful.

        // NOTE: Please be sure to check that the call to NS_NewLocalFile
        //       rejects bad file paths when using this code on a new
        //       platform.

        nsCOMPtr<nsILocalFile> filePath;
        nsCAutoString file; 

        // this is not the real fix but a temporary fix
        // in order to really fix the problem, we need to change the 
        // nsICmdLineService interface to use wstring to pass paramenters 
        // instead of string since path name and other argument could be
        // in non ascii.(see bug 87127) Since it is too risky to make interface change right
        // now, we decide not to do so now.
        // Therefore, the aIn we receive here maybe already in damage form
        // (e.g. treat every bytes as ISO-8859-1 and cast up to PRUnichar
        //  while the real data could be in file system charset )
        // we choice the following logic which will work for most of the case.
        // Case will still failed only if it meet ALL the following condiction:
        //    1. running on CJK, Russian, or Greek system, and 
        //    2. user type it from URL bar
        //    3. the file name contains character in the range of 
        //       U+00A1-U+00FF but encode as different code point in file
        //       system charset (e.g. ACP on window)- this is very rare case
        // We should remove this logic and convert to File system charset here
        // once we change nsICmdLineService to use wstring and ensure
        // all the Unicode data come in is correctly converted. 
        if (PossiblyByteExpandedFileName(aIn)) {
          // removes high byte
          file.AssignWithConversion(aIn);
        }
        else {
          // converts from Unicode to FS Charset
          ConvertStringURIToFileCharset(aIn, file);
        }

        nsresult rv = NS_NewLocalFile(file.get(), PR_FALSE, getter_AddRefs(filePath));
        if (NS_SUCCEEDED(rv))
        {
            nsXPIDLCString fileurl;
            filePath->GetURL(getter_Copies(fileurl));
            aOut.Assign(fileurl);
            return NS_OK;
        }
    }

    return NS_ERROR_FAILURE;
}

PRBool nsDefaultURIFixup::PossiblyByteExpandedFileName(nsString& aIn)
{
  // XXXXX HACK XXXXX : please don't copy this code.
  // There are cases where aIn contains the locale byte chars padded to short
  // (thus the name "ByteExpanded"); whereas other cases 
  // have proper Unicode code points.
  // This is a temporary fix.  Please refer to 58866, 86948
  const PRUnichar* uniChar = aIn.get();
  for (PRUint32 i = 0; i < aIn.Length(); i++)  {
    if ((uniChar[i] >= 0x0080) && (uniChar[i] <= 0x00FF))  {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

nsresult nsDefaultURIFixup::ConvertStringURIToFileCharset(nsString& aIn, 
                                                          nsCString& aOut)
{
    aOut = "";
    // for file url, we need to convert the nsString to the file system
    // charset before we pass to NS_NewURI
    static nsAutoString fsCharset;
    // find out the file system charset first
    if(0 == fsCharset.Length())
    {
        fsCharset.AssignWithConversion("ISO-8859-1"); // set the fallback first.
        nsCOMPtr<nsIPlatformCharset> plat(do_GetService(kPlatformCharsetCID));
        NS_ENSURE_TRUE(plat, NS_ERROR_FAILURE);
        NS_ENSURE_SUCCESS(plat->GetCharset(kPlatformCharsetSel_FileName, fsCharset),
            NS_ERROR_FAILURE);
    }
    // We probably should cache ccm here.
    // get a charset converter from the manager
    nsCOMPtr<nsICharsetConverterManager> ccm(do_GetService(kCharsetConverterManagerCID));
    NS_ENSURE_TRUE(ccm, NS_ERROR_FAILURE);
   
    nsCOMPtr<nsIUnicodeEncoder> fsEncoder;
    NS_ENSURE_SUCCESS(ccm->GetUnicodeEncoder(&fsCharset, 
        getter_AddRefs(fsEncoder)), NS_ERROR_FAILURE);

    PRInt32 bufLen = 0;
    NS_ENSURE_SUCCESS(fsEncoder->GetMaxLength(aIn.get(), aIn.Length(),
        &bufLen), NS_ERROR_FAILURE);
    aOut.SetCapacity(bufLen+1);
    PRInt32 srclen = aIn.Length();
    NS_ENSURE_SUCCESS(fsEncoder->Convert(aIn.get(), &srclen, 
        (char*)aOut.get(), &bufLen), NS_ERROR_FAILURE);

    ((char*)aOut.get())[bufLen]='\0';
    aOut.SetLength(bufLen);

    return NS_OK;
}


nsresult nsDefaultURIFixup::KeywordURIFixup(const PRUnichar* aStringURI, 
                                            nsIURI** aURI)
{
    // These are keyword formatted strings
    // "what is mozilla"
    // "what is mozilla?"
    // "?mozilla"
    // "?What is mozilla"

    // These are not keyword formatted strings
    // "www.blah.com" - anything with a dot in it 
    // "nonQualifiedHost:80" - anything with a colon in it
    // "nonQualifiedHost?"
    // "nonQualifiedHost?args"
    // "nonQualifiedHost?some args"

    nsAutoString uriString(aStringURI);
    if(uriString.FindChar('.') == -1 && uriString.FindChar(':') == -1)
    {
        PRInt32 qMarkLoc = uriString.FindChar('?');
        PRInt32 spaceLoc = uriString.FindChar(' ');

        PRBool keyword = PR_FALSE;
        if(qMarkLoc == 0)
        keyword = PR_TRUE;
        else if((spaceLoc > 0) && ((qMarkLoc == -1) || (spaceLoc < qMarkLoc)))
        keyword = PR_TRUE;

        if(keyword)
        {
            nsCAutoString keywordSpec("keyword:");
            char *utf8Spec = ToNewUTF8String(uriString);
            if(utf8Spec)
            {
                char* escapedUTF8Spec = nsEscape(utf8Spec, url_Path);
                if(escapedUTF8Spec) 
                {
                    keywordSpec.Append(escapedUTF8Spec);
                    NS_NewURI(aURI, keywordSpec.get(), nsnull);
                    nsMemory::Free(escapedUTF8Spec);
                } // escapedUTF8Spec
                nsMemory::Free(utf8Spec);
            } // utf8Spec
        } // keyword 
    } // FindChar

    if(*aURI)
        return NS_OK;

    return NS_ERROR_FAILURE;
}


nsresult NS_NewURIFixup(nsIURIFixup **aURIFixup)
{
    nsDefaultURIFixup *fixup = new nsDefaultURIFixup;
    if (fixup == nsnull)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return fixup->QueryInterface(NS_GET_IID(nsIURIFixup), (void **) aURIFixup);
}

