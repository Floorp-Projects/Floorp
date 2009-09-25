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

static nsIContent*
GetRootFocusedContentAndWindow(nsPIDOMWindow* aContextWindow,
                               nsPIDOMWindow** aWindow)
{
  *aWindow = nsnull;

  if (aContextWindow) {
    nsCOMPtr<nsPIDOMWindow> rootWindow = aContextWindow->GetPrivateRoot();
    if (rootWindow) {
      return nsFocusManager::GetFocusedDescendant(rootWindow, PR_TRUE, aWindow);
    }
  }

  return nsnull;
}

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
nsFocusController::GetControllers(nsPIDOMWindow* aContextWindow, nsIControllers** aResult)
{
  *aResult = nsnull;

  // XXX: we should fix this so there's a generic interface that
  // describes controllers, so this code would have no special
  // knowledge of what object might have controllers.

  nsCOMPtr<nsPIDOMWindow> focusedWindow;
  nsIContent* focusedContent =
    GetRootFocusedContentAndWindow(aContextWindow, getter_AddRefs(focusedWindow));
  if (focusedContent) {
#ifdef MOZ_XUL
    nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(focusedContent));
    if (xulElement)
      return xulElement->GetControllers(aResult);
#endif

    nsCOMPtr<nsIDOMNSHTMLTextAreaElement> htmlTextArea =
      do_QueryInterface(focusedContent);
    if (htmlTextArea)
      return htmlTextArea->GetControllers(aResult);

    nsCOMPtr<nsIDOMNSHTMLInputElement> htmlInputElement =
      do_QueryInterface(focusedContent);
    if (htmlInputElement)
      return htmlInputElement->GetControllers(aResult);

    if (focusedContent->IsEditable() && focusedWindow)
      return focusedWindow->GetControllers(aResult);
  }
  else {
    nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(focusedWindow);
    if (domWindow)
      return domWindow->GetControllers(aResult);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::GetControllerForCommand(nsPIDOMWindow* aContextWindow,
                                           const char * aCommand,
                                           nsIController** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  nsCOMPtr<nsIControllers> controllers;
  nsCOMPtr<nsIController> controller;

  GetControllers(aContextWindow, getter_AddRefs(controllers));
  if(controllers) {
    controllers->GetControllerForCommand(aCommand, getter_AddRefs(controller));
    if(controller) {
      controller.swap(*_retval);
      return NS_OK;
    }
  }

  nsCOMPtr<nsPIDOMWindow> focusedWindow;
  GetRootFocusedContentAndWindow(aContextWindow, getter_AddRefs(focusedWindow));
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
