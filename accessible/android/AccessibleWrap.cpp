/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"

#include "Accessible-inl.h"
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

mozilla::java::GeckoBundle::LocalRef
AccessibleWrap::CreateBundle(int32_t aParentID,
                             role aRole,
                             uint64_t aState,
                             const nsString& aName,
                             const nsString& aTextValue,
                             const nsString& aDOMNodeID,
                             const nsIntRect& aBounds,
                             double aCurVal,
                             double aMinVal,
                             double aMaxVal,
                             double aStep,
                             nsIPersistentProperties* aAttributes,
                             const nsTArray<int32_t>& aChildren) const
{
  GECKOBUNDLE_START(nodeInfo);
  GECKOBUNDLE_PUT(nodeInfo, "id", java::sdk::Integer::ValueOf(VirtualViewID()));
  GECKOBUNDLE_PUT(nodeInfo, "parentId", java::sdk::Integer::ValueOf(aParentID));
  uint64_t flags = GetFlags(aRole, aState);
  GECKOBUNDLE_PUT(nodeInfo, "flags", java::sdk::Integer::ValueOf(flags));

  nsAutoString geckoRole;
  nsAutoString roleDescription;
  nsAutoString className;
  GetAndroidRoleAndClass(aRole, geckoRole, roleDescription, className);
  if (VirtualViewID() == kNoID) {
    className.AssignLiteral("android.webkit.WebView");
    roleDescription.AssignLiteral("");
  }
  GECKOBUNDLE_PUT(
    nodeInfo, "roleDescription", jni::StringParam(roleDescription));
  GECKOBUNDLE_PUT(nodeInfo, "geckoRole", jni::StringParam(geckoRole));
  GECKOBUNDLE_PUT(nodeInfo, "className", jni::StringParam(className));

  if (!aTextValue.IsEmpty() &&
      (flags & java::SessionAccessibility::FLAG_EDITABLE)) {
    GECKOBUNDLE_PUT(nodeInfo, "hint", jni::StringParam(aName));
    GECKOBUNDLE_PUT(nodeInfo, "text", jni::StringParam(aTextValue));
  } else {
    GECKOBUNDLE_PUT(nodeInfo, "text", jni::StringParam(aName));
  }

  if (!aDOMNodeID.IsEmpty()) {
    GECKOBUNDLE_PUT(
      nodeInfo, "viewIdResourceName", jni::StringParam(aDOMNodeID));
  }

  const int32_t data[4] = {
    aBounds.x, aBounds.y, aBounds.x + aBounds.width, aBounds.y + aBounds.height
  };
  GECKOBUNDLE_PUT(nodeInfo, "bounds", jni::IntArray::New(data, 4));

  if (HasNumericValue()) {
    GECKOBUNDLE_START(rangeInfo);
    if (aMaxVal == 1 && aMinVal == 0) {
      GECKOBUNDLE_PUT(
        rangeInfo, "type", java::sdk::Integer::ValueOf(2)); // percent
    } else if (std::round(aStep) != aStep) {
      GECKOBUNDLE_PUT(
        rangeInfo, "type", java::sdk::Integer::ValueOf(1)); // float
    } else {
      GECKOBUNDLE_PUT(
        rangeInfo, "type", java::sdk::Integer::ValueOf(0)); // integer
    }

    if (!IsNaN(aCurVal)) {
      GECKOBUNDLE_PUT(rangeInfo, "current", java::sdk::Double::New(aCurVal));
    }
    if (!IsNaN(aMinVal)) {
      GECKOBUNDLE_PUT(rangeInfo, "min", java::sdk::Double::New(aMinVal));
    }
    if (!IsNaN(aMaxVal)) {
      GECKOBUNDLE_PUT(rangeInfo, "max", java::sdk::Double::New(aMaxVal));
    }

    GECKOBUNDLE_FINISH(rangeInfo);
    GECKOBUNDLE_PUT(nodeInfo, "rangeInfo", rangeInfo);
  }

  nsString inputType;
  nsAccUtils::GetAccAttr(aAttributes, nsGkAtoms::textInputType, inputType);
  if (!inputType.IsEmpty()) {
    GECKOBUNDLE_PUT(nodeInfo, "inputType", jni::StringParam(inputType));
  }

  nsString posinset;
  nsresult rv = aAttributes->GetStringProperty(NS_LITERAL_CSTRING("posinset"), posinset);
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
  rv = aAttributes->GetStringProperty(NS_LITERAL_CSTRING("child-item-count"),
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
      rv = aAttributes->GetStringProperty(NS_LITERAL_CSTRING("hierarchical"),
                                          unused);
      if (NS_SUCCEEDED(rv)) {
        GECKOBUNDLE_PUT(
          collectionInfo, "isHierarchical", java::sdk::Boolean::TRUE());
      }

      if (IsSelect()) {
        int32_t selectionMode = (aState & states::MULTISELECTABLE) ? 2 : 1;
        GECKOBUNDLE_PUT(collectionInfo,
                        "selectionMode",
                        java::sdk::Integer::ValueOf(selectionMode));
      }
      GECKOBUNDLE_FINISH(collectionInfo);

      GECKOBUNDLE_PUT(nodeInfo, "collectionInfo", collectionInfo);
    }
  }

  GECKOBUNDLE_PUT(nodeInfo,
                  "children",
                  jni::IntArray::New(aChildren.Elements(), aChildren.Length()));
  GECKOBUNDLE_FINISH(nodeInfo);

  return nodeInfo;
}

