/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAccessibilityService.h"

// NOTE: alphabetically ordered
#include "AccAttributes.h"
#include "ApplicationAccessibleWrap.h"
#include "ARIAGridAccessibleWrap.h"
#include "ARIAMap.h"
#include "DocAccessible-inl.h"
#include "DocAccessibleChild.h"
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
#include "StyleInfo.h"
#include "nsAccUtils.h"
#include "nsArrayUtils.h"
#include "nsAttrName.h"
#include "nsDOMTokenList.h"
#include "nsCRT.h"
#include "nsEventShell.h"
#include "nsIFrameInlines.h"
#include "nsServiceManagerUtils.h"
#include "nsTextFormatter.h"
#include "OuterDocAccessible.h"
#include "Role.h"
#ifdef MOZ_ACCESSIBILITY_ATK
#  include "RootAccessibleWrap.h"
#endif
#include "States.h"
#include "Statistics.h"
#include "TextLeafAccessibleWrap.h"
#include "TreeWalker.h"
#include "xpcAccessibleApplication.h"

#ifdef MOZ_ACCESSIBILITY_ATK
#  include "AtkSocketAccessible.h"
#endif

#ifdef XP_WIN
#  include "mozilla/a11y/Compatibility.h"
#  include "mozilla/dom/ContentChild.h"
#  include "mozilla/StaticPtr.h"
#endif

#ifdef A11Y_LOG
#  include "Logging.h"
#endif

#include "nsExceptionHandler.h"
#include "nsImageFrame.h"
#include "nsINamed.h"
#include "nsIObserverService.h"
#include "nsMenuPopupFrame.h"
#include "nsLayoutUtils.h"
#include "nsTreeBodyFrame.h"
#include "nsTreeColumns.h"
#include "nsTreeUtils.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/HTMLTableElement.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "mozilla/SVGGeometryFrame.h"
#include "nsDeckFrame.h"

#include "XULAlertAccessible.h"
#include "XULComboboxAccessible.h"
#include "XULElementAccessibles.h"
#include "XULFormControlAccessible.h"
#include "XULListboxAccessibleWrap.h"
#include "XULMenuAccessibleWrap.h"
#include "XULTabAccessible.h"
#include "XULTreeGridAccessibleWrap.h"

using namespace mozilla;
using namespace mozilla::a11y;
using namespace mozilla::dom;

/**
 * Accessibility service force enable/disable preference.
 * Supported values:
 *   Accessibility is force enabled (accessibility should always be enabled): -1
 *   Accessibility is enabled (will be started upon a request, default value): 0
 *   Accessibility is force disabled (never enable accessibility):             1
 */
#define PREF_ACCESSIBILITY_FORCE_DISABLED "accessibility.force_disabled"

////////////////////////////////////////////////////////////////////////////////
// Statics
////////////////////////////////////////////////////////////////////////////////

/**
 * Return true if the element must be accessible.
 */
static bool MustBeAccessible(nsIContent* aContent, DocAccessible* aDocument) {
  if (aContent->GetPrimaryFrame()->IsFocusable()) return true;

  if (aContent->IsElement()) {
    uint32_t attrCount = aContent->AsElement()->GetAttrCount();
    for (uint32_t attrIdx = 0; attrIdx < attrCount; attrIdx++) {
      const nsAttrName* attr = aContent->AsElement()->GetAttrNameAt(attrIdx);
      if (attr->NamespaceEquals(kNameSpaceID_None)) {
        nsAtom* attrAtom = attr->Atom();
        if (attrAtom == nsGkAtoms::title && aContent->IsHTMLElement()) {
          // If the author provided a title on an element that would not
          // be accessible normally, assume an intent and make it accessible.
          return true;
        }

        nsDependentAtomString attrStr(attrAtom);
        if (!StringBeginsWith(attrStr, u"aria-"_ns)) continue;  // not ARIA

        // A global state or a property and in case of token defined.
        uint8_t attrFlags = aria::AttrCharacteristicsFor(attrAtom);
        if ((attrFlags & ATTR_GLOBAL) &&
            (!(attrFlags & ATTR_VALTOKEN) ||
             nsAccUtils::HasDefinedARIAToken(aContent, attrAtom))) {
          return true;
        }
      }
    }

    // If the given ID is referred by relation attribute then create an
    // accessible for it.
    nsAutoString id;
    if (nsCoreUtils::GetID(aContent, id) && !id.IsEmpty()) {
      return aDocument->IsDependentID(aContent->AsElement(), id);
    }
  }

  return false;
}

/**
 * Return true if the SVG element should be accessible
 */
static bool MustSVGElementBeAccessible(nsIContent* aContent) {
  // https://w3c.github.io/svg-aam/#include_elements
  for (nsIContent* childElm = aContent->GetFirstChild(); childElm;
       childElm = childElm->GetNextSibling()) {
    if (childElm->IsAnyOfSVGElements(nsGkAtoms::title, nsGkAtoms::desc)) {
      return true;
    }
  }
  return false;
}

/**
 * Used by XULMap.h to map both menupopup and popup elements
 */
LocalAccessible* CreateMenupopupAccessible(Element* aElement,
                                           LocalAccessible* aContext) {
#ifdef MOZ_ACCESSIBILITY_ATK
  // ATK considers this node to be redundant when within menubars, and it makes
  // menu navigation with assistive technologies more difficult
  // XXX In the future we will should this for consistency across the
  // nsIAccessible implementations on each platform for a consistent scripting
  // environment, but then strip out redundant accessibles in the AccessibleWrap
  // class for each platform.
  nsIContent* parent = aElement->GetParent();
  if (parent && parent->IsXULElement(nsGkAtoms::menu)) return nullptr;
#endif

  return new XULMenupopupAccessible(aElement, aContext->Document());
}

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible constructors

static LocalAccessible* New_HyperText(Element* aElement,
                                      LocalAccessible* aContext) {
  return new HyperTextAccessibleWrap(aElement, aContext->Document());
}

template <typename AccClass>
static LocalAccessible* New_HTMLDtOrDd(Element* aElement,
                                       LocalAccessible* aContext) {
  nsIContent* parent = aContext->GetContent();
  if (parent->IsHTMLElement(nsGkAtoms::div)) {
    // It is conforming in HTML to use a div to group dt/dd elements.
    parent = parent->GetParent();
  }

  if (parent && parent->IsHTMLElement(nsGkAtoms::dl)) {
    return new AccClass(aElement, aContext->Document());
  }

  return nullptr;
}

/**
 * Cached value of the PREF_ACCESSIBILITY_FORCE_DISABLED preference.
 */
static int32_t sPlatformDisabledState = 0;

////////////////////////////////////////////////////////////////////////////////
// Markup maps array.

#define Attr(name, value) \
  { nsGkAtoms::name, nsGkAtoms::value }

#define AttrFromDOM(name, DOMAttrName) \
  { nsGkAtoms::name, nullptr, nsGkAtoms::DOMAttrName }

