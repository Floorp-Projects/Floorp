/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAccessibilityService.h"

// NOTE: alphabetically ordered
#include "Accessible-inl.h"
#include "ApplicationAccessibleWrap.h"
#include "ARIAGridAccessibleWrap.h"
#ifdef MOZ_ACCESSIBILITY_ATK
#include "AtkSocketAccessible.h"
#endif
#include "DocAccessible-inl.h"
#include "FocusManager.h"
#include "HTMLCanvasAccessible.h"
#include "HTMLElementAccessibles.h"
#include "HTMLImageMapAccessible.h"
#include "HTMLLinkAccessible.h"
#include "HTMLListAccessible.h"
#include "HTMLSelectAccessible.h"
#include "HTMLTableAccessibleWrap.h"
#include "HyperTextAccessibleWrap.h"
#include "nsAccessiblePivot.h"
#include "nsAccUtils.h"
#include "nsARIAMap.h"
#include "nsIAccessibleProvider.h"
#include "nsXFormsFormControlsAccessible.h"
#include "nsXFormsWidgetsAccessible.h"
#include "OuterDocAccessible.h"
#include "Role.h"
#include "RootAccessibleWrap.h"
#include "States.h"
#include "Statistics.h"
#ifdef XP_WIN
#include "nsHTMLWin32ObjectAccessible.h"
#endif
#include "TextLeafAccessibleWrap.h"

#ifdef DEBUG
#include "Logging.h"
#endif

#include "nsCURILoader.h"
#include "nsEventStates.h"
#include "nsIContentViewer.h"
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
#include "nsLayoutUtils.h"
#include "nsNPAPIPluginInstance.h"
#include "nsISupportsUtils.h"
#include "nsObjectFrame.h"
#include "nsTextFragment.h"
#include "mozilla/FunctionTimer.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Util.h"

#ifdef MOZ_XUL
#include "XULAlertAccessible.h"
#include "XULColorPickerAccessible.h"
#include "XULComboboxAccessible.h"
#include "XULElementAccessibles.h"
#include "XULFormControlAccessible.h"
#include "XULListboxAccessibleWrap.h"
#include "XULMenuAccessibleWrap.h"
#include "XULSliderAccessible.h"
#include "XULTabAccessible.h"
#include "XULTreeGridAccessibleWrap.h"
#endif

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsAccessibilityService
////////////////////////////////////////////////////////////////////////////////

nsAccessibilityService *nsAccessibilityService::gAccessibilityService = nsnull;
bool nsAccessibilityService::gIsShutdown = true;

nsAccessibilityService::nsAccessibilityService() :
  nsAccDocManager(), FocusManager()
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
    DocAccessible* document = GetDocAccessible(documentNode);
    if (document)
      document->SetAnchorJump(aTargetNode);
  }
}