uint64_t
AccessibleWrap::GetFlags(role aRole, uint64_t aState)
{
  uint64_t flags = 0;
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
AccessibleWrap::GetAndroidRoleAndClass(role aRole,
                                       nsAString& aGeckoRole,
                                       nsAString& aRoleDescription,
                                       nsAString& aClassStr)
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

#define ROLE(geckoRole,                                                        \
             stringRole,                                                       \
             atkRole,                                                          \
             macRole,                                                          \
             msaaRole,                                                         \
             ia2Role,                                                          \
             androidClass,                                                     \
             nameRule)                                                         \
  case roles::geckoRole:                                                       \
    rv = bundle->GetStringFromName(stringRole, aRoleDescription);              \
    if (NS_FAILED(rv))                                                         \
      aRoleDescription.AssignLiteral("");                                      \
    aGeckoRole.AssignLiteral(stringRole);                                      \
    aClassStr.AssignLiteral(androidClass);                                     \
    break;

  switch (aRole) {
#include "RoleMap.h"
    default:
      aRoleDescription.AssignLiteral("");
      aGeckoRole.AssignLiteral("nothing");
      aClassStr.AssignLiteral("android.view.View");
      return;
  }

#undef ROLE
}

void
AccessibleWrap::DOMNodeID(nsString& aDOMNodeID)
{
  if (mContent) {
    nsAtom* id = mContent->GetID();
    if (id) {
      id->ToString(aDOMNodeID);
    }
  }
}

mozilla::java::GeckoBundle::LocalRef
AccessibleWrap::ToBundle()
{
  AccessibleWrap* parent = static_cast<AccessibleWrap*>(Parent());

  nsAutoString name;
  Name(name);

  nsAutoString value;
  Value(value);

  nsAutoString viewIdResourceName;
  DOMNodeID(viewIdResourceName);

  nsCOMPtr<nsIPersistentProperties> attributes = Attributes();

  auto childCount = ChildCount();
  nsTArray<int32_t> children(childCount);
  for (uint32_t i = 0; i < childCount; i++) {
    auto child = static_cast<AccessibleWrap*>(GetChildAt(i));
    children.AppendElement(child->VirtualViewID());
  }

  return CreateBundle(parent ? parent->VirtualViewID() : 0,
                      Role(),
                      State(),
                      name,
                      value,
                      viewIdResourceName,
                      Bounds(),
                      CurValue(),
                      MinValue(),
                      MaxValue(),
                      Step(),
                      attributes,
                      children);
}
