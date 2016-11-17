/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dom.spec.whatwg.org/#element and
 * http://domparsing.spec.whatwg.org/ and
 * http://dev.w3.org/csswg/cssom-view/ and
 * http://www.w3.org/TR/selectors-api/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface Element : Node {
  [Constant]
  readonly attribute DOMString? namespaceURI;
  [Constant]
  readonly attribute DOMString? prefix;
  [Constant]
  readonly attribute DOMString localName;

  // Not [Constant] because it depends on which document we're in
  [Pure]
  readonly attribute DOMString tagName;

  [Pure]
           attribute DOMString id;
  [Pure]
           attribute DOMString className;
  [Constant, PutForwards=value]
  readonly attribute DOMTokenList classList;

  [SameObject]
  readonly attribute NamedNodeMap attributes;
  [Pure]
  sequence<DOMString> getAttributeNames();
  [Pure]
  DOMString? getAttribute(DOMString name);
  [Pure]
  DOMString? getAttributeNS(DOMString? namespace, DOMString localName);
  [Throws]
  void setAttribute(DOMString name, DOMString value);
  [Throws]
  void setAttributeNS(DOMString? namespace, DOMString name, DOMString value);
  [Throws]
  void removeAttribute(DOMString name);
  [Throws]
  void removeAttributeNS(DOMString? namespace, DOMString localName);
  [Pure]
  boolean hasAttribute(DOMString name);
  [Pure]
  boolean hasAttributeNS(DOMString? namespace, DOMString localName);
  [Pure]
  boolean hasAttributes();

  [Throws, Pure]
  Element? closest(DOMString selector);

  [Throws, Pure]
  boolean matches(DOMString selector);
  [Throws, Pure, BinaryName="matches"]
  boolean webkitMatchesSelector(DOMString selector);

  [Pure]
  HTMLCollection getElementsByTagName(DOMString localName);
  [Throws, Pure]
  HTMLCollection getElementsByTagNameNS(DOMString? namespace, DOMString localName);
  [Pure]
  HTMLCollection getElementsByClassName(DOMString classNames);

  [Throws, Pure]
  Element? insertAdjacentElement(DOMString where, Element element); // historical

  [Throws]
  void insertAdjacentText(DOMString where, DOMString data); // historical

  /**
   * The ratio of font-size-inflated text font size to computed font
   * size for this element. This will query the element for its primary frame,
   * and then use this to get font size inflation information about the frame.
   * This will be 1.0 if font size inflation is not enabled, and -1.0 if an
   * error occurred during the retrieval of the font size inflation.
   *
   * @note The font size inflation ratio that is returned is actually the
   *       font size inflation data for the element's _primary frame_, not the
   *       element itself, but for most purposes, this should be sufficient.
   */
  [ChromeOnly]
  readonly attribute float fontSizeInflation;

  // Mozilla specific stuff
  [Pure]
           attribute EventHandler onwheel;

  // Selectors API
  /**
   * Returns whether this element would be selected by the given selector
   * string.
   *
   * See <http://dev.w3.org/2006/webapi/selectors-api2/#matchesselector>
   */
  [Throws, Pure, BinaryName="matches"]
  boolean mozMatchesSelector(DOMString selector);

  // Pointer events methods.
  [Throws, Pref="dom.w3c_pointer_events.enabled", UnsafeInPrerendering]
  void setPointerCapture(long pointerId);

  [Throws, Pref="dom.w3c_pointer_events.enabled"]
  void releasePointerCapture(long pointerId);

  [Pref="dom.w3c_pointer_events.enabled"]
  boolean hasPointerCapture(long pointerId);

  // Proprietary extensions
  /**
   * Set this during a mousedown event to grab and retarget all mouse events
   * to this element until the mouse button is released or releaseCapture is
   * called. If retargetToElement is true, then all events are targetted at
   * this element. If false, events can also fire at descendants of this
   * element.
   * 
   */
  void setCapture(optional boolean retargetToElement = false);

  /**
   * If this element has captured the mouse, release the capture. If another
   * element has captured the mouse, this method has no effect.
   */
  void releaseCapture();

  // Mozilla extensions

  // Obsolete methods.
  Attr? getAttributeNode(DOMString name);
  [Throws]
  Attr? setAttributeNode(Attr newAttr);
  [Throws]
  Attr? removeAttributeNode(Attr oldAttr);
  Attr? getAttributeNodeNS(DOMString? namespaceURI, DOMString localName);
  [Throws]
  Attr? setAttributeNodeNS(Attr newAttr);

  [ChromeOnly]
  /**
   * Scrolls the element by (dx, dy) CSS pixels without doing any
   * layout flushing.
   */
  boolean scrollByNoFlush(long dx, long dy);

  // Support reporting of Grid properties

  /**
   * If this element has a display:grid or display:inline-grid style,
   * this property returns an object with computed values for grid
   * tracks and lines.
   */
  [ChromeOnly, Pure]
  sequence<Grid> getGridFragments();
};

