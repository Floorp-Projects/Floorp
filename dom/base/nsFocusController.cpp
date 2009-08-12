/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David W. Hyatt <hyatt@netscape.com> (Original Author)
 *   Dan Rosen <dr@netscape.com>
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

#include "nsIContent.h"
#include "nsIControllers.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDocument.h"
#include "nsPIDOMWindow.h"
#include "nsFocusController.h"
#include "prlog.h"
#include "nsGlobalWindow.h"
#include "nsFocusManager.h"

#ifdef MOZ_XUL
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULElement.h"
#endif

////////////////////////////////////////////////////////////////////////

NS_IMPL_CYCLE_COLLECTION_CLASS(nsFocusController)

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsFocusController, nsIFocusController)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsFocusController,
                                           nsIFocusController)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFocusController)
  NS_INTERFACE_MAP_ENTRY(nsIFocusController)
  NS_INTERFACE_MAP_ENTRY(nsSupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIFocusController)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_0(nsFocusController)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsFocusController)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mPopupNode)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMETHODIMP
nsFocusController::Create(nsIFocusController** aResult)
{
  nsFocusController* controller = new nsFocusController();
  if (!controller)
    return NS_ERROR_OUT_OF_MEMORY;

  *aResult = controller;
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::GetControllers(nsIControllers** aResult)
{
  // XXX: we should fix this so there's a generic interface that
  // describes controllers, so this code would have no special
  // knowledge of what object might have controllers.
  nsCOMPtr<nsIDOMElement> focusedElement;
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    *aResult = nsnull;
    return NS_OK;
  }

  fm->GetFocusedElement(getter_AddRefs(focusedElement));
  if (focusedElement) {
#ifdef MOZ_XUL
    nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(focusedElement));
    if (xulElement)
      return xulElement->GetControllers(aResult);
#endif

    nsCOMPtr<nsIDOMNSHTMLTextAreaElement> htmlTextArea =
      do_QueryInterface(focusedElement);
    if (htmlTextArea)
      return htmlTextArea->GetControllers(aResult);

    nsCOMPtr<nsIDOMNSHTMLInputElement> htmlInputElement =
      do_QueryInterface(focusedElement);
    if (htmlInputElement)
      return htmlInputElement->GetControllers(aResult);

    nsCOMPtr<nsIContent> content = do_QueryInterface(focusedElement);
    if (content && content->IsEditable()) {
      // Move up to the window.
      nsCOMPtr<nsIDOMDocument> domDoc;
      focusedElement->GetOwnerDocument(getter_AddRefs(domDoc));
      nsCOMPtr<nsIDOMWindowInternal> domWindow =
        do_QueryInterface(GetWindowFromDocument(domDoc));
      if (domWindow)
        return domWindow->GetControllers(aResult);
    }
  }
  else {
    nsCOMPtr<nsIDOMWindow> focusedWindow;
    fm->GetFocusedWindow(getter_AddRefs(focusedWindow));
    nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(focusedWindow);
    if (domWindow)
      return domWindow->GetControllers(aResult);
  }

  *aResult = nsnull;
  return NS_OK;
}


nsPIDOMWindow *
nsFocusController::GetWindowFromDocument(nsIDOMDocument* aDocument)
{
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDocument);
  if (!doc)
    return nsnull;

  return doc->GetWindow();
}

NS_IMETHODIMP
nsFocusController::GetControllerForCommand(const char * aCommand,
                                           nsIController** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);	
  *_retval = nsnull;

  nsCOMPtr<nsIControllers> controllers;
  nsCOMPtr<nsIController> controller;

  GetControllers(getter_AddRefs(controllers));
  if(controllers) {
    controllers->GetControllerForCommand(aCommand, getter_AddRefs(controller));
    if(controller) {
      controller.swap(*_retval);
      return NS_OK;
    }
  }

  nsCOMPtr<nsIDOMElement> focusedElement;
  nsCOMPtr<nsIDOMWindow> focusedWindow;
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    fm->GetFocusedElement(getter_AddRefs(focusedElement));
    fm->GetFocusedWindow(getter_AddRefs(focusedWindow));
  }

  while (focusedWindow) {
    nsCOMPtr<nsIDOMWindowInternal> domWindow(do_QueryInterface(focusedWindow));

    nsCOMPtr<nsIControllers> controllers2;
    domWindow->GetControllers(getter_AddRefs(controllers2));
    if(controllers2) {
      controllers2->GetControllerForCommand(aCommand,
                                            getter_AddRefs(controller));
      if(controller) {
        controller.swap(*_retval);
        return NS_OK;
      }
    }

    // XXXndeakin P3 is this casting safe?
    nsCOMPtr<nsPIDOMWindow> piWindow = do_QueryInterface(focusedWindow); 
    nsGlobalWindow *win =
      static_cast<nsGlobalWindow *>
                 (static_cast<nsIDOMWindowInternal *>(piWindow));
    focusedWindow = win->GetPrivateParent();
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::GetPopupNode(nsIDOMNode** aNode)
{
#ifdef DEBUG_dr
  printf("dr :: nsFocusController::GetPopupNode\n");
#endif

  *aNode = mPopupNode;
  NS_IF_ADDREF(*aNode);
  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::SetPopupNode(nsIDOMNode* aNode)
{
#ifdef DEBUG_dr
  printf("dr :: nsFocusController::SetPopupNode\n");
#endif

  if (aNode) {
    nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
    NS_ENSURE_ARG(node);
  }
  mPopupNode = aNode;
  return NS_OK;
}
