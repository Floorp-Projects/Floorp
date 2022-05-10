/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A struct that encapsulates a JSContex and information about
 * which binding method was called.  The idea is to automatically annotate
 * exceptions thrown via the BindingCallContext with the method name.
 */

#ifndef mozilla_dom_BindingCallContext_h
#define mozilla_dom_BindingCallContext_h

#include <utility>

#include "js/TypeDecls.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"

namespace mozilla::dom {

class MOZ_NON_TEMPORARY_CLASS MOZ_STACK_CLASS BindingCallContext {
 public:
  // aCx is allowed to be null.  If it is, the BindingCallContext should
  // generally act like a null JSContext*: test false when tested as a boolean
  // and produce nullptr when used as a JSContext*.
  //
  // aMethodDescription should be something with longer lifetime than this
  // BindingCallContext.  Most simply, a string literal.  nullptr or "" is
  // allowed if we want to not have any particular message description.  This
  // argument corresponds to the "context" string used for DOM error codes that
  // support one.  See Errors.msg and the documentation for
  // ErrorResult::MaybeSetPendingException for details on he context arg.
  BindingCallContext(JSContext* aCx, const char* aMethodDescription)
      : mCx(aCx), mDescription(aMethodDescription) {}

  ~BindingCallContext() = default;

  // Allow passing a BindingCallContext as a JSContext*, as needed.
  operator JSContext*() const { return mCx; }

  // Allow testing a BindingCallContext for falsiness, just like a
  // JSContext* could be tested.
  explicit operator bool() const { return !!mCx; }

  // Allow throwing an error message, if it has a context.
  template <dom::ErrNum errorNumber, typename... Ts>
  bool ThrowErrorMessage(Ts&&... aMessageArgs) const {
    static_assert(ErrorFormatHasContext[errorNumber],
                  "We plan to add a context; it better be expected!");
    MOZ_ASSERT(mCx);
    return dom::ThrowErrorMessage<errorNumber>(
        mCx, mDescription, std::forward<Ts>(aMessageArgs)...);
  }

 private:
  JSContext* const mCx;
  const char* const mDescription;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_BindingCallContext_h
