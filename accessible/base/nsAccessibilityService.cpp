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
#include "nsArrayUtils.h"
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
#include "TreeWalker.h"
#include "xpcAccessibleApplication.h"
#include "xpcAccessibleDocument.h"

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
#include "nsPluginFrame.h"
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
// Accessible constructors

static Accessible*
New_HTMLLink(nsIContent* aContent, Accessible* aContext)
{
  // Only some roles truly enjoy life as HTMLLinkAccessibles, for details
  // see closed bug 494807.
  const nsRoleMapEntry* roleMapEntry = aria::GetRoleMap(aContent->AsElement());
  if (roleMapEntry && roleMapEntry->role != roles::NOTHING &&
      roleMapEntry->role != roles::LINK) {
    return new HyperTextAccessibleWrap(aContent, aContext->Document());
  }

  return new HTMLLinkAccessible(aContent, aContext->Document());
}

static Accessible* New_HyperText(nsIContent* aContent, Accessible* aContext)
  { return new HyperTextAccessibleWrap(aContent, aContext->Document()); }

static Accessible* New_HTMLFigcaption(nsIContent* aContent, Accessible* aContext)
  { return new HTMLFigcaptionAccessible(aContent, aContext->Document()); }

static Accessible* New_HTMLFigure(nsIContent* aContent, Accessible* aContext)
  { return new HTMLFigureAccessible(aContent, aContext->Document()); }

static Accessible* New_HTMLLegend(nsIContent* aContent, Accessible* aContext)
  { return new HTMLLegendAccessible(aContent, aContext->Document()); }

static Accessible* New_HTMLOption(nsIContent* aContent, Accessible* aContext)
  { return new HTMLSelectOptionAccessible(aContent, aContext->Document()); }

static Accessible* New_HTMLOptgroup(nsIContent* aContent, Accessible* aContext)
  { return new HTMLSelectOptGroupAccessible(aContent, aContext->Document()); }

static Accessible* New_HTMLList(nsIContent* aContent, Accessible* aContext)
  { return new HTMLListAccessible(aContent, aContext->Document()); }

static Accessible*
New_HTMLListitem(nsIContent* aContent, Accessible* aContext)
{
  // If list item is a child of accessible list then create an accessible for
  // it unconditionally by tag name. nsBlockFrame creates the list item
  // accessible for other elements styled as list items.
  if (aContext->IsList() && aContext->GetContent() == aContent->GetParent())
    return new HTMLLIAccessible(aContent, aContext->Document());

  return nullptr;
}

static Accessible*
New_HTMLDefinition(nsIContent* aContent, Accessible* aContext)
{
  if (aContext->IsList())
    return new HyperTextAccessibleWrap(aContent, aContext->Document());
  return nullptr;
}

static Accessible* New_HTMLLabel(nsIContent* aContent, Accessible* aContext)
  { return new HTMLLabelAccessible(aContent, aContext->Document()); }

static Accessible* New_HTMLOutput(nsIContent* aContent, Accessible* aContext)
  { return new HTMLOutputAccessible(aContent, aContext->Document()); }

static Accessible* New_HTMLProgress(nsIContent* aContent, Accessible* aContext)
  { return new HTMLProgressMeterAccessible(aContent, aContext->Document()); }

static Accessible* New_HTMLSummary(nsIContent* aContent, Accessible* aContext)
  { return new HTMLSummaryAccessible(aContent, aContext->Document()); }

static Accessible*
New_HTMLTableAccessible(nsIContent* aContent, Accessible* aContext)
  { return new HTMLTableAccessible(aContent, aContext->Document()); }

static Accessible*
New_HTMLTableRowAccessible(nsIContent* aContent, Accessible* aContext)
  { return new HTMLTableRowAccessible(aContent, aContext->Document()); }

static Accessible*
New_HTMLTableCellAccessible(nsIContent* aContent, Accessible* aContext)
  { return new HTMLTableCellAccessible(aContent, aContext->Document()); }

