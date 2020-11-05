/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PathUtils__
#define mozilla_dom_PathUtils__

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/DOMParser.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class PathUtils final {
 public:
  static void Filename(const GlobalObject&, const nsAString& aPath,
                       nsString& aResult, ErrorResult& aErr);

  static void Parent(const GlobalObject&, const nsAString& aPath,
                     nsString& aResult, ErrorResult& aErr);

  static void Join(const GlobalObject&, const Sequence<nsString>& aComponents,
                   nsString& aResult, ErrorResult& aErr);

  static void Normalize(const GlobalObject&, const nsAString& aPath,
                        nsString& aResult, ErrorResult& aErr);

  static void Split(const GlobalObject&, const nsAString& aPath,
                    nsTArray<nsString>& aResult, ErrorResult& aErr);

  static void ToFileURI(const GlobalObject&, const nsAString& aPath,
                        nsCString& aResult, ErrorResult& aErr);
};

}  // namespace dom
}  // namespace mozilla

#endif  // namespace mozilla_dom_PathUtils__
