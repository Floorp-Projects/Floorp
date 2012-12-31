/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAccessibilityService.h"

// NOTE: alphabetically ordered
#include "Accessible-inl.h"
#include "ApplicationAccessibleWrap.h"
#include "ARIAGridAccessibleWrap.h"
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
#include "nsEventShell.h"
#include "nsIAccessibleProvider.h"
#include "OuterDocAccessible.h"
#include "Platform.h"
#include "Role.h"
#include "RootAccessibleWrap.h"
#include "States.h"
#include "Statistics.h"
#include "TextLeafAccessibleWrap.h"

#ifdef MOZ_ACCESSIBILITY_ATK
#include "AtkSocketAccessible.h"
#endif

#ifdef XP_WIN
#include "HTMLWin32ObjectAccessible.h"
#endif

#ifdef A11Y_LOG
#include "Logging.h"
#endif

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

#include "nsIDOMDocument.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMXULElement.h"
#include "nsImageFrame.h"
#include "nsIObserverService.h"
#include "nsLayoutUtils.h"
#include "nsNPAPIPluginInstance.h"
#include "nsObjectFrame.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Util.h"
#include "nsDeckFrame.h"

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
// Statics
////////////////////////////////////////////////////////////////////////////////

/**
 * Return true if the element must be accessible.
 */
static bool
MustBeAccessible(nsIContent* aContent, DocAccessible* aDocument)
{
  if (aContent->GetPrimaryFrame()->IsFocusable())
    return true;

  PRUint32 attrCount = aContent->GetAttrCount();
  for (PRUint32 attrIdx = 0; attrIdx < attrCount; attrIdx++) {
    const nsAttrName* attr = aContent->GetAttrNameAt(attrIdx);
    if (attr->NamespaceEquals(kNameSpaceID_None)) {
      nsIAtom* attrAtom = attr->Atom();
      nsDependentAtomString attrStr(attrAtom);
      if (!StringBeginsWith(attrStr, NS_LITERAL_STRING("aria-")))
        continue; // not ARIA

      // A global state or a property and in case of token defined.
      uint8_t attrFlags = nsAccUtils::GetAttributeCharacteristics(attrAtom);
      if ((attrFlags & ATTR_GLOBAL) && (!(attrFlags & ATTR_VALTOKEN) ||
           nsAccUtils::HasDefinedARIAToken(aContent, attrAtom))) {
        return true;
      }
    }
  }

  // If the given ID is referred by relation attribute then create an accessible
  // for it.
  nsAutoString id;
  if (nsCoreUtils::GetID(aContent, id) && !id.IsEmpty())
    return aDocument->IsDependentID(id);

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessibilityService
////////////////////////////////////////////////////////////////////////////////

nsAccessibilityService *nsAccessibilityService::gAccessibilityService = nullptr;
ApplicationAccessible* nsAccessibilityService::gApplicationAccessible = nullptr;
bool nsAccessibilityService::gIsShutdown = true;

nsAccessibilityService::nsAccessibilityService() :
  DocManager(), FocusManager()
{
}

nsAccessibilityService::~nsAccessibilityService()
{
  NS_ASSERTION(gIsShutdown, "Accessibility wasn't shutdown!");
  gAccessibilityService = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED3(nsAccessibilityService,
                             DocManager,
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
nsAccessibilityService::FireAccessibleEvent(uint32_t aEvent,
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
  nsIPresShell* ps = aPresShell;
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
        ps = presShell;
      }

      return aCanCreate ? GetDocAccessible(ps) : ps->GetDocAccessible();
    }
  }
  return nullptr;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreatePluginAccessible(nsObjectFrame* aFrame,
                                               nsIContent* aContent,
                                               Accessible* aContext)
{
  // nsObjectFrame means a plugin, so we need to use the accessibility support
  // of the plugin.
  if (aFrame->GetRect().IsEmpty())
    return nullptr;

#if defined(XP_WIN) || defined(MOZ_ACCESSIBILITY_ATK)
  nsRefPtr<nsNPAPIPluginInstance> pluginInstance;
  if (NS_SUCCEEDED(aFrame->GetPluginInstance(getter_AddRefs(pluginInstance))) &&
      pluginInstance) {
#ifdef XP_WIN
    // Note: pluginPort will be null if windowless.
    HWND pluginPort = nullptr;
    aFrame->GetPluginPort(&pluginPort);

    Accessible* accessible =
      new HTMLWin32ObjectOwnerAccessible(aContent, aContext->Document(),
                                         pluginPort);
    NS_ADDREF(accessible);
    return accessible;

#elif MOZ_ACCESSIBILITY_ATK
    if (!AtkSocketAccessible::gCanEmbed)
      return nullptr;

    // Note this calls into the plugin, so crazy things may happen and aFrame
    // may go away.
    nsCString plugId;
    nsresult rv = pluginInstance->GetValueFromPlugin(
      NPPVpluginNativeAccessibleAtkPlugId, &plugId);
    if (NS_SUCCEEDED(rv) && !plugId.IsEmpty()) {
      AtkSocketAccessible* socketAccessible =
        new AtkSocketAccessible(aContent, aContext->Document(), plugId);

      NS_ADDREF(socketAccessible);
      return socketAccessible;
    }
#endif
  }
#endif

  return nullptr;
}

void
nsAccessibilityService::DeckPanelSwitched(nsIPresShell* aPresShell,
                                          nsIContent* aDeckNode,
                                          nsIFrame* aPrevBoxFrame,
                                          nsIFrame* aCurrentBoxFrame)
{
  // Ignore tabpanels elements (a deck having an accessible) since their
  // children are accessible not depending on selected tab.
  DocAccessible* document = GetDocAccessible(aPresShell);
  if (!document || document->HasAccessible(aDeckNode))
    return;

  if (aPrevBoxFrame) {
    nsIContent* panelNode = aPrevBoxFrame->GetContent();
#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eTree)) {
      logging::MsgBegin("TREE", "deck panel unselected");
      logging::Node("container", panelNode);
      logging::Node("content", aDeckNode);
      logging::MsgEnd();
    }
#endif

    document->ContentRemoved(aDeckNode, panelNode);
  }

  if (aCurrentBoxFrame) {
    nsIContent* panelNode = aCurrentBoxFrame->GetContent();
#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eTree)) {
      logging::MsgBegin("TREE", "deck panel selected");
      logging::Node("container", panelNode);
      logging::Node("content", aDeckNode);
      logging::MsgEnd();
    }
#endif

    document->ContentInserted(aDeckNode, panelNode, panelNode->GetNextSibling());
  }
}

