/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SubsumedPrincipalJSONHandler_h
#define mozilla_SubsumedPrincipalJSONHandler_h

#include <stddef.h>  // size_t
#include <stdint.h>  // uint32_t

#include "js/TypeDecls.h"  // JS::Latin1Char

#include "mozilla/AlreadyAddRefed.h"  // already_AddRefed
#include "mozilla/Variant.h"          // Variant
#include "mozilla/RefPtr.h"           // RefPtr

#include "nsDebug.h"  // NS_WARNING

#include "BasePrincipal.h"
#include "ContentPrincipalJSONHandler.h"
#include "NullPrincipalJSONHandler.h"
#include "SharedJSONHandler.h"

namespace mozilla {

class SubsumedPrincipalJSONHandlerTypes {
 public:
  enum class State {
    Init,

    // After the subsumed principal object's '{'.
    StartObject,

    // After the PrincipalKind property key for SystemPrincipal.
    SystemPrincipal_Key,
    // After the SystemPrincipal's inner object's '{'.
    SystemPrincipal_StartObject,
    // After the SystemPrincipal's inner object's '}'.
    SystemPrincipal_EndObject,

    // After the PrincipalKind property key for NullPrincipal or
    // ContentPrincipal, and also the entire inner object.
    // Delegates to mInnerHandler until the inner object's '}'.
    //
    // Unlike PrincipalJSONHandler, subsumed principal doesn't contain
    // ExpandedPrincipal.
    NullPrincipal_Inner,
    ContentPrincipal_Inner,

    // After the subsumed principal object's '}'.
    EndObject,

    Error,
  };

  using InnerHandlerT =
      Maybe<Variant<NullPrincipalJSONHandler, ContentPrincipalJSONHandler>>;

  static constexpr bool CanContainExpandedPrincipal = false;
};

// JSON parse handler for subsumed principal object inside ExpandedPrincipal's
// new format.
//
//                       Subsumed principal object
//                               |
//              ----------------------------------
//              |                                |
// {"2": {"0": [{"1": {"0": https://mozilla.com"}}]}}
//                |   |                         |
//                |   ---------------------------
//                |                |
//                |         inner object
//          PrincipalKind
//
// For inner object except for the system principal case, this delegates
// to NullPrincipalJSONHandler or ContentPrincipalJSONHandler.
class SubsumedPrincipalJSONHandler
    : public ContainerPrincipalJSONHandler<SubsumedPrincipalJSONHandlerTypes> {
  using State = SubsumedPrincipalJSONHandlerTypes::State;
  using InnerHandlerT = SubsumedPrincipalJSONHandlerTypes::InnerHandlerT;

 public:
  SubsumedPrincipalJSONHandler() = default;
  virtual ~SubsumedPrincipalJSONHandler() = default;

  bool HasAccepted() const { return mState == State::EndObject; }

 protected:
  virtual void SetErrorState() override { mState = State::Error; }
};

}  // namespace mozilla

#endif  // mozilla_SubsumedPrincipalJSONHandler_h
