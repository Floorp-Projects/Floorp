/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef A11Y_AOM_ACCESSIBLENODE_H
#define A11Y_AOM_ACCESSIBLENODE_H

#include "nsTHashMap.h"
#include "nsRefPtrHashtable.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/DOMString.h"
#include "mozilla/dom/Nullable.h"

class nsINode;

namespace mozilla {

class ErrorResult;

namespace a11y {
class LocalAccessible;
}

namespace dom {

class DOMStringList;
struct ParentObject;

#define ANODE_ENUM(name) e##name,

#define ANODE_FUNC(typeName, type, name)                    \
  dom::Nullable<type> Get##name() {                         \
    return GetProperty(AOM##typeName##Property::e##name);   \
  }                                                         \
                                                            \
  void Set##name(const dom::Nullable<type>& a##name) {      \
    SetProperty(AOM##typeName##Property::e##name, a##name); \
  }

#define ANODE_STRING_FUNC(name)                              \
  void Get##name(nsAString& a##name) {                       \
    return GetProperty(AOMStringProperty::e##name, a##name); \
  }                                                          \
                                                             \
  void Set##name(const nsAString& a##name) {                 \
    SetProperty(AOMStringProperty::e##name, a##name);        \
  }

#define ANODE_RELATION_FUNC(name)                       \
  already_AddRefed<AccessibleNode> Get##name() {        \
    return GetProperty(AOMRelationProperty::e##name);   \
  }                                                     \
                                                        \
  void Set##name(AccessibleNode* a##name) {             \
    SetProperty(AOMRelationProperty::e##name, a##name); \
  }

#define ANODE_PROPS(typeName, type, ...)            \
  enum class AOM##typeName##Property{               \
      MOZ_FOR_EACH(ANODE_ENUM, (), (__VA_ARGS__))}; \
  MOZ_FOR_EACH(ANODE_FUNC, (typeName, type, ), (__VA_ARGS__))

#define ANODE_STRING_PROPS(...)                 \
  enum class AOMStringProperty {                \
    MOZ_FOR_EACH(ANODE_ENUM, (), (__VA_ARGS__)) \
  };                                            \
  MOZ_FOR_EACH(ANODE_STRING_FUNC, (), (__VA_ARGS__))

#define ANODE_RELATION_PROPS(...)               \
  enum class AOMRelationProperty {              \
    MOZ_FOR_EACH(ANODE_ENUM, (), (__VA_ARGS__)) \
  };                                            \
  MOZ_FOR_EACH(ANODE_RELATION_FUNC, (), (__VA_ARGS__))

#define ANODE_ACCESSOR_MUTATOR(typeName, type, defVal)                      \
  nsTHashMap<nsUint32HashKey, type> m##typeName##Properties;                \
                                                                            \
  dom::Nullable<type> GetProperty(AOM##typeName##Property aProperty) {      \
    type value = defVal;                                                    \
    if (m##typeName##Properties.Get(static_cast<int>(aProperty), &value)) { \
      return dom::Nullable<type>(value);                                    \
    }                                                                       \
    return dom::Nullable<type>();                                           \
  }                                                                         \
                                                                            \
  void SetProperty(AOM##typeName##Property aProperty,                       \
                   const dom::Nullable<type>& aValue) {                     \
    if (aValue.IsNull()) {                                                  \
      m##typeName##Properties.Remove(static_cast<int>(aProperty));          \
    } else {                                                                \
      m##typeName##Properties.InsertOrUpdate(static_cast<int>(aProperty),   \
                                             aValue.Value());               \
    }                                                                       \
  }

class AccessibleNode : public nsISupports, public nsWrapperCache {
 public:
  explicit AccessibleNode(nsINode* aNode);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS;
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(AccessibleNode);

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;
  dom::ParentObject GetParentObject() const;

  void GetComputedRole(nsAString& aRole);
  void GetStates(nsTArray<nsString>& aStates);
  void GetAttributes(nsTArray<nsString>& aAttributes);
  nsINode* GetDOMNode();

  bool Is(const Sequence<nsString>& aFlavors);
  bool Has(const Sequence<nsString>& aAttributes);
  void Get(JSContext* cx, const nsAString& aAttribute,
           JS::MutableHandle<JS::Value> aValue, ErrorResult& aRv);