#define AttrFromDOMIf(name, DOMAttrName, DOMAttrValue) \
  { nsGkAtoms::name, nullptr, nsGkAtoms::DOMAttrName, nsGkAtoms::DOMAttrValue }

#define MARKUPMAP(atom, new_func, r, ...) \
  {nsGkAtoms::atom, new_func, static_cast<a11y::role>(r), {__VA_ARGS__}},

static const MarkupMapInfo sHTMLMarkupMapList[] = {
#include "HTMLMarkupMap.h"
};

static const MarkupMapInfo sMathMLMarkupMapList[] = {
#include "MathMLMarkupMap.h"
};

#undef MARKUPMAP

#define XULMAP(atom, ...) {nsGkAtoms::atom, __VA_ARGS__},

#define XULMAP_TYPE(atom, new_type)                                          \
  XULMAP(                                                                    \
      atom,                                                                  \
      [](Element* aElement, LocalAccessible* aContext) -> LocalAccessible* { \
        return new new_type(aElement, aContext->Document());                 \
      })

static const XULMarkupMapInfo sXULMarkupMapList[] = {
#include "XULMap.h"
};

#undef XULMAP_TYPE
#undef XULMAP

#undef Attr
#undef AttrFromDOM
#undef AttrFromDOMIf

////////////////////////////////////////////////////////////////////////////////
// nsAccessibilityService
////////////////////////////////////////////////////////////////////////////////

nsAccessibilityService* nsAccessibilityService::gAccessibilityService = nullptr;
ApplicationAccessible* nsAccessibilityService::gApplicationAccessible = nullptr;
xpcAccessibleApplication* nsAccessibilityService::gXPCApplicationAccessible =
    nullptr;
uint32_t nsAccessibilityService::gConsumers = 0;

nsAccessibilityService::nsAccessibilityService()
    : DocManager(),
      FocusManager(),
      mHTMLMarkupMap(ArrayLength(sHTMLMarkupMapList)),
      mMathMLMarkupMap(ArrayLength(sMathMLMarkupMapList)),
      mXULMarkupMap(ArrayLength(sXULMarkupMapList)) {}

nsAccessibilityService::~nsAccessibilityService() {
  NS_ASSERTION(IsShutdown(), "Accessibility wasn't shutdown!");
  gAccessibilityService = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// nsIListenerChangeListener

NS_IMETHODIMP
nsAccessibilityService::ListenersChanged(nsIArray* aEventChanges) {
  uint32_t targetCount;
  nsresult rv = aEventChanges->GetLength(&targetCount);
  NS_ENSURE_SUCCESS(rv, rv);

  for (uint32_t i = 0; i < targetCount; i++) {
    nsCOMPtr<nsIEventListenerChange> change =
        do_QueryElementAt(aEventChanges, i);

    RefPtr<EventTarget> target;
    change->GetTarget(getter_AddRefs(target));
    nsIContent* content(nsIContent::FromEventTargetOrNull(target));
    if (!content || !content->IsHTMLElement()) {
      continue;
    }

    uint32_t changeCount;
    change->GetCountOfEventListenerChangesAffectingAccessibility(&changeCount);
    NS_ENSURE_SUCCESS(rv, rv);

    if (changeCount) {
      Document* ownerDoc = content->OwnerDoc();
      DocAccessible* document = GetExistingDocAccessible(ownerDoc);

      if (document) {
        LocalAccessible* acc = document->GetAccessible(content);
        if (!acc && (content == document->GetContent() ||
                     content == document->DocumentNode()->GetRootElement())) {
          acc = document;
        }
        if (!acc && nsCoreUtils::HasClickListener(content)) {
          // Create an accessible for a inaccessible element having click event
          // handler.
          document->ContentInserted(content, content->GetNextSibling());
        } else if (acc) {
          if (acc->IsHTMLLink() && !acc->AsHTMLLink()->IsLinked()) {
            // Notify of a LINKED state change if an HTML link gets a click
            // listener but does not have an href attribute.
            RefPtr<AccEvent> linkedChangeEvent =
                new AccStateChangeEvent(acc, states::LINKED);
            document->FireDelayedEvent(linkedChangeEvent);
          }

          // A click listener change might mean losing or gaining an action.
          acc->SendCache(CacheDomain::Actions, CacheUpdateType::Update);
        }
      }
    }
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED(nsAccessibilityService, DocManager, nsIObserver,
                            nsIListenerChangeListener,
                            nsISelectionListener)  // from SelectionManager

////////////////////////////////////////////////////////////////////////////////
// nsIObserver

NS_IMETHODIMP
nsAccessibilityService::Observe(nsISupports* aSubject, const char* aTopic,
                                const char16_t* aData) {
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    Shutdown();
  }

  return NS_OK;
}

void nsAccessibilityService::NotifyOfAnchorJumpTo(nsIContent* aTargetNode) {
  Document* documentNode = aTargetNode->GetUncomposedDoc();
  if (documentNode) {
    DocAccessible* document = GetDocAccessible(documentNode);
    if (document) document->SetAnchorJump(aTargetNode);
  }
}

void nsAccessibilityService::FireAccessibleEvent(uint32_t aEvent,
                                                 LocalAccessible* aTarget) {
  nsEventShell::FireEvent(aEvent, aTarget);
}

void nsAccessibilityService::NotifyOfPossibleBoundsChange(
    mozilla::PresShell* aPresShell, nsIContent* aContent) {
  if (IPCAccessibilityActive() &&
      StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    DocAccessible* document = GetDocAccessible(aPresShell);
    if (document) {
      LocalAccessible* accessible = document->GetAccessible(aContent);
      if (accessible) {
        document->QueueCacheUpdate(accessible, CacheDomain::Bounds);
      }
    }
  }
}

void nsAccessibilityService::NotifyOfComputedStyleChange(
    mozilla::PresShell* aPresShell, nsIContent* aContent) {
  if (!IPCAccessibilityActive() ||
      !StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return;
  }

  DocAccessible* document = GetDocAccessible(aPresShell);
  if (document) {
    LocalAccessible* accessible = document->GetAccessible(aContent);
    if (accessible) {
      accessible->MaybeQueueCacheUpdateForStyleChanges();
    }
  }
}

void nsAccessibilityService::NotifyOfResolutionChange(
    mozilla::PresShell* aPresShell, float aResolution) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    DocAccessible* document = GetDocAccessible(aPresShell);
    if (document && document->IPCDoc()) {
      AutoTArray<mozilla::a11y::CacheData, 1> data;
      RefPtr<AccAttributes> fields = new AccAttributes();
      fields->SetAttribute(nsGkAtoms::resolution, aResolution);
      data.AppendElement(mozilla::a11y::CacheData(0, fields));
      document->IPCDoc()->SendCache(CacheUpdateType::Update, data, true);
    }
  }
}

