/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef A11Y_AOM_ACCESSIBLENODE_H
#define A11Y_AOM_ACCESSIBLENODE_H

#include "nsWrapperCache.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Nullable.h"

class nsINode;

namespace mozilla {

namespace a11y {
  class Accessible;
}

namespace dom {

class DOMStringList;
struct ParentObject;

#define ANODE_ENUM(name) \
    e##name,

#define ANODE_FUNC(typeName, type, name)                    \
  dom::Nullable<type> Get##name()                           \
  {                                                         \
    return GetProperty(AOM##typeName##Property::e##name);   \
  }                                                         \
                                                            \
  void Set##name(const dom::Nullable<type>& a##name)        \
  {                                                         \
    SetProperty(AOM##typeName##Property::e##name, a##name); \
  }                                                         \

#define ANODE_PROPS(typeName, type, ...)                     \
  enum class AOM##typeName##Property {                       \
    MOZ_FOR_EACH(ANODE_ENUM, (), (__VA_ARGS__))              \
  };                                                         \
  MOZ_FOR_EACH(ANODE_FUNC, (typeName, type,), (__VA_ARGS__)) \

class AccessibleNode : public nsISupports,
                       public nsWrapperCache
{
public:
  explicit AccessibleNode(nsINode* aNode);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS;
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AccessibleNode);

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;
  dom::ParentObject GetParentObject() const;

  void GetRole(nsAString& aRole);
  void GetStates(nsTArray<nsString>& aStates);
  void GetAttributes(nsTArray<nsString>& aAttributes);
  nsINode* GetDOMNode();

  bool Is(const Sequence<nsString>& aFlavors);
  bool Has(const Sequence<nsString>& aAttributes);
  void Get(JSContext* cx, const nsAString& aAttribute,
           JS::MutableHandle<JS::Value> aValue,
           ErrorResult& aRv);

  static bool IsAOMEnabled(JSContext*, JSObject*);

  ANODE_PROPS(Boolean, bool,
    Atomic,
    Busy,
    Disabled,
    Expanded,
    Hidden,
    Modal,
    Multiline,
    Multiselectable,
    ReadOnly,
    Required,
    Selected
  )

protected:
  AccessibleNode(const AccessibleNode& aCopy) = delete;
  AccessibleNode& operator=(const AccessibleNode& aCopy) = delete;
  virtual ~AccessibleNode();

  dom::Nullable<bool> GetProperty(AOMBooleanProperty aProperty)
  {
    int num = static_cast<int>(aProperty);
    if (mBooleanProperties & (1U << (2 * num))) {
      bool data = static_cast<bool>(mBooleanProperties & (1U << (2 * num + 1)));
      return dom::Nullable<bool>(data);
    }
    return dom::Nullable<bool>();
  }

  void SetProperty(AOMBooleanProperty aProperty,
                   const dom::Nullable<bool>& aValue)
  {
    int num = static_cast<int>(aProperty);
    if (aValue.IsNull()) {
      mBooleanProperties &= ~(1U << (2 * num));
    } else {
      mBooleanProperties |= (1U << (2 * num));
      mBooleanProperties = (aValue.Value() ? mBooleanProperties | (1U << (2 * num + 1))
                                           : mBooleanProperties & ~(1U << (2 * num + 1)));
    }
  }

  // The 2k'th bit indicates whether the k'th boolean property is used(1) or not(0)
  // and 2k+1'th bit contains the property's value(1:true, 0:false)
  uint32_t mBooleanProperties;

  RefPtr<a11y::Accessible> mIntl;
  RefPtr<nsINode> mDOMNode;
  RefPtr<dom::DOMStringList> mStates;
};

} // dom
} // mozilla


#endif // A11Y_JSAPI_ACCESSIBLENODE
