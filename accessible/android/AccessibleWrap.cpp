/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"

#include "Accessible-inl.h"
#include "AndroidInputType.h"
#include "DocAccessibleWrap.h"
#include "IDSet.h"
#include "JavaBuiltins.h"
#include "SessionAccessibility.h"
#include "nsAccessibilityService.h"
#include "nsIPersistentProperties2.h"
#include "nsIStringBundle.h"
#include "nsAccUtils.h"

#define ROLE_STRINGS_URL "chrome://global/locale/AccessFu.properties"

using namespace mozilla::a11y;

// IDs should be a positive 32bit integer.
IDSet sIDSet(31UL);

//-----------------------------------------------------
// construction
//-----------------------------------------------------
AccessibleWrap::AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc)
  : Accessible(aContent, aDoc)
{
  if (aDoc) {
    mID = AcquireID();
    DocAccessibleWrap* doc = static_cast<DocAccessibleWrap*>(aDoc);
    doc->AddID(mID, this);
  }
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
AccessibleWrap::~AccessibleWrap() {}

nsresult
AccessibleWrap::HandleAccEvent(AccEvent* aEvent)
{
  nsresult rv = Accessible::HandleAccEvent(aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IPCAccessibilityActive()) {
    return NS_OK;
  }

  auto accessible = static_cast<AccessibleWrap*>(aEvent->GetAccessible());
  NS_ENSURE_TRUE(accessible, NS_ERROR_FAILURE);

  // The accessible can become defunct if we have an xpcom event listener
  // which decides it would be fun to change the DOM and flush layout.
  if (accessible->IsDefunct() || !accessible->IsBoundToParent()) {
    return NS_OK;
  }

  if (DocAccessible* doc = accessible->Document()) {
    if (!nsCoreUtils::IsContentDocument(doc->DocumentNode())) {
      return NS_OK;
    }
  }

  SessionAccessibility* sessionAcc =
    SessionAccessibility::GetInstanceFor(accessible);
  if (!sessionAcc) {
    return NS_OK;
  }

  switch (aEvent->GetEventType()) {
    case nsIAccessibleEvent::EVENT_FOCUS:
      sessionAcc->SendFocusEvent(accessible);
      break;
    case nsIAccessibleEvent::EVENT_VIRTUALCURSOR_CHANGED: {
      AccVCChangeEvent* vcEvent = downcast_accEvent(aEvent);
      auto newPosition = static_cast<AccessibleWrap*>(vcEvent->NewAccessible());
      auto oldPosition = static_cast<AccessibleWrap*>(vcEvent->OldAccessible());

      if (sessionAcc && newPosition) {
        if (oldPosition != newPosition) {
          if (vcEvent->Reason() == nsIAccessiblePivot::REASON_POINT) {
            sessionAcc->SendHoverEnterEvent(newPosition);
          } else {
            sessionAcc->SendAccessibilityFocusedEvent(newPosition);
          }
        }

        if (vcEvent->BoundaryType() != nsIAccessiblePivot::NO_BOUNDARY) {
          sessionAcc->SendTextTraversedEvent(
            newPosition, vcEvent->NewStartOffset(), vcEvent->NewEndOffset());
        }
      }
      break;
    }
    case nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED: {
      AccCaretMoveEvent* event = downcast_accEvent(aEvent);
      sessionAcc->SendTextSelectionChangedEvent(accessible,
                                                event->GetCaretOffset());
      break;
    }
    case nsIAccessibleEvent::EVENT_TEXT_INSERTED:
    case nsIAccessibleEvent::EVENT_TEXT_REMOVED: {
      AccTextChangeEvent* event = downcast_accEvent(aEvent);
      sessionAcc->SendTextChangedEvent(accessible,
                                       event->ModifiedText(),
                                       event->GetStartOffset(),
                                       event->GetLength(),
                                       event->IsTextInserted(),
                                       event->IsFromUserInput());
      break;
    }
    case nsIAccessibleEvent::EVENT_STATE_CHANGE: {
      AccStateChangeEvent* event = downcast_accEvent(aEvent);
      auto state = event->GetState();
      if (state & states::CHECKED) {
        sessionAcc->SendClickedEvent(accessible, event->IsStateEnabled());
      }

      if (state & states::SELECTED) {
        sessionAcc->SendSelectedEvent(accessible);
      }

      if (state & states::BUSY) {
        sessionAcc->SendWindowStateChangedEvent(accessible);
      }
      break;
    }
    case nsIAccessibleEvent::EVENT_SCROLLING: {
      AccScrollingEvent* event = downcast_accEvent(aEvent);
      sessionAcc->SendScrollingEvent(accessible,
                                     event->ScrollX(),
                                     event->ScrollY(),
                                     event->MaxScrollX(),
                                     event->MaxScrollY());
      break;
    }
    case nsIAccessibleEvent::EVENT_SHOW:
    case nsIAccessibleEvent::EVENT_HIDE: {
      AccMutationEvent* event = downcast_accEvent(aEvent);
      auto parent = static_cast<AccessibleWrap*>(event->Parent());
      sessionAcc->SendWindowContentChangedEvent(parent);
      break;
    }
    default:
      break;
  }

  return NS_OK;
}

void
AccessibleWrap::Shutdown()
{
  if (mDoc) {
    if (mID > 0) {
      if (auto doc = static_cast<DocAccessibleWrap*>(mDoc.get())) {
        doc->RemoveID(mID);
      }
      ReleaseID(mID);
      mID = 0;
    }
  }

  Accessible::Shutdown();
}

int32_t
AccessibleWrap::AcquireID()
{
  return sIDSet.GetID();
}

void
AccessibleWrap::ReleaseID(int32_t aID)
{
  sIDSet.ReleaseID(aID);
}

void
AccessibleWrap::SetTextContents(const nsAString& aText) {
  if (IsHyperText()) {
    AsHyperText()->ReplaceText(aText);
  }
}

void
AccessibleWrap::GetTextContents(nsAString& aText) {
  // For now it is a simple wrapper for getting entire range of TextSubstring.
  // In the future this may be smarter and retrieve a flattened string.
  if (IsHyperText()) {
    AsHyperText()->TextSubstring(0, -1, aText);
  }
}

bool
AccessibleWrap::GetSelectionBounds(int32_t* aStartOffset, int32_t* aEndOffset) {
  if (IsHyperText()) {
    return AsHyperText()->SelectionBoundsAt(0, aStartOffset, aEndOffset);
  }

  return false;
}

uint32_t
AccessibleWrap::GetFlags(role aRole, uint64_t aState)
{
  uint32_t flags = 0;
  if (aState & states::CHECKABLE) {
    flags |= java::SessionAccessibility::FLAG_CHECKABLE;
  }

  if (aState & states::CHECKED) {
    flags |= java::SessionAccessibility::FLAG_CHECKED;
  }

  if (aState & states::INVALID) {
    flags |= java::SessionAccessibility::FLAG_CONTENT_INVALID;
  }

  if (aState & states::EDITABLE) {
    flags |= java::SessionAccessibility::FLAG_EDITABLE;
  }

  if (aState & states::SENSITIVE) {
    flags |= java::SessionAccessibility::FLAG_CLICKABLE;
  }

  if (aState & states::ENABLED) {
    flags |= java::SessionAccessibility::FLAG_ENABLED;
  }

  if (aState & states::FOCUSABLE) {
    flags |= java::SessionAccessibility::FLAG_FOCUSABLE;
  }

  if (aState & states::FOCUSED) {
    flags |= java::SessionAccessibility::FLAG_FOCUSED;
  }

  if (aState & states::MULTI_LINE) {
    flags |= java::SessionAccessibility::FLAG_MULTI_LINE;
  }

  if (aState & states::SELECTABLE) {
    flags |= java::SessionAccessibility::FLAG_SELECTABLE;
  }

  if (aState & states::SELECTED) {
    flags |= java::SessionAccessibility::FLAG_SELECTED;
  }

  if ((aState & (states::INVISIBLE | states::OFFSCREEN)) == 0) {
    flags |= java::SessionAccessibility::FLAG_VISIBLE_TO_USER;
  }

  if (aRole == roles::PASSWORD_TEXT) {
    flags |= java::SessionAccessibility::FLAG_PASSWORD;
  }

  return flags;
}

void
AccessibleWrap::GetRoleDescription(role aRole,
                                   nsAString& aGeckoRole,
                                   nsAString& aRoleDescription)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIStringBundleService> sbs =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get string bundle service");
    return;
  }

  nsCOMPtr<nsIStringBundle> bundle;
  rv = sbs->CreateBundle(ROLE_STRINGS_URL, getter_AddRefs(bundle));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get string bundle");
    return;
  }

  GetAccService()->GetStringRole(aRole, aGeckoRole);
  rv = bundle->GetStringFromName(NS_ConvertUTF16toUTF8(aGeckoRole).get(), aRoleDescription);
  if (NS_FAILED(rv)) {
    aRoleDescription.AssignLiteral("");
  }
}