// nsIAccessibilityService
void
nsAccessibilityService::FireAccessibleEvent(PRUint32 aEvent,
                                            Accessible* aTarget)
{
  nsEventShell::FireEvent(aEvent, aTarget);
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibilityService

Accessible*
nsAccessibilityService::GetRootDocumentAccessible(nsIPresShell* aPresShell,
                                                  bool aCanCreate)
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

already_AddRefed<Accessible>
nsAccessibilityService::CreateOuterDocAccessible(nsIContent* aContent,
                                                 nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new OuterDocAccessible(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLButtonAccessible(nsIContent* aContent,
                                                   nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new HTMLButtonAccessible(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLLIAccessible(nsIContent* aContent,
                                               nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new HTMLLIAccessible(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHyperTextAccessible(nsIContent* aContent,
                                                  nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new HyperTextAccessibleWrap(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLCheckboxAccessible(nsIContent* aContent,
                                                     nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new HTMLCheckboxAccessible(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLComboboxAccessible(nsIContent* aContent,
                                                     nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new HTMLComboboxAccessible(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLCanvasAccessible(nsIContent* aContent,
                                                   nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new HTMLCanvasAccessible(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLFileInputAccessible(nsIContent* aContent,
                                                      nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new HTMLFileInputAccessible(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLImageAccessible(nsIContent* aContent,
                                                  nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new ImageAccessibleWrap(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLImageMapAccessible(nsIContent* aContent,
                                                     nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new HTMLImageMapAccessible(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLGroupboxAccessible(nsIContent* aContent,
                                                     nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new HTMLGroupboxAccessible(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLListboxAccessible(nsIContent* aContent,
                                                    nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new HTMLSelectListAccessible(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLMediaAccessible(nsIContent* aContent,
                                                  nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new EnumRoleAccessible(aContent, GetDocAccessible(aPresShell),
                           roles::GROUPING);
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
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

    Accessible* accessible =
      new nsHTMLWin32ObjectOwnerAccessible(aContent,
                                           GetDocAccessible(aPresShell),
                                           pluginPort);
    NS_ADDREF(accessible);
    return accessible;

#elif MOZ_ACCESSIBILITY_ATK
    if (!AtkSocketAccessible::gCanEmbed)
      return nsnull;

    nsCString plugId;
    nsresult rv = pluginInstance->GetValueFromPlugin(
      NPPVpluginNativeAccessibleAtkPlugId, &plugId);
    if (NS_SUCCEEDED(rv) && !plugId.IsEmpty()) {
      AtkSocketAccessible* socketAccessible =
        new AtkSocketAccessible(aContent, GetDocAccessible(aPresShell),
                                plugId);

      NS_ADDREF(socketAccessible);
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

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLRadioButtonAccessible(nsIContent* aContent,
                                                        nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new HTMLRadioButtonAccessible(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLTableAccessible(nsIContent* aContent,
                                                  nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new HTMLTableAccessibleWrap(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLTableCellAccessible(nsIContent* aContent,
                                                      nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new HTMLTableCellAccessibleWrap(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLTableRowAccessible(nsIContent* aContent,
                                                     nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new EnumRoleAccessible(aContent, GetDocAccessible(aPresShell), roles::ROW);
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateTextLeafAccessible(nsIContent* aContent,
                                                 nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new TextLeafAccessibleWrap(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLTextFieldAccessible(nsIContent* aContent,
                                                      nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new HTMLTextFieldAccessible(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLLabelAccessible(nsIContent* aContent,
                                                  nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new HTMLLabelAccessible(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLHRAccessible(nsIContent* aContent,
                                               nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new HTMLHRAccessible(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLBRAccessible(nsIContent* aContent,
                                               nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new HTMLBRAccessible(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLCaptionAccessible(nsIContent* aContent,
                                                    nsIPresShell* aPresShell)
{
  Accessible* accessible =
    new HTMLCaptionAccessible(aContent, GetDocAccessible(aPresShell));
  NS_ADDREF(accessible);
  return accessible;
}

void
nsAccessibilityService::ContentRangeInserted(nsIPresShell* aPresShell,
                                             nsIContent* aContainer,
                                             nsIContent* aStartChild,
                                             nsIContent* aEndChild)
{
#ifdef DEBUG
  if (logging::IsEnabled(logging::eTree)) {
    logging::MsgBegin("TREE", "content inserted");
    logging::Node("container", aContainer);
    for (nsIContent* child = aStartChild; child != aEndChild;
         child = child->GetNextSibling()) {
      logging::Node("content", child);
    }
    logging::MsgEnd();
  }
#endif

  DocAccessible* docAccessible = GetDocAccessible(aPresShell);
  if (docAccessible)
    docAccessible->ContentInserted(aContainer, aStartChild, aEndChild);
}

void
nsAccessibilityService::ContentRemoved(nsIPresShell* aPresShell,
                                       nsIContent* aContainer,
                                       nsIContent* aChild)
{
#ifdef DEBUG
  if (logging::IsEnabled(logging::eTree)) {
    logging::MsgBegin("TREE", "content removed");
    logging::Node("container", aContainer);
    logging::Node("content", aChild);
    logging::MsgEnd();
  }
#endif

  DocAccessible* docAccessible = GetDocAccessible(aPresShell);
  if (docAccessible)
    docAccessible->ContentRemoved(aContainer, aChild);
}

void
nsAccessibilityService::UpdateText(nsIPresShell* aPresShell,
                                   nsIContent* aContent)
{
  DocAccessible* document = GetDocAccessible(aPresShell);
  if (document)
    document->UpdateText(aContent);
}

void
nsAccessibilityService::TreeViewChanged(nsIPresShell* aPresShell,
                                        nsIContent* aContent,
                                        nsITreeView* aView)
{
  DocAccessible* document = GetDocAccessible(aPresShell);
  if (document) {
    Accessible* accessible = document->GetAccessible(aContent);
    if (accessible) {
      XULTreeAccessible* treeAcc = accessible->AsXULTree();
      if (treeAcc) 
        treeAcc->TreeViewChanged(aView);
    }
  }
}

void
nsAccessibilityService::UpdateListBullet(nsIPresShell* aPresShell,
                                         nsIContent* aHTMLListItemContent,
                                         bool aHasBullet)
{
  DocAccessible* document = GetDocAccessible(aPresShell);
  if (document) {
    Accessible* accessible = document->GetAccessible(aHTMLListItemContent);
    if (accessible) {
      HTMLLIAccessible* listItem = accessible->AsHTMLListItem();
      if (listItem)
        listItem->UpdateBullet(aHasBullet);
    }
  }
}

void
nsAccessibilityService::UpdateImageMap(nsImageFrame* aImageFrame)
{
  nsIPresShell* presShell = aImageFrame->PresContext()->PresShell();
  DocAccessible* document = GetDocAccessible(presShell);
  if (document) {
    Accessible* accessible =
      document->GetAccessible(aImageFrame->GetContent());
    if (accessible) {
      HTMLImageMapAccessible* imageMap = accessible->AsImageMap();
      if (imageMap) {
        imageMap->UpdateChildAreas();
        return;
      }

      // If image map was initialized after we created an accessible (that'll
      // be an image accessible) then recreate it.
      RecreateAccessible(presShell, aImageFrame->GetContent());
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

#ifdef DEBUG
  if (logging::IsEnabled(logging::eDocDestroy))
    logging::DocDestroy("presshell destroyed", doc);
#endif

  DocAccessible* docAccessible = GetDocAccessibleFromCache(doc);
  if (docAccessible)
    docAccessible->Shutdown();
}

void
nsAccessibilityService::PresShellActivated(nsIPresShell* aPresShell)
{
  nsIDocument* DOMDoc = aPresShell->GetDocument();
  if (DOMDoc) {
    DocAccessible* document = GetDocAccessibleFromCache(DOMDoc);
    if (document) {
      RootAccessible* rootDocument = document->RootAccessible();
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
  DocAccessible* document = GetDocAccessible(aPresShell);
  if (document)
    document->RecreateAccessible(aContent);
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleRetrieval

NS_IMETHODIMP
nsAccessibilityService::GetApplicationAccessible(nsIAccessible** aAccessibleApplication)
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

  DocAccessible* document = GetDocAccessible(node->OwnerDoc());
  if (document)
    NS_IF_ADDREF(*aAccessible = document->GetAccessible(node));

  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::GetStringRole(PRUint32 aRole, nsAString& aString)
{
#define ROLE(geckoRole, stringRole, atkRole, macRole, msaaRole, ia2Role) \
  case roles::geckoRole: \
    CopyUTF8toUTF16(stringRole, aString); \
    return NS_OK;

  switch (aRole) {
#include "RoleMap.h"
    default:
      aString.AssignLiteral("unknown");
      return NS_OK;
  }

#undef ROLE
}

NS_IMETHODIMP
nsAccessibilityService::GetStringStates(PRUint32 aState, PRUint32 aExtraState,
                                        nsIDOMDOMStringList **aStringStates)
{
  nsAccessibleDOMStringList* stringStates = new nsAccessibleDOMStringList();
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
  NS_ASSERTION(nsIAccessibleEvent::EVENT_LAST_ENTRY == ArrayLength(kEventTypeNames),
               "nsIAccessibleEvent constants are out of sync to kEventTypeNames");

  if (aEventType >= ArrayLength(kEventTypeNames)) {
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
  if (aRelationType >= ArrayLength(kRelationTypeNames)) {
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
  Accessible* accessible = FindAccessibleInCache(node);
  if (!accessible) {
    nsCOMPtr<nsIDocument> document(do_QueryInterface(node));
    if (document)
      accessible = GetDocAccessibleFromCache(document);
  }

  NS_IF_ADDREF(*aAccessible = accessible);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateAccessiblePivot(nsIAccessible* aRoot,
                                              nsIAccessiblePivot** aPivot)
{
  NS_ENSURE_ARG_POINTER(aPivot);
  NS_ENSURE_ARG(aRoot);
  *aPivot = nsnull;

  nsRefPtr<Accessible> accessibleRoot(do_QueryObject(aRoot));
  NS_ENSURE_TRUE(accessibleRoot, NS_ERROR_INVALID_ARG);

  nsAccessiblePivot* pivot = new nsAccessiblePivot(accessibleRoot);
  NS_ADDREF(*aPivot = pivot);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::SetLogging(const nsACString& aModules)
{
#ifdef DEBUG
  logging::Enable(PromiseFlatCString(aModules));
#endif
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessibilityService public

static bool HasRelatedContent(nsIContent *aContent)
{
  nsAutoString id;
  if (!aContent || !nsCoreUtils::GetID(aContent, id) || id.IsEmpty()) {
    return false;
  }

  // If the given ID is referred by relation attribute then create an accessible
  // for it. Take care of HTML elements only for now.
  return aContent->IsHTML() &&
    nsAccUtils::GetDocAccessibleFor(aContent)->IsDependentID(id);
}

Accessible*
nsAccessibilityService::GetOrCreateAccessible(nsINode* aNode,
                                              DocAccessible* aDoc,
                                              bool* aIsSubtreeHidden)
{
  if (!aDoc || !aNode || gIsShutdown)
    return nsnull;

  if (aIsSubtreeHidden)
    *aIsSubtreeHidden = false;

  // Check to see if we already have an accessible for this node in the cache.
  Accessible* cachedAccessible = aDoc->GetAccessible(aNode);
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

  if (aNode->OwnerDoc() != aDoc->GetDocumentNode()) {
    NS_ERROR("Creating accessible for wrong document");
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
    // This setup will change when bug 135040 is fixed. Make sure we don't
    // create area accessible here. Hopefully assertion below will handle that.

#ifdef DEBUG
  nsImageFrame* imageFrame = do_QueryFrame(weakFrame.GetFrame());
  NS_ASSERTION(imageFrame && content->IsHTML() && content->Tag() == nsGkAtoms::area,
               "Unknown case of not main content for the frame!");
#endif
    return nsnull;
  }

#ifdef DEBUG
  nsImageFrame* imageFrame = do_QueryFrame(weakFrame.GetFrame());
  NS_ASSERTION(!imageFrame || !content->IsHTML() || content->Tag() != nsGkAtoms::area,
               "Image map manages the area accessible creation!");
#endif

  DocAccessible* docAcc =
    GetAccService()->GetDocAccessible(aNode->OwnerDoc());
  if (!docAcc) {
    NS_NOTREACHED("Node has no host document accessible!");
    return nsnull;
  }

  // Attempt to create an accessible based on what we know.
  nsRefPtr<Accessible> newAcc;

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

  bool isHTML = content->IsHTML();
  if (isHTML && content->Tag() == nsGkAtoms::map) {
    // Create hyper text accessible for HTML map if it is used to group links
    // (see http://www.w3.org/TR/WCAG10-HTML-TECHS/#group-bypass). If the HTML
    // map rect is empty then it is used for links grouping. Otherwise it should
    // be used in conjunction with HTML image element and in this case we don't
    // create any accessible for it and don't walk into it. The accessibles for
    // HTML area (HTMLAreaAccessible) the map contains are attached as
    // children of the appropriate accessible for HTML image
    // (ImageAccessible).
    if (nsLayoutUtils::GetAllInFlowRectsUnion(weakFrame,
                                              weakFrame->GetParent()).IsEmpty()) {
      if (aIsSubtreeHidden)
        *aIsSubtreeHidden = true;

      return nsnull;
    }

    newAcc = new HyperTextAccessibleWrap(content, docAcc);
    if (docAcc->BindToDocument(newAcc, aria::GetRoleMap(aNode)))
      return newAcc;
    return nsnull;
  }

  nsRoleMapEntry* roleMapEntry = aria::GetRoleMap(aNode);
  if (roleMapEntry && roleMapEntry->Is(nsGkAtoms::presentation)) {
    // Ignore presentation role if element is focusable (focus event shouldn't
    // be ever lost and should be sensible).
    if (content->IsFocusable())
      roleMapEntry = nsnull;
    else
      return nsnull;
  }

  if (weakFrame.IsAlive() && !newAcc && isHTML) {  // HTML accessibles
    bool tryTagNameOrFrame = true;

    nsIAtom *frameType = weakFrame.GetFrame()->GetType();

    bool partOfHTMLTable =
      frameType == nsGkAtoms::tableCaptionFrame ||
      frameType == nsGkAtoms::tableCellFrame ||
      frameType == nsGkAtoms::tableRowGroupFrame ||
      frameType == nsGkAtoms::tableRowFrame;

    if (partOfHTMLTable) {
      // Table-related frames don't get table-related roles
      // unless they are inside a table, but they may still get generic
      // accessibles
      nsIContent *tableContent = content;
      while ((tableContent = tableContent->GetParent()) != nsnull) {
        nsIFrame *tableFrame = tableContent->GetPrimaryFrame();
        if (!tableFrame)
          continue;

        if (tableFrame->GetType() == nsGkAtoms::tableOuterFrame) {
          Accessible* tableAccessible = aDoc->GetAccessible(tableContent);

          if (tableAccessible) {
            if (!roleMapEntry) {
              roles::Role role = tableAccessible->Role();
              // No ARIA role and not in table: override role. For example,
              // <table role="label"><td>content</td></table>
              if (role != roles::TABLE && role != roles::TREE_TABLE)
                roleMapEntry = &nsARIAMap::gEmptyRoleMap;
            }

            break;
          }

#ifdef DEBUG
          nsRoleMapEntry* tableRoleMapEntry = aria::GetRoleMap(tableContent);
          NS_ASSERTION(tableRoleMapEntry && tableRoleMapEntry->Is(nsGkAtoms::presentation),
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
          tryTagNameOrFrame = false;
          break;
        }

        if (tableContent->Tag() == nsGkAtoms::table) {
          // Stop before we are fooled by any additional table ancestors
          // This table cell frameis part of a separate ancestor table.
          tryTagNameOrFrame = false;
          break;
        }
      }

      if (!tableContent)
        tryTagNameOrFrame = false;
    }

    if (roleMapEntry) {
      // Create ARIA grid/treegrid accessibles if node is not of a child or
      // valid child of HTML table and is not a HTML table.
      if ((!partOfHTMLTable || !tryTagNameOrFrame) &&
          frameType != nsGkAtoms::tableOuterFrame) {

        if (roleMapEntry->role == roles::TABLE ||
            roleMapEntry->role == roles::TREE_TABLE) {
          newAcc = new ARIAGridAccessibleWrap(content, docAcc);

        } else if (roleMapEntry->role == roles::GRID_CELL ||
            roleMapEntry->role == roles::ROWHEADER ||
            roleMapEntry->role == roles::COLUMNHEADER) {
          newAcc = new ARIAGridCellAccessibleWrap(content, docAcc);
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
                                            docAcc);

      if (!newAcc) {
        // Do not create accessible object subtrees for non-rendered table
        // captions. This could not be done in
        // nsTableCaptionFrame::GetAccessible() because the descendants of
        // the table caption would still be created. By setting
        // *aIsSubtreeHidden = true we ensure that no descendant accessibles
        // are created.
        nsIFrame* f = weakFrame.GetFrame();
        if (!f) {
          f = aDoc->PresShell()->GetRealPrimaryFrameFor(content);
        }
        if (f->GetType() == nsGkAtoms::tableCaptionFrame &&
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
    newAcc = CreateAccessibleByType(content, docAcc);
  }

  if (!newAcc) {
    // Create generic accessibles for SVG and MathML nodes.
    if (content->IsSVG(nsGkAtoms::svg)) {
      newAcc = new EnumRoleAccessible(content, docAcc, roles::DIAGRAM);
    }
    else if (content->IsMathML(nsGkAtoms::math)) {
      newAcc = new EnumRoleAccessible(content, docAcc, roles::EQUATION);
    }
  }

  if (!newAcc) {
    newAcc = CreateAccessibleForDeckChild(weakFrame.GetFrame(), content,
                                          docAcc);
  }

  // If no accessible, see if we need to create a generic accessible because
  // of some property that makes this object interesting
  // We don't do this for <body>, <html>, <window>, <dialog> etc. which
  // correspond to the doc accessible and will be created in any case
  if (!newAcc && content->Tag() != nsGkAtoms::body && content->GetParent() &&
      ((weakFrame.GetFrame() && weakFrame.GetFrame()->IsFocusable()) ||
       (isHTML && nsCoreUtils::HasClickListener(content)) ||
       HasUniversalAriaProperty(content) || roleMapEntry ||
       HasRelatedContent(content) || nsCoreUtils::IsXLink(content))) {
    // This content is focusable or has an interesting dynamic content accessibility property.
    // If it's interesting we need it in the accessibility hierarchy so that events or
    // other accessibles can point to it, or so that it can hold a state, etc.
    if (isHTML) {
      // Interesting HTML container which may have selectable text and/or embedded objects
      newAcc = new HyperTextAccessibleWrap(content, docAcc);
    }
    else {  // XUL, SVG, MathML etc.
      // Interesting generic non-HTML container
      newAcc = new AccessibleWrap(content, docAcc);
    }
  }

  return docAcc->BindToDocument(newAcc, roleMapEntry) ? newAcc : nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessibilityService private

bool
nsAccessibilityService::Init()
{
  // Initialize accessible document manager.
  if (!nsAccDocManager::Init())
    return false;

  // Add observers.
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService)
    return false;

  observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);

  static const PRUnichar kInitIndicator[] = { '1', 0 };
  observerService->NotifyObservers(nsnull, "a11y-init-or-shutdown", kInitIndicator);

#ifdef DEBUG
  logging::CheckEnv();
#endif

  // Initialize accessibility.
  nsAccessNodeWrap::InitAccessibility();

  gIsShutdown = false;
  return true;
}

void
nsAccessibilityService::Shutdown()
{
  // Remove observers.
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);

    static const PRUnichar kShutdownIndicator[] = { '0', 0 };
    observerService->NotifyObservers(nsnull, "a11y-init-or-shutdown", kShutdownIndicator);
  }

  // Stop accessible document loader.
  nsAccDocManager::Shutdown();

  // Application is going to be closed, shutdown accessibility and mark
  // accessibility service as shutdown to prevent calls of its methods.
  // Don't null accessibility service static member at this point to be safe
  // if someone will try to operate with it.

  NS_ASSERTION(!gIsShutdown, "Accessibility was shutdown already");

  gIsShutdown = true;

  nsAccessNodeWrap::ShutdownAccessibility();
}

bool
nsAccessibilityService::HasUniversalAriaProperty(nsIContent *aContent)
{
  // ARIA attributes that take token values (NMTOKEN, bool) are special cased
  // because of special value "undefined" (see HasDefinedARIAToken).
  return nsAccUtils::HasDefinedARIAToken(aContent, nsGkAtoms::aria_atomic) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsGkAtoms::aria_busy) ||
         aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_controls) ||
         aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_describedby) ||
         aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_disabled) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsGkAtoms::aria_dropeffect) ||
         aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_flowto) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsGkAtoms::aria_grabbed) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsGkAtoms::aria_haspopup) ||
         aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_hidden) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsGkAtoms::aria_invalid) ||
         aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_label) ||
         aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_labelledby) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsGkAtoms::aria_live) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsGkAtoms::aria_owns) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsGkAtoms::aria_relevant);
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateAccessibleByType(nsIContent* aContent,
                                               DocAccessible* aDoc)
{
  nsCOMPtr<nsIAccessibleProvider> accessibleProvider(do_QueryInterface(aContent));
  if (!accessibleProvider)
    return nsnull;

  PRInt32 type;
  nsresult rv = accessibleProvider->GetAccessibleType(&type);
  if (NS_FAILED(rv))
    return nsnull;

  if (type == nsIAccessibleProvider::OuterDoc) {
    Accessible* accessible = new OuterDocAccessible(aContent, aDoc);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  Accessible* accessible = nsnull;
  switch (type)
  {
#ifdef MOZ_XUL
    case nsIAccessibleProvider::NoAccessible:
      return nsnull;

    // XUL controls
    case nsIAccessibleProvider::XULAlert:
      accessible = new XULAlertAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULButton:
      accessible = new XULButtonAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULCheckbox:
      accessible = new XULCheckboxAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULColorPicker:
      accessible = new XULColorPickerAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULColorPickerTile:
      accessible = new XULColorPickerTileAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULCombobox:
      accessible = new XULComboboxAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULDropmarker:
      accessible = new XULDropmarkerAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULGroupbox:
      accessible = new XULGroupboxAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULImage:
    {
      // Don't include nameless images in accessible tree.
      if (!aContent->HasAttr(kNameSpaceID_None,
                             nsGkAtoms::tooltiptext))
        return nsnull;

      accessible = new ImageAccessibleWrap(aContent, aDoc);
      break;

    }
    case nsIAccessibleProvider::XULLink:
      accessible = new XULLinkAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULListbox:
      accessible = new XULListboxAccessibleWrap(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULListCell:
      accessible = new XULListCellAccessibleWrap(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULListHead:
      accessible = new XULColumAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULListHeader:
      accessible = new XULColumnItemAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULListitem:
      accessible = new XULListitemAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULMenubar:
      accessible = new XULMenubarAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULMenuitem:
      accessible = new XULMenuitemAccessibleWrap(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULMenupopup:
    {
#ifdef MOZ_ACCESSIBILITY_ATK
      // ATK considers this node to be redundant when within menubars, and it makes menu
      // navigation with assistive technologies more difficult
      // XXX In the future we will should this for consistency across the nsIAccessible
      // implementations on each platform for a consistent scripting environment, but
      // then strip out redundant accessibles in the AccessibleWrap class for each platform.
      nsIContent *parent = aContent->GetParent();
      if (parent && parent->NodeInfo()->Equals(nsGkAtoms::menu,
                                               kNameSpaceID_XUL))
        return nsnull;
#endif
      accessible = new XULMenupopupAccessible(aContent, aDoc);
      break;

    }
    case nsIAccessibleProvider::XULMenuSeparator:
      accessible = new XULMenuSeparatorAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULPane:
      accessible = new EnumRoleAccessible(aContent, aDoc, roles::PANE);
      break;

    case nsIAccessibleProvider::XULProgressMeter:
      accessible = new XULProgressMeterAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULStatusBar:
      accessible = new XULStatusBarAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULScale:
      accessible = new XULSliderAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULRadioButton:
      accessible = new XULRadioButtonAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULRadioGroup:
      accessible = new XULRadioGroupAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULTab:
      accessible = new XULTabAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULTabs:
      accessible = new XULTabsAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULTabpanels:
      accessible = new XULTabpanelsAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULText:
      accessible = new XULLabelAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULTextBox:
      accessible = new XULTextFieldAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULThumb:
      accessible = new XULThumbAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULTree:
      return CreateAccessibleForXULTree(aContent, aDoc);

    case nsIAccessibleProvider::XULTreeColumns:
      accessible = new XULTreeColumAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULTreeColumnItem:
      accessible = new XULColumnItemAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULToolbar:
      accessible = new XULToolbarAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULToolbarSeparator:
      accessible = new XULToolbarSeparatorAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULTooltip:
      accessible = new XULTooltipAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XULToolbarButton:
      accessible = new XULToolbarButtonAccessible(aContent, aDoc);
      break;

#endif // MOZ_XUL

    // XForms elements
    case nsIAccessibleProvider::XFormsContainer:
      accessible = new nsXFormsContainerAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XFormsLabel:
      accessible = new nsXFormsLabelAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XFormsOutput:
      accessible = new nsXFormsOutputAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XFormsTrigger:
      accessible = new nsXFormsTriggerAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XFormsInput:
      accessible = new nsXFormsInputAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XFormsInputBoolean:
      accessible = new nsXFormsInputBooleanAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XFormsInputDate:
      accessible = new nsXFormsInputDateAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XFormsSecret:
      accessible = new nsXFormsSecretAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XFormsSliderRange:
      accessible = new nsXFormsRangeAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XFormsSelect:
      accessible = new nsXFormsSelectAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XFormsChoices:
      accessible = new nsXFormsChoicesAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XFormsSelectFull:
      accessible = new nsXFormsSelectFullAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XFormsItemCheckgroup:
      accessible = new nsXFormsItemCheckgroupAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XFormsItemRadiogroup:
      accessible = new nsXFormsItemRadiogroupAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XFormsSelectCombobox:
      accessible = new nsXFormsSelectComboboxAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XFormsItemCombobox:
      accessible = new nsXFormsItemComboboxAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XFormsDropmarkerWidget:
      accessible = new nsXFormsDropmarkerWidgetAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XFormsCalendarWidget:
      accessible = new nsXFormsCalendarWidgetAccessible(aContent, aDoc);
      break;

    case nsIAccessibleProvider::XFormsComboboxPopupWidget:
      accessible = new nsXFormsComboboxPopupWidgetAccessible(aContent, aDoc);
      break;

    default:
      return nsnull;
  }

  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLAccessibleByMarkup(nsIFrame* aFrame,
                                                     nsIContent* aContent,
                                                     DocAccessible* aDoc)
{
  // This method assumes we're in an HTML namespace.
  nsIAtom* tag = aContent->Tag();
  if (tag == nsGkAtoms::figcaption) {
    Accessible* accessible = new HTMLFigcaptionAccessible(aContent, aDoc);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsGkAtoms::figure) {
    Accessible* accessible = new HTMLFigureAccessible(aContent, aDoc);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsGkAtoms::legend) {
    Accessible* accessible = new HTMLLegendAccessible(aContent, aDoc);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsGkAtoms::option) {
    Accessible* accessible = new HTMLSelectOptionAccessible(aContent, aDoc);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsGkAtoms::optgroup) {
    Accessible* accessible = new HTMLSelectOptGroupAccessible(aContent, aDoc);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsGkAtoms::ul || tag == nsGkAtoms::ol ||
      tag == nsGkAtoms::dl) {
    Accessible* accessible = new HTMLListAccessible(aContent, aDoc);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsGkAtoms::a) {
    // Only some roles truly enjoy life as HTMLLinkAccessibles, for details
    // see closed bug 494807.
    nsRoleMapEntry* roleMapEntry = aria::GetRoleMap(aContent);
    if (roleMapEntry && roleMapEntry->role != roles::NOTHING &&
        roleMapEntry->role != roles::LINK) {
      Accessible* accessible = new HyperTextAccessibleWrap(aContent, aDoc);
      NS_IF_ADDREF(accessible);
      return accessible;
    }

    Accessible* accessible = new HTMLLinkAccessible(aContent, aDoc);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsGkAtoms::dt ||
      (tag == nsGkAtoms::li &&
       aFrame->GetType() != nsGkAtoms::blockFrame)) {
    // Normally for li, it is created by the list item frame (in nsBlockFrame)
    // which knows about the bullet frame; however, in this case the list item
    // must have been styled using display: foo
    Accessible* accessible = new HTMLLIAccessible(aContent, aDoc);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsGkAtoms::abbr ||
      tag == nsGkAtoms::acronym ||
      tag == nsGkAtoms::blockquote ||
      tag == nsGkAtoms::dd ||
      tag == nsGkAtoms::form ||
      tag == nsGkAtoms::h1 ||
      tag == nsGkAtoms::h2 ||
      tag == nsGkAtoms::h3 ||
      tag == nsGkAtoms::h4 ||
      tag == nsGkAtoms::h5 ||
      tag == nsGkAtoms::h6 ||
      tag == nsGkAtoms::q) {
    Accessible* accessible = new HyperTextAccessibleWrap(aContent, aDoc);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (nsCoreUtils::IsHTMLTableHeader(aContent)) {
    Accessible* accessible = new HTMLTableHeaderCellAccessibleWrap(aContent, aDoc);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsGkAtoms::output) {
    Accessible* accessible = new HTMLOutputAccessible(aContent, aDoc);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsGkAtoms::progress) {
    Accessible* accessible =
      new HTMLProgressMeterAccessible(aContent, aDoc);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  return nsnull;
 }

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibilityService (DON'T put methods here)

Accessible*
nsAccessibilityService::AddNativeRootAccessible(void* aAtkAccessible)
{
#ifdef MOZ_ACCESSIBILITY_ATK
  ApplicationAccessible* applicationAcc =
    nsAccessNode::GetApplicationAccessible();
  if (!applicationAcc)
    return nsnull;

  nsRefPtr<NativeRootAccessibleWrap> nativeRootAcc =
    new NativeRootAccessibleWrap(static_cast<AtkObject*>(aAtkAccessible));
  if (!nativeRootAcc)
    return nsnull;

  if (applicationAcc->AppendChild(nativeRootAcc))
    return nativeRootAcc;
#endif

  return nsnull;
}

void
nsAccessibilityService::RemoveNativeRootAccessible(Accessible* aAccessible)
{
#ifdef MOZ_ACCESSIBILITY_ATK
  ApplicationAccessible* applicationAcc =
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

already_AddRefed<Accessible>
nsAccessibilityService::CreateAccessibleForDeckChild(nsIFrame* aFrame,
                                                     nsIContent* aContent,
                                                     DocAccessible* aDoc)
{
  if (aFrame->GetType() == nsGkAtoms::boxFrame ||
      aFrame->GetType() == nsGkAtoms::scrollFrame) {

    nsIFrame* parentFrame = aFrame->GetParent();
    if (parentFrame && parentFrame->GetType() == nsGkAtoms::deckFrame) {
      // If deck frame is for xul:tabpanels element then the given node has
      // tabpanel accessible.
      nsCOMPtr<nsIContent> parentContent = parentFrame->GetContent();
#ifdef MOZ_XUL
      if (parentContent->NodeInfo()->Equals(nsGkAtoms::tabpanels,
                                            kNameSpaceID_XUL)) {
        Accessible* accessible = new XULTabpanelAccessible(aContent, aDoc);
        NS_IF_ADDREF(accessible);
        return accessible;
      }
#endif
      Accessible* accessible = new EnumRoleAccessible(aContent, aDoc,
                                                      roles::PROPERTYPAGE);
      NS_IF_ADDREF(accessible);
      return accessible;
    }
  }

  return nsnull;
}

#ifdef MOZ_XUL
already_AddRefed<Accessible>
nsAccessibilityService::CreateAccessibleForXULTree(nsIContent* aContent,
                                                   DocAccessible* aDoc)
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
    Accessible* accessible = new XULTreeAccessible(aContent, aDoc);
    NS_IF_ADDREF(accessible);
    return accessible;
  }

  // Table or tree table accessible.
  Accessible* accessible = new XULTreeGridAccessibleWrap(aContent, aDoc);
  NS_IF_ADDREF(accessible);
  return accessible;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Services
////////////////////////////////////////////////////////////////////////////////

namespace mozilla {
namespace a11y {

FocusManager*
FocusMgr()
{
  return nsAccessibilityService::gAccessibilityService;
}

EPlatformDisabledState
PlatformDisabledState()
{
  static int disabledState = 0xff;

  if (disabledState == 0xff) {
    disabledState = Preferences::GetInt("accessibility.force_disabled", 0);
    if (disabledState < ePlatformIsForceEnabled)
      disabledState = ePlatformIsForceEnabled;
    else if (disabledState > ePlatformIsDisabled)
      disabledState = ePlatformIsDisabled;
  }

  return (EPlatformDisabledState)disabledState;
}

}
}
