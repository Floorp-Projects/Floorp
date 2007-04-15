/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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
 * Mozilla Japan.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#include "nsIMEStateManager.h"
#include "nsCOMPtr.h"
#include "nsIWidget.h"
#include "nsIViewManager.h"
#include "nsIViewObserver.h"
#include "nsIPresShell.h"
#include "nsISupports.h"
#include "nsPIDOMWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIEditorDocShell.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsIKBStateControl.h"
#include "nsIFocusController.h"
#include "nsIDOMWindow.h"
#include "nsContentUtils.h"

/******************************************************************/
/* nsIMEStateManager                                              */
/******************************************************************/

nsIContent*    nsIMEStateManager::sContent      = nsnull;
nsPresContext* nsIMEStateManager::sPresContext  = nsnull;
nsPIDOMWindow* nsIMEStateManager::sActiveWindow = nsnull;

nsresult
nsIMEStateManager::OnDestroyPresContext(nsPresContext* aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  if (aPresContext != sPresContext)
    return NS_OK;
  sContent = nsnull;
  sPresContext = nsnull;
  return NS_OK;
}

nsresult
nsIMEStateManager::OnRemoveContent(nsPresContext* aPresContext,
                                   nsIContent* aContent)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  if (!sPresContext || !sContent ||
      aPresContext != sPresContext ||
      aContent != sContent)
    return NS_OK;

  // Current IME transaction should commit
  nsIKBStateControl* kb = GetKBStateControl(sPresContext);
  if (kb) {
    nsresult rv = kb->CancelIMEComposition();
    if (NS_FAILED(rv))
      kb->ResetInputState();
  }

  sContent = nsnull;
  sPresContext = nsnull;

  return NS_OK;
}

nsresult
nsIMEStateManager::OnChangeFocus(nsPresContext* aPresContext,
                                 nsIContent* aContent)
{
  NS_ENSURE_ARG_POINTER(aPresContext);

  if (!IsActive(aPresContext)) {
    // The actual focus isn't changing, because this presContext isn't active.
    return NS_OK;
  }

  nsIKBStateControl* kb = GetKBStateControl(aPresContext);
  if (!kb) {
    // This platform doesn't support IME controlling
    return NS_OK;
  }

  PRUint32 newState = GetNewIMEState(aPresContext, aContent);
  if (aPresContext == sPresContext && aContent == sContent) {
    // actual focus isn't changing, but if IME enabled state is changing,
    // we should do it.
    PRUint32 newEnabledState = newState & nsIContent::IME_STATUS_MASK_ENABLED;
    if (newEnabledState == 0) {
      // the enabled state isn't changing, we should do nothing.
      return NS_OK;
    }
    PRUint32 enabled;
    if (NS_FAILED(kb->GetIMEEnabled(&enabled))) {
      // this platform doesn't support IME controlling
      return NS_OK;
    }
    if (enabled ==
        nsContentUtils::GetKBStateControlStatusFromIMEStatus(newEnabledState)) {
      // the enabled state isn't changing.
      return NS_OK;
    }
  }

  // Current IME transaction should commit
  if (sPresContext) {
    nsIKBStateControl* oldKB;
    if (sPresContext == aPresContext)
      oldKB = kb;
    else
      oldKB = GetKBStateControl(sPresContext);
    if (oldKB)
      oldKB->ResetInputState();
  }

  if (newState != nsIContent::IME_STATUS_NONE) {
    // Update IME state for new focus widget
    SetIMEState(aPresContext, newState, kb);
  }

  sPresContext = aPresContext;
  sContent = aContent;

  return NS_OK;
}

nsresult
nsIMEStateManager::OnActivate(nsPresContext* aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  sActiveWindow = aPresContext->Document()->GetWindow();
  NS_ENSURE_TRUE(sActiveWindow, NS_ERROR_FAILURE);
  sActiveWindow = sActiveWindow->GetPrivateRoot();
  return NS_OK;
}