int32_t
AccessibleWrap::GetAndroidClass(role aRole)
{
#define ROLE(geckoRole,                                                        \
             stringRole,                                                       \
             atkRole,                                                          \
             macRole,                                                          \
             msaaRole,                                                         \
             ia2Role,                                                          \
             androidClass,                                                     \
             nameRule)                                                         \
  case roles::geckoRole:                                                       \
    return androidClass;

  switch (aRole) {
#include "RoleMap.h"
    default:
      return java::SessionAccessibility::CLASSNAME_VIEW;
  }

#undef ROLE
}

int32_t
AccessibleWrap::GetInputType(const nsString& aInputTypeAttr)
{
  if (aInputTypeAttr.EqualsIgnoreCase("email")) {
    return java::sdk::InputType::TYPE_CLASS_TEXT | java::sdk::InputType::TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS;
  }

  if (aInputTypeAttr.EqualsIgnoreCase("number")) {
    return java::sdk::InputType::TYPE_CLASS_NUMBER;
  }

  if (aInputTypeAttr.EqualsIgnoreCase("password")) {
    return java::sdk::InputType::TYPE_CLASS_TEXT | java::sdk::InputType::TYPE_TEXT_VARIATION_WEB_PASSWORD;
  }

  if (aInputTypeAttr.EqualsIgnoreCase("tel")) {
    return java::sdk::InputType::TYPE_CLASS_PHONE;
  }

  if (aInputTypeAttr.EqualsIgnoreCase("text")) {
    return java::sdk::InputType::TYPE_CLASS_TEXT | java::sdk::InputType::TYPE_TEXT_VARIATION_WEB_EDIT_TEXT;
  }

  if (aInputTypeAttr.EqualsIgnoreCase("url")) {
    return java::sdk::InputType::TYPE_CLASS_TEXT | java::sdk::InputType::TYPE_TEXT_VARIATION_URI;
  }

  return 0;
}

