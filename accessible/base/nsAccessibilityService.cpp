/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAccessibilityService.h"

// NOTE: alphabetically ordered
#include "ApplicationAccessibleWrap.h"
#include "ARIAGridAccessibleWrap.h"
#include "ARIAMap.h"
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
#include "RootAccessible.h"
#include "nsAccessiblePivot.h"
#include "nsAccUtils.h"
#include "nsAttrName.h"
#include "nsEventShell.h"
#include "nsIURI.h"
#include "OuterDocAccessible.h"
#include "Platform.h"
#include "Role.h"
#ifdef MOZ_ACCESSIBILITY_ATK
#include "RootAccessibleWrap.h"
#endif
#include "States.h"
#include "Statistics.h"
#include "TextLeafAccessibleWrap.h"

#ifdef MOZ_ACCESSIBILITY_ATK
#include "AtkSocketAccessible.h"
#endif

#ifdef XP_WIN
#include "mozilla/a11y/Compatibility.h"
#include "HTMLWin32ObjectAccessible.h"
#include "mozilla/StaticPtr.h"
#endif

#ifdef A11Y_LOG
#include "Logging.h"
#endif

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

#include "nsImageFrame.h"
#include "nsIObserverService.h"
#include "nsLayoutUtils.h"
#include "nsObjectFrame.h"
#include "nsSVGPathGeometryFrame.h"
#include "nsTreeBodyFrame.h"
#include "nsTreeColumns.h"
#include "nsTreeUtils.h"
#include "nsXBLPrototypeBinding.h"
#include "nsXBLBinding.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
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

#if defined(XP_WIN) || defined(MOZ_ACCESSIBILITY_ATK)
#include "nsNPAPIPluginInstance.h"
#endif

using namespace mozilla;
using namespace mozilla::a11y;
using namespace mozilla::dom;

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

  uint32_t attrCount = aContent->GetAttrCount();
  for (uint32_t attrIdx = 0; attrIdx < attrCount; attrIdx++) {
    const nsAttrName* attr = aContent->GetAttrNameAt(attrIdx);
    if (attr->NamespaceEquals(kNameSpaceID_None)) {
      nsIAtom* attrAtom = attr->Atom();
      nsDependentAtomString attrStr(attrAtom);
      if (!StringBeginsWith(attrStr, NS_LITERAL_STRING("aria-")))
        continue; // not ARIA

      // A global state or a property and in case of token defined.
      uint8_t attrFlags = aria::AttrCharacteristicsFor(attrAtom);
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

NS_IMPL_ISUPPORTS_INHERITED(nsAccessibilityService,
                            DocManager,
                            nsIAccessibilityService,
                            nsIAccessibleRetrieval,
                            nsIObserver,
                            nsISelectionListener) // from SelectionManager

////////////////////////////////////////////////////////////////////////////////
// nsIObserver

NS_IMETHODIMP
nsAccessibilityService::Observe(nsISupports *aSubject, const char *aTopic,
                         const char16_t *aData)
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
    nsCOMPtr<nsIDocShellTreeItem> treeItem(documentNode->GetDocShell());
    if (treeItem) {
      nsCOMPtr<nsIDocShellTreeItem> rootTreeItem;
      treeItem->GetRootTreeItem(getter_AddRefs(rootTreeItem));
      if (treeItem != rootTreeItem) {
        nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(rootTreeItem));
        ps = docShell->GetPresShell();
      }

      return aCanCreate ? GetDocAccessible(ps) : ps->GetDocAccessible();
    }
  }
  return nullptr;
}

#ifdef XP_WIN
static StaticAutoPtr<nsTArray<nsCOMPtr<nsIContent> > > sPendingPlugins;
static StaticAutoPtr<nsTArray<nsCOMPtr<nsITimer> > > sPluginTimers;

class PluginTimerCallBack MOZ_FINAL : public nsITimerCallback
{
public:
  PluginTimerCallBack(nsIContent* aContent) : mContent(aContent) {}

  NS_DECL_ISUPPORTS

