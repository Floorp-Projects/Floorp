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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "msgCore.h"    // precompiled header...
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIIOService.h"

#include "nsIStreamListener.h"
#include "nsAddbookProtocolHandler.h"

#include "nsAddbookUrl.h"
#include "nsAddbookProtocolHandler.h"
#include "nsIFileSpec.h"
#include "nsCOMPtr.h"
#include "nsAbBaseCID.h"
#include "nsNetUtil.h"
#include "nsIStringStream.h"
#include "nsIAddrBookSession.h"
#include "nsIAbMDBCard.h"
#include "nsIAbDirectory.h"
#include "nsIRDFResource.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "prmem.h"

#include "nsIStringBundle.h"
#include "nsIServiceManager.h"

nsAddbookProtocolHandler::nsAddbookProtocolHandler()
{
  mAddbookOperation = nsIAddbookUrlOperation::InvalidUrl;
}

nsAddbookProtocolHandler::~nsAddbookProtocolHandler()
{
}

NS_IMPL_ISUPPORTS1(nsAddbookProtocolHandler, nsIProtocolHandler)

NS_IMETHODIMP nsAddbookProtocolHandler::GetScheme(nsACString &aScheme)
{
	aScheme = "addbook";
	return NS_OK; 
}

NS_IMETHODIMP nsAddbookProtocolHandler::GetDefaultPort(PRInt32 *aDefaultPort)
{
  return NS_OK;
}

NS_IMETHODIMP nsAddbookProtocolHandler::GetProtocolFlags(PRUint32 *aUritype)
{
  *aUritype = URI_STD;
  return NS_OK;
}

NS_IMETHODIMP nsAddbookProtocolHandler::NewURI(const nsACString &aSpec,
                                               const char *aOriginCharset, // ignored
                                               nsIURI *aBaseURI,
                                               nsIURI **_retval)
{
  nsresult rv;
	nsCOMPtr <nsIAddbookUrl> addbookUrl = do_CreateInstance(NS_ADDBOOKURL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = addbookUrl->SetSpec(aSpec);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIURI> uri = do_QueryInterface(addbookUrl, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  NS_ADDREF(*_retval = uri);
  return NS_OK;
}

NS_IMETHODIMP 
nsAddbookProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}

nsresult
nsAddbookProtocolHandler::GenerateXMLOutputChannel( nsString &aOutput,
                                                     nsIAddbookUrl *addbookUrl,
                                                     nsIURI *aURI, 
                                                     nsIChannel **_retval)
{
  nsIChannel                *channel;
  nsCOMPtr<nsIInputStream>  inStr;
  NS_ConvertUCS2toUTF8 utf8String(aOutput.get());

  nsresult rv = NS_NewCStringInputStream(getter_AddRefs(inStr), utf8String);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = NS_NewInputStreamChannel(&channel, aURI, inStr,
                                NS_LITERAL_CSTRING("text/xml"));
  NS_ENSURE_SUCCESS(rv, rv);
  
  *_retval = channel;
  return rv;
}

NS_IMETHODIMP 
nsAddbookProtocolHandler::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
  nsresult rv;
  nsCOMPtr <nsIAddbookUrl> addbookUrl = do_QueryInterface(aURI, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = addbookUrl->GetAddbookOperation(&mAddbookOperation);
  NS_ENSURE_SUCCESS(rv,rv);

  if (mAddbookOperation == nsIAddbookUrlOperation::InvalidUrl) {
    nsAutoString errorString;
    errorString.Append(NS_LITERAL_STRING("Unsupported format/operation requested for ").get());
    nsCAutoString spec;
    rv = aURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv,rv);

    errorString.Append(NS_ConvertUTF8toUCS2(spec));
    rv = GenerateXMLOutputChannel(errorString, addbookUrl, aURI, _retval);
    NS_ENSURE_SUCCESS(rv,rv);
    return NS_OK;
  }
 
  if (mAddbookOperation == nsIAddbookUrlOperation::AddVCard) {
      // create an empty pipe for use with the input stream channel.
      nsCOMPtr<nsIInputStream> pipeIn;
      nsCOMPtr<nsIOutputStream> pipeOut;
      rv = NS_NewPipe(getter_AddRefs(pipeIn),
          getter_AddRefs(pipeOut));
      if (NS_FAILED(rv)) 
          return rv;
      
      pipeOut->Close();
      
      return NS_NewInputStreamChannel(_retval, aURI, pipeIn,
          NS_LITERAL_CSTRING("x-application-addvcard"));
  }

  nsString output;
  rv = GeneratePrintOutput(addbookUrl, output);
  if (NS_FAILED(rv)) {
    output.Assign(NS_LITERAL_STRING("failed to print. url=").get());
    nsCAutoString spec;
    rv = aURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv,rv);
    output.Append(NS_ConvertUTF8toUCS2(spec));
  }
 
  rv = GenerateXMLOutputChannel(output, addbookUrl, aURI, _retval);
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}

