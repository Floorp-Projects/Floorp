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
 * Portions created by the Initial Developer are Copyright (C) 2001-2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
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

#include "nsCidProtocolHandler.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsNetCID.h"
#include "nsComponentManagerUtils.h"

nsCidProtocolHandler::nsCidProtocolHandler()
{
}

nsCidProtocolHandler::~nsCidProtocolHandler()
{
}

NS_IMPL_ISUPPORTS1(nsCidProtocolHandler, nsIProtocolHandler)

NS_IMETHODIMP nsCidProtocolHandler::GetScheme(nsACString & aScheme)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCidProtocolHandler::GetDefaultPort(PRInt32 *aDefaultPort)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCidProtocolHandler::GetProtocolFlags(PRUint32 *aProtocolFlags)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCidProtocolHandler::NewURI(const nsACString & aSpec, const char *aOriginCharset, nsIURI *aBaseURI, nsIURI **_retval)
{
  nsresult rv;
  nsCOMPtr <nsIURI> url = do_CreateInstance(NS_SIMPLEURI_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // the right fix is to use the baseSpec (or aBaseUri)
  // and specify the cid, and then fix mime
  // to handle that, like it does with "...&part=1.2"
  // for now, do about blank to prevent spam
  // from popping up annoying alerts about not implementing the cid
  // protocol
  rv = url->SetSpec(nsDependentCString("about:blank"));
  NS_ENSURE_SUCCESS(rv,rv);

  NS_IF_ADDREF(*_retval = url);
  return NS_OK;
}

NS_IMETHODIMP nsCidProtocolHandler::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCidProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