  NS_IMETHODIMP Notify(nsITimer* aTimer) MOZ_FINAL
  {
    if (!mContent->IsInDoc())
      return NS_OK;

    nsIPresShell* ps = mContent->OwnerDoc()->GetShell();
    if (ps) {
      DocAccessible* doc = ps->GetDocAccessible();
      if (doc) {
        // Make sure that if we created an accessible for the plugin that wasn't
        // a plugin accessible we remove it before creating the right accessible.
        doc->RecreateAccessible(mContent);
        sPluginTimers->RemoveElement(aTimer);
        return NS_OK;
      }
    }

    // We couldn't get a doc accessible so presumably the document went away.
    // In this case don't leak our ref to the content or timer.
    sPendingPlugins->RemoveElement(mContent);
    sPluginTimers->RemoveElement(aTimer);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIContent> mContent;
};

NS_IMPL_ISUPPORTS(PluginTimerCallBack, nsITimerCallback)
#endif

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
    if (!sPendingPlugins->Contains(aContent) &&
        (Preferences::GetBool("accessibility.delay_plugins") ||
         Compatibility::IsJAWS() || Compatibility::IsWE())) {
      nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
      nsRefPtr<PluginTimerCallBack> cb = new PluginTimerCallBack(aContent);
      timer->InitWithCallback(cb, Preferences::GetUint("accessibility.delay_plugin_time"),
                              nsITimer::TYPE_ONE_SHOT);
      sPluginTimers->AppendElement(timer);
      sPendingPlugins->AppendElement(aContent);
      return nullptr;
    }

    // We need to remove aContent from the pending plugins here to avoid
    // reentrancy.  When the timer fires it calls
    // DocAccessible::ContentInserted() which does the work async.
    sPendingPlugins->RemoveElement(aContent);

    // Note: pluginPort will be null if windowless.
    HWND pluginPort = nullptr;
    aFrame->GetPluginPort(&pluginPort);

    nsRefPtr<Accessible> accessible =
      new HTMLWin32ObjectOwnerAccessible(aContent, aContext->Document(),
                                         pluginPort);
    return accessible.forget();

#elif MOZ_ACCESSIBILITY_ATK
    if (!AtkSocketAccessible::gCanEmbed)
      return nullptr;

