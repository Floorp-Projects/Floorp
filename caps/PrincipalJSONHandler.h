/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PrincipalJSONHandler_h
#define mozilla_PrincipalJSONHandler_h

#include <stddef.h>  // size_t
#include <stdint.h>  // uint32_t

#include "js/JSON.h"       // JS::JSONParseHandler
#include "js/TypeDecls.h"  // JS::Latin1Char

#include "mozilla/AlreadyAddRefed.h"  // already_AddRefed
#include "mozilla/RefPtr.h"           // RefPtr
#include "mozilla/Variant.h"          // Variant

#include "nsDebug.h"          // NS_WARNING
#include "nsPrintfCString.h"  // nsPrintfCString

#include "BasePrincipal.h"
#include "ContentPrincipalJSONHandler.h"
#include "ExpandedPrincipalJSONHandler.h"
#include "NullPrincipalJSONHandler.h"
#include "SharedJSONHandler.h"

namespace mozilla {

class PrincipalJSONHandlerTypes {
 public:
  enum class State {
    Init,

    // After top-level object's '{'.
    StartObject,

    // After the PrincipalKind property key for SystemPrincipal.
    SystemPrincipal_Key,
    // After the SystemPrincipal's inner object's '{'.
    SystemPrincipal_StartObject,
    // After the SystemPrincipal's inner object's '}'.
    SystemPrincipal_EndObject,

    // After the PrincipalKind property key for NullPrincipal, ContentPrincipal,
    // or ExpandedPrincipal, and also the entire inner object.
    // Delegates to mInnerHandler until the inner object's '}'.
    NullPrincipal_Inner,
    ContentPrincipal_Inner,
    ExpandedPrincipal_Inner,

    // After top-level object's '}'.
    EndObject,

    Error,
  };

  using InnerHandlerT =
      Maybe<Variant<NullPrincipalJSONHandler, ContentPrincipalJSONHandler,
                    ExpandedPrincipalJSONHandler>>;

  static constexpr bool CanContainExpandedPrincipal = true;
};

// JSON parse handler for the top-level object for principal.
//
//                                 inner object
//                                      |
//       ---------------------------------------------------------
//       |                                                       |
// {"1": {"0": "https://mozilla.com", "2": "^privateBrowsingId=1"}}
//   |
//   |
//   |
// PrincipalKind
//
// For inner object except for the system principal case, this delegates
// to NullPrincipalJSONHandler, ContentPrincipalJSONHandler, or
// ExpandedPrincipalJSONHandler.
//
//// Null principal:
// {"0":{"0":"moz-nullprincipal:{56cac540-864d-47e7-8e25-1614eab5155e}"}}
//
// Content principal:
// {"1":{"0":"https://mozilla.com"}} -> {"0":"https://mozilla.com"}
//
// Expanded principal:
// {"2":{"0":"<base64principal1>,<base64principal2>"}}
//
// System principal:
// {"3":{}}
class PrincipalJSONHandler
    : public ContainerPrincipalJSONHandler<PrincipalJSONHandlerTypes> {
  using State = PrincipalJSONHandlerTypes::State;
  using InnerHandlerT = PrincipalJSONHandlerTypes::InnerHandlerT;

 public:
  PrincipalJSONHandler() = default;
  virtual ~PrincipalJSONHandler() = default;

  virtual void error(const char* msg, uint32_t line, uint32_t column) override {
    NS_WARNING(
        nsPrintfCString("JSON Error: %s at line %u column %u of the JSON data",
                        msg, line, column)
            .get());
  }

  already_AddRefed<BasePrincipal> Get() { return mPrincipal.forget(); }

 protected:
  virtual void SetErrorState() override { mState = State::Error; }
};

}  // namespace mozilla

#endif  // mozilla_PrincipalJSONHandler_h
