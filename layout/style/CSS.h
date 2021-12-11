/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object holding utility CSS functions */

#ifndef mozilla_dom_CSS_h_
#define mozilla_dom_CSS_h_

#include "mozilla/Attributes.h"
#include "mozilla/Preferences.h"

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;

class CSS {
 private:
  CSS() = delete;

 public:
  static bool Supports(const GlobalObject&, const nsACString& aProperty,
                       const nsACString& aValue);

  static bool Supports(const GlobalObject&, const nsACString& aDeclaration);

  static void Escape(const GlobalObject&, const nsAString& aIdent,
                     nsAString& aReturn);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_CSS_h_
