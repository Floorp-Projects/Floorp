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
 * The Original Code is Mozilla Communicator
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Rick Potts <rpotts@netscape.com>
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

#include "nsScriptEventManager.h"

#include "nsString.h"
#include "nsReadableUtils.h"

#include "nsIDOMNode.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMDocument.h"

#include "nsIScriptElement.h"

#include "nsIScriptEventHandler.h"
#include "nsIDocument.h"

nsScriptEventManager::nsScriptEventManager(nsIDOMDocument *aDocument)
{
  nsresult rv;

  NS_INIT_ISUPPORTS();

  rv = aDocument->GetElementsByTagName(NS_LITERAL_STRING("script"),
                                       getter_AddRefs(mScriptElements));

}


nsScriptEventManager::~nsScriptEventManager()
{
}


NS_IMPL_ISUPPORTS1(nsScriptEventManager, nsIScriptEventManager)


NS_IMETHODIMP
nsScriptEventManager::FindEventHandler(const nsAString &aObjectName,
                                       const nsAString &aEventName,
                                       PRUint32 aArgCount,
                                       nsISupports **aScriptHandler)
{
  nsresult rv;

  if (!mScriptElements) {
    return NS_ERROR_FAILURE;
  }

  // Null out the return value...
  if (!aScriptHandler) {
    return NS_ERROR_NULL_POINTER;
  }
  *aScriptHandler = nsnull;

  // Get the number of script elements in the current document...
  PRUint32 count = 0;
  rv = mScriptElements->GetLength(&count);
  if (NS_FAILED(rv)) {
    return rv;
  }

  //
  // Iterate over all of the SCRIPT elements looking for a handler.
  //
  // Walk the list backwards in order to pick up the most recently
  // defined script handler (if more than one is present)...
  //
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIScriptEventHandler> handler;

  while (count--) {
    rv = mScriptElements->Item(count, getter_AddRefs(node));
    if (NS_FAILED(rv)) break;

    // Check if the SCRIPT element is an event handler.
    handler = do_QueryInterface(node, &rv);
    if (NS_FAILED(rv)) continue;

    PRBool bFound = PR_FALSE;
    rv = handler->IsSameEvent(aObjectName, aEventName, aArgCount, &bFound);

    if (NS_SUCCEEDED(rv) && bFound) {
        *aScriptHandler = handler;
        NS_ADDREF(*aScriptHandler);

        return NS_OK;
    }
  }

  // Errors just fall of the end and return 'rv'
  return rv;
}

NS_IMETHODIMP
nsScriptEventManager::InvokeEventHandler(nsISupports *aHandler,
                                         nsISupports *aTargetObject,
                                         void * aArgs,
                                         PRUint32 aArgCount)
{
  nsCOMPtr<nsIScriptEventHandler> handler(do_QueryInterface(aHandler));

  if (!handler) {
    return NS_ERROR_FAILURE;
  }

  return handler->Invoke(aTargetObject, aArgs, aArgCount);
}
