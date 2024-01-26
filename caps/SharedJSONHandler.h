/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SharedJSONHandler_h
#define mozilla_SharedJSONHandler_h

#include "js/JSON.h"  // JS::JSONParseHandler

#include "mozilla/RefPtr.h"  // RefPtr

#include "BasePrincipal.h"  // BasePrincipal

namespace mozilla {

// Base class of all principal JSON parse handlers.
class PrincipalJSONHandlerShared : public JS::JSONParseHandler {
 public:
  // Common handlers for inner objects.

  // NOTE: propertyName and stringValue are partial overloads.
  //       Subclasses should put the following:
  //         * `using PrincipalJSONHandlerShared::propertyName`
  //         * `using PrincipalJSONHandlerShared::stringValue`
  virtual bool propertyName(const char16_t* name, size_t length) override {
    NS_WARNING("Principal JSON shouldn't use non-ASCII");
    SetErrorState();
    return false;
  };

  virtual bool stringValue(const char16_t* str, size_t length) override {
    NS_WARNING("Principal JSON shouldn't use non-ASCII");
    SetErrorState();
    return true;
  }

  virtual bool numberValue(double d) override {
    NS_WARNING("Unexpected number value");
    SetErrorState();
    return false;
  }

  virtual bool booleanValue(bool v) override {
    NS_WARNING("Unexpected boolean value");
    SetErrorState();
    return false;
  }

  virtual bool nullValue() override {
    NS_WARNING("Unexpected null value");
    SetErrorState();
    return false;
  }

  virtual void error(const char* msg, uint32_t line, uint32_t column) override {
    // Unused.
  }

 protected:
  // Set to the error state for the above handlers.
  virtual void SetErrorState() = 0;

 public:
  RefPtr<BasePrincipal> mPrincipal;
};

// Base class shared between PrincipalJSONHandler and
// SubsumedPrincipalJSONHandler.
// This implements the common code for them, absorbing the difference about
// whether it can contain ExpandedPrincipal or not.
template <typename HandlerTypesT>
class ContainerPrincipalJSONHandler : public PrincipalJSONHandlerShared {
  using State = typename HandlerTypesT::State;
  using InnerHandlerT = typename HandlerTypesT::InnerHandlerT;
  static constexpr bool CanContainExpandedPrincipal =
      HandlerTypesT::CanContainExpandedPrincipal;

 public:
  // Common handlers.

  virtual bool startObject() override;

  using PrincipalJSONHandlerShared::propertyName;
  virtual bool propertyName(const JS::Latin1Char* name, size_t length) override;
  virtual bool endObject() override;

  virtual bool startArray() override;

  virtual bool endArray() override;

  using PrincipalJSONHandlerShared::stringValue;
  virtual bool stringValue(const JS::Latin1Char* str, size_t length) override;

 private:
  bool ProcessInnerResult(bool aResult);

  template <class Func>
  bool CallOnInner(Func&& aFunc) {
    return mInnerHandler->match([&](auto& aInner) {
      bool result = aFunc(aInner);
      return ProcessInnerResult(result);
    });
  }

 protected:
  State mState = State::Init;

  InnerHandlerT mInnerHandler;
};

}  // namespace mozilla

#endif  // mozilla_SharedJSONHandler_h
