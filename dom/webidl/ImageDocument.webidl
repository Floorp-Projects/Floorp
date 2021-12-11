/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[ChromeOnly, LegacyOverrideBuiltIns,
 Exposed=Window]
interface ImageDocument : HTMLDocument {
  /* Whether the image is overflowing visible area. */
  readonly attribute boolean imageIsOverflowing;

  /* Whether the image has been resized to fit visible area. */
  readonly attribute boolean imageIsResized;

  /* Resize the image to fit visible area. */
  void shrinkToFit();

  /* Restore image original size. */
  void restoreImage();
};
