/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ContentPrincipalJSONHandler_h
#define mozilla_ContentPrincipalJSONHandler_h

#include <stddef.h>  // size_t
#include <stdint.h>  // uint32_t

#include "js/TypeDecls.h"  // JS::Latin1Char

#include "mozilla/AlreadyAddRefed.h"  // already_AddRefed
#include "mozilla/RefPtr.h"           // RefPtr

#include "nsCOMPtr.h"  // nsCOMPtr
#include "nsDebug.h"   // NS_WARNING
#include "nsIURI.h"    // nsIURI

#include "ContentPrincipal.h"
#include "OriginAttributes.h"
#include "SharedJSONHandler.h"

namespace mozilla {

// JSON parse handler for an inner object for ContentPrincipal.
// Used by PrincipalJSONHandler or SubsumedPrincipalJSONHandler.
class ContentPrincipalJSONHandler : public PrincipalJSONHandlerShared {
  enum class State {
    Init,

    // After the inner object's '{'.
    StartObject,

    // After the property key for eURI.
    URIKey,

    // After the property key for eDomain.
    DomainKey,

    // After the property key for eSuffix.
    SuffixKey,

    // After the property value for eURI, eDomain, or eSuffix.
    AfterPropertyValue,

    // After the inner object's '}'.
    EndObject,

    Error,
  };

 public:
  ContentPrincipalJSONHandler() = default;
  virtual ~ContentPrincipalJSONHandler() = default;

  virtual bool startObject() override;

  using PrincipalJSONHandlerShared::propertyName;
  virtual bool propertyName(const JS::Latin1Char* name, size_t length) override;

  virtual bool endObject() override;

  virtual bool startArray() override {
    NS_WARNING("Unexpected array value");
    mState = State::Error;
    return false;
  }
  virtual bool endArray() override {
    NS_WARNING("Unexpected array value");
    mState = State::Error;
    return false;
  }

  using PrincipalJSONHandlerShared::stringValue;
  virtual bool stringValue(const JS::Latin1Char* str, size_t length) override;

  bool HasAccepted() const { return mState == State::EndObject; }

 protected:
  virtual void SetErrorState() override { mState = State::Error; }

 private:
  State mState = State::Init;

  nsCOMPtr<nsIURI> mPrincipalURI;
  nsCOMPtr<nsIURI> mDomain;
  OriginAttributes mAttrs;
};

}  // namespace mozilla

#endif  // mozilla_ContentPrincipalJSONHandler_h
