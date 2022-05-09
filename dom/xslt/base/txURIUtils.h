/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_URIUTILS_H
#define TRANSFRMX_URIUTILS_H

#include "txCore.h"

class nsINode;

namespace mozilla::dom {
class Document;
}  // namespace mozilla::dom

/**
 * A utility class for URI handling
 * Not yet finished, only handles file URI at this point
 **/

class URIUtils {
 public:
  /**
   * Reset the given document with the document of the source node
   */
  static void ResetWithSource(mozilla::dom::Document* aNewDoc,
                              nsINode* aSourceNode);

  /**
   * Resolves the given href argument, using the given documentBase
   * if necessary.
   * The new resolved href will be appended to the given dest String
   **/
  static void resolveHref(const nsAString& href, const nsAString& base,
                          nsAString& dest);
};  //-- URIUtils

/* */
#endif