void
nsAccessibilityService::ContentRangeInserted(nsIPresShell* aPresShell,
                                             nsIContent* aContainer,
                                             nsIContent* aStartChild,
                                             nsIContent* aEndChild)
{
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eTree)) {
    logging::MsgBegin("TREE", "content inserted");
    logging::Node("container", aContainer);
    for (nsIContent* child = aStartChild; child != aEndChild;
         child = child->GetNextSibling()) {
      logging::Node("content", child);
    }
    logging::MsgEnd();
    logging::Stack();
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
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eTree)) {
    logging::MsgBegin("TREE", "content removed");
    logging::Node("container", aContainer);
    logging::Node("content", aChild);
    logging::MsgEnd();
    logging::Stack();
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
nsAccessibilityService::PresShellActivated(nsIPresShell* aPresShell)
{
  DocAccessible* document = aPresShell->GetDocAccessible();
  if (document) {
    RootAccessible* rootDocument = document->RootAccessible();
    NS_ASSERTION(rootDocument, "Entirely broken tree: no root document!");
    if (rootDocument)
      rootDocument->DocumentActivated(document);
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

  NS_IF_ADDREF(*aAccessibleApplication = ApplicationAcc());

  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::GetAccessibleFor(nsIDOMNode *aNode,
                                         nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nullptr;
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
nsAccessibilityService::GetStringRole(uint32_t aRole, nsAString& aString)
{
#define ROLE(geckoRole, stringRole, atkRole, \
             macRole, msaaRole, ia2Role, nameRule) \
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
nsAccessibilityService::GetStringStates(uint32_t aState, uint32_t aExtraState,
                                        nsIDOMDOMStringList **aStringStates)
{
  nsAccessibleDOMStringList* stringStates = new nsAccessibleDOMStringList();
  NS_ENSURE_TRUE(stringStates, NS_ERROR_OUT_OF_MEMORY);

  uint64_t state = nsAccUtils::To64State(aState, aExtraState);

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
  uint32_t stringStatesLength = 0;
  stringStates->GetLength(&stringStatesLength);
  if (!stringStatesLength)
    stringStates->Add(NS_LITERAL_STRING("unknown"));

  NS_ADDREF(*aStringStates = stringStates);
  return NS_OK;
}

// nsIAccessibleRetrieval::getStringEventType()
NS_IMETHODIMP
nsAccessibilityService::GetStringEventType(uint32_t aEventType,
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
nsAccessibilityService::GetStringRelationType(uint32_t aRelationType,
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
  *aAccessible = nullptr;
  if (!aNode)
    return NS_OK;

  nsCOMPtr<nsINode> node(do_QueryInterface(aNode));
  if (!node)
    return NS_ERROR_INVALID_ARG;

  // Search for an accessible in each of our per document accessible object
  // caches. If we don't find it, and the given node is itself a document, check
  // our cache of document accessibles (document cache). Note usually shutdown
  // document accessibles are not stored in the document cache, however an
  // "unofficially" shutdown document (i.e. not from DocManager) can still
  // exist in the document cache.
  Accessible* accessible = FindAccessibleInCache(node);
  if (!accessible) {
    nsCOMPtr<nsIDocument> document(do_QueryInterface(node));
    if (document)
      accessible = GetExistingDocAccessible(document);
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
  *aPivot = nullptr;

  nsRefPtr<Accessible> accessibleRoot(do_QueryObject(aRoot));
  NS_ENSURE_TRUE(accessibleRoot, NS_ERROR_INVALID_ARG);

  nsAccessiblePivot* pivot = new nsAccessiblePivot(accessibleRoot);
  NS_ADDREF(*aPivot = pivot);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::SetLogging(const nsACString& aModules)
{
#ifdef A11Y_LOG
  logging::Enable(PromiseFlatCString(aModules));
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::IsLogged(const nsAString& aModule, bool* aIsLogged)
{
  NS_ENSURE_ARG_POINTER(aIsLogged);
  *aIsLogged = false;

#ifdef A11Y_LOG
  *aIsLogged = logging::IsEnabled(aModule);
#endif

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessibilityService public

Accessible*
nsAccessibilityService::GetOrCreateAccessible(nsINode* aNode,
                                              Accessible* aContext,
                                              bool* aIsSubtreeHidden)
{
  NS_PRECONDITION(aContext && aNode && !gIsShutdown,
                  "Maybe let'd do a crash? Oh, yes, baby!");

  if (aIsSubtreeHidden)
    *aIsSubtreeHidden = false;

  DocAccessible* document = aContext->Document();

  // Check to see if we already have an accessible for this node in the cache.
  // XXX: we don't have context check here. It doesn't really necessary until
  // we have in-law children adoption.
  Accessible* cachedAccessible = document->GetAccessible(aNode);
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
    return nullptr;
  }

  if (aNode->OwnerDoc() != document->DocumentNode()) {
    NS_ERROR("Creating accessible for wrong document");
    return nullptr;
  }

  if (!aNode->IsContent())
    return nullptr;

  nsIContent* content = aNode->AsContent();
  nsIFrame* frame = content->GetPrimaryFrame();

  // Check frame and its visibility. Note, hidden frame allows visible
  // elements in subtree.
  if (!frame || !frame->GetStyleVisibility()->IsVisible()) {
    if (aIsSubtreeHidden && !frame)
      *aIsSubtreeHidden = true;

    return nullptr;
  }

  if (frame->GetContent() != content) {
    // Not the main content for this frame. This happens because <area>
    // elements return the image frame as their primary frame. The main content
    // for the image frame is the image content. If the frame is not an image
    // frame or the node is not an area element then null is returned.
    // This setup will change when bug 135040 is fixed. Make sure we don't
    // create area accessible here. Hopefully assertion below will handle that.

#ifdef DEBUG
  nsImageFrame* imageFrame = do_QueryFrame(frame);
  NS_ASSERTION(imageFrame && content->IsHTML() && content->Tag() == nsGkAtoms::area,
               "Unknown case of not main content for the frame!");
#endif
    return nullptr;
  }

#ifdef DEBUG
  nsImageFrame* imageFrame = do_QueryFrame(frame);
  NS_ASSERTION(!imageFrame || !content->IsHTML() || content->Tag() != nsGkAtoms::area,
               "Image map manages the area accessible creation!");
#endif

  // Attempt to create an accessible based on what we know.
  nsRefPtr<Accessible> newAcc;

  // Create accessible for visible text frames.
  if (content->IsNodeOfType(nsINode::eTEXT)) {
    nsAutoString text;
    frame->GetRenderedText(&text, nullptr, nullptr, 0, UINT32_MAX);
    if (text.IsEmpty()) {
      if (aIsSubtreeHidden)
        *aIsSubtreeHidden = true;

      return nullptr;
    }

    newAcc = CreateAccessibleByFrameType(frame, content, aContext);
    if (document->BindToDocument(newAcc, nullptr)) {
      newAcc->AsTextLeaf()->SetText(text);
      return newAcc;
    }

    return nullptr;
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
    if (nsLayoutUtils::GetAllInFlowRectsUnion(frame,
                                              frame->GetParent()).IsEmpty()) {
      if (aIsSubtreeHidden)
        *aIsSubtreeHidden = true;

      return nullptr;
    }

    newAcc = new HyperTextAccessibleWrap(content, document);
    if (document->BindToDocument(newAcc, aria::GetRoleMap(aNode)))
      return newAcc;
    return nullptr;
  }

  nsRoleMapEntry* roleMapEntry = aria::GetRoleMap(aNode);

  // If the element is focusable or global ARIA attribute is applied to it or
  // it is referenced by ARIA relationship then treat role="presentation" on
  // the element as the role is not there.
  if (roleMapEntry && roleMapEntry->Is(nsGkAtoms::presentation)) {
    if (!MustBeAccessible(content, document))
      return nullptr;

    roleMapEntry = nullptr;
  }

  if (!newAcc && isHTML) {  // HTML accessibles
    if (roleMapEntry) {
      // Create pure ARIA grid/treegrid related accessibles if they weren't used
      // on accessible HTML table elements.
      if ((roleMapEntry->accTypes & eTableCell)) {
        if (aContext->IsTableRow() &&
            (frame->AccessibleType() != eHTMLTableCellType ||
             aContext->GetContent() != content->GetParent())) {
          newAcc = new ARIAGridCellAccessibleWrap(content, document);
        }

      } else if ((roleMapEntry->IsOfType(eTable)) &&
                 frame->AccessibleType() != eHTMLTableType) {
        newAcc = new ARIAGridAccessibleWrap(content, document);
      }
    }

    if (!newAcc) {
      // Prefer to use markup (mostly tag name, perhaps attributes) to decide if
      // and what kind of accessible to create.
      newAcc = CreateHTMLAccessibleByMarkup(frame, content, aContext);

      // Try using frame to do it.
      if (!newAcc)
        newAcc = CreateAccessibleByFrameType(frame, content, aContext);

      // If table has strong ARIA role then all table descendants shouldn't
      // expose their native roles.
      if (!roleMapEntry && newAcc) {
        if (frame->AccessibleType() == eHTMLTableRowType) {
          nsRoleMapEntry* contextRoleMap = aContext->ARIARoleMap();
          if (contextRoleMap && !(contextRoleMap->IsOfType(eTable)))
            roleMapEntry = &nsARIAMap::gEmptyRoleMap;

        } else if (frame->AccessibleType() == eHTMLTableCellType &&
                   aContext->ARIARoleMap() == &nsARIAMap::gEmptyRoleMap) {
          roleMapEntry = &nsARIAMap::gEmptyRoleMap;

        } else if (content->Tag() == nsGkAtoms::dt ||
                   content->Tag() == nsGkAtoms::li ||
                   content->Tag() == nsGkAtoms::dd ||
                   frame->AccessibleType() == eHTMLLiType) {
          nsRoleMapEntry* contextRoleMap = aContext->ARIARoleMap();
          if (contextRoleMap && !(contextRoleMap->IsOfType(eList)))
            roleMapEntry = &nsARIAMap::gEmptyRoleMap;
        }
      }
    }
  }

  // Accessible XBL types and deck stuff are used in XUL only currently.
  if (!newAcc && content->IsXUL()) {
    // No accessible for not selected deck panel and its children.
    if (!aContext->IsXULTabpanels()) {
      nsDeckFrame* deckFrame = do_QueryFrame(frame->GetParent());
      if (deckFrame && deckFrame->GetSelectedBox() != frame) {
        if (aIsSubtreeHidden)
          *aIsSubtreeHidden = true;

        return nullptr;
      }
    }

    // Elements may implement nsIAccessibleProvider via XBL. This allows them to
    // say what kind of accessible to create.
    newAcc = CreateAccessibleByType(content, document);

    // Any XUL box can be used as tabpanel, make sure we create a proper
    // accessible for it.
    if (!newAcc && aContext->IsXULTabpanels() &&
        content->GetParent() == aContext->GetContent()) {
      nsIAtom* frameType = frame->GetType();
      if (frameType == nsGkAtoms::boxFrame ||
          frameType == nsGkAtoms::scrollFrame) {
        newAcc = new XULTabpanelAccessible(content, document);
      }
    }
  }

  if (!newAcc) {
    if (content->IsSVG(nsGkAtoms::svg)) {
      newAcc = new EnumRoleAccessible(content, document, roles::DIAGRAM);
    } else if (content->IsMathML(nsGkAtoms::math)) {
      newAcc = new EnumRoleAccessible(content, document, roles::EQUATION);
    }
  }

  // If no accessible, see if we need to create a generic accessible because
  // of some property that makes this object interesting
  // We don't do this for <body>, <html>, <window>, <dialog> etc. which
  // correspond to the doc accessible and will be created in any case
  if (!newAcc && content->Tag() != nsGkAtoms::body && content->GetParent() &&
      (roleMapEntry || MustBeAccessible(content, document) ||
       (isHTML && nsCoreUtils::HasClickListener(content)))) {
    // This content is focusable or has an interesting dynamic content accessibility property.
    // If it's interesting we need it in the accessibility hierarchy so that events or
    // other accessibles can point to it, or so that it can hold a state, etc.
    if (isHTML) {
      // Interesting HTML container which may have selectable text and/or embedded objects
      newAcc = new HyperTextAccessibleWrap(content, document);
    } else {  // XUL, SVG, MathML etc.
      // Interesting generic non-HTML container
      newAcc = new AccessibleWrap(content, document);
    }
  }

  return document->BindToDocument(newAcc, roleMapEntry) ? newAcc : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessibilityService private

bool
nsAccessibilityService::Init()
{
  // Initialize accessible document manager.
  if (!DocManager::Init())
    return false;

  // Add observers.
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService)
    return false;

  observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);

  static const PRUnichar kInitIndicator[] = { '1', 0 };
  observerService->NotifyObservers(nullptr, "a11y-init-or-shutdown", kInitIndicator);

#ifdef A11Y_LOG
  logging::CheckEnv();
#endif

  gApplicationAccessible = new ApplicationAccessibleWrap();
  NS_ADDREF(gApplicationAccessible); // will release in Shutdown()

#ifdef MOZ_CRASHREPORTER
  CrashReporter::
    AnnotateCrashReport(NS_LITERAL_CSTRING("Accessibility"),
                        NS_LITERAL_CSTRING("Active"));
#endif

  gIsShutdown = false;

  // Now its safe to start platform accessibility.
  PlatformInit();

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
    observerService->NotifyObservers(nullptr, "a11y-init-or-shutdown", kShutdownIndicator);
  }

  // Stop accessible document loader.
  DocManager::Shutdown();

  // Application is going to be closed, shutdown accessibility and mark
  // accessibility service as shutdown to prevent calls of its methods.
  // Don't null accessibility service static member at this point to be safe
  // if someone will try to operate with it.

  NS_ASSERTION(!gIsShutdown, "Accessibility was shutdown already");

  gIsShutdown = true;

  PlatformShutdown();
  gApplicationAccessible->Shutdown();
  NS_RELEASE(gApplicationAccessible);
  gApplicationAccessible = nullptr;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateAccessibleByType(nsIContent* aContent,
                                               DocAccessible* aDoc)
{
  nsCOMPtr<nsIAccessibleProvider> accessibleProvider(do_QueryInterface(aContent));
  if (!accessibleProvider)
    return nullptr;

  int32_t type;
  nsresult rv = accessibleProvider->GetAccessibleType(&type);
  if (NS_FAILED(rv))
    return nullptr;

  if (type == nsIAccessibleProvider::OuterDoc) {
    Accessible* accessible = new OuterDocAccessible(aContent, aDoc);
    NS_ADDREF(accessible);
    return accessible;
  }

  Accessible* accessible = nullptr;
  switch (type)
  {
#ifdef MOZ_XUL
    case nsIAccessibleProvider::NoAccessible:
      return nullptr;

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

    case nsIAccessibleProvider::XULTabpanels:
      accessible = new XULTabpanelsAccessible(aContent, aDoc);
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
        return nullptr;

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
        return nullptr;
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

    default:
      return nullptr;
  }

  NS_IF_ADDREF(accessible);
  return accessible;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateHTMLAccessibleByMarkup(nsIFrame* aFrame,
                                                     nsIContent* aContent,
                                                     Accessible* aContext)
{
  DocAccessible* document = aContext->Document();
  if (aContext->IsTableRow()) {
    if (nsCoreUtils::IsHTMLTableHeader(aContent) &&
        aContext->GetContent() == aContent->GetParent()) {
      Accessible* accessible = new HTMLTableHeaderCellAccessibleWrap(aContent,
                                                                     document);
      NS_ADDREF(accessible);
      return accessible;
    }

    return nullptr;
  }

  // This method assumes we're in an HTML namespace.
  nsIAtom* tag = aContent->Tag();
  if (tag == nsGkAtoms::figcaption) {
    Accessible* accessible = new HTMLFigcaptionAccessible(aContent, document);
    NS_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsGkAtoms::figure) {
    Accessible* accessible = new HTMLFigureAccessible(aContent, document);
    NS_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsGkAtoms::legend) {
    Accessible* accessible = new HTMLLegendAccessible(aContent, document);
    NS_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsGkAtoms::option) {
    Accessible* accessible = new HTMLSelectOptionAccessible(aContent, document);
    NS_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsGkAtoms::optgroup) {
    Accessible* accessible =
      new HTMLSelectOptGroupAccessible(aContent, document);
    NS_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsGkAtoms::ul || tag == nsGkAtoms::ol ||
      tag == nsGkAtoms::dl) {
    Accessible* accessible = new HTMLListAccessible(aContent, document);
    NS_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsGkAtoms::a) {
    // Only some roles truly enjoy life as HTMLLinkAccessibles, for details
    // see closed bug 494807.
    nsRoleMapEntry* roleMapEntry = aria::GetRoleMap(aContent);
    if (roleMapEntry && roleMapEntry->role != roles::NOTHING &&
        roleMapEntry->role != roles::LINK) {
      Accessible* accessible = new HyperTextAccessibleWrap(aContent, document);
      NS_ADDREF(accessible);
      return accessible;
    }

    Accessible* accessible = new HTMLLinkAccessible(aContent, document);
    NS_ADDREF(accessible);
    return accessible;
  }

  if (aContext->IsList()) {
    // If list item is a child of accessible list then create an accessible for
    // it unconditionally by tag name. nsBlockFrame creates the list item
    // accessible for other elements styled as list items.
    if (aContext->GetContent() == aContent->GetParent()) {
      if (tag == nsGkAtoms::dt || tag == nsGkAtoms::li) {
        Accessible* accessible = new HTMLLIAccessible(aContent, document);
        NS_ADDREF(accessible);
        return accessible;
      }

      if (tag == nsGkAtoms::dd) {
        Accessible* accessible = new HyperTextAccessibleWrap(aContent, document);
        NS_ADDREF(accessible);
        return accessible;
      }
    }

    return nullptr;
  }

  if (tag == nsGkAtoms::abbr ||
      tag == nsGkAtoms::acronym ||
      tag == nsGkAtoms::blockquote ||
      tag == nsGkAtoms::form ||
      tag == nsGkAtoms::h1 ||
      tag == nsGkAtoms::h2 ||
      tag == nsGkAtoms::h3 ||
      tag == nsGkAtoms::h4 ||
      tag == nsGkAtoms::h5 ||
      tag == nsGkAtoms::h6 ||
      tag == nsGkAtoms::q) {
    Accessible* accessible = new HyperTextAccessibleWrap(aContent, document);
    NS_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsGkAtoms::output) {
    Accessible* accessible = new HTMLOutputAccessible(aContent, document);
    NS_ADDREF(accessible);
    return accessible;
  }

  if (tag == nsGkAtoms::progress) {
    Accessible* accessible =
      new HTMLProgressMeterAccessible(aContent, document);
    NS_ADDREF(accessible);
    return accessible;
  }

  return nullptr;
 }

already_AddRefed<Accessible>
nsAccessibilityService::CreateAccessibleByFrameType(nsIFrame* aFrame,
                                                    nsIContent* aContent,
                                                    Accessible* aContext)
{
  DocAccessible* document = aContext->Document();

  nsRefPtr<Accessible> newAcc;
  switch (aFrame->AccessibleType()) {
    case eNoType:
      return nullptr;
    case eHTMLBRType:
      newAcc = new HTMLBRAccessible(aContent, document);
      break;
    case eHTMLButtonType:
      newAcc = new HTMLButtonAccessible(aContent, document);
      break;
    case eHTMLCanvasType:
      newAcc = new HTMLCanvasAccessible(aContent, document);
      break;
    case eHTMLCaptionType:
      if (aContext->IsTable() &&
          aContext->GetContent() == aContent->GetParent()) {
        newAcc = new HTMLCaptionAccessible(aContent, document);
      }
      break;
    case eHTMLCheckboxType:
      newAcc = new HTMLCheckboxAccessible(aContent, document);
      break;
    case eHTMLComboboxType:
      newAcc = new HTMLComboboxAccessible(aContent, document);
      break;
    case eHTMLFileInputType:
      newAcc = new HTMLFileInputAccessible(aContent, document);
      break;
    case eHTMLGroupboxType:
      newAcc = new HTMLGroupboxAccessible(aContent, document);
      break;
    case eHTMLHRType:
      newAcc = new HTMLHRAccessible(aContent, document);
      break;
    case eHTMLImageMapType:
      newAcc = new HTMLImageMapAccessible(aContent, document);
      break;
    case eHTMLLabelType:
      newAcc = new HTMLLabelAccessible(aContent, document);
      break;
    case eHTMLLiType:
      if (aContext->IsList() &&
          aContext->GetContent() == aContent->GetParent()) {
        newAcc = new HTMLLIAccessible(aContent, document);
      }
      break;
    case eHTMLSelectListType:
      newAcc = new HTMLSelectListAccessible(aContent, document);
      break;
    case eHTMLMediaType:
      newAcc = new EnumRoleAccessible(aContent, document, roles::GROUPING);
      break;
    case eHTMLRadioButtonType:
      newAcc = new HTMLRadioButtonAccessible(aContent, document);
      break;
    case eHTMLTableType:
      newAcc = new HTMLTableAccessibleWrap(aContent, document);
      break;
    case eHTMLTableCellType:
      // Accessible HTML table cell must be a child of accessible HTML table row.
      if (aContext->IsHTMLTableRow())
        newAcc = new HTMLTableCellAccessibleWrap(aContent, document);
      break;

    case eHTMLTableRowType: {
      // Accessible HTML table row must be a child of tbody/tfoot/thead of
      // accessible HTML table or must be a child of accessible of HTML table.
      if (aContext->IsTable()) {
        nsIContent* parentContent = aContent->GetParent();
        nsIFrame* parentFrame = parentContent->GetPrimaryFrame();
        if (parentFrame->GetType() == nsGkAtoms::tableRowGroupFrame) {
          parentContent = parentContent->GetParent();
          parentFrame = parentContent->GetPrimaryFrame();
        }

        if (parentFrame->GetType() == nsGkAtoms::tableOuterFrame &&
            aContext->GetContent() == parentContent) {
          newAcc = new HTMLTableRowAccessible(aContent, document);
        }
      }
      break;
    }
    case eHTMLTextFieldType:
      newAcc = new HTMLTextFieldAccessible(aContent, document);
      break;
    case eHyperTextType:
      if (aContent->Tag() != nsGkAtoms::dt && aContent->Tag() != nsGkAtoms::dd)
        newAcc = new HyperTextAccessibleWrap(aContent, document);
      break;

    case eImageType:
      newAcc = new ImageAccessibleWrap(aContent, document);
      break;
    case eOuterDocType:
      newAcc = new OuterDocAccessible(aContent, document);
      break;
    case ePluginType: {
      nsObjectFrame* objectFrame = do_QueryFrame(aFrame);
      newAcc = CreatePluginAccessible(objectFrame, aContent, aContext);
      break;
    }
    case eTextLeafType:
      newAcc = new TextLeafAccessibleWrap(aContent, document);
      break;
  }

  return newAcc.forget();
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibilityService (DON'T put methods here)

Accessible*
nsAccessibilityService::AddNativeRootAccessible(void* aAtkAccessible)
{
#ifdef MOZ_ACCESSIBILITY_ATK
  ApplicationAccessible* applicationAcc = ApplicationAcc();
  if (!applicationAcc)
    return nullptr;

  GtkWindowAccessible* nativeWnd =
    new GtkWindowAccessible(static_cast<AtkObject*>(aAtkAccessible));

  if (applicationAcc->AppendChild(nativeWnd))
    return nativeWnd;
#endif

  return nullptr;
}

void
nsAccessibilityService::RemoveNativeRootAccessible(Accessible* aAccessible)
{
#ifdef MOZ_ACCESSIBILITY_ATK
  ApplicationAccessible* applicationAcc = ApplicationAcc();

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
  *aResult = nullptr;
 
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

#ifdef MOZ_XUL
already_AddRefed<Accessible>
nsAccessibilityService::CreateAccessibleForXULTree(nsIContent* aContent,
                                                   DocAccessible* aDoc)
{
  nsCOMPtr<nsITreeBoxObject> treeBoxObj = nsCoreUtils::GetTreeBoxObject(aContent);
  if (!treeBoxObj)
    return nullptr;

  nsCOMPtr<nsITreeColumns> treeColumns;
  treeBoxObj->GetColumns(getter_AddRefs(treeColumns));
  if (!treeColumns)
    return nullptr;

  int32_t count = 0;
  treeColumns->GetCount(&count);

  // Outline of list accessible.
  if (count == 1) {
    Accessible* accessible = new XULTreeAccessible(aContent, aDoc);
    NS_ADDREF(accessible);
    return accessible;
  }

  // Table or tree table accessible.
  Accessible* accessible = new XULTreeGridAccessibleWrap(aContent, aDoc);
  NS_ADDREF(accessible);
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

ApplicationAccessible*
ApplicationAcc()
{
  return nsAccessibilityService::gApplicationAccessible;
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
