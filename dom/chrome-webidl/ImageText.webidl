/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * Text that has been extracted from an image using an image recognition API.
 */
dictionary ImageText {
  /**
   * A scalar value representing how confident the text recognition model was in the
   * produced result.
   */
  required float confidence;

  /**
   * The recognized text.
   */
  required DOMString string;

  /**
   * A DOMQuad representing the bounds of the text. This can be a free-form four pointed
   * polygon, and not necessarily a square or parallelogram. It does not actually contain
   * CSSPixels, but the points are ranged 0-1 and represent a ratio of the width / height
   * of the source image.
   */
  required DOMQuad quad;
};
