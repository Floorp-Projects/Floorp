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

#include "nsSOAPResponse.h"
#include "nsSOAPUtils.h"
#include "nsSOAPFault.h"
#include "nsISOAPFault.h"
#include "nsCOMPtr.h"
#include "nsISOAPParameter.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"

nsSOAPResponse::nsSOAPResponse()
{
}

nsSOAPResponse::~nsSOAPResponse()
{
  /* destructor code */
}

NS_IMPL_CI_INTERFACE_GETTER2(nsSOAPResponse, nsISOAPMessage,
                             nsISOAPResponse)
    NS_IMPL_ADDREF_INHERITED(nsSOAPResponse, nsSOAPMessage)
    NS_IMPL_RELEASE_INHERITED(nsSOAPResponse, nsSOAPMessage)
    NS_INTERFACE_MAP_BEGIN(nsSOAPResponse)
    NS_INTERFACE_MAP_ENTRY(nsISOAPResponse)
    NS_IMPL_QUERY_CLASSINFO(nsSOAPResponse)
    NS_INTERFACE_MAP_END_INHERITING(nsSOAPMessage)
/* readonly attribute nsISOAPFault fault; */
NS_IMETHODIMP nsSOAPResponse::GetFault(nsISOAPFault * *aFault)
{
  NS_ENSURE_ARG_POINTER(aFault);
  nsCOMPtr < nsIDOMElement > body;

  *aFault = nsnull;
  nsresult rc = GetBody(getter_AddRefs(body));
  if (NS_FAILED(rc))
    return rc;
  if (body) {
    PRUint16 version;
    rc = GetVersion(&version);
    if (NS_FAILED(rc))
      return rc;
    if (rc != nsSOAPMessage::VERSION_UNKNOWN) {
      nsCOMPtr < nsIDOMElement > fault;
      nsSOAPUtils::GetSpecificChildElement(nsnull, body,
                                           *gSOAPStrings->kSOAPEnvURI[version],
                                           gSOAPStrings->kFaultTagName,
                                           getter_AddRefs(fault));
      if (fault) {
        nsCOMPtr < nsISOAPFault > f =
            do_CreateInstance(NS_SOAPFAULT_CONTRACTID);
        if (!f)
          return NS_ERROR_OUT_OF_MEMORY;
        rc = f->SetElement(fault);
        if (NS_FAILED(rc))
          return rc;
        *aFault = f;
        NS_ADDREF(*aFault);
      }
    }
  } else {
    *aFault = nsnull;
  }
  return NS_OK;
}