static Accessible*
New_HTMLTableHeaderCell(nsIContent* aContent, Accessible* aContext)
{
  if (aContext->IsTableRow() && aContext->GetContent() == aContent->GetParent())
    return new HTMLTableHeaderCellAccessibleWrap(aContent, aContext->Document());
  return nullptr;
}

static Accessible*
New_HTMLTableHeaderCellIfScope(nsIContent* aContent, Accessible* aContext)
{
  if (aContext->IsTableRow() && aContext->GetContent() == aContent->GetParent() &&
      aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::scope))
    return new HTMLTableHeaderCellAccessibleWrap(aContent, aContext->Document());
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Markup maps array.

#define Attr(name, value) \
  { &nsGkAtoms::name, &nsGkAtoms::value }

#define AttrFromDOM(name, DOMAttrName) \
  { &nsGkAtoms::name, nullptr, &nsGkAtoms::DOMAttrName }

#define AttrFromDOMIf(name, DOMAttrName, DOMAttrValue) \
  { &nsGkAtoms::name, nullptr,  &nsGkAtoms::DOMAttrName, &nsGkAtoms::DOMAttrValue }

#define MARKUPMAP(atom, new_func, r, ... ) \
  { &nsGkAtoms::atom, new_func, static_cast<a11y::role>(r), { __VA_ARGS__ } },

static const MarkupMapInfo sMarkupMapList[] = {
  #include "MarkupMap.h"
};

#undef Attr
#undef AttrFromDOM
#undef AttrFromDOMIf
#undef MARKUPMAP

////////////////////////////////////////////////////////////////////////////////
// nsAccessibilityService
////////////////////////////////////////////////////////////////////////////////

nsAccessibilityService *nsAccessibilityService::gAccessibilityService = nullptr;
ApplicationAccessible* nsAccessibilityService::gApplicationAccessible = nullptr;
xpcAccessibleApplication* nsAccessibilityService::gXPCApplicationAccessible = nullptr;
bool nsAccessibilityService::gIsShutdown = true;

nsAccessibilityService::nsAccessibilityService() :
  DocManager(), FocusManager(), mMarkupMaps(ArrayLength(sMarkupMapList))
{
}