void
AccessibleWrap::WrapperDOMNodeID(nsString& aDOMNodeID)
{
  if (mContent) {
    nsAtom* id = mContent->GetID();
    if (id) {
      id->ToString(aDOMNodeID);
    }
  }
}

bool
AccessibleWrap::WrapperRangeInfo(double* aCurVal, double* aMinVal,
                             double* aMaxVal, double* aStep)
{
  if (HasNumericValue()) {
    *aCurVal = CurValue();
    *aMinVal = MinValue();
    *aMaxVal = MaxValue();
    *aStep = Step();
    return true;
  }

  return false;
}

mozilla::java::GeckoBundle::LocalRef
AccessibleWrap::ToBundle()
{
  if (!IsProxy() && IsDefunct()) {
    return nullptr;
  }

  GECKOBUNDLE_START(nodeInfo);
  GECKOBUNDLE_PUT(nodeInfo, "id", java::sdk::Integer::ValueOf(VirtualViewID()));

  AccessibleWrap* parent = WrapperParent();
  GECKOBUNDLE_PUT(nodeInfo, "parentId",
    java::sdk::Integer::ValueOf(parent ? parent->VirtualViewID() : 0));

  role role = WrapperRole();
  uint64_t state = State();
  uint32_t flags = GetFlags(role, state);
  GECKOBUNDLE_PUT(nodeInfo, "flags", java::sdk::Integer::ValueOf(flags));
  GECKOBUNDLE_PUT(nodeInfo, "className", java::sdk::Integer::ValueOf(AndroidClass()));

  nsAutoString text;
  if (state & states::EDITABLE) {
    Value(text);
  }

  if (!text.IsEmpty()) {
    nsAutoString hint;
    Name(hint);
    GECKOBUNDLE_PUT(nodeInfo, "hint", jni::StringParam(hint));
  } else {
    Name(text);
  }
  GECKOBUNDLE_PUT(nodeInfo, "text", jni::StringParam(text));

  nsAutoString geckoRole;
  nsAutoString roleDescription;
  if (VirtualViewID() != kNoID) {
    GetRoleDescription(role, geckoRole, roleDescription);
  }

  GECKOBUNDLE_PUT(
    nodeInfo, "roleDescription", jni::StringParam(roleDescription));
  GECKOBUNDLE_PUT(nodeInfo, "geckoRole", jni::StringParam(geckoRole));

  GECKOBUNDLE_PUT(
    nodeInfo, "roleDescription", jni::StringParam(roleDescription));
  GECKOBUNDLE_PUT(nodeInfo, "geckoRole", jni::StringParam(geckoRole));

  nsAutoString viewIdResourceName;
  WrapperDOMNodeID(viewIdResourceName);
  if (!viewIdResourceName.IsEmpty()) {
    GECKOBUNDLE_PUT(
      nodeInfo, "viewIdResourceName", jni::StringParam(viewIdResourceName));
  }

  nsIntRect bounds = Bounds();
  const int32_t data[4] = {
    bounds.x, bounds.y, bounds.x + bounds.width, bounds.y + bounds.height
  };
  GECKOBUNDLE_PUT(nodeInfo, "bounds", jni::IntArray::New(data, 4));

  double curValue = 0;
  double minValue = 0;
  double maxValue = 0;
  double step = 0;
  if (WrapperRangeInfo(&curValue, &minValue, &maxValue, &step)) {
    GECKOBUNDLE_START(rangeInfo);
    if (maxValue == 1 && minValue == 0) {
      GECKOBUNDLE_PUT(
        rangeInfo, "type", java::sdk::Integer::ValueOf(2)); // percent
    } else if (std::round(step) != step) {
      GECKOBUNDLE_PUT(
        rangeInfo, "type", java::sdk::Integer::ValueOf(1)); // float
    } else {
      GECKOBUNDLE_PUT(
        rangeInfo, "type", java::sdk::Integer::ValueOf(0)); // integer
    }

    if (!IsNaN(curValue)) {
      GECKOBUNDLE_PUT(rangeInfo, "current", java::sdk::Double::New(curValue));
    }
    if (!IsNaN(minValue)) {
      GECKOBUNDLE_PUT(rangeInfo, "min", java::sdk::Double::New(minValue));
    }
    if (!IsNaN(maxValue)) {
      GECKOBUNDLE_PUT(rangeInfo, "max", java::sdk::Double::New(maxValue));
    }

    GECKOBUNDLE_FINISH(rangeInfo);
    GECKOBUNDLE_PUT(nodeInfo, "rangeInfo", rangeInfo);
  }

  nsCOMPtr<nsIPersistentProperties> attributes = Attributes();

  nsString inputTypeAttr;
  nsAccUtils::GetAccAttr(attributes, nsGkAtoms::textInputType, inputTypeAttr);
  int32_t inputType = GetInputType(inputTypeAttr);
  if (inputType) {
    GECKOBUNDLE_PUT(nodeInfo, "inputType", java::sdk::Integer::ValueOf(inputType));
  }

  nsString posinset;
  nsresult rv = attributes->GetStringProperty(NS_LITERAL_CSTRING("posinset"), posinset);
  if (NS_SUCCEEDED(rv)) {
    int32_t rowIndex;
    if (sscanf(NS_ConvertUTF16toUTF8(posinset).get(), "%d", &rowIndex) > 0) {
      GECKOBUNDLE_START(collectionItemInfo);
      GECKOBUNDLE_PUT(
        collectionItemInfo, "rowIndex", java::sdk::Integer::ValueOf(rowIndex));
      GECKOBUNDLE_PUT(
        collectionItemInfo, "columnIndex", java::sdk::Integer::ValueOf(0));
      GECKOBUNDLE_PUT(
        collectionItemInfo, "rowSpan", java::sdk::Integer::ValueOf(1));
      GECKOBUNDLE_PUT(
        collectionItemInfo, "columnSpan", java::sdk::Integer::ValueOf(1));
      GECKOBUNDLE_FINISH(collectionItemInfo);

      GECKOBUNDLE_PUT(nodeInfo, "collectionItemInfo", collectionItemInfo);
    }
  }

  nsString colSize;
  rv = attributes->GetStringProperty(NS_LITERAL_CSTRING("child-item-count"),
                                      colSize);
  if (NS_SUCCEEDED(rv)) {
    int32_t rowCount;
    if (sscanf(NS_ConvertUTF16toUTF8(colSize).get(), "%d", &rowCount) > 0) {
      GECKOBUNDLE_START(collectionInfo);
      GECKOBUNDLE_PUT(
        collectionInfo, "rowCount", java::sdk::Integer::ValueOf(rowCount));
      GECKOBUNDLE_PUT(
        collectionInfo, "columnCount", java::sdk::Integer::ValueOf(1));

      nsString unused;
      rv = attributes->GetStringProperty(NS_LITERAL_CSTRING("hierarchical"),
                                          unused);
      if (NS_SUCCEEDED(rv)) {
        GECKOBUNDLE_PUT(
          collectionInfo, "isHierarchical", java::sdk::Boolean::TRUE());
      }

      if (IsSelect()) {
        int32_t selectionMode = (state & states::MULTISELECTABLE) ? 2 : 1;
        GECKOBUNDLE_PUT(collectionInfo,
                        "selectionMode",
                        java::sdk::Integer::ValueOf(selectionMode));
      }
      GECKOBUNDLE_FINISH(collectionInfo);

      GECKOBUNDLE_PUT(nodeInfo, "collectionInfo", collectionInfo);
    }
  }

  auto childCount = ChildCount();
  nsTArray<int32_t> children(childCount);
  for (uint32_t i = 0; i < childCount; i++) {
    auto child = static_cast<AccessibleWrap*>(GetChildAt(i));
    children.AppendElement(child->VirtualViewID());
  }

  GECKOBUNDLE_PUT(nodeInfo,
                  "children",
                  jni::IntArray::New(children.Elements(), children.Length()));
  GECKOBUNDLE_FINISH(nodeInfo);

  return nodeInfo;
}

