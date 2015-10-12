/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information on this interface, please see
 * https://wiki.whatwg.org/wiki/OffscreenCanvas
 *
 * Current implementation focus on transfer canvas from main thread to worker.
 * So there are some spec doesn't implement, such as [Constructor], toBlob() and
 * transferToImageBitmap in OffscreenCanvas. Bug 1172796 will implement
 * remaining spec.
 */

[Exposed=(Window,Worker),
 Func="mozilla::dom::OffscreenCanvas::PrefEnabled"]
interface OffscreenCanvas : EventTarget {
  [Pure, SetterThrows]
  attribute unsigned long width;
  [Pure, SetterThrows]
  attribute unsigned long height;

  [Throws]
  nsISupports? getContext(DOMString contextId,
                          optional any contextOptions = null);
};

// OffscreenCanvas implements Transferable;
