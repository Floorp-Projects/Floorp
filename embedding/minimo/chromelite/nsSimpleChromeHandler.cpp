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
 * The Original Code is Minimo.
 *
 * The Initial Developer of the Original Code is
 * Doug Turner <dougt@meer.net>.
 * Portions created by the Initial Developer are Copyright (C) 2003
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


#include "nsSimpleChromeHandler.h"
#include "nsIServiceManager.h"

#include "nsILocalFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"

#include "nsIURL.h"
#include "nsIIOService.h"

#include "nsNetUtil.h"
#include "nsURLHelper.h"

#include "nsIGenericFactory.h"
#include "nsSimpleChromeRegistry.h"

#include "nsURLHelper.h"
#include "nsStandardURL.h"

class nsSimpleChromeURL : public nsIFileURL
{
public:
    nsSimpleChromeURL(nsIFile* file);

    nsresult Init(PRUint32 urlType,
		  PRInt32 defaultPort,
		  const nsACString &spec,
		  const char *charset,
		  nsIURI *baseURI);
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIFILEURL
    NS_FORWARD_SAFE_NSIURI(mStandardURL)
    NS_FORWARD_SAFE_NSIURL(mStandardURL)

private:
    nsCOMPtr<nsIFile> mChromeDir;
    nsCOMPtr<nsIURL>  mStandardURL;
};

nsSimpleChromeURL::nsSimpleChromeURL(nsIFile *file) : mChromeDir(file)
{
}

nsresult
nsSimpleChromeURL::Init(PRUint32 urlType,
                    PRInt32 defaultPort,
                    const nsACString &spec,
                    const char *charset,
                    nsIURI *baseURI)

{
  nsresult rv;
  static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);    
  mStandardURL = do_CreateInstance(kStandardURLCID, &rv);
  NS_ASSERTION(mStandardURL, "Could not create a Standard URL");
  
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIStandardURL> surl = do_QueryInterface(mStandardURL);
  return surl->Init(urlType, defaultPort, spec, charset, baseURI);
}

NS_IMPL_ADDREF(nsSimpleChromeURL)
NS_IMPL_RELEASE(nsSimpleChromeURL)

  // DO we need to implements a QI for equals?
NS_INTERFACE_MAP_BEGIN(nsSimpleChromeURL)
    NS_INTERFACE_MAP_ENTRY(nsIURI)
    NS_INTERFACE_MAP_ENTRY(nsIURL)
    NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIFileURL, mChromeDir)
NS_INTERFACE_MAP_END


NS_IMETHODIMP 
nsSimpleChromeURL::SetFile(nsIFile * aFile) { return NS_ERROR_FAILURE; } 

NS_IMETHODIMP
nsSimpleChromeURL::GetFile(nsIFile **result)
{
    nsCAutoString fileName;
    GetFileName(fileName);

    nsCOMPtr<nsIFile> newFile;
    mChromeDir->Clone(getter_AddRefs(newFile));
    nsresult rv = newFile->AppendNative(fileName);
    NS_IF_ADDREF(*result = newFile);
    return rv;
}

nsSimpleChromeHandler::nsSimpleChromeHandler()
{
}

nsSimpleChromeHandler::~nsSimpleChromeHandler()
{
}

nsresult
nsSimpleChromeHandler::Init()
{
    nsresult rv;

    mIOService = do_GetIOService(&rv);
    if (NS_FAILED(rv)) return rv;

    rv = NS_GetSpecialDirectory(NS_APP_CHROME_DIR, getter_AddRefs(mChromeDir));
    
    return rv;
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsSimpleChromeHandler,
                              nsIProtocolHandler,
                              nsISupportsWeakReference)

NS_IMETHODIMP
nsSimpleChromeHandler::GetScheme(nsACString &result)
{
    result = "chrome";
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleChromeHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleChromeHandler::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_STD;
    return NS_OK;
}

static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);

NS_IMETHODIMP
nsSimpleChromeHandler::NewURI(const nsACString &aSpec,
                             const char *aCharset,
                             nsIURI *aBaseURI,
                             nsIURI **result)

{
    nsresult rv;

    nsSimpleChromeURL *chromeURL = new nsSimpleChromeURL(mChromeDir);
    if (!chromeURL)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(chromeURL);

    rv = chromeURL->Init(nsIStandardURL::URLTYPE_STANDARD, -1, aSpec, aCharset, aBaseURI);
    if (NS_SUCCEEDED(rv))
        rv = CallQueryInterface(chromeURL, result);
    
    NS_RELEASE(chromeURL);
    return rv;
}

NS_IMETHODIMP
nsSimpleChromeHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
    nsCOMPtr<nsIURL> url = do_QueryInterface(uri);
    if (!url) return NS_ERROR_UNEXPECTED;

    nsCAutoString fileName;
    url->GetFileName(fileName);

    nsCOMPtr<nsIFile> newFile;
    mChromeDir->Clone(getter_AddRefs(newFile));
    newFile->AppendNative(fileName);

    nsCOMPtr<nsIURI> resultingURI;
    mIOService->NewFileURI(newFile, getter_AddRefs(resultingURI));

    nsresult rv = NS_NewChannel(result, 
                                resultingURI,
                                mIOService);
    
    (*result)->SetOriginalURI(uri);
    return rv;
}

NS_IMETHODIMP 
nsSimpleChromeHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}

#define NS_SIMPLECHROMEPROTOCOLHANDLER_CLASSNAME \
    "nsSimpleChromeHandler"

#define NS_SIMPLECHROMEPROTOCOLHANDLER_CID           \
{ /* f6b3c2cc-b2a3-11d7-82a0-000802c1aa31 */         \
    0xf6b3c2cc,                                      \
    0xb2a3,                                          \
    0x11d7,                                          \
    {0x82, 0xa0, 0x00, 0x08, 0x02, 0xc1, 0xaa, 0x31} \
}


#define NS_SIMPLECHROMEREGISTRY_CID                  \
{ /* 5972e8f4-a3a6-44d0-8994-57bf4eeb066d */         \
    0x5972e8f4,                                      \
    0xa3a6,                                          \
    0x44d0,                                          \
    {0x89, 0x94, 0x57, 0xbf, 0x4e, 0xeb, 0x06, 0x6d} \
}

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSimpleChromeHandler, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSimpleChromeRegistry)

static const nsModuleComponentInfo components[] =
{
    { "Simple Chrome Registry", 
      NS_SIMPLECHROMEREGISTRY_CID,
      "@mozilla.org/chrome/chrome-registry;1", 
      nsSimpleChromeRegistryConstructor,
    },
    { NS_SIMPLECHROMEPROTOCOLHANDLER_CLASSNAME,
      NS_SIMPLECHROMEPROTOCOLHANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "chrome",
      nsSimpleChromeHandlerConstructor
    },
};

NS_IMPL_NSGETMODULE(chromelite, components)
