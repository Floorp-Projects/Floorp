/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 * Contributor(s): Brendan Eich (brendan@mozilla.org)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsXBLPrototypeHandler.h"
#include "nsXBLEventHandler.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsINameSpaceManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIJSEventListener.h"
#include "nsIController.h"
#include "nsIControllers.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIDOMText.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventListener.h"
#include "nsXBLBinding.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMWindowInternal.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsXPIDLString.h"

#include "nsIDOMFocusListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMXULListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMDragListener.h"
#include "nsIDOMScrollListener.h"
#include "nsIDOMFormListener.h"
#include "nsXBLAtoms.h"

nsXBLEventHandler::nsXBLEventHandler(nsIDOMEventReceiver* aEventReceiver, nsIXBLPrototypeHandler* aHandler)
{
  NS_INIT_REFCNT();
  mEventReceiver = aEventReceiver;
  mProtoHandler = aHandler;
  mNextHandler = nsnull;
}

nsXBLEventHandler::~nsXBLEventHandler()
{
}

NS_IMPL_ISUPPORTS1(nsXBLEventHandler, nsISupports)

void
nsXBLEventHandler::RemoveEventHandlers()
{
  if (mNextHandler)
    mNextHandler->RemoveEventHandlers();

  // Figure out if we're using capturing or not.
  if (!mProtoHandler)
    return;

  nsCOMPtr<nsIAtom> eventName;
  mProtoHandler->GetEventName(getter_AddRefs(eventName));

  nsAutoString type;
  eventName->ToString(type);
  
  PRUint8 phase;
  mProtoHandler->GetPhase(&phase);
  PRBool useCapture = (phase == NS_PHASE_CAPTURING);
  
  PRBool found = PR_FALSE;
  nsIID iid;
  nsXBLBinding::GetEventHandlerIID(eventName, &iid, &found);

  nsCOMPtr<nsIDOMEventListener> listener(do_QueryInterface(this));

  if (found && listener)
    mEventReceiver->RemoveEventListener(type, listener, useCapture);
}

/// Helpers that are relegated to the end of the file /////////////////////////////

nsresult
nsXBLEventHandler::GetTextData(nsIContent *aParent, nsAWritableString& aResult)
{
  aResult.Truncate(0);

  nsCOMPtr<nsIContent> textChild;
  PRInt32 textCount;
  aParent->ChildCount(textCount);

  for (PRInt32 j = 0; j < textCount; j++) {
    // Get the child.
    aParent->ChildAt(j, *getter_AddRefs(textChild));
    nsCOMPtr<nsIDOMText> text(do_QueryInterface(textChild));
    if (text) {
      nsAutoString data;
      text->GetData(data);
      aResult.Append(data);
    }
  }
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLEventHandler(nsIDOMEventReceiver* aRec, nsIXBLPrototypeHandler* aHandler, 
                      nsXBLEventHandler** aResult)
{
  *aResult = new nsXBLEventHandler(aRec, aHandler);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
