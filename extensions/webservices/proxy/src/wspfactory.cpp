/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   John Bandhauer (jband@netscape.com)
 *   Vidur Apparao (vidur@netscape.com)
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

#include "wspprivate.h"

// NSPR includes
#include "prprf.h"
#include "nsIWebServiceErrorHandler.h"

/***************************************************************************/
class WSPAsyncProxyCreator : public nsIWSDLLoadListener
{
public:
  WSPAsyncProxyCreator();
  virtual ~WSPAsyncProxyCreator();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLLOADLISTENER
  NS_DECL_NSIWEBSERVICEERRORHANDLER

  nsresult Run(const nsAString & wsdlURL, const nsAString & portname,
               const nsAString & qualifier, PRBool isAsync,
               nsIWebServiceProxyCreationListener* aListener);

private:
  nsString mWSDLURL;
  nsString mPortName;
  nsString mQualifier;
  PRBool   mIsAsync;
  nsCOMPtr<nsIWebServiceProxyCreationListener> mListener;
};

WSPAsyncProxyCreator::WSPAsyncProxyCreator()
{
}

WSPAsyncProxyCreator::~WSPAsyncProxyCreator()
{
  // do nothing...
}

NS_IMPL_ISUPPORTS2(WSPAsyncProxyCreator,
                   nsIWSDLLoadListener,
                   nsIWebServiceErrorHandler)