nsresult    
nsAddbookProtocolHandler::GeneratePrintOutput(nsIAddbookUrl *addbookUrl, 
                                              nsString &aOutput)
{
  NS_ENSURE_ARG_POINTER(addbookUrl);

  nsCAutoString uri;
  nsresult rv = addbookUrl->GetPath(uri);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIRDFService> rdfService = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  /* turn
   "//moz-abmdbdirectory/abook.mab?action=print"
   into "moz-abmdbdirectory://abook.mab"
  */

  /* step 1:  
   turn "//moz-abmdbdirectory/abook.mab?action=print"
   into "moz-abmdbdirectory/abook.mab?action=print"
   */
  if (uri[0] != '/' && uri[1] != '/')
    return NS_ERROR_UNEXPECTED;

  uri.Cut(0,2);

  /* step 2:  
   turn "moz-abmdbdirectory/abook.mab?action=print"
   into "moz-abmdbdirectory/abook.mab"
   */
  PRInt32 pos = uri.Find("?action=print");
	if (pos == kNotFound)
    return NS_ERROR_UNEXPECTED;

  uri.Truncate(pos);

  /* step 2:  
   turn "moz-abmdbdirectory/abook.mab"
   into "moz-abmdbdirectory://abook.mab"
   */
  pos = uri.Find("/");
  if (pos == kNotFound)
    return NS_ERROR_UNEXPECTED;

  uri.Insert('/', pos);
  uri.Insert(':', pos);

  nsCOMPtr <nsIRDFResource> resource;
  rv = rdfService->GetResource(uri, getter_AddRefs(resource));
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsCOMPtr <nsIAbDirectory> directory = do_QueryInterface(resource, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = BuildDirectoryXML(directory, aOutput);
  NS_ENSURE_SUCCESS(rv,rv);

  return NS_OK;
}

nsresult
nsAddbookProtocolHandler::BuildDirectoryXML(nsIAbDirectory *aDirectory, 
                                       nsString &aOutput)
{
  NS_ENSURE_ARG_POINTER(aDirectory);

  nsresult rv;    
  nsCOMPtr<nsIEnumerator> cardsEnumerator;
  nsCOMPtr<nsIAbCard> card;

  aOutput.Append(NS_LITERAL_STRING("<?xml version=\"1.0\"?>\n").get());
  aOutput.Append(NS_LITERAL_STRING("<?xml-stylesheet type=\"text/css\" href=\"chrome://messenger/content/addressbook/print.css\"?>\n").get());
  aOutput.Append(NS_LITERAL_STRING("<directory>\n").get());

  // Get Address Book string and set it as title of XML document
  nsCOMPtr<nsIStringBundle> bundle;
  nsCOMPtr<nsIStringBundleService> stringBundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv); 
  if (NS_SUCCEEDED(rv)) {
    rv = stringBundleService->CreateBundle("chrome://messenger/locale/addressbook/addressBook.properties", getter_AddRefs(bundle));
    if (NS_SUCCEEDED(rv)) {
      nsXPIDLString addrBook;
      rv = bundle->GetStringFromName(NS_LITERAL_STRING("addressBook").get(), getter_Copies(addrBook));
      if (NS_SUCCEEDED(rv)) {
        aOutput.Append(NS_LITERAL_STRING("<title xmlns=\"http://www.w3.org/1999/xhtml\">").get());
        aOutput.Append(addrBook);
        aOutput.Append(NS_LITERAL_STRING("</title>\n").get());
      }
    }
  }

 
  rv = aDirectory->GetChildCards(getter_AddRefs(cardsEnumerator));
  if (NS_SUCCEEDED(rv) && cardsEnumerator)
  {
    nsCOMPtr<nsISupports> item;
    for (rv = cardsEnumerator->First(); NS_SUCCEEDED(rv); rv = cardsEnumerator->Next())
    {
      rv = cardsEnumerator->CurrentItem(getter_AddRefs(item));
      if (NS_SUCCEEDED(rv))
      {
        nsCOMPtr <nsIAbCard> card = do_QueryInterface(item);
        nsXPIDLString xmlSubstr;

        rv = card->ConvertToXMLPrintData(getter_Copies(xmlSubstr));
        NS_ENSURE_SUCCESS(rv,rv);

        aOutput.Append(NS_LITERAL_STRING("<separator/>").get());
        aOutput.Append(xmlSubstr.get());
      }
    }
	aOutput.Append(NS_LITERAL_STRING("<separator/>").get());
  }

  aOutput.Append(NS_LITERAL_STRING("</directory>\n").get());

  return NS_OK;
}