void nsAccessibilityService::NotifyOfDevPixelRatioChange(
    mozilla::PresShell* aPresShell, int32_t aAppUnitsPerDevPixel) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    DocAccessible* document = GetDocAccessible(aPresShell);
    if (document && document->IPCDoc()) {
      AutoTArray<mozilla::a11y::CacheData, 1> data;
      RefPtr<AccAttributes> fields = new AccAttributes();
      fields->SetAttribute(nsGkAtoms::_moz_device_pixel_ratio,
                           aAppUnitsPerDevPixel);
      data.AppendElement(mozilla::a11y::CacheData(0, fields));
      document->IPCDoc()->SendCache(CacheUpdateType::Update, data, true);
    }
  }
}

LocalAccessible* nsAccessibilityService::GetRootDocumentAccessible(
    PresShell* aPresShell, bool aCanCreate) {
  PresShell* presShell = aPresShell;
  Document* documentNode = aPresShell->GetDocument();
  if (documentNode) {
    nsCOMPtr<nsIDocShellTreeItem> treeItem(documentNode->GetDocShell());
    if (treeItem) {
      nsCOMPtr<nsIDocShellTreeItem> rootTreeItem;
      treeItem->GetInProcessRootTreeItem(getter_AddRefs(rootTreeItem));
      if (treeItem != rootTreeItem) {
        nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(rootTreeItem));
        presShell = docShell->GetPresShell();
      }

      return aCanCreate ? GetDocAccessible(presShell)
                        : presShell->GetDocAccessible();
    }
  }
  return nullptr;
}

void nsAccessibilityService::DeckPanelSwitched(PresShell* aPresShell,
                                               nsIContent* aDeckNode,
                                               nsIFrame* aPrevBoxFrame,
                                               nsIFrame* aCurrentBoxFrame) {
  DocAccessible* document = GetDocAccessible(aPresShell);
  if (!document) {
    return;
  }
  // A deck with a LocalAccessible is a tabpanels element.
  const bool isTabPanels = document->HasAccessible(aDeckNode);
  MOZ_ASSERT(!isTabPanels || aDeckNode->IsXULElement(nsGkAtoms::tabpanels),
             "A deck with a LocalAccessible should be a tabpanels element");

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
    if (isTabPanels) {
      // Tabpanels are accessible even when not selected.
      if (LocalAccessible* acc = document->GetAccessible(panelNode)) {
        RefPtr<AccEvent> event =
            new AccStateChangeEvent(acc, states::OFFSCREEN, true);
        document->FireDelayedEvent(event);
      }
    } else {
      document->ContentRemoved(panelNode);
    }
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
    if (isTabPanels) {
      // Tabpanels are accessible even when not selected, so we don't have to
      // insert a LocalAccessible.
      if (LocalAccessible* acc = document->GetAccessible(panelNode)) {
        RefPtr<AccEvent> event =
            new AccStateChangeEvent(acc, states::OFFSCREEN, false);
        document->FireDelayedEvent(event);
      }
    } else {
      document->ContentInserted(panelNode, panelNode->GetNextSibling());
    }
  }
}

void nsAccessibilityService::ContentRangeInserted(PresShell* aPresShell,
                                                  nsIContent* aStartChild,
                                                  nsIContent* aEndChild) {
  DocAccessible* document = GetDocAccessible(aPresShell);
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eTree)) {
    logging::MsgBegin("TREE", "content inserted; doc: %p", document);
    logging::Node("container", aStartChild->GetParent());
    for (nsIContent* child = aStartChild; child != aEndChild;
         child = child->GetNextSibling()) {
      logging::Node("content", child);
    }
    logging::MsgEnd();
    logging::Stack();
  }
#endif

  if (document) {
    document->ContentInserted(aStartChild, aEndChild);
  }
}

void nsAccessibilityService::ScheduleAccessibilitySubtreeUpdate(
    PresShell* aPresShell, nsIContent* aContent) {
  DocAccessible* document = GetDocAccessible(aPresShell);
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eTree)) {
    logging::MsgBegin("TREE", "schedule update; doc: %p", document);
    logging::Node("content node", aContent);
    logging::MsgEnd();
  }
#endif

  if (document) {
    document->ScheduleTreeUpdate(aContent);
  }
}

void nsAccessibilityService::ContentRemoved(PresShell* aPresShell,
                                            nsIContent* aChildNode) {
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
    document->ContentRemoved(aChildNode);
  }

#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eTree)) {
    logging::MsgEnd();
    logging::Stack();
  }
#endif
}

void nsAccessibilityService::TableLayoutGuessMaybeChanged(
    PresShell* aPresShell, nsIContent* aContent) {
  if (DocAccessible* document = GetDocAccessible(aPresShell)) {
    if (LocalAccessible* accessible = document->GetAccessible(aContent)) {
      document->FireDelayedEvent(
          nsIAccessibleEvent::EVENT_TABLE_STYLING_CHANGED, accessible);
    }
  }
}

void nsAccessibilityService::ComboboxOptionMaybeChanged(
    PresShell* aPresShell, nsIContent* aMutatingNode) {
  DocAccessible* document = GetDocAccessible(aPresShell);
  if (!document) {
    return;
  }

  for (nsIContent* cur = aMutatingNode; cur; cur = cur->GetParent()) {
    if (cur->IsHTMLElement(nsGkAtoms::option)) {
      if (LocalAccessible* accessible = document->GetAccessible(cur)) {
        document->FireDelayedEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE,
                                   accessible);
        break;
      }
      if (cur->IsHTMLElement(nsGkAtoms::select)) {
        break;
      }
    }
  }
}

void nsAccessibilityService::UpdateText(PresShell* aPresShell,
                                        nsIContent* aContent) {
  DocAccessible* document = GetDocAccessible(aPresShell);
  if (document) document->UpdateText(aContent);
}

void nsAccessibilityService::TreeViewChanged(PresShell* aPresShell,
                                             nsIContent* aContent,
                                             nsITreeView* aView) {
  DocAccessible* document = GetDocAccessible(aPresShell);
  if (document) {
    LocalAccessible* accessible = document->GetAccessible(aContent);
    if (accessible) {
      XULTreeAccessible* treeAcc = accessible->AsXULTree();
      if (treeAcc) treeAcc->TreeViewChanged(aView);
    }
  }
}

void nsAccessibilityService::RangeValueChanged(PresShell* aPresShell,
                                               nsIContent* aContent) {
  DocAccessible* document = GetDocAccessible(aPresShell);
  if (document) {
    LocalAccessible* accessible = document->GetAccessible(aContent);
    if (accessible) {
      document->FireDelayedEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE,
                                 accessible);
    }
  }
}

