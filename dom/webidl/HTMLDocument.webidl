/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[OverrideBuiltins]
interface HTMLDocument : Document {
  // DOM tree accessors
  [Throws]
  getter object (DOMString name);

  readonly attribute HTMLAllCollection all;
};

partial interface HTMLDocument {
  /*
   * Number of nodes that have been blocked by the Safebrowsing API to prevent
   * tracking, cryptomining and so on. This method is for testing only.
   */
  [ChromeOnly, Pure]
  readonly attribute long blockedNodeByClassifierCount;

  /*
   * List of nodes that have been blocked by the Safebrowsing API to prevent
   * tracking, fingerprinting, cryptomining and so on. This method is for
   * testing only.
   */
  [ChromeOnly, Pure]
  readonly attribute NodeList blockedNodesByClassifier;

  [ChromeOnly]
  void userInteractionForTesting();
};
