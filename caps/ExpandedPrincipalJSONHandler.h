/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ExpandedPrincipalJSONHandler_h
#define mozilla_ExpandedPrincipalJSONHandler_h

#include <stddef.h>  // size_t
#include <stdint.h>  // uint32_t

#include "js/TypeDecls.h"  // JS::Latin1Char

#include "mozilla/AlreadyAddRefed.h"  // already_AddRefed
#include "mozilla/Maybe.h"            // Maybe
#include "mozilla/RefPtr.h"           // RefPtr

#include "nsCOMPtr.h"      // nsCOMPtr
#include "nsDebug.h"       // NS_WARNING
#include "nsIPrincipal.h"  // nsIPrincipal
#include "nsTArray.h"      // nsTArray

#include "OriginAttributes.h"
#include "ExpandedPrincipal.h"
#include "SubsumedPrincipalJSONHandler.h"
#include "SharedJSONHandler.h"

namespace mozilla {

// JSON parse handler for an inner object for ExpandedPrincipal.
//
// # Legacy format
//
//                           inner object
//                                |
//       -------------------------------------------------
//       |                                               |
// {"2": {"0": "eyIxIjp7IjAiOiJodHRwczovL2EuY29tLyJ9fQ=="}}
//   |     |                      |
//   |     ----------           Value
//   |              |
// PrincipalKind    |
//                  |
//           SerializableKeys
//
// The value is a CSV list of Base64 encoded prinipcals.
//
// # New format
//
//                           inner object
//                                |
//       -------------------------------------------
//       |                                         |
//       |               Subsumed principals       |
//       |                       |                 |
//       |     ------------------------------------|
//       |     |                                  ||
// {"2": {"0": [{"1": {"0": https://mozilla.com"}}]}}
//   |            |                  |
//   --------------                Value
//         |
//   PrincipalKind
//
// Used by PrincipalJSONHandler.
class ExpandedPrincipalJSONHandler : public PrincipalJSONHandlerShared {
  enum class State {
    Init,

    // After the inner object's '{'.
    StartObject,

    // After the property key for eSpecs.
    SpecsKey,

    // After the property key for eSuffix.
    SuffixKey,

    // After the subsumed principals array's '['.
    StartArray,

    // Subsumed principals array's item.
    // Delegates to mSubsumedHandler until the subsumed object's '}'.
    SubsumedPrincipal,

    // After the property value for eSpecs or eSuffix,
    // including after the subsumed principals array's ']'.
    AfterPropertyValue,

    // After the inner object's '}'.
    EndObject,

    Error,
  };

 public:
  ExpandedPrincipalJSONHandler() = default;
  virtual ~ExpandedPrincipalJSONHandler() = default;

  virtual bool startObject() override;

  using PrincipalJSONHandlerShared::propertyName;
  virtual bool propertyName(const JS::Latin1Char* name, size_t length) override;

  virtual bool endObject() override;

  virtual bool startArray() override;
  virtual bool endArray() override;

  using PrincipalJSONHandlerShared::stringValue;
  virtual bool stringValue(const JS::Latin1Char* str, size_t length) override;

  bool HasAccepted() const { return mState == State::EndObject; }

 protected:
  virtual void SetErrorState() override { mState = State::Error; }

 private:
  bool ProcessSubsumedResult(bool aResult);

 private:
  State mState = State::Init;

  nsTArray<nsCOMPtr<nsIPrincipal>> mAllowList;
  OriginAttributes mAttrs;
  Maybe<SubsumedPrincipalJSONHandler> mSubsumedHandler;
};

}  // namespace mozilla

#endif  // mozilla_ExpandedPrincipalJSONHandler_h
