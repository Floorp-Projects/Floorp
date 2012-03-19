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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: Aaron Leventhal (aaronl@netscape.com)
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

#include "nsAccessNode.h"

#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsApplicationAccessibleWrap.h"
#include "nsCoreUtils.h"
#include "nsRootAccessible.h"

#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMWindow.h"
#include "nsIFrame.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObserverService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIPresShell.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsFocusManager.h"
#include "nsPresContext.h"
#include "mozilla/Services.h"

/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

nsIStringBundle *nsAccessNode::gStringBundle = 0;

bool nsAccessNode::gIsFormFillEnabled = false;

nsApplicationAccessible *nsAccessNode::gApplicationAccessible = nsnull;

/*
 * Class nsAccessNode
 */
 
////////////////////////////////////////////////////////////////////////////////
// nsAccessible. nsISupports

NS_IMPL_CYCLE_COLLECTION_1(nsAccessNode, mContent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsAccessNode)
  NS_INTERFACE_MAP_ENTRY(nsAccessNode)
NS_INTERFACE_MAP_END
 
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsAccessNode)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_DESTROY(nsAccessNode, LastRelease())

////////////////////////////////////////////////////////////////////////////////
// nsAccessNode construction/desctruction

nsAccessNode::
  nsAccessNode(nsIContent* aContent, nsDocAccessible* aDoc) :
  mContent(aContent), mDoc(aDoc)
{
#ifdef DEBUG_A11Y
  mIsInitialized = false;
#endif
}

nsAccessNode::~nsAccessNode()
{
  NS_ASSERTION(!mDoc, "LastRelease was never called!?!");
}

void nsAccessNode::LastRelease()
{
  // First cleanup if needed...
  if (mDoc) {
    Shutdown();
    NS_ASSERTION(!mDoc, "A Shutdown() impl forgot to call its parent's Shutdown?");
  }
  // ... then die.
  delete this;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessNode public

bool
nsAccessNode::IsDefunct() const
{
  return !mContent;
}

bool
nsAccessNode::Init()
{
  return true;
}


void
nsAccessNode::Shutdown()
{
  mContent = nsnull;
  mDoc = nsnull;
}

nsApplicationAccessible*
nsAccessNode::GetApplicationAccessible()
{
  NS_ASSERTION(!nsAccessibilityService::IsShutdown(),
               "Accessibility wasn't initialized!");

  if (!gApplicationAccessible) {
    nsApplicationAccessibleWrap::PreCreate();

    gApplicationAccessible = new nsApplicationAccessibleWrap();
    if (!gApplicationAccessible)
      return nsnull;

    // Addref on create. Will Release in ShutdownXPAccessibility()
    NS_ADDREF(gApplicationAccessible);

    nsresult rv = gApplicationAccessible->Init();
    if (NS_FAILED(rv)) {
      gApplicationAccessible->Shutdown();
      NS_RELEASE(gApplicationAccessible);
      return nsnull;
    }
  }

  return gApplicationAccessible;
}

void nsAccessNode::InitXPAccessibility()
{
  nsCOMPtr<nsIStringBundleService> stringBundleService =
    mozilla::services::GetStringBundleService();
  if (stringBundleService) {
    // Static variables are released in ShutdownAllXPAccessibility();
    stringBundleService->CreateBundle(ACCESSIBLE_BUNDLE_URL, 
                                      &gStringBundle);
  }

  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefBranch) {
    prefBranch->GetBoolPref("browser.formfill.enable", &gIsFormFillEnabled);
  }

  NotifyA11yInitOrShutdown(true);
}

// nsAccessNode protected static
void nsAccessNode::NotifyA11yInitOrShutdown(bool aIsInit)
{
  nsCOMPtr<nsIObserverService> obsService =
    mozilla::services::GetObserverService();
  NS_ASSERTION(obsService, "No observer service to notify of a11y init/shutdown");
  if (!obsService)
    return;

  static const PRUnichar kInitIndicator[] = { '1', 0 };
  static const PRUnichar kShutdownIndicator[] = { '0', 0 }; 
  obsService->NotifyObservers(nsnull, "a11y-init-or-shutdown",
                              aIsInit ? kInitIndicator  : kShutdownIndicator);
}

void nsAccessNode::ShutdownXPAccessibility()
{
  // Called by nsAccessibilityService::Shutdown()
  // which happens when xpcom is shutting down
  // at exit of program

  NS_IF_RELEASE(gStringBundle);

  // Release gApplicationAccessible after everything else is shutdown
  // so we don't accidently create it again while tearing down root accessibles
  nsApplicationAccessibleWrap::Unload();
  if (gApplicationAccessible) {
    gApplicationAccessible->Shutdown();
    NS_RELEASE(gApplicationAccessible);
  }

  NotifyA11yInitOrShutdown(false);
}

// nsAccessNode protected
nsPresContext* nsAccessNode::GetPresContext()
{
  if (IsDefunct())
    return nsnull;

  nsIPresShell* presShell(mDoc->PresShell());

  return presShell ? presShell->GetPresContext() : nsnull;
}

nsRootAccessible*
nsAccessNode::RootAccessible() const
{
  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
    nsCoreUtils::GetDocShellTreeItemFor(mContent);
  NS_ASSERTION(docShellTreeItem, "No docshell tree item for mContent");
  if (!docShellTreeItem) {
    return nsnull;
  }
  nsCOMPtr<nsIDocShellTreeItem> root;
  docShellTreeItem->GetRootTreeItem(getter_AddRefs(root));
  NS_ASSERTION(root, "No root content tree item");
  if (!root) {
    return nsnull;
  }

  nsDocAccessible* docAcc = nsAccUtils::GetDocAccessibleFor(root);
  return docAcc ? docAcc->AsRoot() : nsnull;
}

nsIFrame*
nsAccessNode::GetFrame() const
{
  return mContent ? mContent->GetPrimaryFrame() : nsnull;
}

bool
nsAccessNode::IsPrimaryForNode() const
{
  return true;
}

////////////////////////////////////////////////////////////////////////////////
void
nsAccessNode::ScrollTo(PRUint32 aScrollType)
{
  if (IsDefunct())
    return;

  nsIPresShell* shell = mDoc->PresShell();
  if (!shell)
    return;

  nsIFrame *frame = GetFrame();
  if (!frame)
    return;

  nsIContent* content = frame->GetContent();
  if (!content)
    return;

  PRInt16 vPercent, hPercent;
  nsCoreUtils::ConvertScrollTypeToPercents(aScrollType, &vPercent, &hPercent);
  shell->ScrollContentIntoView(content, vPercent, hPercent,
                               nsIPresShell::SCROLL_OVERFLOW_HIDDEN);
}

void
nsAccessNode::Language(nsAString& aLanguage)
{
  aLanguage.Truncate();

  if (IsDefunct())
    return;

  nsCoreUtils::GetLanguageFor(mContent, nsnull, aLanguage);
  if (aLanguage.IsEmpty()) { // Nothing found, so use document's language
    mContent->OwnerDoc()->GetHeaderData(nsGkAtoms::headerContentLanguage,
                                        aLanguage);
  }
}