  static bool IsAOMEnabled(JSContext*, JSObject*);

  ANODE_STRING_PROPS(Autocomplete, Checked, Current, HasPopUp, Invalid,
                     KeyShortcuts, Label, Live, Orientation, Placeholder,
                     Pressed, Relevant, Role, RoleDescription, Sort, ValueText)

  ANODE_PROPS(Boolean, bool, Atomic, Busy, Disabled, Expanded, Hidden, Modal,
              Multiline, Multiselectable, ReadOnly, Required, Selected)

  ANODE_PROPS(UInt, uint32_t, ColIndex, ColSpan, Level, PosInSet, RowIndex,
              RowSpan)

  ANODE_PROPS(Int, int32_t, ColCount, RowCount, SetSize)

  ANODE_PROPS(Double, double, ValueMax, ValueMin, ValueNow)

  ANODE_RELATION_PROPS(ActiveDescendant, Details, ErrorMessage)

 protected:
  AccessibleNode(const AccessibleNode& aCopy) = delete;
  AccessibleNode& operator=(const AccessibleNode& aCopy) = delete;
  virtual ~AccessibleNode();

  void GetProperty(AOMStringProperty aProperty, nsAString& aRetval) {
    nsString data;
    if (!mStringProperties.Get(static_cast<int>(aProperty), &data)) {
      SetDOMStringToNull(data);
    }
    aRetval = data;
  }

  void SetProperty(AOMStringProperty aProperty, const nsAString& aValue) {
    if (DOMStringIsNull(aValue)) {
      mStringProperties.Remove(static_cast<int>(aProperty));
    } else {
      nsString value(aValue);
      mStringProperties.InsertOrUpdate(static_cast<int>(aProperty), value);
    }
  }

  dom::Nullable<bool> GetProperty(AOMBooleanProperty aProperty) {
    int num = static_cast<int>(aProperty);
    if (mBooleanProperties & (1U << (2 * num))) {
      bool data = static_cast<bool>(mBooleanProperties & (1U << (2 * num + 1)));
      return dom::Nullable<bool>(data);
    }
    return dom::Nullable<bool>();
  }

  void SetProperty(AOMBooleanProperty aProperty,
                   const dom::Nullable<bool>& aValue) {
    int num = static_cast<int>(aProperty);
    if (aValue.IsNull()) {
      mBooleanProperties &= ~(1U << (2 * num));
    } else {
      mBooleanProperties |= (1U << (2 * num));
      mBooleanProperties =
          (aValue.Value() ? mBooleanProperties | (1U << (2 * num + 1))
                          : mBooleanProperties & ~(1U << (2 * num + 1)));
    }
  }

  ANODE_ACCESSOR_MUTATOR(Double, double, 0.0)
  ANODE_ACCESSOR_MUTATOR(Int, int32_t, 0)
  ANODE_ACCESSOR_MUTATOR(UInt, uint32_t, 0)

  already_AddRefed<AccessibleNode> GetProperty(AOMRelationProperty aProperty) {
    return mRelationProperties.Get(static_cast<int>(aProperty));
  }

  void SetProperty(AOMRelationProperty aProperty, AccessibleNode* aValue) {
    if (!aValue) {
      mRelationProperties.Remove(static_cast<int>(aProperty));
    } else {
      mRelationProperties.InsertOrUpdate(static_cast<int>(aProperty),
                                         RefPtr{aValue});
    }
  }

  // The 2k'th bit indicates whether the k'th boolean property is used(1) or
  // not(0) and 2k+1'th bit contains the property's value(1:true, 0:false)
  uint32_t mBooleanProperties;
  nsRefPtrHashtable<nsUint32HashKey, AccessibleNode> mRelationProperties;
  nsTHashMap<nsUint32HashKey, nsString> mStringProperties;

  RefPtr<a11y::LocalAccessible> mIntl;
  RefPtr<nsINode> mDOMNode;
  RefPtr<dom::DOMStringList> mStates;
};

}  // namespace dom
}  // namespace mozilla

#endif  // A11Y_JSAPI_ACCESSIBLENODE