void nsAccessibilityService::UpdateImageMap(nsImageFrame* aImageFrame) {
  PresShell* presShell = aImageFrame->PresShell();
  DocAccessible* document = GetDocAccessible(presShell);
  if (document) {
    LocalAccessible* accessible =
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

void nsAccessibilityService::UpdateLabelValue(PresShell* aPresShell,
                                              nsIContent* aLabelElm,
                                              const nsString& aNewValue) {
  DocAccessible* document = GetDocAccessible(aPresShell);
  if (document) {
    LocalAccessible* accessible = document->GetAccessible(aLabelElm);
    if (accessible) {
      XULLabelAccessible* xulLabel = accessible->AsXULLabel();
      NS_ASSERTION(xulLabel,
                   "UpdateLabelValue was called for wrong accessible!");
      if (xulLabel) xulLabel->UpdateLabelValue(aNewValue);
    }
  }
}

void nsAccessibilityService::PresShellActivated(PresShell* aPresShell) {
  DocAccessible* document = aPresShell->GetDocAccessible();
  if (document) {
    RootAccessible* rootDocument = document->RootAccessible();
    NS_ASSERTION(rootDocument, "Entirely broken tree: no root document!");
    if (rootDocument) rootDocument->DocumentActivated(document);
  }
}

void nsAccessibilityService::RecreateAccessible(PresShell* aPresShell,
                                                nsIContent* aContent) {
  DocAccessible* document = GetDocAccessible(aPresShell);
  if (document) document->RecreateAccessible(aContent);
}

void nsAccessibilityService::GetStringRole(uint32_t aRole, nsAString& aString) {
#define ROLE(geckoRole, stringRole, atkRole, macRole, macSubrole, msaaRole, \
             ia2Role, androidClass, nameRule)                               \
  case roles::geckoRole:                                                    \
    aString.AssignLiteral(stringRole);                                      \
    return;

  switch (aRole) {
#include "RoleMap.h"
    default:
      aString.AssignLiteral("unknown");
      return;
  }

#undef ROLE
}

void nsAccessibilityService::GetStringStates(uint32_t aState,
                                             uint32_t aExtraState,
                                             nsISupports** aStringStates) {
  RefPtr<DOMStringList> stringStates =
      GetStringStates(nsAccUtils::To64State(aState, aExtraState));

  // unknown state
  if (!stringStates->Length()) {
    stringStates->Add(u"unknown"_ns);
  }

  stringStates.forget(aStringStates);
}

already_AddRefed<DOMStringList> nsAccessibilityService::GetStringStates(
    uint64_t aStates) const {
  RefPtr<DOMStringList> stringStates = new DOMStringList();

  if (aStates & states::UNAVAILABLE) {
    stringStates->Add(u"unavailable"_ns);
  }
  if (aStates & states::SELECTED) {
    stringStates->Add(u"selected"_ns);
  }
  if (aStates & states::FOCUSED) {
    stringStates->Add(u"focused"_ns);
  }
  if (aStates & states::PRESSED) {
    stringStates->Add(u"pressed"_ns);
  }
  if (aStates & states::CHECKED) {
    stringStates->Add(u"checked"_ns);
  }
  if (aStates & states::MIXED) {
    stringStates->Add(u"mixed"_ns);
  }
  if (aStates & states::READONLY) {
    stringStates->Add(u"readonly"_ns);
  }
  if (aStates & states::HOTTRACKED) {
    stringStates->Add(u"hottracked"_ns);
  }
  if (aStates & states::DEFAULT) {
    stringStates->Add(u"default"_ns);
  }
  if (aStates & states::EXPANDED) {
    stringStates->Add(u"expanded"_ns);
  }
  if (aStates & states::COLLAPSED) {
    stringStates->Add(u"collapsed"_ns);
  }
  if (aStates & states::BUSY) {
    stringStates->Add(u"busy"_ns);
  }
  if (aStates & states::FLOATING) {
    stringStates->Add(u"floating"_ns);
  }
  if (aStates & states::ANIMATED) {
    stringStates->Add(u"animated"_ns);
  }
  if (aStates & states::INVISIBLE) {
    stringStates->Add(u"invisible"_ns);
  }
  if (aStates & states::OFFSCREEN) {
    stringStates->Add(u"offscreen"_ns);
  }
  if (aStates & states::SIZEABLE) {
    stringStates->Add(u"sizeable"_ns);
  }
  if (aStates & states::MOVEABLE) {
    stringStates->Add(u"moveable"_ns);
  }
  if (aStates & states::SELFVOICING) {
    stringStates->Add(u"selfvoicing"_ns);
  }
  if (aStates & states::FOCUSABLE) {
    stringStates->Add(u"focusable"_ns);
  }
  if (aStates & states::SELECTABLE) {
    stringStates->Add(u"selectable"_ns);
  }
  if (aStates & states::LINKED) {
    stringStates->Add(u"linked"_ns);
  }
  if (aStates & states::TRAVERSED) {
    stringStates->Add(u"traversed"_ns);
  }
  if (aStates & states::MULTISELECTABLE) {
    stringStates->Add(u"multiselectable"_ns);
  }
  if (aStates & states::EXTSELECTABLE) {
    stringStates->Add(u"extselectable"_ns);
  }
  if (aStates & states::PROTECTED) {
    stringStates->Add(u"protected"_ns);
  }
  if (aStates & states::HASPOPUP) {
    stringStates->Add(u"haspopup"_ns);
  }
  if (aStates & states::REQUIRED) {
    stringStates->Add(u"required"_ns);
  }
  if (aStates & states::ALERT) {
    stringStates->Add(u"alert"_ns);
  }
  if (aStates & states::INVALID) {
    stringStates->Add(u"invalid"_ns);
  }
  if (aStates & states::CHECKABLE) {
    stringStates->Add(u"checkable"_ns);
  }
  if (aStates & states::SUPPORTS_AUTOCOMPLETION) {
    stringStates->Add(u"autocompletion"_ns);
  }
  if (aStates & states::DEFUNCT) {
    stringStates->Add(u"defunct"_ns);
  }
  if (aStates & states::SELECTABLE_TEXT) {
    stringStates->Add(u"selectable text"_ns);
  }
  if (aStates & states::EDITABLE) {
    stringStates->Add(u"editable"_ns);
  }
  if (aStates & states::ACTIVE) {
    stringStates->Add(u"active"_ns);
  }
  if (aStates & states::MODAL) {
    stringStates->Add(u"modal"_ns);
  }
  if (aStates & states::MULTI_LINE) {
    stringStates->Add(u"multi line"_ns);
  }
  if (aStates & states::HORIZONTAL) {
    stringStates->Add(u"horizontal"_ns);
  }
  if (aStates & states::OPAQUE1) {
    stringStates->Add(u"opaque"_ns);
  }
  if (aStates & states::SINGLE_LINE) {
    stringStates->Add(u"single line"_ns);
  }
  if (aStates & states::TRANSIENT) {
    stringStates->Add(u"transient"_ns);
  }
  if (aStates & states::VERTICAL) {
    stringStates->Add(u"vertical"_ns);
  }
  if (aStates & states::STALE) {
    stringStates->Add(u"stale"_ns);
  }
  if (aStates & states::ENABLED) {
    stringStates->Add(u"enabled"_ns);
  }
  if (aStates & states::SENSITIVE) {
    stringStates->Add(u"sensitive"_ns);
  }
  if (aStates & states::EXPANDABLE) {
    stringStates->Add(u"expandable"_ns);
  }
  if (aStates & states::PINNED) {
    stringStates->Add(u"pinned"_ns);
  }
  if (aStates & states::CURRENT) {
    stringStates->Add(u"current"_ns);
  }

  return stringStates.forget();
}

void nsAccessibilityService::GetStringEventType(uint32_t aEventType,
                                                nsAString& aString) {
  NS_ASSERTION(
      nsIAccessibleEvent::EVENT_LAST_ENTRY == ArrayLength(kEventTypeNames),
      "nsIAccessibleEvent constants are out of sync to kEventTypeNames");

  if (aEventType >= ArrayLength(kEventTypeNames)) {
    aString.AssignLiteral("unknown");
    return;
  }

  aString.AssignASCII(kEventTypeNames[aEventType]);
}

void nsAccessibilityService::GetStringEventType(uint32_t aEventType,
                                                nsACString& aString) {
  MOZ_ASSERT(
      nsIAccessibleEvent::EVENT_LAST_ENTRY == ArrayLength(kEventTypeNames),
      "nsIAccessibleEvent constants are out of sync to kEventTypeNames");

  if (aEventType >= ArrayLength(kEventTypeNames)) {
    aString.AssignLiteral("unknown");
    return;
  }

  aString = nsDependentCString(kEventTypeNames[aEventType]);
}

void nsAccessibilityService::GetStringRelationType(uint32_t aRelationType,
                                                   nsAString& aString) {
  NS_ENSURE_TRUE_VOID(aRelationType <=
                      static_cast<uint32_t>(RelationType::LAST));

#define RELATIONTYPE(geckoType, geckoTypeName, atkType, msaaType, ia2Type) \
  case RelationType::geckoType:                                            \
    aString.AssignLiteral(geckoTypeName);                                  \
    return;

  RelationType relationType = static_cast<RelationType>(aRelationType);
  switch (relationType) {
#include "RelationTypeMap.h"
    default:
      aString.AssignLiteral("unknown");
      return;
  }

#undef RELATIONTYPE
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessibilityService public

LocalAccessible* nsAccessibilityService::CreateAccessible(
    nsINode* aNode, LocalAccessible* aContext, bool* aIsSubtreeHidden) {
  MOZ_ASSERT(aContext, "No context provided");
  MOZ_ASSERT(aNode, "No node to create an accessible for");
  MOZ_ASSERT(gConsumers, "No creation after shutdown");

  if (aIsSubtreeHidden) *aIsSubtreeHidden = false;

  DocAccessible* document = aContext->Document();
  MOZ_ASSERT(!document->GetAccessible(aNode),
             "We already have an accessible for this node.");

  if (aNode->IsDocument()) {
    // If it's document node then ask accessible document loader for
    // document accessible, otherwise return null.
    return GetDocAccessible(aNode->AsDocument());
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

  if (!aNode->IsContent()) return nullptr;

  nsIContent* content = aNode->AsContent();
  if (aria::HasDefinedARIAHidden(content)) {
    if (aIsSubtreeHidden) {
      *aIsSubtreeHidden = true;
    }
    return nullptr;
  }

  // Check frame and its visibility. Note, hidden frame allows visible
  // elements in subtree.
  nsIFrame* frame = content->GetPrimaryFrame();
  if (!frame || !frame->StyleVisibility()->IsVisible()) {
    // display:contents element doesn't have a frame, but retains the semantics.
    // All its children are unaffected.
    if (nsCoreUtils::CanCreateAccessibleWithoutFrame(content)) {
      const MarkupMapInfo* markupMap = GetMarkupMapInfoForNode(content);
      if (markupMap && markupMap->new_func) {
        RefPtr<LocalAccessible> newAcc =
            markupMap->new_func(content->AsElement(), aContext);
        if (newAcc) {
          document->BindToDocument(newAcc,
                                   aria::GetRoleMap(content->AsElement()));
        }
        return newAcc;
      }
      return nullptr;
    }

    if (aIsSubtreeHidden && !frame) *aIsSubtreeHidden = true;

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
  RefPtr<LocalAccessible> newAcc;

  // Create accessible for visible text frames.
  if (content->IsText()) {
    nsIFrame::RenderedText text = frame->GetRenderedText(
        0, UINT32_MAX, nsIFrame::TextOffsetType::OffsetsInContentText,
        nsIFrame::TrailingWhitespace::DontTrim);
    // Ignore not rendered text nodes and whitespace text nodes between table
    // cells.
    if (text.mString.IsEmpty() ||
        (aContext->IsTableRow() &&
         nsCoreUtils::IsWhitespaceString(text.mString))) {
      if (aIsSubtreeHidden) *aIsSubtreeHidden = true;

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
    if (nsLayoutUtils::GetAllInFlowRectsUnion(frame, frame->GetParent())
            .IsEmpty()) {
      if (aIsSubtreeHidden) *aIsSubtreeHidden = true;

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
  if (roleMapEntry && (roleMapEntry->Is(nsGkAtoms::presentation) ||
                       roleMapEntry->Is(nsGkAtoms::none))) {
    if (!MustBeAccessible(content, document)) return nullptr;

    roleMapEntry = nullptr;
  }

  if (!newAcc && content->IsHTMLElement()) {  // HTML accessibles
    bool isARIATablePart = roleMapEntry && (roleMapEntry->accTypes &
                                            (eTableCell | eTableRow | eTable));

    if (!isARIATablePart || frame->AccessibleType() == eHTMLTableCellType ||
        frame->AccessibleType() == eHTMLTableRowType ||
        frame->AccessibleType() == eHTMLTableType ||
        // We should always use OuterDocAccessible for OuterDocs, even for
        // ARIA table roles.
        frame->AccessibleType() == eOuterDocType) {
      // Prefer to use markup to decide if and what kind of accessible to
      // create,
      const MarkupMapInfo* markupMap =
          mHTMLMarkupMap.Get(content->NodeInfo()->NameAtom());
      if (markupMap && markupMap->new_func) {
        newAcc = markupMap->new_func(content->AsElement(), aContext);
      }

      if (!newAcc) {  // try by frame accessible type.
        newAcc = CreateAccessibleByFrameType(frame, content, aContext);
      }
    }

    // In case of ARIA grid or table use table-specific classes if it's not
    // native table based.
    if (isARIATablePart && (!newAcc || newAcc->IsGenericHyperText())) {
      if ((roleMapEntry->accTypes & eTableCell)) {
        if (aContext->IsTableRow()) {
          newAcc = new ARIAGridCellAccessibleWrap(content, document);
        }

      } else if (roleMapEntry->IsOfType(eTableRow)) {
        if (aContext->IsTable() ||
            (aContext->LocalParent() && aContext->LocalParent()->IsTable())) {
          newAcc = new ARIARowAccessible(content, document);
        }

      } else if (roleMapEntry->IsOfType(eTable)) {
        newAcc = new ARIAGridAccessibleWrap(content, document);
      }
    }

    // If table has strong ARIA role then all table descendants shouldn't
    // expose their native roles.
    if (!roleMapEntry && newAcc && aContext->HasStrongARIARole()) {
      if (frame->AccessibleType() == eHTMLTableRowType) {
        const nsRoleMapEntry* contextRoleMap = aContext->ARIARoleMap();
        if (!contextRoleMap->IsOfType(eTable)) {
          roleMapEntry = &aria::gEmptyRoleMap;
        }

      } else if (frame->AccessibleType() == eHTMLTableCellType &&
                 aContext->ARIARoleMap() == &aria::gEmptyRoleMap) {
        roleMapEntry = &aria::gEmptyRoleMap;

      } else if (content->IsAnyOfHTMLElements(nsGkAtoms::dt, nsGkAtoms::li,
                                              nsGkAtoms::dd) ||
                 frame->AccessibleType() == eHTMLLiType) {
        const nsRoleMapEntry* contextRoleMap = aContext->ARIARoleMap();
        if (!contextRoleMap->IsOfType(eList)) {
          roleMapEntry = &aria::gEmptyRoleMap;
        }
      }
    }
  }

  // XUL accessibles.
  if (!newAcc && content->IsXULElement()) {
    // No accessible for not selected deck panel and its children.
    if (!aContext->IsXULTabpanels()) {
      nsDeckFrame* deckFrame = do_QueryFrame(frame->GetParent());
      if (deckFrame && deckFrame->GetSelectedBox() != frame) {
        if (aIsSubtreeHidden) *aIsSubtreeHidden = true;

        return nullptr;
      }
    }

    if (content->IsXULElement(nsGkAtoms::panel)) {
      // We filter here instead of in the XUL map because
      // if we filter there and return null, we still end up
      // creating a generic accessible at the end of this function.
      // Doing the filtering here ensures we never create accessibles
      // for panels whose popups aren't visible.
      nsMenuPopupFrame* popupFrame = do_QueryFrame(frame);
      if (!popupFrame) {
        return nullptr;
      }

      nsPopupState popupState = popupFrame->PopupState();
      if (popupState == ePopupHiding || popupState == ePopupInvisible ||
          popupState == ePopupClosed) {
        return nullptr;
      }
    }

    // Prefer to use XUL to decide if and what kind of accessible to create.
    const XULMarkupMapInfo* xulMap =
        mXULMarkupMap.Get(content->NodeInfo()->NameAtom());
    if (xulMap && xulMap->new_func) {
      newAcc = xulMap->new_func(content->AsElement(), aContext);
    }

    // Any XUL box can be used as tabpanel, make sure we create a proper
    // accessible for it.
    if (!newAcc && aContext->IsXULTabpanels() &&
        content->GetParent() == aContext->GetContent()) {
      LayoutFrameType frameType = frame->Type();
      if (frameType == LayoutFrameType::Box ||
          frameType == LayoutFrameType::Scroll) {
        newAcc = new XULTabpanelAccessible(content, document);
      }
    }
  }

  if (!newAcc) {
    if (content->IsSVGElement()) {
      SVGGeometryFrame* geometryFrame = do_QueryFrame(frame);
      if (geometryFrame && MustSVGElementBeAccessible(content)) {
        // A graphic elements: rect, circle, ellipse, line, path, polygon,
        // polyline and image. A 'use' and 'text' graphic elements require
        // special support.
        newAcc = new EnumRoleAccessible<roles::GRAPHIC>(content, document);
      } else if (content->IsSVGElement(nsGkAtoms::text)) {
        newAcc = new HyperTextAccessibleWrap(content->AsElement(), document);
      } else if (content->IsSVGElement(nsGkAtoms::svg)) {
        newAcc = new EnumRoleAccessible<roles::DIAGRAM>(content, document);
      } else if (content->IsSVGElement(nsGkAtoms::g) &&
                 MustSVGElementBeAccessible(content)) {
        newAcc = new EnumRoleAccessible<roles::GROUPING>(content, document);
      }

    } else if (content->IsMathMLElement()) {
      const MarkupMapInfo* markupMap =
          mMathMLMarkupMap.Get(content->NodeInfo()->NameAtom());
      if (markupMap && markupMap->new_func) {
        newAcc = markupMap->new_func(content->AsElement(), aContext);
      }

      // Fall back to text when encountering Content MathML.
      if (!newAcc && !content->IsAnyOfMathMLElements(
                         nsGkAtoms::annotation_, nsGkAtoms::annotation_xml_,
                         nsGkAtoms::mpadded_, nsGkAtoms::mphantom_,
                         nsGkAtoms::maligngroup_, nsGkAtoms::malignmark_,
                         nsGkAtoms::mspace_, nsGkAtoms::semantics_)) {
        newAcc = new HyperTextAccessible(content, document);
      }
    } else if (content->IsGeneratedContentContainerForMarker()) {
      if (aContext->IsHTMLListItem()) {
        newAcc = new HTMLListBulletAccessible(content, document);
      }
      if (aIsSubtreeHidden) {
        *aIsSubtreeHidden = true;
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
       (content->IsHTMLElement() && nsCoreUtils::HasClickListener(content)))) {
    // This content is focusable or has an interesting dynamic content
    // accessibility property. If it's interesting we need it in the
    // accessibility hierarchy so that events or other accessibles can point to
    // it, or so that it can hold a state, etc.
    if (content->IsHTMLElement()) {
      // Interesting HTML container which may have selectable text and/or
      // embedded objects
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

bool nsAccessibilityService::Init() {
  // Initialize accessible document manager.
  if (!DocManager::Init()) return false;

  // Add observers.
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (!observerService) return false;

  observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);

#if defined(XP_WIN)
  // This information needs to be initialized before the observer fires.
  if (XRE_IsParentProcess()) {
    Compatibility::Init();
  }
#endif  // defined(XP_WIN)

  // Subscribe to EventListenerService.
  nsCOMPtr<nsIEventListenerService> eventListenerService =
      do_GetService("@mozilla.org/eventlistenerservice;1");
  if (!eventListenerService) return false;

  eventListenerService->AddListenerChangeListener(this);

  for (uint32_t i = 0; i < ArrayLength(sHTMLMarkupMapList); i++) {
    mHTMLMarkupMap.InsertOrUpdate(sHTMLMarkupMapList[i].tag,
                                  &sHTMLMarkupMapList[i]);
  }
  for (const auto& info : sMathMLMarkupMapList) {
    mMathMLMarkupMap.InsertOrUpdate(info.tag, &info);
  }

  for (uint32_t i = 0; i < ArrayLength(sXULMarkupMapList); i++) {
    mXULMarkupMap.InsertOrUpdate(sXULMarkupMapList[i].tag,
                                 &sXULMarkupMapList[i]);
  }

#ifdef A11Y_LOG
  logging::CheckEnv();
#endif

  gAccessibilityService = this;
  NS_ADDREF(gAccessibilityService);  // will release in Shutdown()

  if (XRE_IsParentProcess()) {
    gApplicationAccessible = new ApplicationAccessibleWrap();
  } else {
#if defined(XP_WIN)
    dom::ContentChild* contentChild = dom::ContentChild::GetSingleton();
    MOZ_ASSERT(contentChild);
    // If we were instantiated by the chrome process, GetMsaaID() will return
    // a non-zero value and we may safely continue with initialization.
    if (!StaticPrefs::accessibility_cache_enabled_AtStartup() &&
        !contentChild->GetMsaaID()) {
      // Since we were not instantiated by chrome, we need to synchronously
      // obtain a MSAA content process id.
      contentChild->SendGetA11yContentId();
    }

    gApplicationAccessible = new ApplicationAccessibleWrap();
#else
    gApplicationAccessible = new ApplicationAccessible();
#endif  // defined(XP_WIN)
  }

  NS_ADDREF(gApplicationAccessible);  // will release in Shutdown()
  gApplicationAccessible->Init();

  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::Accessibility,
                                     "Active"_ns);

  // Now its safe to start platform accessibility.
  if (XRE_IsParentProcess()) PlatformInit();

  statistics::A11yInitialized();

  static const char16_t kInitIndicator[] = {'1', 0};
  observerService->NotifyObservers(nullptr, "a11y-init-or-shutdown",
                                   kInitIndicator);

  return true;
}

void nsAccessibilityService::Shutdown() {
  // Application is going to be closed, shutdown accessibility and mark
  // accessibility service as shutdown to prevent calls of its methods.
  // Don't null accessibility service static member at this point to be safe
  // if someone will try to operate with it.

  MOZ_ASSERT(gConsumers, "Accessibility was shutdown already");
  UnsetConsumers(eXPCOM | eMainProcess | ePlatformAPI);

  // Remove observers.
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }

  // Stop accessible document loader.
  DocManager::Shutdown();

  SelectionManager::Shutdown();

  if (XRE_IsParentProcess()) PlatformShutdown();

  gApplicationAccessible->Shutdown();
  NS_RELEASE(gApplicationAccessible);
  gApplicationAccessible = nullptr;

  NS_IF_RELEASE(gXPCApplicationAccessible);
  gXPCApplicationAccessible = nullptr;

  NS_RELEASE(gAccessibilityService);
  gAccessibilityService = nullptr;

  if (observerService) {
    static const char16_t kShutdownIndicator[] = {'0', 0};
    observerService->NotifyObservers(nullptr, "a11y-init-or-shutdown",
                                     kShutdownIndicator);
  }
}

already_AddRefed<LocalAccessible>
nsAccessibilityService::CreateAccessibleByFrameType(nsIFrame* aFrame,
                                                    nsIContent* aContent,
                                                    LocalAccessible* aContext) {
  DocAccessible* document = aContext->Document();

  RefPtr<LocalAccessible> newAcc;
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
      newAcc = new CheckboxAccessible(aContent, document);
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
      if (aContent->IsHTMLElement(nsGkAtoms::table)) {
        newAcc = new HTMLTableAccessibleWrap(aContent, document);
      } else {
        newAcc = new HyperTextAccessibleWrap(aContent, document);
      }
      break;
    case eHTMLTableCellType:
      // LocalAccessible HTML table cell should be a child of accessible HTML
      // table or its row (CSS HTML tables are polite to the used markup at
      // certain degree).
      // Otherwise create a generic text accessible to avoid text jamming
      // when reading by AT.
      if (aContext->IsHTMLTableRow() || aContext->IsHTMLTable()) {
        newAcc = new HTMLTableCellAccessibleWrap(aContent, document);
      } else {
        newAcc = new HyperTextAccessibleWrap(aContent, document);
      }
      break;

    case eHTMLTableRowType: {
      // LocalAccessible HTML table row may be a child of tbody/tfoot/thead of
      // accessible HTML table or a direct child of accessible of HTML table.
      LocalAccessible* table = aContext->IsTable() ? aContext : nullptr;
      if (!table && aContext->LocalParent() &&
          aContext->LocalParent()->IsTable()) {
        table = aContext->LocalParent();
      }

      if (table) {
        nsIContent* parentContent =
            aContent->GetParentOrShadowHostNode()->AsContent();
        nsIFrame* parentFrame = nullptr;
        if (parentContent) {
          parentFrame = parentContent->GetPrimaryFrame();
          if (!parentFrame || !parentFrame->IsTableWrapperFrame()) {
            parentContent =
                parentContent->GetParentOrShadowHostNode()->AsContent();
            if (parentContent) {
              parentFrame = parentContent->GetPrimaryFrame();
            }
          }
        }

        if (parentFrame && parentFrame->IsTableWrapperFrame() &&
            table->GetContent() == parentContent) {
          newAcc = new HTMLTableRowAccessible(aContent, document);
        }
      }
      break;
    }
    case eHTMLTextFieldType:
      newAcc = new HTMLTextFieldAccessible(aContent, document);
      break;
    case eHyperTextType: {
      if (aContext->IsTable() || aContext->IsTableRow()) {
        // This is some generic hyperText, for example a block frame element
        // inserted between a table and table row. Treat it as presentational.
        return nullptr;
      }

      if (!aContent->IsAnyOfHTMLElements(nsGkAtoms::dt, nsGkAtoms::dd,
                                         nsGkAtoms::div, nsGkAtoms::thead,
                                         nsGkAtoms::tfoot, nsGkAtoms::tbody)) {
        newAcc = new HyperTextAccessibleWrap(aContent, document);
      }
      break;
    }
    case eImageType:
      newAcc = new ImageAccessibleWrap(aContent, document);
      break;
    case eOuterDocType:
      newAcc = new OuterDocAccessible(aContent, document);
      break;
    case eTextLeafType:
      newAcc = new TextLeafAccessibleWrap(aContent, document);
      break;
    default:
      MOZ_ASSERT(false);
      break;
  }

  return newAcc.forget();
}

void nsAccessibilityService::MarkupAttributes(
    const nsIContent* aContent, AccAttributes* aAttributes) const {
  const mozilla::a11y::MarkupMapInfo* markupMap =
      GetMarkupMapInfoForNode(aContent);
  if (!markupMap) return;

  for (uint32_t i = 0; i < ArrayLength(markupMap->attrs); i++) {
    const MarkupAttrInfo* info = markupMap->attrs + i;
    if (!info->name) break;

    if (info->DOMAttrName) {
      if (info->DOMAttrValue) {
        if (aContent->IsElement() && aContent->AsElement()->AttrValueIs(
                                         kNameSpaceID_None, info->DOMAttrName,
                                         info->DOMAttrValue, eCaseMatters)) {
          aAttributes->SetAttribute(info->name, info->DOMAttrValue);
        }
        continue;
      }

      nsString value;
      if (aContent->IsElement()) {
        aContent->AsElement()->GetAttr(kNameSpaceID_None, info->DOMAttrName,
                                       value);
      }

      if (!value.IsEmpty()) {
        aAttributes->SetAttribute(info->name, std::move(value));
      }

      continue;
    }

    aAttributes->SetAttribute(info->name, info->value);
  }
}

#if defined(ANDROID)
#  include "mozilla/Monitor.h"
#  include "mozilla/Maybe.h"

static Maybe<Monitor> sAndroidMonitor;

mozilla::Monitor& nsAccessibilityService::GetAndroidMonitor() {
  if (!sAndroidMonitor.isSome()) {
    sAndroidMonitor.emplace("nsAccessibility::sAndroidMonitor");
  }

  return *sAndroidMonitor;
}
#endif

LocalAccessible* nsAccessibilityService::AddNativeRootAccessible(
    void* aAtkAccessible) {
#ifdef MOZ_ACCESSIBILITY_ATK
  ApplicationAccessible* applicationAcc = ApplicationAcc();
  if (!applicationAcc) return nullptr;

  GtkWindowAccessible* nativeWnd =
      new GtkWindowAccessible(static_cast<AtkObject*>(aAtkAccessible));

  if (applicationAcc->AppendChild(nativeWnd)) return nativeWnd;
#endif

  return nullptr;
}

void nsAccessibilityService::RemoveNativeRootAccessible(
    LocalAccessible* aAccessible) {
#ifdef MOZ_ACCESSIBILITY_ATK
  ApplicationAccessible* applicationAcc = ApplicationAcc();

  if (applicationAcc) applicationAcc->RemoveChild(aAccessible);
#endif
}

bool nsAccessibilityService::HasAccessible(nsINode* aDOMNode) {
  if (!aDOMNode) return false;

  Document* document = aDOMNode->OwnerDoc();
  if (!document) return false;

  DocAccessible* docAcc = GetExistingDocAccessible(aDOMNode->OwnerDoc());
  if (!docAcc) return false;

  return docAcc->HasAccessible(aDOMNode);
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessibilityService private (DON'T put methods here)

void nsAccessibilityService::SetConsumers(uint32_t aConsumers, bool aNotify) {
  if (gConsumers & aConsumers) {
    return;
  }

  gConsumers |= aConsumers;
  if (aNotify) {
    NotifyOfConsumersChange();
  }
}

void nsAccessibilityService::UnsetConsumers(uint32_t aConsumers) {
  if (!(gConsumers & aConsumers)) {
    return;
  }

  gConsumers &= ~aConsumers;
  NotifyOfConsumersChange();
}

void nsAccessibilityService::GetConsumers(nsAString& aString) {
  const char16_t* kJSONFmt =
      u"{ \"XPCOM\": %s, \"MainProcess\": %s, \"PlatformAPI\": %s }";
  nsString json;
  nsTextFormatter::ssprintf(json, kJSONFmt,
                            gConsumers & eXPCOM ? "true" : "false",
                            gConsumers & eMainProcess ? "true" : "false",
                            gConsumers & ePlatformAPI ? "true" : "false");
  aString.Assign(json);
}

void nsAccessibilityService::NotifyOfConsumersChange() {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();

  if (!observerService) {
    return;
  }

  nsAutoString consumers;
  GetConsumers(consumers);
  observerService->NotifyObservers(nullptr, "a11y-consumers-changed",
                                   consumers.get());
}

nsAccessibilityService* GetOrCreateAccService(uint32_t aNewConsumer) {
  // Do not initialize accessibility if it is force disabled.
  if (PlatformDisabledState() == ePlatformIsDisabled) {
    return nullptr;
  }

  if (!nsAccessibilityService::gAccessibilityService) {
    RefPtr<nsAccessibilityService> service = new nsAccessibilityService();
    if (!service->Init()) {
      service->Shutdown();
      return nullptr;
    }
  }

  MOZ_ASSERT(nsAccessibilityService::gAccessibilityService,
             "LocalAccessible service is not initialized.");
  nsAccessibilityService::gAccessibilityService->SetConsumers(aNewConsumer);
  return nsAccessibilityService::gAccessibilityService;
}

void MaybeShutdownAccService(uint32_t aFormerConsumer) {
  nsAccessibilityService* accService =
      nsAccessibilityService::gAccessibilityService;

  if (!accService || nsAccessibilityService::IsShutdown()) {
    return;
  }

  // Still used by XPCOM
  if (nsCoreUtils::AccEventObserversExist() ||
      xpcAccessibilityService::IsInUse() || accService->HasXPCDocuments()) {
    // In case the XPCOM flag was unset (possibly because of the shutdown
    // timer in the xpcAccessibilityService) ensure it is still present. Note:
    // this should be fixed when all the consumer logic is taken out as a
    // separate class.
    accService->SetConsumers(nsAccessibilityService::eXPCOM, false);

    if (aFormerConsumer != nsAccessibilityService::eXPCOM) {
      // Only unset non-XPCOM consumers.
      accService->UnsetConsumers(aFormerConsumer);
    }
    return;
  }

  if (nsAccessibilityService::gConsumers & ~aFormerConsumer) {
    accService->UnsetConsumers(aFormerConsumer);
  } else {
    accService
        ->Shutdown();  // Will unset all nsAccessibilityService::gConsumers
  }
}

////////////////////////////////////////////////////////////////////////////////
// Services
////////////////////////////////////////////////////////////////////////////////

namespace mozilla {
namespace a11y {

FocusManager* FocusMgr() {
  return nsAccessibilityService::gAccessibilityService;
}

SelectionManager* SelectionMgr() {
  return nsAccessibilityService::gAccessibilityService;
}

ApplicationAccessible* ApplicationAcc() {
  return nsAccessibilityService::gApplicationAccessible;
}

xpcAccessibleApplication* XPCApplicationAcc() {
  if (!nsAccessibilityService::gXPCApplicationAccessible &&
      nsAccessibilityService::gApplicationAccessible) {
    nsAccessibilityService::gXPCApplicationAccessible =
        new xpcAccessibleApplication(
            nsAccessibilityService::gApplicationAccessible);
    NS_ADDREF(nsAccessibilityService::gXPCApplicationAccessible);
  }

  return nsAccessibilityService::gXPCApplicationAccessible;
}

EPlatformDisabledState PlatformDisabledState() {
  static bool platformDisabledStateCached = false;
  if (platformDisabledStateCached) {
    return static_cast<EPlatformDisabledState>(sPlatformDisabledState);
  }

  platformDisabledStateCached = true;
  Preferences::RegisterCallback(PrefChanged, PREF_ACCESSIBILITY_FORCE_DISABLED);
  return ReadPlatformDisabledState();
}

EPlatformDisabledState ReadPlatformDisabledState() {
  sPlatformDisabledState =
      Preferences::GetInt(PREF_ACCESSIBILITY_FORCE_DISABLED, 0);
  if (sPlatformDisabledState < ePlatformIsForceEnabled) {
    sPlatformDisabledState = ePlatformIsForceEnabled;
  } else if (sPlatformDisabledState > ePlatformIsDisabled) {
    sPlatformDisabledState = ePlatformIsDisabled;
  }

  return static_cast<EPlatformDisabledState>(sPlatformDisabledState);
}

void PrefChanged(const char* aPref, void* aClosure) {
  if (ReadPlatformDisabledState() == ePlatformIsDisabled) {
    // Force shut down accessibility.
    nsAccessibilityService* accService =
        nsAccessibilityService::gAccessibilityService;
    if (accService && !nsAccessibilityService::IsShutdown()) {
      accService->Shutdown();
    }
  }
}

}  // namespace a11y
}  // namespace mozilla
