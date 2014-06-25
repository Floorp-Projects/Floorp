/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[OverrideBuiltins]
interface HTMLDocument : Document {
           [Throws]
           attribute DOMString? domain;
           [Throws]
           attribute DOMString cookie;
  // DOM tree accessors
  [Throws]
  getter object (DOMString name);
  [Pure, SetterThrows]
           attribute HTMLElement? body;
  [Pure]
  readonly attribute HTMLHeadElement? head;
  [Pure]
  readonly attribute HTMLCollection images;
  [Pure]
  readonly attribute HTMLCollection embeds;
  [Pure]
  readonly attribute HTMLCollection plugins;
  [Pure]
  readonly attribute HTMLCollection links;
  [Pure]
  readonly attribute HTMLCollection forms;
  [Pure]
  readonly attribute HTMLCollection scripts;
  NodeList getElementsByName(DOMString elementName);
  NodeList getItems(optional DOMString typeNames = ""); // microdata

  // dynamic markup insertion
  [Throws]
  Document open(optional DOMString type = "text/html", optional DOMString replace = "");
  [Throws]
  WindowProxy open(DOMString url, DOMString name, DOMString features, optional boolean replace = false);
  [Throws]
  void close();
  [Throws]
  void write(DOMString... text);
  [Throws]
  void writeln(DOMString... text);

           [SetterThrows]
           attribute DOMString designMode;
  [Throws]
  boolean execCommand(DOMString commandId, optional boolean showUI = false,
                      optional DOMString value = "");
  [Throws]
  boolean queryCommandEnabled(DOMString commandId);
  [Throws]
  boolean queryCommandIndeterm(DOMString commandId);
  [Throws]
  boolean queryCommandState(DOMString commandId);
  boolean queryCommandSupported(DOMString commandId);
  [Throws]
  DOMString queryCommandValue(DOMString commandId);

  [TreatNullAs=EmptyString] attribute DOMString fgColor;
  [TreatNullAs=EmptyString] attribute DOMString linkColor;
  [TreatNullAs=EmptyString] attribute DOMString vlinkColor;
  [TreatNullAs=EmptyString] attribute DOMString alinkColor;
  [TreatNullAs=EmptyString] attribute DOMString bgColor;

  [Pure]
  readonly attribute HTMLCollection anchors;
  [Pure]
  readonly attribute HTMLCollection applets;

  void clear();

  readonly attribute HTMLAllCollection all;

  // https://dvcs.w3.org/hg/editing/raw-file/tip/editing.html#selections
  [Throws]
  Selection? getSelection();

  // @deprecated These are old Netscape 4 methods. Do not use,
  //             the implementation is no-op.
  // XXXbz do we actually need these anymore?
  void                      captureEvents();
  void                      releaseEvents();
};

partial interface HTMLDocument {
  /*
   * Number of nodes that have been blocked by
   * the Safebrowsing API to prevent tracking.
   */
  [ChromeOnly, Pure]
  readonly attribute long blockedTrackingNodeCount;

  /*
   * List of nodes that have been blocked by
   * the Safebrowsing API to prevent tracking.
   */
  [ChromeOnly, Pure]
  readonly attribute NodeList blockedTrackingNodes;
};