    // Note this calls into the plugin, so crazy things may happen and aFrame
    // may go away.
    nsCString plugId;
    nsresult rv = pluginInstance->GetValueFromPlugin(
      NPPVpluginNativeAccessibleAtkPlugId, &plugId);
    if (NS_SUCCEEDED(rv) && !plugId.IsEmpty()) {
      nsRefPtr<AtkSocketAccessible> socketAccessible =
        new AtkSocketAccessible(aContent, aContext->Document(), plugId);

      return socketAccessible.forget();
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
nsAccessibilityService::RangeValueChanged(nsIPresShell* aPresShell,
                                          nsIContent* aContent)
{
  DocAccessible* document = GetDocAccessible(aPresShell);
  if (document) {
    Accessible* accessible = document->GetAccessible(aContent);
    if (accessible) {
      document->FireDelayedEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE,
                                 accessible);
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
nsAccessibilityService::UpdateLabelValue(nsIPresShell* aPresShell,
                                         nsIContent* aLabelElm,
                                         const nsString& aNewValue)
{
  DocAccessible* document = GetDocAccessible(aPresShell);
  if (document) {
    Accessible* accessible = document->GetAccessible(aLabelElm);
    if (accessible) {
      XULLabelAccessible* xulLabel = accessible->AsXULLabel();
      NS_ASSERTION(xulLabel,
                   "UpdateLabelValue was called for wrong accessible!");
      if (xulLabel)
        xulLabel->UpdateLabelValue(aNewValue);
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
                                        nsISupports **aStringStates)
{
  nsRefPtr<DOMStringList> stringStates = new DOMStringList();

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
  if (!stringStates->Length())
    stringStates->Add(NS_LITERAL_STRING("unknown"));

  stringStates.forget(aStringStates);
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
  NS_ENSURE_ARG(aRelationType <= static_cast<uint32_t>(RelationType::LAST));

#define RELATIONTYPE(geckoType, geckoTypeName, atkType, msaaType, ia2Type) \
  case RelationType::geckoType: \
    aString.AssignLiteral(geckoTypeName); \
    return NS_OK;

  RelationType relationType = static_cast<RelationType>(aRelationType);
  switch (relationType) {
#include "RelationTypeMap.h"
    default:
      aString.AssignLiteral("unknown");
      return NS_OK;
  }

#undef RELATIONTYPE
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
  if (!aNode->GetCrossShadowCurrentDoc()) {
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
  if (!frame || !frame->StyleVisibility()->IsVisible()) {
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
    // Ignore not rendered text nodes and whitespace text nodes between table
    // cells.
    if (text.IsEmpty() ||
        (aContext->IsTableRow() && nsCoreUtils::IsWhitespaceString(text))) {
      if (aIsSubtreeHidden)
        *aIsSubtreeHidden = true;

      return nullptr;
    }

    newAcc = CreateAccessibleByFrameType(frame, content, aContext);
    if (!aContext->IsAcceptableChild(newAcc))
      return nullptr;

    document->BindToDocument(newAcc, nullptr);
    newAcc->AsTextLeaf()->SetText(text);
    return newAcc;
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
    if (!aContext->IsAcceptableChild(newAcc))
      return nullptr;

    document->BindToDocument(newAcc, aria::GetRoleMap(aNode));
    return newAcc;
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
            roleMapEntry = &aria::gEmptyRoleMap;

        } else if (frame->AccessibleType() == eHTMLTableCellType &&
                   aContext->ARIARoleMap() == &aria::gEmptyRoleMap) {
          roleMapEntry = &aria::gEmptyRoleMap;

        } else if (content->Tag() == nsGkAtoms::dt ||
                   content->Tag() == nsGkAtoms::li ||
                   content->Tag() == nsGkAtoms::dd ||
                   frame->AccessibleType() == eHTMLLiType) {
          nsRoleMapEntry* contextRoleMap = aContext->ARIARoleMap();
          if (contextRoleMap && !(contextRoleMap->IsOfType(eList)))
            roleMapEntry = &aria::gEmptyRoleMap;
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

    // XBL bindings may use @role attribute to point the accessible type
    // they belong to.
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
    if (content->IsSVG()) {
      nsSVGPathGeometryFrame* pathGeometryFrame = do_QueryFrame(frame);
      if (pathGeometryFrame) {
        // A graphic elements: rect, circle, ellipse, line, path, polygon,
        // polyline and image. A 'use' and 'text' graphic elements require
        // special support.
        newAcc = new EnumRoleAccessible(content, document, roles::GRAPHIC);
      } else if (content->Tag() == nsGkAtoms::svg) {
        newAcc = new EnumRoleAccessible(content, document, roles::DIAGRAM);
      }
    } else if (content->IsMathML()){
      if (content->Tag() == nsGkAtoms::math)
        newAcc = new EnumRoleAccessible(content, document, roles::EQUATION);
      else
        newAcc = new HyperTextAccessible(content, document);
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

  if (!newAcc || !aContext->IsAcceptableChild(newAcc))
    return nullptr;

  document->BindToDocument(newAcc, roleMapEntry);
  return newAcc;
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

  static const char16_t kInitIndicator[] = { '1', 0 };
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

#ifdef XP_WIN
  sPendingPlugins = new nsTArray<nsCOMPtr<nsIContent> >;
  sPluginTimers = new nsTArray<nsCOMPtr<nsITimer> >;
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

    static const char16_t kShutdownIndicator[] = { '0', 0 };
    observerService->NotifyObservers(nullptr, "a11y-init-or-shutdown", kShutdownIndicator);
  }

  // Stop accessible document loader.
  DocManager::Shutdown();

  SelectionManager::Shutdown();

#ifdef XP_WIN
  sPendingPlugins = nullptr;

  uint32_t timerCount = sPluginTimers->Length();
  for (uint32_t i = 0; i < timerCount; i++)
    sPluginTimers->ElementAt(i)->Cancel();

  sPluginTimers = nullptr;
#endif

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
  nsAutoString role;
  for (const nsXBLBinding* binding = aContent->GetXBLBinding(); binding; binding = binding->GetBaseBinding()) {
    nsIContent* bindingElm = binding->PrototypeBinding()->GetBindingElement();
    bindingElm->GetAttr(kNameSpaceID_None, nsGkAtoms::role, role);
    if (!role.IsEmpty())
      break;
  }

  if (role.IsEmpty() || role.EqualsLiteral("none"))
    return nullptr;

  if (role.EqualsLiteral("outerdoc")) {
    nsRefPtr<Accessible> accessible = new OuterDocAccessible(aContent, aDoc);
    return accessible.forget();
  }

  nsRefPtr<Accessible> accessible;
#ifdef MOZ_XUL
  // XUL controls
  if (role.EqualsLiteral("xul:alert")) {
    accessible = new XULAlertAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:button")) {
    accessible = new XULButtonAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:checkbox")) {
    accessible = new XULCheckboxAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:colorpicker")) {
    accessible = new XULColorPickerAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:colorpickertile")) {
    accessible = new XULColorPickerTileAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:combobox")) {
    accessible = new XULComboboxAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:tabpanels")) {
      accessible = new XULTabpanelsAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:dropmarker")) {
      accessible = new XULDropmarkerAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:groupbox")) {
      accessible = new XULGroupboxAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:image")) {
    if (aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::onclick)) {
      accessible = new XULToolbarButtonAccessible(aContent, aDoc);

    } else {
      // Don't include nameless images in accessible tree.
      if (!aContent->HasAttr(kNameSpaceID_None,
                             nsGkAtoms::tooltiptext))
        return nullptr;

      accessible = new ImageAccessibleWrap(aContent, aDoc);
    }

  } else if (role.EqualsLiteral("xul:link")) {
    accessible = new XULLinkAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:listbox")) {
      accessible = new XULListboxAccessibleWrap(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:listcell")) {
    // Only create cells if there's more than one per row.
    nsIContent* listItem = aContent->GetParent();
    if (!listItem)
      return nullptr;

    for (nsIContent* child = listItem->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      if (child->IsXUL(nsGkAtoms::listcell) && child != aContent) {
        accessible = new XULListCellAccessibleWrap(aContent, aDoc);
        break;
      }
    }

  } else if (role.EqualsLiteral("xul:listhead")) {
    accessible = new XULColumAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:listheader")) {
    accessible = new XULColumnItemAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:listitem")) {
    accessible = new XULListitemAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:menubar")) {
    accessible = new XULMenubarAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:menulist")) {
    accessible = new XULComboboxAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:menuitem")) {
    accessible = new XULMenuitemAccessibleWrap(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:menupopup")) {
#ifdef MOZ_ACCESSIBILITY_ATK
    // ATK considers this node to be redundant when within menubars, and it makes menu
    // navigation with assistive technologies more difficult
    // XXX In the future we will should this for consistency across the nsIAccessible
    // implementations on each platform for a consistent scripting environment, but
    // then strip out redundant accessibles in the AccessibleWrap class for each platform.
    nsIContent *parent = aContent->GetParent();
    if (parent && parent->IsXUL() && parent->Tag() == nsGkAtoms::menu)
      return nullptr;
#endif

    accessible = new XULMenupopupAccessible(aContent, aDoc);

  } else if(role.EqualsLiteral("xul:menuseparator")) {
    accessible = new XULMenuSeparatorAccessible(aContent, aDoc);

  } else if(role.EqualsLiteral("xul:pane")) {
    accessible = new EnumRoleAccessible(aContent, aDoc, roles::PANE);

  } else if (role.EqualsLiteral("xul:panel")) {
    if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::noautofocus,
                              nsGkAtoms::_true, eCaseMatters))
      accessible = new XULAlertAccessible(aContent, aDoc);
    else
      accessible = new EnumRoleAccessible(aContent, aDoc, roles::PANE);

  } else if (role.EqualsLiteral("xul:progressmeter")) {
    accessible = new XULProgressMeterAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xulstatusbar")) {
    accessible = new XULStatusBarAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:scale")) {
    accessible = new XULSliderAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:radiobutton")) {
    accessible = new XULRadioButtonAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:radiogroup")) {
    accessible = new XULRadioGroupAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:tab")) {
    accessible = new XULTabAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:tabs")) {
    accessible = new XULTabsAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:text")) {
    accessible = new XULLabelAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:textbox")) {
    accessible = new EnumRoleAccessible(aContent, aDoc, roles::SECTION);

  } else if (role.EqualsLiteral("xul:thumb")) {
    accessible = new XULThumbAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:tree")) {
    accessible = CreateAccessibleForXULTree(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:treecolumns")) {
    accessible = new XULTreeColumAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:treecolumnitem")) {
    accessible = new XULColumnItemAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:toolbar")) {
    accessible = new XULToolbarAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:toolbarseparator")) {
    accessible = new XULToolbarSeparatorAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:tooltip")) {
    accessible = new XULTooltipAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:toolbarbutton")) {
    accessible = new XULToolbarButtonAccessible(aContent, aDoc);

  }
#endif // MOZ_XUL

  return accessible.forget();
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
      nsRefPtr<Accessible> accessible =
        new HTMLTableHeaderCellAccessibleWrap(aContent, document);
      return accessible.forget();
    }

    return nullptr;
  }

  // This method assumes we're in an HTML namespace.
  nsIAtom* tag = aContent->Tag();
  if (tag == nsGkAtoms::figcaption) {
    nsRefPtr<Accessible> accessible =
      new HTMLFigcaptionAccessible(aContent, document);
    return accessible.forget();
  }

  if (tag == nsGkAtoms::figure) {
    nsRefPtr<Accessible> accessible =
      new HTMLFigureAccessible(aContent, document);
    return accessible.forget();
  }

  if (tag == nsGkAtoms::legend) {
    nsRefPtr<Accessible> accessible =
      new HTMLLegendAccessible(aContent, document);
    return accessible.forget();
  }

  if (tag == nsGkAtoms::option) {
    nsRefPtr<Accessible> accessible =
      new HTMLSelectOptionAccessible(aContent, document);
    return accessible.forget();
  }

  if (tag == nsGkAtoms::optgroup) {
    nsRefPtr<Accessible> accessible =
      new HTMLSelectOptGroupAccessible(aContent, document);
    return accessible.forget();
  }

  if (tag == nsGkAtoms::ul || tag == nsGkAtoms::ol ||
      tag == nsGkAtoms::dl) {
    nsRefPtr<Accessible> accessible =
      new HTMLListAccessible(aContent, document);
    return accessible.forget();
  }

  if (tag == nsGkAtoms::a) {
    // Only some roles truly enjoy life as HTMLLinkAccessibles, for details
    // see closed bug 494807.
    nsRoleMapEntry* roleMapEntry = aria::GetRoleMap(aContent);
    if (roleMapEntry && roleMapEntry->role != roles::NOTHING &&
        roleMapEntry->role != roles::LINK) {
      nsRefPtr<Accessible> accessible =
        new HyperTextAccessibleWrap(aContent, document);
      return accessible.forget();
    }

    nsRefPtr<Accessible> accessible =
      new HTMLLinkAccessible(aContent, document);
    return accessible.forget();
  }

  if (aContext->IsList()) {
    // If list item is a child of accessible list then create an accessible for
    // it unconditionally by tag name. nsBlockFrame creates the list item
    // accessible for other elements styled as list items.
    if (aContext->GetContent() == aContent->GetParent()) {
      if (tag == nsGkAtoms::dt || tag == nsGkAtoms::li) {
        nsRefPtr<Accessible> accessible =
          new HTMLLIAccessible(aContent, document);
        return accessible.forget();
      }

      if (tag == nsGkAtoms::dd) {
        nsRefPtr<Accessible> accessible =
          new HyperTextAccessibleWrap(aContent, document);
        return accessible.forget();
      }
    }

    return nullptr;
  }

  if (tag == nsGkAtoms::abbr ||
      tag == nsGkAtoms::acronym ||
      tag == nsGkAtoms::article ||
      tag == nsGkAtoms::aside ||
      tag == nsGkAtoms::blockquote ||
      tag == nsGkAtoms::form ||
      tag == nsGkAtoms::footer ||
      tag == nsGkAtoms::header ||
      tag == nsGkAtoms::h1 ||
      tag == nsGkAtoms::h2 ||
      tag == nsGkAtoms::h3 ||
      tag == nsGkAtoms::h4 ||
      tag == nsGkAtoms::h5 ||
      tag == nsGkAtoms::h6 ||
      tag == nsGkAtoms::nav ||
      tag == nsGkAtoms::q ||
      tag == nsGkAtoms::section) {
    nsRefPtr<Accessible> accessible =
      new HyperTextAccessibleWrap(aContent, document);
    return accessible.forget();
  }

  if (tag == nsGkAtoms::label) {
    nsRefPtr<Accessible> accessible =
      new HTMLLabelAccessible(aContent, document);
    return accessible.forget();
  }

  if (tag == nsGkAtoms::output) {
    nsRefPtr<Accessible> accessible =
      new HTMLOutputAccessible(aContent, document);
    return accessible.forget();
  }

  if (tag == nsGkAtoms::progress) {
    nsRefPtr<Accessible> accessible =
      new HTMLProgressMeterAccessible(aContent, document);
    return accessible.forget();
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
    case eHTMLRangeType:
      newAcc = new HTMLRangeAccessible(aContent, document);
      break;
    case eHTMLSpinnerType:
      newAcc = new HTMLSpinnerAccessible(aContent, document);
      break;
    case eHTMLTableType:
      newAcc = new HTMLTableAccessibleWrap(aContent, document);
      break;
    case eHTMLTableCellType:
      // Accessible HTML table cell should be a child of accessible HTML table
      // or its row (CSS HTML tables are polite to the used markup at
      // certain degree).
      // Otherwise create a generic text accessible to avoid text jamming
      // when reading by AT.
      if (aContext->IsHTMLTableRow() || aContext->IsHTMLTable())
        newAcc = new HTMLTableCellAccessibleWrap(aContent, document);
      else
        newAcc = new HyperTextAccessibleWrap(aContent, document);
      break;

    case eHTMLTableRowType: {
      // Accessible HTML table row may be a child of tbody/tfoot/thead of
      // accessible HTML table or a direct child of accessible of HTML table.
      Accessible* table = aContext->IsTable() ? aContext : nullptr;
      if (!table && aContext->Parent() && aContext->Parent()->IsTable())
        table = aContext->Parent();

      if (table) {
        nsIContent* parentContent = aContent->GetParent();
        nsIFrame* parentFrame = parentContent->GetPrimaryFrame();
        if (parentFrame->GetType() != nsGkAtoms::tableOuterFrame) {
          parentContent = parentContent->GetParent();
          parentFrame = parentContent->GetPrimaryFrame();
        }

        if (parentFrame->GetType() == nsGkAtoms::tableOuterFrame &&
            table->GetContent() == parentContent) {
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
    default:
      MOZ_ASSERT(false);
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
  nsIContent* child = nsTreeUtils::GetDescendantChild(aContent,
                                                      nsGkAtoms::treechildren);
  if (!child)
    return nullptr;

  nsTreeBodyFrame* treeFrame = do_QueryFrame(child->GetPrimaryFrame());
  if (!treeFrame)
    return nullptr;

  nsRefPtr<nsTreeColumns> treeCols = treeFrame->Columns();
  int32_t count = 0;
  treeCols->GetCount(&count);

  // Outline of list accessible.
  if (count == 1) {
    nsRefPtr<Accessible> accessible =
      new XULTreeAccessible(aContent, aDoc, treeFrame);
    return accessible.forget();
  }

  // Table or tree table accessible.
  nsRefPtr<Accessible> accessible =
    new XULTreeGridAccessibleWrap(aContent, aDoc, treeFrame);
  return accessible.forget();
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

SelectionManager*
SelectionMgr()
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
