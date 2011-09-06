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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Gaunt (jgaunt@netscape.com)
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

// NOTE: alphabetically ordered
#include "nsAccessibilityAtoms.h"
#include "nsAccessibilityService.h"
#include "nsCoreUtils.h"
#include "nsAccUtils.h"
#include "nsApplicationAccessibleWrap.h"
#include "nsARIAGridAccessibleWrap.h"
#include "nsARIAMap.h"
#include "nsIContentViewer.h"
#include "nsCURILoader.h"
#include "nsDocAccessible.h"
#include "nsHTMLImageMapAccessible.h"
#include "nsHTMLLinkAccessible.h"
#include "nsHTMLSelectAccessible.h"
#include "nsHTMLTableAccessibleWrap.h"
#include "nsHTMLTextAccessible.h"
#include "nsHyperTextAccessibleWrap.h"
#include "nsIAccessibilityService.h"
#include "nsIAccessibleProvider.h"
#include "States.h"
#include "Statistics.h"

#include "nsIDOMDocument.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMXULElement.h"
#include "nsIHTMLDocument.h"
#include "nsImageFrame.h"
#include "nsILink.h"
#include "nsIObserverService.h"
#include "nsNPAPIPluginInstance.h"
#include "nsISupportsUtils.h"
#include "nsObjectFrame.h"
#include "nsOuterDocAccessible.h"
#include "nsRootAccessibleWrap.h"
#include "nsTextFragment.h"
#include "mozilla/Services.h"
#include "nsEventStates.h"

#ifdef MOZ_XUL
#include "nsXULAlertAccessible.h"
#include "nsXULColorPickerAccessible.h"
#include "nsXULComboboxAccessible.h"
#include "nsXULFormControlAccessible.h"
#include "nsXULListboxAccessibleWrap.h"
#include "nsXULMenuAccessibleWrap.h"
#include "nsXULSliderAccessible.h"
#include "nsXULTabAccessible.h"
#include "nsXULTextAccessible.h"
#include "nsXULTreeGridAccessibleWrap.h"
#endif

// For native window support for object/embed/applet tags
#ifdef XP_WIN
#include "nsHTMLWin32ObjectAccessible.h"
#endif

// For embedding plugin accessibles
#ifdef MOZ_ACCESSIBILITY_ATK
#include "AtkSocketAccessible.h"
#endif

#ifndef DISABLE_XFORMS_HOOKS
#include "nsXFormsFormControlsAccessible.h"
#include "nsXFormsWidgetsAccessible.h"
#endif

#include "mozilla/FunctionTimer.h"
#include "mozilla/dom/Element.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsAccessibilityService
////////////////////////////////////////////////////////////////////////////////

nsAccessibilityService *nsAccessibilityService::gAccessibilityService = nsnull;
PRBool nsAccessibilityService::gIsShutdown = PR_TRUE;

nsAccessibilityService::nsAccessibilityService() : nsAccDocManager()
{
  NS_TIME_FUNCTION;
}

nsAccessibilityService::~nsAccessibilityService()
{
  NS_ASSERTION(gIsShutdown, "Accessibility wasn't shutdown!");
  gAccessibilityService = nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED3(nsAccessibilityService,
                             nsAccDocManager,
                             nsIAccessibilityService,
                             nsIAccessibleRetrieval,
                             nsIObserver)

////////////////////////////////////////////////////////////////////////////////
// nsIObserver

NS_IMETHODIMP
nsAccessibilityService::Observe(nsISupports *aSubject, const char *aTopic,
                         const PRUnichar *aData)
{
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID))
    Shutdown();

  return NS_OK;
}

// nsIAccessibilityService
void
nsAccessibilityService::NotifyOfAnchorJumpTo(nsIContent* aTargetNode)
{
  nsIDocument* documentNode = aTargetNode->GetCurrentDoc();
  if (documentNode) {
    nsDocAccessible* document = GetDocAccessible(documentNode);
    if (document)
      document->HandleAnchorJump(aTargetNode);
  }
}

