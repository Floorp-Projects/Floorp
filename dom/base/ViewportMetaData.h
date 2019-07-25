/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_VIEWPORT_META_DATA_H_
#define DOM_VIEWPORT_META_DATA_H_

#include "nsAString.h"

namespace mozilla {

namespace dom {
class Document;

struct ViewportMetaData {
  /* Process viewport META data. This gives us information for the scale
   * and zoom of a page on mobile devices. We stick the information in
   * the document header and use it later on after rendering.
   *
   * See Bug #436083
   */
  static void ProcessViewportInfo(Document* aDocument,
                                  const nsAString& viewportInfo);
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_VIEWPORT_META_DATA_H_