mozilla::java::GeckoBundle::LocalRef
AccessibleWrap::ToSmallBundle()
{
  return ToSmallBundle(State(), Bounds());
}

mozilla::java::GeckoBundle::LocalRef
AccessibleWrap::ToSmallBundle(const uint64_t aState, const nsIntRect& aBounds)
{
  GECKOBUNDLE_START(nodeInfo);
  GECKOBUNDLE_PUT(nodeInfo, "id", java::sdk::Integer::ValueOf(VirtualViewID()));

  AccessibleWrap* parent = WrapperParent();
  GECKOBUNDLE_PUT(nodeInfo, "parentId",
    java::sdk::Integer::ValueOf(parent ? parent->VirtualViewID() : 0));

  uint32_t flags = GetFlags(WrapperRole(), aState);
  GECKOBUNDLE_PUT(nodeInfo, "flags", java::sdk::Integer::ValueOf(flags));
  GECKOBUNDLE_PUT(nodeInfo, "className", java::sdk::Integer::ValueOf(AndroidClass()));

  const int32_t data[4] = {
    aBounds.x, aBounds.y, aBounds.x + aBounds.width, aBounds.y + aBounds.height
  };
  GECKOBUNDLE_PUT(nodeInfo, "bounds", jni::IntArray::New(data, 4));

  auto childCount = ChildCount();
  nsTArray<int32_t> children(childCount);
  for (uint32_t i = 0; i < childCount; ++i) {
    auto child = static_cast<AccessibleWrap*>(GetChildAt(i));
    children.AppendElement(child->VirtualViewID());
  }

  GECKOBUNDLE_PUT(nodeInfo,
                  "children",
                  jni::IntArray::New(children.Elements(), children.Length()));

  GECKOBUNDLE_FINISH(nodeInfo);

  return nodeInfo;
}