nsresult
nsIMEStateManager::OnDeactivate(nsPresContext* aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  NS_ENSURE_TRUE(aPresContext->Document()->GetWindow(), NS_ERROR_FAILURE);
  if (sActiveWindow !=
      aPresContext->Document()->GetWindow()->GetPrivateRoot())
    return NS_OK;

  sActiveWindow = nsnull;
#ifdef NS_KBSC_USE_SHARED_CONTEXT
  // Reset the latest content. When the window is activated, the IME state
  // may be changed on other applications.
  sContent = nsnull;
  // We should enable the IME state for other applications.
  nsIKBStateControl* kb = GetKBStateControl(aPresContext);
  if (kb)
    SetIMEState(aPresContext, nsIContent::IME_STATUS_ENABLE, kb);
#endif // NS_KBSC_USE_SHARED_CONTEXT
  return NS_OK;
}

PRBool
nsIMEStateManager::IsActive(nsPresContext* aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  nsPIDOMWindow* window = aPresContext->Document()->GetWindow();
  NS_ENSURE_TRUE(window, PR_FALSE);
  if (!sActiveWindow || sActiveWindow != window->GetPrivateRoot()) {
    // This root window is not active.
    return PR_FALSE;
  }
  NS_ENSURE_TRUE(aPresContext->GetPresShell(), PR_FALSE);
  nsIViewManager* vm = aPresContext->GetViewManager();
  NS_ENSURE_TRUE(vm, PR_FALSE);
  nsCOMPtr<nsIViewObserver> observer;
  vm->GetViewObserver(*getter_AddRefs(observer));
  NS_ENSURE_TRUE(observer, PR_FALSE);
  return observer->IsVisible();
}

nsIFocusController*
nsIMEStateManager::GetFocusController(nsPresContext* aPresContext)
{
  nsCOMPtr<nsISupports> container =
    aPresContext->Document()->GetContainer();
  nsCOMPtr<nsPIDOMWindow> windowPrivate = do_GetInterface(container);

  return windowPrivate ? windowPrivate->GetRootFocusController() : nsnull;
}

PRUint32
nsIMEStateManager::GetNewIMEState(nsPresContext* aPresContext,
                                  nsIContent*    aContent)
{
  // On Printing or Print Preview, we don't need IME.
  if (aPresContext->Type() == nsPresContext::eContext_PrintPreview ||
      aPresContext->Type() == nsPresContext::eContext_Print) {
    return nsIContent::IME_STATUS_DISABLE;
  }

  PRBool isEditable = PR_FALSE;
  nsCOMPtr<nsISupports> container = aPresContext->GetContainer();
  nsCOMPtr<nsIEditorDocShell> editorDocShell(do_QueryInterface(container));
  if (editorDocShell)
    editorDocShell->GetEditable(&isEditable);

  if (isEditable)
    return nsIContent::IME_STATUS_ENABLE;

  if (aContent)
    return aContent->GetDesiredIMEState();

  return nsIContent::IME_STATUS_DISABLE;
}

void
nsIMEStateManager::SetIMEState(nsPresContext*     aPresContext,
                               PRUint32           aState,
                               nsIKBStateControl* aKB)
{
  if (aState & nsIContent::IME_STATUS_MASK_ENABLED) {
    PRUint32 state =
      nsContentUtils::GetKBStateControlStatusFromIMEStatus(aState);
    aKB->SetIMEEnabled(state);
  }
  if (aState & nsIContent::IME_STATUS_MASK_OPENED) {
    PRBool open = (aState & nsIContent::IME_STATUS_OPEN);
    aKB->SetIMEOpenState(open);
  }
}

nsIKBStateControl*
nsIMEStateManager::GetKBStateControl(nsPresContext* aPresContext)
{
  nsIViewManager* vm = aPresContext->GetViewManager();
  if (!vm)
    return nsnull;
  nsCOMPtr<nsIWidget> widget = nsnull;
  nsresult rv = vm->GetWidget(getter_AddRefs(widget));
  NS_ENSURE_SUCCESS(rv, nsnull);
  NS_ENSURE_TRUE(widget, nsnull);
  nsCOMPtr<nsIKBStateControl> kb = do_QueryInterface(widget);
  return kb;
}