nsAccessibilityService::~nsAccessibilityService()
{
  NS_ASSERTION(gIsShutdown, "Accessibility wasn't shutdown!");
  gAccessibilityService = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// nsIListenerChangeListener

NS_IMETHODIMP
nsAccessibilityService::ListenersChanged(nsIArray* aEventChanges)
{
  uint32_t targetCount;
  nsresult rv = aEventChanges->GetLength(&targetCount);
  NS_ENSURE_SUCCESS(rv, rv);

  for (uint32_t i = 0 ; i < targetCount ; i++) {
    nsCOMPtr<nsIEventListenerChange> change = do_QueryElementAt(aEventChanges, i);

    nsCOMPtr<nsIDOMEventTarget> target;
    change->GetTarget(getter_AddRefs(target));
    nsCOMPtr<nsIContent> node(do_QueryInterface(target));
    if (!node || !node->IsHTMLElement()) {
      continue;
    }
    nsCOMPtr<nsIArray> listenerNames;
    change->GetChangedListenerNames(getter_AddRefs(listenerNames));

    uint32_t changeCount;
    rv = listenerNames->GetLength(&changeCount);
    NS_ENSURE_SUCCESS(rv, rv);

    for (uint32_t i = 0 ; i < changeCount ; i++) {
      nsCOMPtr<nsIAtom> listenerName = do_QueryElementAt(listenerNames, i);

      // We are only interested in event listener changes which may
      // make an element accessible or inaccessible.
      if (listenerName != nsGkAtoms::onclick &&
          listenerName != nsGkAtoms::onmousedown &&
          listenerName != nsGkAtoms::onmouseup) {
        continue;
      }

      nsIDocument* ownerDoc = node->OwnerDoc();
      DocAccessible* document = GetExistingDocAccessible(ownerDoc);

      // Always recreate for onclick changes.
      if (document) {
        if (nsCoreUtils::HasClickListener(node)) {
          if (!document->GetAccessible(node)) {
            document->RecreateAccessible(node);
          }
        } else {
          if (document->GetAccessible(node)) {
            document->RecreateAccessible(node);
          }
        }
        break;
      }
    }
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED(nsAccessibilityService,
                            DocManager,
                            nsIAccessibilityService,
                            nsIAccessibleRetrieval,
                            nsIObserver,
                            nsIListenerChangeListener,
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
  nsIDocument* documentNode = aTargetNode->GetUncomposedDoc();
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

class PluginTimerCallBack final : public nsITimerCallback
{
  ~PluginTimerCallBack() {}

public:
  PluginTimerCallBack(nsIContent* aContent) : mContent(aContent) {}

  NS_DECL_ISUPPORTS

  NS_IMETHODIMP Notify(nsITimer* aTimer) final
  {
    if (!mContent->IsInUncomposedDoc())
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
nsAccessibilityService::CreatePluginAccessible(nsPluginFrame* aFrame,
                                               nsIContent* aContent,
                                               Accessible* aContext)
{
  // nsPluginFrame means a plugin, so we need to use the accessibility support
  // of the plugin.
  if (aFrame->GetRect().IsEmpty())
    return nullptr;

#if defined(XP_WIN) || defined(MOZ_ACCESSIBILITY_ATK)
  RefPtr<nsNPAPIPluginInstance> pluginInstance;
  if (NS_SUCCEEDED(aFrame->GetPluginInstance(getter_AddRefs(pluginInstance))) &&
      pluginInstance) {
#ifdef XP_WIN
    if (!sPendingPlugins->Contains(aContent) &&
        (Preferences::GetBool("accessibility.delay_plugins") ||
         Compatibility::IsJAWS() || Compatibility::IsWE())) {
      nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
      RefPtr<PluginTimerCallBack> cb = new PluginTimerCallBack(aContent);
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

    RefPtr<Accessible> accessible =
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
      RefPtr<AtkSocketAccessible> socketAccessible =
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
  DocAccessible* document = GetDocAccessible(aPresShell);
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eTree)) {
    logging::MsgBegin("TREE", "content inserted; doc: %p", document);
    logging::Node("container", aContainer);
    for (nsIContent* child = aStartChild; child != aEndChild;
         child = child->GetNextSibling()) {
      logging::Node("content", child);
    }
    logging::MsgEnd();
    logging::Stack();
  }
#endif

  if (document) {
    document->ContentInserted(aContainer, aStartChild, aEndChild);
  }
}

void
nsAccessibilityService::ContentRemoved(nsIPresShell* aPresShell,
                                       nsIContent* aChildNode)
{
  DocAccessible* document = GetDocAccessible(aPresShell);
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eTree)) {
    logging::MsgBegin("TREE", "content removed; doc: %p", document);
    logging::Node("container node", aChildNode->GetFlattenedTreeParent());
    logging::Node("content node", aChildNode);
    logging::MsgEnd();
  }
#endif

  if (document) {
    // Flatten hierarchy may be broken at this point so we cannot get a true
    // container by traversing up the DOM tree. Find a parent of first accessible
    // from the subtree of the given DOM node, that'll be a container. If no
    // accessibles in subtree then we don't care about the change.
    Accessible* child = document->GetAccessible(aChildNode);
    if (!child) {
      Accessible* container = document->GetContainerAccessible(aChildNode);
      a11y::TreeWalker walker(container ? container : document, aChildNode,
                              a11y::TreeWalker::eWalkCache);
      child = walker.Next();
    }

    if (child) {
      document->ContentRemoved(child->Parent(), aChildNode);
#ifdef A11Y_LOG
      if (logging::IsEnabled(logging::eTree))
        logging::AccessibleNNode("real container", child->Parent());
#endif
    }
  }

#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eTree)) {
    logging::MsgEnd();
    logging::Stack();
  }
#endif
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

  NS_IF_ADDREF(*aAccessibleApplication = XPCApplicationAcc());
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
    NS_IF_ADDREF(*aAccessible = ToXPC(document->GetAccessible(node)));

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
  RefPtr<DOMStringList> stringStates = new DOMStringList();

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

  NS_IF_ADDREF(*aAccessible = ToXPC(accessible));
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::CreateAccessiblePivot(nsIAccessible* aRoot,
                                              nsIAccessiblePivot** aPivot)
{
  NS_ENSURE_ARG_POINTER(aPivot);
  NS_ENSURE_ARG(aRoot);
  *aPivot = nullptr;

  Accessible* accessibleRoot = aRoot->ToInternalAccessible();
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
nsAccessibilityService::CreateAccessible(nsINode* aNode,
                                         Accessible* aContext,
                                         bool* aIsSubtreeHidden)
{
  MOZ_ASSERT(aContext, "No context provided");
  MOZ_ASSERT(aNode, "No node to create an accessible for");
  MOZ_ASSERT(!gIsShutdown, "No creation after shutdown");

  if (aIsSubtreeHidden)
    *aIsSubtreeHidden = false;

  DocAccessible* document = aContext->Document();
  MOZ_ASSERT(!document->GetAccessible(aNode),
             "We already have an accessible for this node.");

  if (aNode->IsNodeOfType(nsINode::eDOCUMENT)) {
    // If it's document node then ask accessible document loader for
    // document accessible, otherwise return null.
    nsCOMPtr<nsIDocument> document(do_QueryInterface(aNode));
    return GetDocAccessible(document);
  }

  // We have a content node.
  if (!aNode->GetComposedDoc()) {
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
  NS_ASSERTION(imageFrame && content->IsHTMLElement(nsGkAtoms::area),
               "Unknown case of not main content for the frame!");
#endif
    return nullptr;
  }

#ifdef DEBUG
  nsImageFrame* imageFrame = do_QueryFrame(frame);
  NS_ASSERTION(!imageFrame || !content->IsHTMLElement(nsGkAtoms::area),
               "Image map manages the area accessible creation!");
#endif

  // Attempt to create an accessible based on what we know.
  RefPtr<Accessible> newAcc;

  // Create accessible for visible text frames.
  if (content->IsNodeOfType(nsINode::eTEXT)) {
    nsIFrame::RenderedText text = frame->GetRenderedText(0,
        UINT32_MAX, nsIFrame::TextOffsetType::OFFSETS_IN_CONTENT_TEXT,
        nsIFrame::TrailingWhitespace::DONT_TRIM_TRAILING_WHITESPACE);
    // Ignore not rendered text nodes and whitespace text nodes between table
    // cells.
    if (text.mString.IsEmpty() ||
        (aContext->IsTableRow() && nsCoreUtils::IsWhitespaceString(text.mString))) {
      if (aIsSubtreeHidden)
        *aIsSubtreeHidden = true;

      return nullptr;
    }

    newAcc = CreateAccessibleByFrameType(frame, content, aContext);
    document->BindToDocument(newAcc, nullptr);
    newAcc->AsTextLeaf()->SetText(text.mString);
    return newAcc;
  }

  if (content->IsHTMLElement(nsGkAtoms::map)) {
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
    document->BindToDocument(newAcc, aria::GetRoleMap(content->AsElement()));
    return newAcc;
  }

  const nsRoleMapEntry* roleMapEntry = aria::GetRoleMap(content->AsElement());

  // If the element is focusable or global ARIA attribute is applied to it or
  // it is referenced by ARIA relationship then treat role="presentation" on
  // the element as the role is not there.
  if (roleMapEntry &&
      (roleMapEntry->Is(nsGkAtoms::presentation) ||
       roleMapEntry->Is(nsGkAtoms::none))) {
    if (!MustBeAccessible(content, document))
      return nullptr;

    roleMapEntry = nullptr;
  }

  if (!newAcc && content->IsHTMLElement()) {  // HTML accessibles
    bool isARIATablePart = roleMapEntry &&
      (roleMapEntry->accTypes & (eTableCell | eTableRow | eTable));

    if (!isARIATablePart ||
        frame->AccessibleType() == eHTMLTableCellType ||
        frame->AccessibleType() == eHTMLTableRowType ||
        frame->AccessibleType() == eHTMLTableType) {
      // Prefer to use markup to decide if and what kind of accessible to create,
      const MarkupMapInfo* markupMap =
        mMarkupMaps.Get(content->NodeInfo()->NameAtom());
      if (markupMap && markupMap->new_func)
        newAcc = markupMap->new_func(content, aContext);

      if (!newAcc) // try by frame accessible type.
        newAcc = CreateAccessibleByFrameType(frame, content, aContext);
    }

    // In case of ARIA grid or table use table-specific classes if it's not
    // native table based.
    if (isARIATablePart && (!newAcc || newAcc->IsGenericHyperText())) {
      if ((roleMapEntry->accTypes & eTableCell)) {
        if (aContext->IsTableRow())
          newAcc = new ARIAGridCellAccessibleWrap(content, document);

      } else if (roleMapEntry->IsOfType(eTableRow)) {
        if (aContext->IsTable())
          newAcc = new ARIARowAccessible(content, document);

      } else if (roleMapEntry->IsOfType(eTable)) {
        newAcc = new ARIAGridAccessibleWrap(content, document);
      }
    }

    // If table has strong ARIA role then all table descendants shouldn't
    // expose their native roles.
    if (!roleMapEntry && newAcc && aContext->HasStrongARIARole()) {
      if (frame->AccessibleType() == eHTMLTableRowType) {
        const nsRoleMapEntry* contextRoleMap = aContext->ARIARoleMap();
        if (!contextRoleMap->IsOfType(eTable))
          roleMapEntry = &aria::gEmptyRoleMap;

      } else if (frame->AccessibleType() == eHTMLTableCellType &&
                 aContext->ARIARoleMap() == &aria::gEmptyRoleMap) {
        roleMapEntry = &aria::gEmptyRoleMap;

      } else if (content->IsAnyOfHTMLElements(nsGkAtoms::dt,
                                              nsGkAtoms::li,
                                              nsGkAtoms::dd) ||
                 frame->AccessibleType() == eHTMLLiType) {
        const nsRoleMapEntry* contextRoleMap = aContext->ARIARoleMap();
        if (!contextRoleMap->IsOfType(eList))
          roleMapEntry = &aria::gEmptyRoleMap;
      }
    }
  }

  // Accessible XBL types and deck stuff are used in XUL only currently.
  if (!newAcc && content->IsXULElement()) {
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
    if (content->IsSVGElement()) {
      nsSVGPathGeometryFrame* pathGeometryFrame = do_QueryFrame(frame);
      if (pathGeometryFrame) {
        // A graphic elements: rect, circle, ellipse, line, path, polygon,
        // polyline and image. A 'use' and 'text' graphic elements require
        // special support.
        newAcc = new EnumRoleAccessible<roles::GRAPHIC>(content, document);
      } else if (content->IsSVGElement(nsGkAtoms::svg)) {
        newAcc = new EnumRoleAccessible<roles::DIAGRAM>(content, document);
      }

    } else if (content->IsMathMLElement()) {
      const MarkupMapInfo* markupMap =
        mMarkupMaps.Get(content->NodeInfo()->NameAtom());
      if (markupMap && markupMap->new_func)
        newAcc = markupMap->new_func(content, aContext);

      // Fall back to text when encountering Content MathML.
      if (!newAcc && !content->IsAnyOfMathMLElements(nsGkAtoms::annotation_,
                                                     nsGkAtoms::annotation_xml_,
                                                     nsGkAtoms::mpadded_,
                                                     nsGkAtoms::mphantom_,
                                                     nsGkAtoms::maligngroup_,
                                                     nsGkAtoms::malignmark_,
                                                     nsGkAtoms::mspace_,
                                                     nsGkAtoms::semantics_)) {
       newAcc = new HyperTextAccessible(content, document);
      }
    }
  }

  // If no accessible, see if we need to create a generic accessible because
  // of some property that makes this object interesting
  // We don't do this for <body>, <html>, <window>, <dialog> etc. which
  // correspond to the doc accessible and will be created in any case
  if (!newAcc && !content->IsHTMLElement(nsGkAtoms::body) &&
      content->GetParent() &&
      (roleMapEntry || MustBeAccessible(content, document) ||
       (content->IsHTMLElement() &&
        nsCoreUtils::HasClickListener(content)))) {
    // This content is focusable or has an interesting dynamic content accessibility property.
    // If it's interesting we need it in the accessibility hierarchy so that events or
    // other accessibles can point to it, or so that it can hold a state, etc.
    if (content->IsHTMLElement()) {
      // Interesting HTML container which may have selectable text and/or embedded objects
      newAcc = new HyperTextAccessibleWrap(content, document);
    } else {  // XUL, SVG, MathML etc.
      // Interesting generic non-HTML container
      newAcc = new AccessibleWrap(content, document);
    }
  }

  if (newAcc) {
    document->BindToDocument(newAcc, roleMapEntry);
  }
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

  // Subscribe to EventListenerService.
  nsCOMPtr<nsIEventListenerService> eventListenerService =
    do_GetService("@mozilla.org/eventlistenerservice;1");
  if (!eventListenerService)
    return false;

  eventListenerService->AddListenerChangeListener(this);

  for (uint32_t i = 0; i < ArrayLength(sMarkupMapList); i++)
    mMarkupMaps.Put(*sMarkupMapList[i].tag, &sMarkupMapList[i]);

#ifdef A11Y_LOG
  logging::CheckEnv();
#endif

  gAccessibilityService = this;

  if (XRE_IsParentProcess())
    gApplicationAccessible = new ApplicationAccessibleWrap();
  else
    gApplicationAccessible = new ApplicationAccessible();

  NS_ADDREF(gApplicationAccessible); // will release in Shutdown()
  gApplicationAccessible->Init();

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
  if (XRE_IsParentProcess())
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

  if (XRE_IsParentProcess())
    PlatformShutdown();

  gApplicationAccessible->Shutdown();
  NS_RELEASE(gApplicationAccessible);
  gApplicationAccessible = nullptr;

  NS_IF_RELEASE(gXPCApplicationAccessible);
  gXPCApplicationAccessible = nullptr;
}

already_AddRefed<Accessible>
nsAccessibilityService::CreateAccessibleByType(nsIContent* aContent,
                                               DocAccessible* aDoc)
{
  nsAutoString role;
  nsCoreUtils::XBLBindingRole(aContent, role);
  if (role.IsEmpty() || role.EqualsLiteral("none"))
    return nullptr;

  if (role.EqualsLiteral("outerdoc")) {
    RefPtr<Accessible> accessible = new OuterDocAccessible(aContent, aDoc);
    return accessible.forget();
  }

  RefPtr<Accessible> accessible;
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
      if (child->IsXULElement(nsGkAtoms::listcell) && child != aContent) {
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
    if (parent && parent->IsXULElement(nsGkAtoms::menu))
      return nullptr;
#endif

    accessible = new XULMenupopupAccessible(aContent, aDoc);

  } else if(role.EqualsLiteral("xul:menuseparator")) {
    accessible = new XULMenuSeparatorAccessible(aContent, aDoc);

  } else if(role.EqualsLiteral("xul:pane")) {
    accessible = new EnumRoleAccessible<roles::PANE>(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:panel")) {
    if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::noautofocus,
                              nsGkAtoms::_true, eCaseMatters))
      accessible = new XULAlertAccessible(aContent, aDoc);
    else
      accessible = new EnumRoleAccessible<roles::PANE>(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:progressmeter")) {
    accessible = new XULProgressMeterAccessible(aContent, aDoc);

  } else if (role.EqualsLiteral("xul:statusbar")) {
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
    accessible = new EnumRoleAccessible<roles::SECTION>(aContent, aDoc);

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
nsAccessibilityService::CreateAccessibleByFrameType(nsIFrame* aFrame,
                                                    nsIContent* aContent,
                                                    Accessible* aContext)
{
  DocAccessible* document = aContext->Document();

  RefPtr<Accessible> newAcc;
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
      } else {
        // Otherwise create a generic text accessible to avoid text jamming.
        newAcc = new HyperTextAccessibleWrap(aContent, document);
      }
      break;
    case eHTMLSelectListType:
      newAcc = new HTMLSelectListAccessible(aContent, document);
      break;
    case eHTMLMediaType:
      newAcc = new EnumRoleAccessible<roles::GROUPING>(aContent, document);
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
      if (aContent->IsHTMLElement(nsGkAtoms::table))
        newAcc = new HTMLTableAccessibleWrap(aContent, document);
      else
        newAcc = new HyperTextAccessibleWrap(aContent, document);
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
      if (!aContent->IsAnyOfHTMLElements(nsGkAtoms::dt, nsGkAtoms::dd))
        newAcc = new HyperTextAccessibleWrap(aContent, document);
      break;

    case eImageType:
      newAcc = new ImageAccessibleWrap(aContent, document);
      break;
    case eOuterDocType:
      newAcc = new OuterDocAccessible(aContent, document);
      break;
    case ePluginType: {
      nsPluginFrame* pluginFrame = do_QueryFrame(aFrame);
      newAcc = CreatePluginAccessible(pluginFrame, aContent, aContext);
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

void
nsAccessibilityService::MarkupAttributes(const nsIContent* aContent,
                                         nsIPersistentProperties* aAttributes) const
{
  const mozilla::a11y::MarkupMapInfo* markupMap =
    mMarkupMaps.Get(aContent->NodeInfo()->NameAtom());
  if (!markupMap)
    return;

  for (uint32_t i = 0; i < ArrayLength(markupMap->attrs); i++) {
    const MarkupAttrInfo* info = markupMap->attrs + i;
    if (!info->name)
      break;

    if (info->DOMAttrName) {
      if (info->DOMAttrValue) {
        if (aContent->AttrValueIs(kNameSpaceID_None, *info->DOMAttrName,
                                  *info->DOMAttrValue, eCaseMatters)) {
          nsAccUtils::SetAccAttr(aAttributes, *info->name, *info->DOMAttrValue);
        }
        continue;
      }

      nsAutoString value;
      aContent->GetAttr(kNameSpaceID_None, *info->DOMAttrName, value);
      if (!value.IsEmpty())
        nsAccUtils::SetAccAttr(aAttributes, *info->name, value);

      continue;
    }

    nsAccUtils::SetAccAttr(aAttributes, *info->name, *info->value);
  }
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

bool
nsAccessibilityService::HasAccessible(nsIDOMNode* aDOMNode)
{
  nsCOMPtr<nsINode> node(do_QueryInterface(aDOMNode));
  if (!node)
    return false;

  DocAccessible* document = GetDocAccessible(node->OwnerDoc());
  if (!document)
    return false;

  return document->HasAccessible(node);
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

  RefPtr<nsAccessibilityService> service = new nsAccessibilityService();
  NS_ENSURE_TRUE(service, NS_ERROR_OUT_OF_MEMORY);

  if (!service->Init()) {
    service->Shutdown();
    return NS_ERROR_FAILURE;
  }

  statistics::A11yInitialized();

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

  RefPtr<nsTreeColumns> treeCols = treeFrame->Columns();
  int32_t count = 0;
  treeCols->GetCount(&count);

  // Outline of list accessible.
  if (count == 1) {
    RefPtr<Accessible> accessible =
      new XULTreeAccessible(aContent, aDoc, treeFrame);
    return accessible.forget();
  }

  // Table or tree table accessible.
  RefPtr<Accessible> accessible =
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

xpcAccessibleApplication*
XPCApplicationAcc()
{
  if (!nsAccessibilityService::gXPCApplicationAccessible &&
      nsAccessibilityService::gApplicationAccessible) {
    nsAccessibilityService::gXPCApplicationAccessible =
      new xpcAccessibleApplication(nsAccessibilityService::gApplicationAccessible);
    NS_ADDREF(nsAccessibilityService::gXPCApplicationAccessible);
  }

  return nsAccessibilityService::gXPCApplicationAccessible;
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