nsresult
WSPAsyncProxyCreator::Run(const nsAString& wsdlURL, const nsAString& portname,
                          const nsAString& qualifier, PRBool isAsync,
                          nsIWebServiceProxyCreationListener* aListener)
{
  mWSDLURL   = wsdlURL;
  mPortName  = portname;
  mQualifier = qualifier;
  mIsAsync   = isAsync;
  mListener  = aListener;

  nsresult rv;
  nsCOMPtr<nsIWSDLLoader> loader =
    do_CreateInstance(NS_WSDLLOADER_CONTRACTID, &rv);
  if (!loader) {
    return rv;
  }

  rv = loader->LoadAsync(mWSDLURL, mPortName, this);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

/* void onLoad (in nsIWSDLPort port); */
NS_IMETHODIMP
WSPAsyncProxyCreator::OnLoad(nsIWSDLPort *port)
{
  nsresult rv;

  nsCOMPtr<nsIWSPInterfaceInfoService> iis =
    do_GetService(NS_WSP_INTERFACEINFOSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return OnError(rv,
                   NS_LITERAL_STRING("Can't get nsIWSPInterfaceInfoService"));
  }
  nsCOMPtr<nsIInterfaceInfoManager> manager;
  nsCOMPtr<nsIInterfaceInfo> iinfo;

  rv = iis->InfoForPort(port, mWSDLURL, mQualifier, mIsAsync,
                        getter_AddRefs(manager), getter_AddRefs(iinfo));
  if (NS_FAILED(rv)) {
    return OnError(rv,
                   NS_LITERAL_STRING("Couldn't find interface info for port"));
  }

  nsCOMPtr<nsIWebServiceProxy> proxy =
    do_CreateInstance(NS_WEBSERVICEPROXY_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return OnError(rv, NS_LITERAL_STRING("Couldn't create proxy"));
  }

  // XXX We want to pass the manager too.
  rv = proxy->Init(port, iinfo, manager, mQualifier, mIsAsync);
  if (NS_FAILED(rv)) {
    return OnError(rv, NS_LITERAL_STRING("Couldn't init proxy"));
  }

  mListener->OnLoad(proxy);
  return NS_OK;
}

/* void onError (in nsresult status, in AString statusMessage); */
NS_IMETHODIMP
WSPAsyncProxyCreator::OnError(nsresult status, const nsAString & statusMessage)
{
  // XXX need to build an exception. It would be nice to have a generic
  // exception class!

  nsCOMPtr<nsIException> e = new WSPException(status, 
    NS_ConvertUCS2toUTF8(statusMessage).get(), nsnull);
  if (!e) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mListener->OnError(e);
  return NS_OK;
}


/***************************************************************************/

WSPFactory::WSPFactory()
{
}

WSPFactory::~WSPFactory()
{
}

NS_IMPL_ISUPPORTS1_CI(WSPFactory, nsIWebServiceProxyFactory)

/* nsIWebServiceProxy createProxy (in AString wsdlURL, in AString portname, in AString qualifier, in boolean isAsync); */
NS_IMETHODIMP
WSPFactory::CreateProxy(const nsAString & wsdlURL,
                        const nsAString & portname,
                        const nsAString & qualifier,
                        PRBool isAsync,
                        nsIWebServiceProxy **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void createProxyAsync (in AString wsdlURL, in AString portname, in AString qualifier, in boolean isAsync, in nsIWebServiceProxyCreationListener listener); */
NS_IMETHODIMP
WSPFactory::CreateProxyAsync(const nsAString& wsdlURL,
                             const nsAString& portname,
                             const nsAString& qualifier, PRBool isAsync,
                             nsIWebServiceProxyCreationListener *listener)
{
  if (!listener) {
    // A listener is required.
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<WSPAsyncProxyCreator> creator = new WSPAsyncProxyCreator();
  if(!creator)
    return NS_ERROR_OUT_OF_MEMORY;
  nsresult rv = creator->Run(wsdlURL, portname, qualifier, isAsync, listener);
  if (rv == NS_ERROR_WSDL_NOT_ENABLED)
    rv = creator->OnError(rv, NS_LITERAL_STRING("WSDL not enabled"));
  return rv;
}


#define P2M_ESCAPE_CHARACTER '_'

nsresult
WSPFactory::C2XML(const nsACString& aCIdentifier,
                  nsAString& aXMLIdentifier)
{
  nsReadingIterator<char> current, end;

  aXMLIdentifier.Truncate();
  aCIdentifier.BeginReading(current);
  aCIdentifier.EndReading(end);

  while (current != end) {
    char ch = *current++;
    PRUnichar uch;
    if (ch == P2M_ESCAPE_CHARACTER) {
      // Grab the next 4 characters that make up the
      // escape sequence
      PRUint16 i;
      PRUint16 acc = 0;
      for (i = 0; (i < 4) && (current != end); i++) {
        acc <<= 4;
        ch = *current++;
        if (('0' <= ch) && (ch <= '9')) {
          acc += ch - '0';
        }
        else if (('a' <= ch) && (ch <= 'f')) {
          acc += ch - ('a' - 10);
        }
        else if (('A' <= ch) && (ch <= 'F')) {
          acc += ch - ('A' - 10);
        }
        else {
          return NS_ERROR_FAILURE;
        }
      }

      // If we didn't get through the entire escape sequence, then
      // it's an error.
      if (i < 4) {
        return NS_ERROR_FAILURE;
      }

      uch = PRUnichar(acc);
    }
    else {
      uch = PRUnichar(ch);
    }
    aXMLIdentifier.Append(uch);
  }

  return NS_OK;
}

void
WSPFactory::XML2C(const nsAString& aXMLIndentifier,
                  nsACString& aCIdentifier)
{
  nsReadingIterator<PRUnichar> current, end;

  aCIdentifier.Truncate();
  aXMLIndentifier.BeginReading(current);
  aXMLIndentifier.EndReading(end);

  while (current != end) {
    PRUnichar uch = *current++;
    if (((PRUnichar('a') <= uch) && (uch <= PRUnichar('z'))) ||
        ((PRUnichar('A') <= uch) && (uch <= PRUnichar('Z'))) ||
        ((PRUnichar('0') <= uch) && (uch <= PRUnichar('9')))) {
      // Casting is safe since we know that it's an ASCII character
      aCIdentifier.Append(char(uch));
    }
    else {
      // Escape the character and append to the string
      char buf[6];
      buf[0] = P2M_ESCAPE_CHARACTER;

      for (int i = 3; i >= 0; i--) {
        PRUint16 v = (uch >> 4*i) & 0xf;
        buf[4-i] = (char) (v + ((v > 9) ? 'a'-10 : '0'));
      }

      buf[5] = 0;

      aCIdentifier.Append(buf, 5);
    }
  }
}