// http://dev.w3.org/csswg/cssom-view/
enum ScrollLogicalPosition { "start", "end" };
dictionary ScrollIntoViewOptions : ScrollOptions {
  ScrollLogicalPosition block = "start";
};

// http://dev.w3.org/csswg/cssom-view/#extensions-to-the-element-interface
partial interface Element {
  DOMRectList getClientRects();
  DOMRect getBoundingClientRect();

  // scrolling
  void scrollIntoView(boolean top);
  void scrollIntoView(optional ScrollIntoViewOptions options);
  // None of the CSSOM attributes are [Pure], because they flush
           attribute long scrollTop;   // scroll on setting
           attribute long scrollLeft;  // scroll on setting
  readonly attribute long scrollWidth;
  readonly attribute long scrollHeight;
  
  void scroll(unrestricted double x, unrestricted double y);
  void scroll(optional ScrollToOptions options);
  void scrollTo(unrestricted double x, unrestricted double y);
  void scrollTo(optional ScrollToOptions options);
  void scrollBy(unrestricted double x, unrestricted double y);
  void scrollBy(optional ScrollToOptions options);
  // mozScrollSnap is used by chrome to perform scroll snapping after the
  // user performs actions that may affect scroll position
  // mozScrollSnap is deprecated, to be replaced by a web accessible API, such
  // as an extension to the ScrollOptions dictionary.  See bug 1137937.
  [ChromeOnly] void mozScrollSnap();

  readonly attribute long clientTop;
  readonly attribute long clientLeft;
  readonly attribute long clientWidth;
  readonly attribute long clientHeight;

  // Mozilla specific stuff
  /* The minimum/maximum offset that the element can be scrolled to
     (i.e., the value that scrollLeft/scrollTop would be clamped to if they were
     set to arbitrarily large values. */
  [ChromeOnly] readonly attribute long scrollTopMin;
               readonly attribute long scrollTopMax;
  [ChromeOnly] readonly attribute long scrollLeftMin;
               readonly attribute long scrollLeftMax;
};

// http://domparsing.spec.whatwg.org/#extensions-to-the-element-interface
partial interface Element {
  [Pure,SetterThrows,TreatNullAs=EmptyString]
  attribute DOMString innerHTML;
  [Pure,SetterThrows,TreatNullAs=EmptyString]
  attribute DOMString outerHTML;
  [Throws]
  void insertAdjacentHTML(DOMString position, DOMString text);
};

// http://www.w3.org/TR/selectors-api/#interface-definitions
partial interface Element {
  [Throws, Pure]
  Element?  querySelector(DOMString selectors);
  [Throws, Pure]
  NodeList  querySelectorAll(DOMString selectors);
};

// http://w3c.github.io/webcomponents/spec/shadow/#extensions-to-element-interface
partial interface Element {
  [Throws,Func="nsDocument::IsWebComponentsEnabled"]
  ShadowRoot createShadowRoot();
  [Func="nsDocument::IsWebComponentsEnabled"]
  NodeList getDestinationInsertionPoints();
  [Func="nsDocument::IsWebComponentsEnabled"]
  readonly attribute ShadowRoot? shadowRoot;
};

Element implements ChildNode;
Element implements NonDocumentTypeChildNode;
Element implements ParentNode;
Element implements Animatable;
Element implements GeometryUtils;

// https://fullscreen.spec.whatwg.org/#api
partial interface Element {
  [Throws, UnsafeInPrerendering, Func="nsDocument::IsUnprefixedFullscreenEnabled"]
  void requestFullscreen();
  [Throws, UnsafeInPrerendering, BinaryName="requestFullscreen"]
  void mozRequestFullScreen();
};

// https://w3c.github.io/pointerlock/#extensions-to-the-element-interface
partial interface Element {
  [UnsafeInPrerendering]
  void requestPointerLock();
  [UnsafeInPrerendering, BinaryName="requestPointerLock", Pref="pointer-lock-api.prefixed.enabled"]
  void mozRequestPointerLock();
};