// nsIAccessibilityService
void
nsAccessibilityService::FireAccessibleEvent(PRUint32 aEvent,
                                            nsAccessible* aTarget)
{
  nsEventShell::FireEvent(aEvent, aTarget);
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibilityService

nsAccessible*
nsAccessibilityService::GetRootDocumentAccessible(nsIPresShell* aPresShell,
                                                  PRBool aCanCreate)
{
  nsIDocument* documentNode = aPresShell->GetDocument();
  if (documentNode) {
    nsCOMPtr<nsISupports> container = documentNode->GetContainer();
    nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(container));
    if (treeItem) {
      nsCOMPtr<nsIDocShellTreeItem> rootTreeItem;
      treeItem->GetRootTreeItem(getter_AddRefs(rootTreeItem));
      if (treeItem != rootTreeItem) {
        nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(rootTreeItem));
        nsCOMPtr<nsIPresShell> presShell;
        docShell->GetPresShell(getter_AddRefs(presShell));
        documentNode = presShell->GetDocument();
      }

      return aCanCreate ?
        GetDocAccessible(documentNode) : GetDocAccessibleFromCache(documentNode);
    }
  }
  return nsnull;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateOuterDocAccessible(nsIContent* aContent,
                                                 nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsOuterDocAccessible(aContent, weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTML4ButtonAccessible(nsIContent* aContent,
                                                    nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsHTML4ButtonAccessible(aContent, weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLButtonAccessible(nsIContent* aContent,
                                                   nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsHTMLButtonAccessible(aContent, weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLLIAccessible(nsIContent* aContent,
                                               nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsHTMLLIAccessible(aContent, weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHyperTextAccessible(nsIContent* aContent,
                                                  nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsHyperTextAccessibleWrap(aContent, weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLCheckboxAccessible(nsIContent* aContent,
                                                     nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsHTMLCheckboxAccessible(aContent, weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLComboboxAccessible(nsIContent* aContent,
                                                     nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsHTMLComboboxAccessible(aContent, weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLImageAccessible(nsIContent* aContent,
                                                  nsIPresShell* aPresShell)
{
  nsAutoString mapElmName;
  aContent->GetAttr(kNameSpaceID_None,
                    nsAccessibilityAtoms::usemap,
                    mapElmName);
  nsCOMPtr<nsIDOMHTMLMapElement> mapElm;
  if (nsIDocument* document = aContent->GetCurrentDoc()) {
    mapElm = do_QueryInterface(document->FindImageMap(mapElmName));
  }

  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = mapElm ?
    new nsHTMLImageMapAccessible(aContent, weakShell, mapElm) :
    new nsHTMLImageAccessibleWrap(aContent, weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLGroupboxAccessible(nsIContent* aContent,
                                                     nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsHTMLGroupboxAccessible(aContent, weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLListboxAccessible(nsIContent* aContent,
                                                    nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsHTMLSelectListAccessible(aContent, weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLMediaAccessible(nsIContent* aContent,
                                                  nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsEnumRoleAccessible(aContent, weakShell,
                                                      nsIAccessibleRole::ROLE_GROUPING);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLObjectFrameAccessible(nsObjectFrame* aFrame,
                                                        nsIContent* aContent,
                                                        nsIPresShell* aPresShell)
{
  // We can have several cases here:
  // 1) a text or html embedded document where the contentDocument variable in
  //    the object element holds the content;
  // 2) web content that uses a plugin, which means we will have to go to
  //    the plugin to get the accessible content;
  // 3) an image or imagemap, where the image frame points back to the object
  //    element DOMNode.

  if (aFrame->GetRect().IsEmpty())
    return nsnull;

  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));

  // 1) for object elements containing either HTML or TXT documents
  nsCOMPtr<nsIDOMHTMLObjectElement> obj(do_QueryInterface(aContent));
  if (obj) {
    nsCOMPtr<nsIDOMDocument> domDoc;
    obj->GetContentDocument(getter_AddRefs(domDoc));
    if (domDoc)
      return CreateOuterDocAccessible(aContent, aPresShell);
  }

#if defined(XP_WIN) || defined(MOZ_ACCESSIBILITY_ATK)
  // 2) for plugins
  nsRefPtr<nsNPAPIPluginInstance> pluginInstance;
  if (NS_SUCCEEDED(aFrame->GetPluginInstance(getter_AddRefs(pluginInstance))) &&
      pluginInstance) {
#ifdef XP_WIN
    // Note: pluginPort will be null if windowless.
    HWND pluginPort = nsnull;
    aFrame->GetPluginPort(&pluginPort);

    nsAccessible* accessible = new nsHTMLWin32ObjectOwnerAccessible(aContent,
                                                                    weakShell,
                                                                    pluginPort);
    NS_IF_ADDREF(accessible);
    return accessible;

#elif MOZ_ACCESSIBILITY_ATK
    if (!AtkSocketAccessible::gCanEmbed)
      return nsnull;

    nsCString plugId;
    nsresult rv = pluginInstance->GetValueFromPlugin(
      NPPVpluginNativeAccessibleAtkPlugId, &plugId);
    if (NS_SUCCEEDED(rv) && !plugId.IsVoid()) {
      AtkSocketAccessible* socketAccessible =
        new AtkSocketAccessible(aContent, weakShell, plugId);

      NS_IF_ADDREF(socketAccessible);
      return socketAccessible;
    }
#endif
  }
#endif

  // 3) for images and imagemaps, or anything else with a child frame
  // we have the object frame, get the image frame
  nsIFrame* frame = aFrame->GetFirstPrincipalChild();
  return frame ? frame->CreateAccessible() : nsnull;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLRadioButtonAccessible(nsIContent* aContent,
                                                        nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsHTMLRadioButtonAccessible(aContent, weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLTableAccessible(nsIContent* aContent,
                                                  nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsHTMLTableAccessibleWrap(aContent, weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLTableCellAccessible(nsIContent* aContent,
                                                      nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsHTMLTableCellAccessibleWrap(aContent,
                                                               weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLTextAccessible(nsIContent* aContent,
                                                 nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsHTMLTextAccessible(aContent, weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLTextFieldAccessible(nsIContent* aContent,
                                                      nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsHTMLTextFieldAccessible(aContent, weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLLabelAccessible(nsIContent* aContent,
                                                  nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsHTMLLabelAccessible(aContent, weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLHRAccessible(nsIContent* aContent,
                                               nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsHTMLHRAccessible(aContent, weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLBRAccessible(nsIContent* aContent,
                                               nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsHTMLBRAccessible(aContent, weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLCaptionAccessible(nsIContent* aContent,
                                                    nsIPresShell* aPresShell)
{
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsAccessible* accessible = new nsHTMLCaptionAccessible(aContent, weakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}

void
nsAccessibilityService::ContentRangeInserted(nsIPresShell* aPresShell,
                                             nsIContent* aContainer,
                                             nsIContent* aStartChild,
                                             nsIContent* aEndChild)
{
#ifdef DEBUG_CONTENTMUTATION
  nsAutoString tag;
  aStartChild->Tag()->ToString(tag);

  nsIAtom* atomid = aStartChild->GetID();
  nsCAutoString id;
  if (atomid)
    atomid->ToUTF8String(id);

  nsAutoString ctag;
  nsCAutoString cid;
  nsIAtom* catomid = nsnull;
  if (aContainer) {
    aContainer->Tag()->ToString(ctag);
    catomid = aContainer->GetID();
    if (catomid)
      catomid->ToUTF8String(cid);
  }

  printf("\ncontent inserted: %s@id='%s', container: %s@id='%s', end node: %p\n\n",
         NS_ConvertUTF16toUTF8(tag).get(), id.get(),
         NS_ConvertUTF16toUTF8(ctag).get(), cid.get(), aEndChild);
#endif

  nsDocAccessible* docAccessible = GetDocAccessible(aPresShell->GetDocument());
  if (docAccessible)
    docAccessible->ContentInserted(aContainer, aStartChild, aEndChild);
}

void
nsAccessibilityService::ContentRemoved(nsIPresShell* aPresShell,
                                       nsIContent* aContainer,
                                       nsIContent* aChild)
{
#ifdef DEBUG_CONTENTMUTATION
  nsAutoString tag;
  aChild->Tag()->ToString(tag);

  nsIAtom* atomid = aChild->GetID();
  nsCAutoString id;
  if (atomid)
    atomid->ToUTF8String(id);

  nsAutoString ctag;
  nsCAutoString cid;
  nsIAtom* catomid = nsnull;
  if (aContainer) {
    aContainer->Tag()->ToString(ctag);
    catomid = aContainer->GetID();
    if (catomid)
      catomid->ToUTF8String(cid);
  }

  printf("\ncontent removed: %s@id='%s', container: %s@id='%s'\n\n",
           NS_ConvertUTF16toUTF8(tag).get(), id.get(),
           NS_ConvertUTF16toUTF8(ctag).get(), cid.get());
#endif

  nsDocAccessible* docAccessible = GetDocAccessible(aPresShell->GetDocument());
  if (docAccessible)
    docAccessible->ContentRemoved(aContainer, aChild);
}

void
nsAccessibilityService::UpdateText(nsIPresShell* aPresShell,
                                   nsIContent* aContent)
{
  nsDocAccessible* document = GetDocAccessible(aPresShell->GetDocument());
  if (document)
    document->UpdateText(aContent);
}

void
nsAccessibilityService::UpdateListBullet(nsIPresShell* aPresShell,
                                         nsIContent* aHTMLListItemContent,
                                         bool aHasBullet)
{
  nsDocAccessible* document = GetDocAccessible(aPresShell->GetDocument());
  if (document) {
    nsAccessible* accessible = document->GetAccessible(aHTMLListItemContent);
    if (accessible) {
      nsHTMLLIAccessible* listItem = accessible->AsHTMLListItem();
      if (listItem)
        listItem->UpdateBullet(aHasBullet);
    }
  }
}

void
nsAccessibilityService::PresShellDestroyed(nsIPresShell *aPresShell)
{
  // Presshell destruction will automatically destroy shells for descendant
  // documents, so no need to worry about those. Just shut down the accessible
  // for this one document. That keeps us from having bad behavior in case of
  // deep bushy subtrees.
  // When document subtree containing iframe is hidden then we don't get
  // pagehide event for the iframe's underlying document and its presshell is
  // destroyed before we're notified styles were changed. Shutdown the document
  // accessible early.
  nsIDocument* doc = aPresShell->GetDocument();
  if (!doc)
    return;

  NS_LOG_ACCDOCDESTROY("presshell destroyed", doc)

  nsDocAccessible* docAccessible = GetDocAccessibleFromCache(doc);
  if (docAccessible)
    docAccessible->Shutdown();
}

void
nsAccessibilityService::PresShellActivated(nsIPresShell* aPresShell)
{
  nsIDocument* DOMDoc = aPresShell->GetDocument();
  if (DOMDoc) {
    nsDocAccessible* document = GetDocAccessibleFromCache(DOMDoc);
    if (document) {
      nsRootAccessible* rootDocument = document->RootAccessible();
      NS_ASSERTION(rootDocument, "Entirely broken tree: no root document!");
      if (rootDocument)
        rootDocument->DocumentActivated(document);
    }
  }
}

void
nsAccessibilityService::RecreateAccessible(nsIPresShell* aPresShell,
                                           nsIContent* aContent)
{
  nsDocAccessible* document = GetDocAccessible(aPresShell->GetDocument());
  if (document) {
    document->HandleNotification<nsDocAccessible, nsIContent>
      (document, &nsDocAccessible::RecreateAccessible, aContent);
  }
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleRetrieval

NS_IMETHODIMP
nsAccessibilityService::GetApplicationAccessible(nsIAccessible **aAccessibleApplication)
{
  NS_ENSURE_ARG_POINTER(aAccessibleApplication);

  NS_IF_ADDREF(*aAccessibleApplication = nsAccessNode::GetApplicationAccessible());
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::GetAccessibleFor(nsIDOMNode *aNode,
                                         nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;
  if (!aNode)
    return NS_OK;

  nsCOMPtr<nsINode> node(do_QueryInterface(aNode));
  if (!node)
    return NS_ERROR_INVALID_ARG;

  NS_IF_ADDREF(*aAccessible = GetAccessible(node));
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::GetStringRole(PRUint32 aRole, nsAString& aString)
{
  if ( aRole >= NS_ARRAY_LENGTH(kRoleNames)) {
    aString.AssignLiteral("unknown");
    return NS_OK;
  }

  CopyUTF8toUTF16(kRoleNames[aRole], aString);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::GetStringStates(PRUint32 aState, PRUint32 aExtraState,
                                        nsIDOMDOMStringList **aStringStates)
{
  nsAccessibleDOMStringList *stringStates = new nsAccessibleDOMStringList();
  NS_ENSURE_TRUE(stringStates, NS_ERROR_OUT_OF_MEMORY);

  PRUint64 state = nsAccUtils::To64State(aState, aExtraState);

  // states
  if (state & states::UNAVAILABLE)
    stringStates->Add(NS_LITERAL_STRING("unavailable"));
  if (state & states::SELECTED)
    stringStates->Add(NS_LITERAL_STRING("selected"));
  if (state & states::FOCUSED)
    stringStates->Add(NS_LITERAL_STRING("focused"));
  if (state & states::PRESSED)
    stringStates->Add(NS_LITERAL_STRING("pressed"));
  if (state & states::CHECKED)
    stringStates->Add(NS_LITERAL_STRING("checked"));
  if (state & states::MIXED)
    stringStates->Add(NS_LITERAL_STRING("mixed"));
  if (state & states::READONLY)
    stringStates->Add(NS_LITERAL_STRING("readonly"));
  if (state & states::HOTTRACKED)
    stringStates->Add(NS_LITERAL_STRING("hottracked"));
  if (state & states::DEFAULT)
    stringStates->Add(NS_LITERAL_STRING("default"));
  if (state & states::EXPANDED)
    stringStates->Add(NS_LITERAL_STRING("expanded"));
  if (state & states::COLLAPSED)
    stringStates->Add(NS_LITERAL_STRING("collapsed"));
  if (state & states::BUSY)
    stringStates->Add(NS_LITERAL_STRING("busy"));
  if (state & states::FLOATING)
    stringStates->Add(NS_LITERAL_STRING("floating"));
  if (state & states::ANIMATED)
    stringStates->Add(NS_LITERAL_STRING("animated"));
  if (state & states::INVISIBLE)
    stringStates->Add(NS_LITERAL_STRING("invisible"));
  if (state & states::OFFSCREEN)
    stringStates->Add(NS_LITERAL_STRING("offscreen"));
  if (state & states::SIZEABLE)
    stringStates->Add(NS_LITERAL_STRING("sizeable"));
  if (state & states::MOVEABLE)
    stringStates->Add(NS_LITERAL_STRING("moveable"));
  if (state & states::SELFVOICING)
    stringStates->Add(NS_LITERAL_STRING("selfvoicing"));
  if (state & states::FOCUSABLE)
    stringStates->Add(NS_LITERAL_STRING("focusable"));
  if (state & states::SELECTABLE)
    stringStates->Add(NS_LITERAL_STRING("selectable"));
  if (state & states::LINKED)
    stringStates->Add(NS_LITERAL_STRING("linked"));
  if (state & states::TRAVERSED)
    stringStates->Add(NS_LITERAL_STRING("traversed"));
  if (state & states::MULTISELECTABLE)
    stringStates->Add(NS_LITERAL_STRING("multiselectable"));
  if (state & states::EXTSELECTABLE)
    stringStates->Add(NS_LITERAL_STRING("extselectable"));
  if (state & states::PROTECTED)
    stringStates->Add(NS_LITERAL_STRING("protected"));
  if (state & states::HASPOPUP)
    stringStates->Add(NS_LITERAL_STRING("haspopup"));
  if (state & states::REQUIRED)
    stringStates->Add(NS_LITERAL_STRING("required"));
  if (state & states::ALERT)
    stringStates->Add(NS_LITERAL_STRING("alert"));
  if (state & states::INVALID)
    stringStates->Add(NS_LITERAL_STRING("invalid"));
  if (state & states::CHECKABLE)
    stringStates->Add(NS_LITERAL_STRING("checkable"));

  // extraStates
  if (state & states::SUPPORTS_AUTOCOMPLETION)
    stringStates->Add(NS_LITERAL_STRING("autocompletion"));
  if (state & states::DEFUNCT)
    stringStates->Add(NS_LITERAL_STRING("defunct"));
  if (state & states::SELECTABLE_TEXT)
    stringStates->Add(NS_LITERAL_STRING("selectable text"));
  if (state & states::EDITABLE)
    stringStates->Add(NS_LITERAL_STRING("editable"));
  if (state & states::ACTIVE)
    stringStates->Add(NS_LITERAL_STRING("active"));
  if (state & states::MODAL)
    stringStates->Add(NS_LITERAL_STRING("modal"));
  if (state & states::MULTI_LINE)
    stringStates->Add(NS_LITERAL_STRING("multi line"));
  if (state & states::HORIZONTAL)
    stringStates->Add(NS_LITERAL_STRING("horizontal"));
  if (state & states::OPAQUE1)
    stringStates->Add(NS_LITERAL_STRING("opaque"));
  if (state & states::SINGLE_LINE)
    stringStates->Add(NS_LITERAL_STRING("single line"));
  if (state & states::TRANSIENT)
    stringStates->Add(NS_LITERAL_STRING("transient"));
  if (state & states::VERTICAL)
    stringStates->Add(NS_LITERAL_STRING("vertical"));
  if (state & states::STALE)
    stringStates->Add(NS_LITERAL_STRING("stale"));
  if (state & states::ENABLED)
    stringStates->Add(NS_LITERAL_STRING("enabled"));
  if (state & states::SENSITIVE)
    stringStates->Add(NS_LITERAL_STRING("sensitive"));
  if (state & states::EXPANDABLE)
    stringStates->Add(NS_LITERAL_STRING("expandable"));

  //unknown states
  PRUint32 stringStatesLength = 0;
  stringStates->GetLength(&stringStatesLength);
  if (!stringStatesLength)
    stringStates->Add(NS_LITERAL_STRING("unknown"));

  NS_ADDREF(*aStringStates = stringStates);
  return NS_OK;
}

// nsIAccessibleRetrieval::getStringEventType()
NS_IMETHODIMP
nsAccessibilityService::GetStringEventType(PRUint32 aEventType,
                                           nsAString& aString)
{
  NS_ASSERTION(nsIAccessibleEvent::EVENT_LAST_ENTRY == NS_ARRAY_LENGTH(kEventTypeNames),
               "nsIAccessibleEvent constants are out of sync to kEventTypeNames");

  if (aEventType >= NS_ARRAY_LENGTH(kEventTypeNames)) {
    aString.AssignLiteral("unknown");
    return NS_OK;
  }

  CopyUTF8toUTF16(kEventTypeNames[aEventType], aString);
  return NS_OK;
}

// nsIAccessibleRetrieval::getStringRelationType()
NS_IMETHODIMP
nsAccessibilityService::GetStringRelationType(PRUint32 aRelationType,
                                              nsAString& aString)
{
  if (aRelationType >= NS_ARRAY_LENGTH(kRelationTypeNames)) {
    aString.AssignLiteral("unknown");
    return NS_OK;
  }

  CopyUTF8toUTF16(kRelationTypeNames[aRelationType], aString);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::GetAccessibleFromCache(nsIDOMNode* aNode,
                                               nsIAccessible** aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;
  if (!aNode)
    return NS_OK;

  nsCOMPtr<nsINode> node(do_QueryInterface(aNode));
  if (!node)
    return NS_ERROR_INVALID_ARG;

  // Search for an accessible in each of our per document accessible object
  // caches. If we don't find it, and the given node is itself a document, check
  // our cache of document accessibles (document cache). Note usually shutdown
  // document accessibles are not stored in the document cache, however an
  // "unofficially" shutdown document (i.e. not from nsAccDocManager) can still
  // exist in the document cache.
  nsAccessible* accessible = FindAccessibleInCache(node);
  if (!accessible) {
    nsCOMPtr<nsIDocument> document(do_QueryInterface(node));
    if (document)
      accessible = GetDocAccessibleFromCache(document);
  }

  NS_IF_ADDREF(*aAccessible = accessible);
  return NS_OK;
}

// nsIAccesibilityService
nsAccessible*
nsAccessibilityService::GetAccessibleInShell(nsINode* aNode,
                                             nsIPresShell* aPresShell)
{
  if (!aNode || !aPresShell)
    return nsnull;

  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  return GetAccessibleInWeakShell(aNode, weakShell);
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessibilityService public

nsAccessible*
nsAccessibilityService::GetAccessible(nsINode* aNode)
{
  NS_PRECONDITION(aNode, "Getting an accessible for null node! Crash.");

  nsDocAccessible* document = GetDocAccessible(aNode->GetOwnerDoc());
  return document ? document->GetAccessible(aNode) : nsnull;
}

nsAccessible*
nsAccessibilityService::GetAccessibleOrContainer(nsINode* aNode,
                                                 nsIWeakReference* aWeakShell)
{
  if (!aNode)
    return nsnull;

  // XXX: weak shell is ignored until multiple shell documents are supported.
  nsDocAccessible* document = GetDocAccessible(aNode->GetOwnerDoc());
  return document ? document->GetAccessibleOrContainer(aNode) : nsnull;
}

static PRBool HasRelatedContent(nsIContent *aContent)
{
  nsAutoString id;
  if (!aContent || !nsCoreUtils::GetID(aContent, id) || id.IsEmpty()) {
    return PR_FALSE;
  }

  // If the given ID is referred by relation attribute then create an accessible
  // for it. Take care of HTML elements only for now.
  if (aContent->IsHTML() &&
      nsAccUtils::GetDocAccessibleFor(aContent)->IsDependentID(id))
    return PR_TRUE;

  nsIContent *ancestorContent = aContent;
  while ((ancestorContent = ancestorContent->GetParent()) != nsnull) {
    if (ancestorContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_activedescendant)) {
        // ancestor has activedescendant property, this content could be active
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

nsAccessible*
nsAccessibilityService::GetOrCreateAccessible(nsINode* aNode,
                                              nsIPresShell* aPresShell,
                                              nsIWeakReference* aWeakShell,
                                              bool* aIsSubtreeHidden)
{
  if (!aPresShell || !aWeakShell || !aNode || gIsShutdown)
    return nsnull;

  if (aIsSubtreeHidden)
    *aIsSubtreeHidden = false;

  // Check to see if we already have an accessible for this node in the cache.
  nsAccessible* cachedAccessible = GetAccessibleInWeakShell(aNode, aWeakShell);
  if (cachedAccessible)
    return cachedAccessible;

  // No cache entry, so we must create the accessible.

  if (aNode->IsNodeOfType(nsINode::eDOCUMENT)) {
    // If it's document node then ask accessible document loader for
    // document accessible, otherwise return null.
    nsCOMPtr<nsIDocument> document(do_QueryInterface(aNode));
    return GetDocAccessible(document);
  }

  // We have a content node.
  if (!aNode->IsInDoc()) {
    NS_WARNING("Creating accessible for node with no document");
    return nsnull;
  }

  if (aNode->GetOwnerDoc() != aPresShell->GetDocument()) {
    NS_ERROR("Creating accessible for wrong pres shell");
    return nsnull;
  }

  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  if (!content)
    return nsnull;

  // Frames can be deallocated when we flush layout, or when we call into code
  // that can flush layout, either directly, or via DOM manipulation, or some
  // CSS styles like :hover. We use the weak frame checks to avoid calling
  // methods on a dead frame pointer.
  nsWeakFrame weakFrame = content->GetPrimaryFrame();

  // Check frame and its visibility. Note, hidden frame allows visible
  // elements in subtree.
  if (!weakFrame.GetFrame() || !weakFrame->GetStyleVisibility()->IsVisible()) {
    if (aIsSubtreeHidden && !weakFrame.GetFrame())
      *aIsSubtreeHidden = true;

    return nsnull;
  }

  if (weakFrame.GetFrame()->GetContent() != content) {
    // Not the main content for this frame. This happens because <area>
    // elements return the image frame as their primary frame. The main content
    // for the image frame is the image content. If the frame is not an image
    // frame or the node is not an area element then null is returned.
    // This setup will change when bug 135040 is fixed.
    return GetAreaAccessible(weakFrame.GetFrame(), aNode, aWeakShell);
  }

  nsDocAccessible* docAcc =
    GetAccService()->GetDocAccessible(aNode->GetOwnerDoc());
  if (!docAcc) {
    NS_NOTREACHED("Node has no host document accessible!");
    return nsnull;
  }

  // Attempt to create an accessible based on what we know.
  nsRefPtr<nsAccessible> newAcc;

  // Create accessible for visible text frames.
  if (content->IsNodeOfType(nsINode::eTEXT)) {
    nsAutoString text;
    weakFrame->GetRenderedText(&text, nsnull, nsnull, 0, PR_UINT32_MAX);
    if (text.IsEmpty()) {
      if (aIsSubtreeHidden)
        *aIsSubtreeHidden = true;

      return nsnull;
    }

    newAcc = weakFrame->CreateAccessible();
    if (docAcc->BindToDocument(newAcc, nsnull)) {
      newAcc->AsTextLeaf()->SetText(text);
      return newAcc;
    }

    return nsnull;
  }

  PRBool isHTML = content->IsHTML();
  if (isHTML && content->Tag() == nsAccessibilityAtoms::map) {
    // Create hyper text accessible for HTML map if it is used to group links
    // (see http://www.w3.org/TR/WCAG10-HTML-TECHS/#group-bypass). If the HTML
    // map doesn't have 'name' attribute (or has empty name attribute) then we
    // suppose it is used for links grouping. Otherwise we think it is used in
    // conjuction with HTML image element and in this case we don't create any
    // accessible for it and don't walk into it. The accessibles for HTML area
    // (nsHTMLAreaAccessible) the map contains are attached as children of the
    // appropriate accessible for HTML image (nsHTMLImageAccessible).
    nsAutoString name;
    content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::name, name);
    if (!name.IsEmpty()) {
      if (aIsSubtreeHidden)
        *aIsSubtreeHidden = true;

      return nsnull;
    }

    newAcc = new nsHyperTextAccessibleWrap(content, aWeakShell);
    if (docAcc->BindToDocument(newAcc, nsAccUtils::GetRoleMapEntry(aNode)))
      return newAcc;
    return nsnull;
  }

  nsRoleMapEntry *roleMapEntry = nsAccUtils::GetRoleMapEntry(aNode);
  if (roleMapEntry && !nsCRT::strcmp(roleMapEntry->roleString, "presentation")) {
    // Ignore presentation role if element is focusable (focus event shouldn't
    // be ever lost and should be sensible).
    if (content->IsFocusable())
      roleMapEntry = nsnull;
    else
      return nsnull;
  }

  if (weakFrame.IsAlive() && !newAcc && isHTML) {  // HTML accessibles
    PRBool tryTagNameOrFrame = PR_TRUE;

    nsIAtom *frameType = weakFrame.GetFrame()->GetType();

    PRBool partOfHTMLTable =
      frameType == nsAccessibilityAtoms::tableCaptionFrame ||
      frameType == nsAccessibilityAtoms::tableCellFrame ||
      frameType == nsAccessibilityAtoms::tableRowGroupFrame ||
      frameType == nsAccessibilityAtoms::tableRowFrame;

    if (partOfHTMLTable) {
      // Table-related frames don't get table-related roles
      // unless they are inside a table, but they may still get generic
      // accessibles
      nsIContent *tableContent = content;
      while ((tableContent = tableContent->GetParent()) != nsnull) {
        nsIFrame *tableFrame = tableContent->GetPrimaryFrame();
        if (!tableFrame)
          continue;

        if (tableFrame->GetType() == nsAccessibilityAtoms::tableOuterFrame) {
          nsAccessible *tableAccessible =
            GetAccessibleInWeakShell(tableContent, aWeakShell);

          if (tableAccessible) {
            if (!roleMapEntry) {
              PRUint32 role = tableAccessible->Role();
              if (role != nsIAccessibleRole::ROLE_TABLE &&
                  role != nsIAccessibleRole::ROLE_TREE_TABLE) {
                // No ARIA role and not in table: override role. For example,
                // <table role="label"><td>content</td></table>
                roleMapEntry = &nsARIAMap::gEmptyRoleMap;
              }
            }

            break;
          }

#ifdef DEBUG
          nsRoleMapEntry *tableRoleMapEntry =
            nsAccUtils::GetRoleMapEntry(tableContent);
          NS_ASSERTION(tableRoleMapEntry &&
                       !nsCRT::strcmp(tableRoleMapEntry->roleString, "presentation"),
                       "No accessible for parent table and it didn't have role of presentation");
#endif

          if (!roleMapEntry && !content->IsFocusable()) {
            // Table-related descendants of presentation table are also
            // presentation if they aren't focusable and have not explicit ARIA
            // role (don't create accessibles for them unless they need to fire
            // focus events).
            return nsnull;
          }

          // otherwise create ARIA based accessible.
          tryTagNameOrFrame = PR_FALSE;
          break;
        }

        if (tableContent->Tag() == nsAccessibilityAtoms::table) {
          // Stop before we are fooled by any additional table ancestors
          // This table cell frameis part of a separate ancestor table.
          tryTagNameOrFrame = PR_FALSE;
          break;
        }
      }

      if (!tableContent)
        tryTagNameOrFrame = PR_FALSE;
    }

    if (roleMapEntry) {
      // Create ARIA grid/treegrid accessibles if node is not of a child or
      // valid child of HTML table and is not a HTML table.
      if ((!partOfHTMLTable || !tryTagNameOrFrame) &&
          frameType != nsAccessibilityAtoms::tableOuterFrame) {

        if (roleMapEntry->role == nsIAccessibleRole::ROLE_TABLE ||
            roleMapEntry->role == nsIAccessibleRole::ROLE_TREE_TABLE) {
          newAcc = new nsARIAGridAccessibleWrap(content, aWeakShell);

        } else if (roleMapEntry->role == nsIAccessibleRole::ROLE_GRID_CELL ||
            roleMapEntry->role == nsIAccessibleRole::ROLE_ROWHEADER ||
            roleMapEntry->role == nsIAccessibleRole::ROLE_COLUMNHEADER) {
          newAcc = new nsARIAGridCellAccessibleWrap(content, aWeakShell);
        }
      }
    }

    if (!newAcc && tryTagNameOrFrame) {
      // Prefer to use markup (mostly tag name, perhaps attributes) to
      // decide if and what kind of accessible to create.
      // The method creates accessibles for table related content too therefore
      // we do not call it if accessibles for table related content are
      // prevented above.
      newAcc = CreateHTMLAccessibleByMarkup(weakFrame.GetFrame(), content,
                                            aWeakShell);

      if (!newAcc) {
        // Do not create accessible object subtrees for non-rendered table
        // captions. This could not be done in
        // nsTableCaptionFrame::GetAccessible() because the descendants of
        // the table caption would still be created. By setting
        // *aIsSubtreeHidden = true we ensure that no descendant accessibles
        // are created.
        nsIFrame* f = weakFrame.GetFrame();
        if (!f) {
          f = aPresShell->GetRealPrimaryFrameFor(content);
        }
        if (f->GetType() == nsAccessibilityAtoms::tableCaptionFrame &&
           f->GetRect().IsEmpty()) {
          // XXX This is not the ideal place for this code, but right now there
          // is no better place:
          if (aIsSubtreeHidden)
            *aIsSubtreeHidden = true;

          return nsnull;
        }

        // Try using frame to do it.
        newAcc = f->CreateAccessible();
      }
    }
  }

  if (!newAcc) {
    // Elements may implement nsIAccessibleProvider via XBL. This allows them to
    // say what kind of accessible to create.
    newAcc = CreateAccessibleByType(content, aWeakShell);
  }

  if (!newAcc) {
    // Create generic accessibles for SVG and MathML nodes.
    if (content->GetNameSpaceID() == kNameSpaceID_SVG &&
        content->Tag() == nsAccessibilityAtoms::svg) {
      newAcc = new nsEnumRoleAccessible(content, aWeakShell,
                                        nsIAccessibleRole::ROLE_DIAGRAM);
    }
    else if (content->GetNameSpaceID() == kNameSpaceID_MathML &&
             content->Tag() == nsAccessibilityAtoms::math) {
      newAcc = new nsEnumRoleAccessible(content, aWeakShell,
                                        nsIAccessibleRole::ROLE_EQUATION);
    }
  }

  if (!newAcc) {
    newAcc = CreateAccessibleForDeckChild(weakFrame.GetFrame(), content,
                                          aWeakShell);
  }

  // If no accessible, see if we need to create a generic accessible because
  // of some property that makes this object interesting
  // We don't do this for <body>, <html>, <window>, <dialog> etc. which 
  // correspond to the doc accessible and will be created in any case
  if (!newAcc && content->Tag() != nsAccessibilityAtoms::body && content->GetParent() && 
      ((weakFrame.GetFrame() && weakFrame.GetFrame()->IsFocusable()) ||
       (isHTML && nsCoreUtils::HasClickListener(content)) ||
       HasUniversalAriaProperty(content) || roleMapEntry ||
       HasRelatedContent(content) || nsCoreUtils::IsXLink(content))) {
    // This content is focusable or has an interesting dynamic content accessibility property.
    // If it's interesting we need it in the accessibility hierarchy so that events or
    // other accessibles can point to it, or so that it can hold a state, etc.
    if (isHTML) {
      // Interesting HTML container which may have selectable text and/or embedded objects
      newAcc = new nsHyperTextAccessibleWrap(content, aWeakShell);
    }
    else {  // XUL, SVG, MathML etc.
      // Interesting generic non-HTML container
      newAcc = new nsAccessibleWrap(content, aWeakShell);
    }
  }

  return docAcc->BindToDocument(newAcc, roleMapEntry) ? newAcc : nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessibilityService private

PRBool
nsAccessibilityService::Init()
{
  // Initialize accessible document manager.
  if (!nsAccDocManager::Init())
    return PR_FALSE;

  // Add observers.
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService)
    return PR_FALSE;

  observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);

  // Initialize accessibility.
  nsAccessNodeWrap::InitAccessibility();

  gIsShutdown = PR_FALSE;
  return PR_TRUE;
}

void
nsAccessibilityService::Shutdown()
{
  // Remove observers.
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService)
    observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);

  // Stop accessible document loader.
  nsAccDocManager::Shutdown();

  // Application is going to be closed, shutdown accessibility and mark
  // accessibility service as shutdown to prevent calls of its methods.
  // Don't null accessibility service static member at this point to be safe
  // if someone will try to operate with it.

  NS_ASSERTION(!gIsShutdown, "Accessibility was shutdown already");

  gIsShutdown = PR_TRUE;

  nsAccessNodeWrap::ShutdownAccessibility();
}

PRBool
nsAccessibilityService::HasUniversalAriaProperty(nsIContent *aContent)
{
  // ARIA attributes that take token values (NMTOKEN, bool) are special cased
  // because of special value "undefined" (see HasDefinedARIAToken).
  return nsAccUtils::HasDefinedARIAToken(aContent, nsAccessibilityAtoms::aria_atomic) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsAccessibilityAtoms::aria_busy) ||
         aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_controls) ||
         aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_describedby) ||
         aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_disabled) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsAccessibilityAtoms::aria_dropeffect) ||
         aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_flowto) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsAccessibilityAtoms::aria_grabbed) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsAccessibilityAtoms::aria_haspopup) ||
         aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_hidden) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsAccessibilityAtoms::aria_invalid) ||
         aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_label) ||
         aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_labelledby) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsAccessibilityAtoms::aria_live) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsAccessibilityAtoms::aria_owns) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsAccessibilityAtoms::aria_relevant);
}

nsAccessible*
nsAccessibilityService::GetAreaAccessible(nsIFrame* aImageFrame,
                                          nsINode* aAreaNode,
                                          nsIWeakReference* aWeakShell,
                                          nsAccessible** aImageAccessible)
{
  // Check if frame is an image frame, and content is <area>.
  nsImageFrame *imageFrame = do_QueryFrame(aImageFrame);
  if (!imageFrame)
    return nsnull;

  nsCOMPtr<nsIDOMHTMLAreaElement> areaElmt = do_QueryInterface(aAreaNode);
  if (!areaElmt)
    return nsnull;

  // Try to get image map accessible from the global cache or create it
  // if failed.
  nsRefPtr<nsAccessible> image =
    GetAccessibleInWeakShell(aImageFrame->GetContent(), aWeakShell);
  if (!image) {
    image = CreateHTMLImageAccessible(aImageFrame->GetContent(),
                                      aImageFrame->PresContext()->PresShell());

    nsDocAccessible* document =
      GetAccService()->GetDocAccessible(aAreaNode->GetOwnerDoc());
    if (!document) {
      NS_NOTREACHED("No document for accessible being created!");
      return nsnull;
    }

    if (!document->BindToDocument(image, nsnull))
      return nsnull;
  }

  if (aImageAccessible)
    *aImageAccessible = image;

  // Make sure <area> accessible children of the image map are cached so
  // that they should be available in global cache.
  image->EnsureChildren();

  return GetAccessibleInWeakShell(aAreaNode, aWeakShell);
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateAccessibleByType(nsIContent* aContent,
                                               nsIWeakReference* aWeakShell)
{
  nsCOMPtr<nsIAccessibleProvider> accessibleProvider(do_QueryInterface(aContent));
  if (!accessibleProvider)
    return nsnull;

  PRInt32 type;
  nsresult rv = accessibleProvider->GetAccessibleType(&type);
  if (NS_FAILED(rv))
    return nsnull;

  if (type == nsIAccessibleProvider::OuterDoc) {
    nsAccessible* accessible = new nsOuterDocAccessible(aContent, aWeakShell);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  nsAccessible* accessible = nsnull;
  switch (type)
  {
#ifdef MOZ_XUL
    case nsIAccessibleProvider::NoAccessible:
      return nsnull;

    // XUL controls
    case nsIAccessibleProvider::XULAlert:
      accessible = new nsXULAlertAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULButton:
      accessible = new nsXULButtonAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULCheckbox:
      accessible = new nsXULCheckboxAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULColorPicker:
      accessible = new nsXULColorPickerAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULColorPickerTile:
      accessible = new nsXULColorPickerTileAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULCombobox:
      accessible = new nsXULComboboxAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULDropmarker:
      accessible = new nsXULDropmarkerAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULGroupbox:
      accessible = new nsXULGroupboxAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULImage:
    {
      // Don't include nameless images in accessible tree.
      if (!aContent->HasAttr(kNameSpaceID_None,
                             nsAccessibilityAtoms::tooltiptext))
        return nsnull;

      accessible = new nsHTMLImageAccessibleWrap(aContent, aWeakShell);
      break;

    }
    case nsIAccessibleProvider::XULLink:
      accessible = new nsXULLinkAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULListbox:
      accessible = new nsXULListboxAccessibleWrap(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULListCell:
      accessible = new nsXULListCellAccessibleWrap(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULListHead:
      accessible = new nsXULColumnsAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULListHeader:
      accessible = new nsXULColumnItemAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULListitem:
      accessible = new nsXULListitemAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULMenubar:
      accessible = new nsXULMenubarAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULMenuitem:
      accessible = new nsXULMenuitemAccessibleWrap(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULMenupopup:
    {
#ifdef MOZ_ACCESSIBILITY_ATK
      // ATK considers this node to be redundant when within menubars, and it makes menu
      // navigation with assistive technologies more difficult
      // XXX In the future we will should this for consistency across the nsIAccessible
      // implementations on each platform for a consistent scripting environment, but
      // then strip out redundant accessibles in the nsAccessibleWrap class for each platform.
      nsIContent *parent = aContent->GetParent();
      if (parent && parent->NodeInfo()->Equals(nsAccessibilityAtoms::menu,
                                               kNameSpaceID_XUL))
        return nsnull;
#endif
      accessible = new nsXULMenupopupAccessible(aContent, aWeakShell);
      break;

    }
    case nsIAccessibleProvider::XULMenuSeparator:
      accessible = new nsXULMenuSeparatorAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULPane:
      accessible = new nsEnumRoleAccessible(aContent, aWeakShell,
                                            nsIAccessibleRole::ROLE_PANE);
      break;

    case nsIAccessibleProvider::XULProgressMeter:
      accessible = new XULProgressMeterAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULStatusBar:
      accessible = new nsXULStatusBarAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULScale:
      accessible = new nsXULSliderAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULRadioButton:
      accessible = new nsXULRadioButtonAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULRadioGroup:
      accessible = new nsXULRadioGroupAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULTab:
      accessible = new nsXULTabAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULTabs:
      accessible = new nsXULTabsAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULTabpanels:
      accessible = new nsXULTabpanelsAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULText:
      accessible = new nsXULTextAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULTextBox:
      accessible = new nsXULTextFieldAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULThumb:
      accessible = new nsXULThumbAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULTree:
      return CreateAccessibleForXULTree(aContent, aWeakShell);

    case nsIAccessibleProvider::XULTreeColumns:
      accessible = new nsXULTreeColumnsAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULTreeColumnItem:
      accessible = new nsXULColumnItemAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULToolbar:
      accessible = new nsXULToolbarAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULToolbarSeparator:
      accessible = new nsXULToolbarSeparatorAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULTooltip:
      accessible = new nsXULTooltipAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XULToolbarButton:
      accessible = new nsXULToolbarButtonAccessible(aContent, aWeakShell);
      break;

#endif // MOZ_XUL

#ifndef DISABLE_XFORMS_HOOKS
    // XForms elements
    case nsIAccessibleProvider::XFormsContainer:
      accessible = new nsXFormsContainerAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsLabel:
      accessible = new nsXFormsLabelAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsOutput:
      accessible = new nsXFormsOutputAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsTrigger:
      accessible = new nsXFormsTriggerAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsInput:
      accessible = new nsXFormsInputAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsInputBoolean:
      accessible = new nsXFormsInputBooleanAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsInputDate:
      accessible = new nsXFormsInputDateAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsSecret:
      accessible = new nsXFormsSecretAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsSliderRange:
      accessible = new nsXFormsRangeAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsSelect:
      accessible = new nsXFormsSelectAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsChoices:
      accessible = new nsXFormsChoicesAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsSelectFull:
      accessible = new nsXFormsSelectFullAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsItemCheckgroup:
      accessible = new nsXFormsItemCheckgroupAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsItemRadiogroup:
      accessible = new nsXFormsItemRadiogroupAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsSelectCombobox:
      accessible = new nsXFormsSelectComboboxAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsItemCombobox:
      accessible = new nsXFormsItemComboboxAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsDropmarkerWidget:
      accessible = new nsXFormsDropmarkerWidgetAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsCalendarWidget:
      accessible = new nsXFormsCalendarWidgetAccessible(aContent, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsComboboxPopupWidget:
      accessible = new nsXFormsComboboxPopupWidgetAccessible(aContent, aWeakShell);
      break;

#endif

    default:
      return nsnull;
  }

  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLAccessibleByMarkup(nsIFrame* aFrame,
                                                     nsIContent* aContent,
                                                     nsIWeakReference* aWeakShell)
{
  // This method assumes we're in an HTML namespace.
  nsIAtom *tag = aContent->Tag();
  if (tag == nsAccessibilityAtoms::legend) {
    nsAccessible* accessible = new nsHTMLLegendAccessible(aContent, aWeakShell);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsAccessibilityAtoms::option) {
    nsAccessible* accessible = new nsHTMLSelectOptionAccessible(aContent,
                                                                aWeakShell);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsAccessibilityAtoms::optgroup) {
    nsAccessible* accessible = new nsHTMLSelectOptGroupAccessible(aContent,
                                                                  aWeakShell);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsAccessibilityAtoms::ul || tag == nsAccessibilityAtoms::ol ||
      tag == nsAccessibilityAtoms::dl) {
    nsAccessible* accessible = new nsHTMLListAccessible(aContent, aWeakShell);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsAccessibilityAtoms::a) {
    // Only some roles truly enjoy life as nsHTMLLinkAccessibles, for details
    // see closed bug 494807.
    nsRoleMapEntry *roleMapEntry = nsAccUtils::GetRoleMapEntry(aContent);
    if (roleMapEntry && roleMapEntry->role != nsIAccessibleRole::ROLE_NOTHING &&
        roleMapEntry->role != nsIAccessibleRole::ROLE_LINK) {
      nsAccessible* accessible = new nsHyperTextAccessibleWrap(aContent,
                                                               aWeakShell);
      NS_IF_ADDREF(accessible);
      return accessible;
    }

    nsAccessible* accessible = new nsHTMLLinkAccessible(aContent, aWeakShell);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsAccessibilityAtoms::dt ||
      (tag == nsAccessibilityAtoms::li &&
       aFrame->GetType() != nsAccessibilityAtoms::blockFrame)) {
    // Normally for li, it is created by the list item frame (in nsBlockFrame)
    // which knows about the bullet frame; however, in this case the list item
    // must have been styled using display: foo
    nsAccessible* accessible = new nsHTMLLIAccessible(aContent, aWeakShell);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsAccessibilityAtoms::abbr ||
      tag == nsAccessibilityAtoms::acronym ||
      tag == nsAccessibilityAtoms::blockquote ||
      tag == nsAccessibilityAtoms::dd ||
      tag == nsAccessibilityAtoms::form ||
      tag == nsAccessibilityAtoms::h1 ||
      tag == nsAccessibilityAtoms::h2 ||
      tag == nsAccessibilityAtoms::h3 ||
      tag == nsAccessibilityAtoms::h4 ||
      tag == nsAccessibilityAtoms::h5 ||
      tag == nsAccessibilityAtoms::h6 ||
      tag == nsAccessibilityAtoms::q) {
    nsAccessible* accessible = new nsHyperTextAccessibleWrap(aContent, aWeakShell);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsAccessibilityAtoms::tr) {
    nsAccessible* accessible = new nsEnumRoleAccessible(aContent, aWeakShell,
                                                        nsIAccessibleRole::ROLE_ROW);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (nsCoreUtils::IsHTMLTableHeader(aContent)) {
    nsAccessible* accessible = new nsHTMLTableHeaderCellAccessibleWrap(aContent,
                                                                       aWeakShell);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsAccessibilityAtoms::output) {
    nsAccessible* accessible = new nsHTMLOutputAccessible(aContent, aWeakShell);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsAccessibilityAtoms::progress) {
    nsAccessible* accessible =
      new HTMLProgressMeterAccessible(aContent, aWeakShell);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  return nsnull;
 }

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibilityService (DON'T put methods here)

nsAccessible*
nsAccessibilityService::AddNativeRootAccessible(void* aAtkAccessible)
 {
#ifdef MOZ_ACCESSIBILITY_ATK
  nsApplicationAccessible* applicationAcc =
    nsAccessNode::GetApplicationAccessible();
  if (!applicationAcc)
    return nsnull;

  nsRefPtr<nsNativeRootAccessibleWrap> nativeRootAcc =
     new nsNativeRootAccessibleWrap((AtkObject*)aAtkAccessible);
  if (!nativeRootAcc)
    return nsnull;

  if (applicationAcc->AppendChild(nativeRootAcc))
    return nativeRootAcc;
#endif

  return nsnull;
 }

void
nsAccessibilityService::RemoveNativeRootAccessible(nsAccessible* aAccessible)
{
#ifdef MOZ_ACCESSIBILITY_ATK
  nsApplicationAccessible* applicationAcc =
    nsAccessNode::GetApplicationAccessible();

  if (applicationAcc)
    applicationAcc->RemoveChild(aAccessible);
#endif
}

////////////////////////////////////////////////////////////////////////////////
// NS_GetAccessibilityService
////////////////////////////////////////////////////////////////////////////////

/**
 * Return accessibility service; creating one if necessary.
 */
nsresult
NS_GetAccessibilityService(nsIAccessibilityService** aResult)
{
  NS_ENSURE_TRUE(aResult, NS_ERROR_NULL_POINTER);
  *aResult = nsnull;
 
  if (nsAccessibilityService::gAccessibilityService) {
    NS_ADDREF(*aResult = nsAccessibilityService::gAccessibilityService);
    return NS_OK;
  }

  nsRefPtr<nsAccessibilityService> service = new nsAccessibilityService();
  NS_ENSURE_TRUE(service, NS_ERROR_OUT_OF_MEMORY);

  if (!service->Init()) {
    service->Shutdown();
    return NS_ERROR_FAILURE;
  }

  statistics::A11yInitialized();

  nsAccessibilityService::gAccessibilityService = service;
  NS_ADDREF(*aResult = service);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessibilityService private (DON'T put methods here)

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateAccessibleForDeckChild(nsIFrame* aFrame,
                                                     nsIContent* aContent,
                                                     nsIWeakReference* aWeakShell)
{
  if (aFrame->GetType() == nsAccessibilityAtoms::boxFrame ||
      aFrame->GetType() == nsAccessibilityAtoms::scrollFrame) {

    nsIFrame* parentFrame = aFrame->GetParent();
    if (parentFrame && parentFrame->GetType() == nsAccessibilityAtoms::deckFrame) {
      // If deck frame is for xul:tabpanels element then the given node has
      // tabpanel accessible.
      nsCOMPtr<nsIContent> parentContent = parentFrame->GetContent();
#ifdef MOZ_XUL
      if (parentContent->NodeInfo()->Equals(nsAccessibilityAtoms::tabpanels,
                                            kNameSpaceID_XUL)) {
        nsAccessible* accessible = new nsXULTabpanelAccessible(aContent,
                                                               aWeakShell);
        NS_IF_ADDREF(accessible);
        return accessible;
      }
#endif
      nsAccessible* accessible = new nsEnumRoleAccessible(aContent, aWeakShell,
                                                          nsIAccessibleRole::ROLE_PROPERTYPAGE);
      NS_IF_ADDREF(accessible);
      return accessible;
    }
  }

  return nsnull;
}

#ifdef MOZ_XUL
already_AddRefed<nsAccessible>
nsAccessibilityService::CreateAccessibleForXULTree(nsIContent* aContent,
                                                   nsIWeakReference* aWeakShell)
{
  nsCOMPtr<nsITreeBoxObject> treeBoxObj = nsCoreUtils::GetTreeBoxObject(aContent);
  if (!treeBoxObj)
    return nsnull;

  nsCOMPtr<nsITreeColumns> treeColumns;
  treeBoxObj->GetColumns(getter_AddRefs(treeColumns));
  if (!treeColumns)
    return nsnull;

  PRInt32 count = 0;
  treeColumns->GetCount(&count);

  // Outline of list accessible.
  if (count == 1) {
    nsAccessible* accessible = new nsXULTreeAccessible(aContent, aWeakShell);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  // Table or tree table accessible.
  nsAccessible* accessible = new nsXULTreeGridAccessibleWrap(aContent, aWeakShell);
  NS_IF_ADDREF(accessible);
  return accessible;
}
#endif
