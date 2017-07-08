/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/csswg/cssom/
 */

interface StyleSheet {
  [Constant]
  readonly attribute DOMString type;
  [Constant]
  readonly attribute DOMString? href;
  // Spec says "Node", but it can go null when the node gets a new
  // sheet.  That's also why it's not [Constant]
  [Pure]
  readonly attribute Node? ownerNode;
  [Pure]
  readonly attribute StyleSheet? parentStyleSheet;
  [Pure]
  readonly attribute DOMString? title;
  [Constant]
  readonly attribute MediaList media;
  [Pure]
  attribute boolean disabled;
  // If a SourceMap or X-SourceMap response header is seen, this is
  // the value.  If both are seen, SourceMap is preferred.  If neither
  // is seen, this will be an empty string.  Because this relies on
  // the HTTP response, it can change if checked before the response
  // is available -- which is why it is not [Constant].
  [ChromeOnly, Pure]
  readonly attribute DOMString sourceMapURL;
};
