/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is:
 * http://dom.spec.whatwg.org/#interface-document
 * http://www.whatwg.org/specs/web-apps/current-work/#the-document-object
 * http://dvcs.w3.org/hg/fullscreen/raw-file/tip/Overview.html#api
 * http://dvcs.w3.org/hg/pointerlock/raw-file/default/index.html#extensions-to-the-document-interface
 * http://dvcs.w3.org/hg/webperf/raw-file/tip/specs/PageVisibility/Overview.html#sec-document-interface
 * http://dev.w3.org/csswg/cssom/#extensions-to-the-document-interface
 * http://dev.w3.org/csswg/cssom-view/#extensions-to-the-document-interface
 *
 * http://mxr.mozilla.org/mozilla-central/source/dom/interfaces/core/nsIDOMDocument.idl
 */

interface Attr;
interface CDATASection;
interface Comment;
interface NodeIterator;
interface ProcessingInstruction;
interface Range;
interface StyleSheetList;
interface Touch;
interface TouchList;
interface TreeWalker;
interface WindowProxy;
interface nsISupports;

enum VisibilityState { "hidden", "visible" };

/* http://dom.spec.whatwg.org/#interface-document */
[Constructor]
interface Document : Node {
  [Throws, Constant]
  readonly attribute DOMImplementation implementation;
  readonly attribute DOMString URL;
  readonly attribute DOMString documentURI;
  readonly attribute DOMString compatMode;
  readonly attribute DOMString characterSet;
  readonly attribute DOMString contentType;

  readonly attribute DocumentType? doctype;
  readonly attribute Element? documentElement;
  HTMLCollection getElementsByTagName(DOMString localName);
  HTMLCollection getElementsByTagNameNS(DOMString? namespace, DOMString localName);
  HTMLCollection getElementsByClassName(DOMString classNames);
  Element? getElementById(DOMString elementId);

  [Creator, Throws]
  Element createElement(DOMString localName);
  [Creator, Throws]
  Element createElementNS(DOMString? namespace, DOMString qualifiedName);
  [Creator, Throws]
  DocumentFragment createDocumentFragment();
  [Creator, Throws]
  Text createTextNode(DOMString data);
  [Creator, Throws]
  Comment createComment(DOMString data);
  [Creator, Throws]
  ProcessingInstruction createProcessingInstruction(DOMString target, DOMString data);

  [Throws]
  Node importNode(Node node, optional boolean deep = true);
  [Throws]
  Node adoptNode(Node node);

  [Creator, Throws]
  Event createEvent(DOMString interface);

  [Creator, Throws]
  Range createRange();

  // NodeFilter.SHOW_ALL = 0xFFFFFFFF
  [Creator, Throws]
  NodeIterator createNodeIterator(Node root, optional unsigned long whatToShow = 0xFFFFFFFF, optional NodeFilter? filter = null);
  [Creator, Throws]
  TreeWalker createTreeWalker(Node root, optional unsigned long whatToShow = 0xFFFFFFFF, optional NodeFilter? filter = null);

  // NEW
  // No support for prepend/append yet
  // void prepend((Node or DOMString)... nodes);
  // void append((Node or DOMString)... nodes);

  // These are not in the spec, but leave them for now for backwards compat.
  // So sort of like Gecko extensions
  [Creator, Throws]
  CDATASection createCDATASection(DOMString data);
  [Creator, Throws]
  Attr createAttribute(DOMString name);
  [Creator, Throws]
  Attr createAttributeNS(DOMString? namespace, DOMString name);
  readonly attribute DOMString? inputEncoding;
/*
};

http://www.whatwg.org/specs/web-apps/current-work/#the-document-object
partial interface Document {
*/
  [PutForwards=href, Unforgeable] readonly attribute Location? location;
  //(HTML only)         attribute DOMString domain;
  readonly attribute DOMString referrer;
  //(HTML only)         attribute DOMString cookie;
  readonly attribute DOMString lastModified;
  readonly attribute DOMString readyState;

  // DOM tree accessors
  //(Not proxy yet)getter object (DOMString name);
           [SetterThrows]
           attribute DOMString title;
           [SetterThrows]
           attribute DOMString dir;
  //(HTML only)         attribute HTMLElement? body;
  //(HTML only)readonly attribute HTMLHeadElement? head;
  //(HTML only)readonly attribute HTMLCollection images;
  //(HTML only)readonly attribute HTMLCollection embeds;
  //(HTML only)readonly attribute HTMLCollection plugins;
  //(HTML only)readonly attribute HTMLCollection links;
  //(HTML only)readonly attribute HTMLCollection forms;
  //(HTML only)readonly attribute HTMLCollection scripts;
  //(HTML only)NodeList getElementsByName(DOMString elementName);
  //(HTML only)NodeList getItems(optional DOMString typeNames); // microdata
  //(Not implemented)readonly attribute DOMElementMap cssElementMap;

  // dynamic markup insertion
  //(HTML only)Document open(optional DOMString type, optional DOMString replace);
  //(HTML only)WindowProxy open(DOMString url, DOMString name, DOMString features, optional boolean replace);
  //(HTML only)void close();
  //(HTML only)void write(DOMString... text);
  //(HTML only)void writeln(DOMString... text);

  // user interaction
  readonly attribute WindowProxy? defaultView;
  readonly attribute Element? activeElement;
  [Throws]
  boolean hasFocus();
  //(HTML only)         attribute DOMString designMode;
  //(HTML only)boolean execCommand(DOMString commandId);
  //(HTML only)boolean execCommand(DOMString commandId, boolean showUI);
  //(HTML only)boolean execCommand(DOMString commandId, boolean showUI, DOMString value);
  //(HTML only)boolean queryCommandEnabled(DOMString commandId);
  //(HTML only)boolean queryCommandIndeterm(DOMString commandId);
  //(HTML only)boolean queryCommandState(DOMString commandId);
  //(HTML only)boolean queryCommandSupported(DOMString commandId);
  //(HTML only)DOMString queryCommandValue(DOMString commandId);
  //(Not implemented)readonly attribute HTMLCollection commands;

  // event handler IDL attributes
           [SetterThrows]
           attribute EventHandler onabort;
           [SetterThrows]
           attribute EventHandler onblur;
  //(Not implemented)         attribute EventHandler oncancel;
           [SetterThrows]
           attribute EventHandler oncanplay;
           [SetterThrows]
           attribute EventHandler oncanplaythrough;
           [SetterThrows]
           attribute EventHandler onchange;
           [SetterThrows]
           attribute EventHandler onclick;
  //(Not implemented)         attribute EventHandler onclose;
           [SetterThrows]
           attribute EventHandler oncontextmenu;
  //(Not implemented)         attribute EventHandler oncuechange;
           [SetterThrows]
           attribute EventHandler ondblclick;
           [SetterThrows]
           attribute EventHandler ondrag;
           [SetterThrows]
           attribute EventHandler ondragend;
           [SetterThrows]
           attribute EventHandler ondragenter;
           [SetterThrows]
           attribute EventHandler ondragleave;
           [SetterThrows]
           attribute EventHandler ondragover;
           [SetterThrows]
           attribute EventHandler ondragstart;
           [SetterThrows]
           attribute EventHandler ondrop;
           [SetterThrows]
           attribute EventHandler ondurationchange;
           [SetterThrows]
           attribute EventHandler onemptied;
           [SetterThrows]
           attribute EventHandler onended;
           [SetterThrows]
           attribute EventHandler onerror;
           [SetterThrows]
           attribute EventHandler onfocus;
           [SetterThrows]
           attribute EventHandler oninput;
           [SetterThrows]
           attribute EventHandler oninvalid;
           [SetterThrows]
           attribute EventHandler onkeydown;
           [SetterThrows]
           attribute EventHandler onkeypress;
           [SetterThrows]
           attribute EventHandler onkeyup;
           [SetterThrows]
           attribute EventHandler onload;
           [SetterThrows]
           attribute EventHandler onloadeddata;
           [SetterThrows]
           attribute EventHandler onloadedmetadata;
           [SetterThrows]
           attribute EventHandler onloadstart;
           [SetterThrows]
           attribute EventHandler onmousedown;
           [SetterThrows]
           attribute EventHandler onmousemove;
           [SetterThrows]
           attribute EventHandler onmouseout;
           [SetterThrows]
           attribute EventHandler onmouseover;
           [SetterThrows]
           attribute EventHandler onmouseup;
  //(Not implemented)         attribute EventHandler onmousewheel;
           [SetterThrows]
           attribute EventHandler onpause;
           [SetterThrows]
           attribute EventHandler onplay;
           [SetterThrows]
           attribute EventHandler onplaying;
           [SetterThrows]
           attribute EventHandler onprogress;
           [SetterThrows]
           attribute EventHandler onratechange;
           [SetterThrows]
           attribute EventHandler onreset;
           [SetterThrows]
           attribute EventHandler onscroll;
           [SetterThrows]
           attribute EventHandler onseeked;
           [SetterThrows]
           attribute EventHandler onseeking;
           [SetterThrows]
           attribute EventHandler onselect;
           [SetterThrows]
           attribute EventHandler onshow;
           [SetterThrows]
           attribute EventHandler onstalled;
           [SetterThrows]
           attribute EventHandler onsubmit;
           [SetterThrows]
           attribute EventHandler onsuspend;
           [SetterThrows]
           attribute EventHandler ontimeupdate;
           [SetterThrows]
           attribute EventHandler onvolumechange;
           [SetterThrows]
           attribute EventHandler onwaiting;

  // special event handler IDL attributes that only apply to Document objects
  [LenientThis, SetterThrows] attribute EventHandler onreadystatechange;

  // Gecko extensions?
  [LenientThis, SetterThrows] attribute EventHandler onmouseenter;
  [LenientThis, SetterThrows] attribute EventHandler onmouseleave;
  [SetterThrows] attribute EventHandler onmozfullscreenchange;
  [SetterThrows] attribute EventHandler onmozfullscreenerror;
  [SetterThrows] attribute EventHandler onmozpointerlockchange;
  [SetterThrows] attribute EventHandler onmozpointerlockerror;
  [SetterThrows] attribute EventHandler onwheel;
  [SetterThrows] attribute EventHandler oncopy;
  [SetterThrows] attribute EventHandler oncut;
  [SetterThrows] attribute EventHandler onpaste;
  [SetterThrows] attribute EventHandler onbeforescriptexecute;
  [SetterThrows] attribute EventHandler onafterscriptexecute;
  /**
   * True if this document is synthetic : stand alone image, video, audio file,
   * etc.
   */
  [ChromeOnly] readonly attribute boolean mozSyntheticDocument;
  /**
   * Returns the script element whose script is currently being processed.
   *
   * @see <https://developer.mozilla.org/en/DOM/document.currentScript>
   */
  readonly attribute Element? currentScript;
  /**
   * Release the current mouse capture if it is on an element within this
   * document.
   *
   * @see <https://developer.mozilla.org/en/DOM/document.releaseCapture>
   */
  void releaseCapture();
  /**
   * Use the given DOM element as the source image of target |-moz-element()|.
   *
   * This function introduces a new special ID (called "image element ID"),
   * which is only used by |-moz-element()|, and associates it with the given
   * DOM element.  Image elements ID's have the higher precedence than general
   * HTML id's, so if |document.mozSetImageElement(<id>, <element>)| is called,
   * |-moz-element(#<id>)| uses |<element>| as the source image even if there
   * is another element with id attribute = |<id>|.  To unregister an image
   * element ID |<id>|, call |document.mozSetImageElement(<id>, null)|.
   *
   * Example:
   * <script>
   *   canvas = document.createElement("canvas");
   *   canvas.setAttribute("width", 100);
   *   canvas.setAttribute("height", 100);
   *   // draw to canvas
   *   document.mozSetImageElement("canvasbg", canvas);
   * </script>
   * <div style="background-image: -moz-element(#canvasbg);"></div>
   *
   * @param aImageElementId an image element ID to associate with
   * |aImageElement|
   * @param aImageElement a DOM element to be used as the source image of
   * |-moz-element(#aImageElementId)|. If this is null, the function will
   * unregister the image element ID |aImageElementId|.
   *
   * @see <https://developer.mozilla.org/en/DOM/document.mozSetImageElement>
   */
  void mozSetImageElement(DOMString aImageElementId,
                          Element? aImageElement);

/*
};

http://dvcs.w3.org/hg/fullscreen/raw-file/tip/Overview.html#api
partial interface Document {
*/
  // Note: Per spec the 'S' in these two is lowercase, but the "Moz"
  // versions hve it uppercase.
  readonly attribute boolean mozFullScreenEnabled;
  [Throws]
  readonly attribute Element? mozFullScreenElement;

  //(Renamed?)void exitFullscreen();

  // Gecko-specific fullscreen bits
  readonly attribute boolean mozFullScreen;
  void mozCancelFullScreen();
/*
};

http://dvcs.w3.org/hg/pointerlock/raw-file/default/index.html#extensions-to-the-document-interface
partial interface Document {
*/
    readonly attribute Element? mozPointerLockElement;
    void mozExitPointerLock ();
/*
};

http://dvcs.w3.org/hg/webperf/raw-file/tip/specs/PageVisibility/Overview.html#sec-document-interface
partial interface Document {
*/
  readonly attribute boolean hidden;
  readonly attribute boolean mozHidden;
  readonly attribute VisibilityState visibilityState;
  readonly attribute VisibilityState mozVisibilityState;
/*
};

http://dev.w3.org/csswg/cssom/#extensions-to-the-document-interface
partial interface Document {
*/
    [Constant]
    readonly attribute StyleSheetList styleSheets;
    attribute DOMString? selectedStyleSheetSet;
    readonly attribute DOMString? lastStyleSheetSet;
    readonly attribute DOMString? preferredStyleSheetSet;
    [Constant]
    readonly attribute DOMStringList styleSheetSets;
    void enableStyleSheetsForSet (DOMString? name);
/*
};

http://dev.w3.org/csswg/cssom-view/#extensions-to-the-document-interface
partial interface Document {
*/
    Element? elementFromPoint (float x, float y);
    //(Not implemented)CaretPosition? caretPositionFromPoint (float x, float y);
/*
};

http://dvcs.w3.org/hg/undomanager/raw-file/tip/undomanager.html
partial interface Document {
*/
    [Pref="dom.undo_manager.enabled"]
    readonly attribute UndoManager? undoManager;
/*
};

http://dev.w3.org/2006/webapi/selectors-api2/#interface-definitions
partial interface Document {
*/
  [Throws]
  Element?  querySelector(DOMString selectors);
  [Throws]
  NodeList  querySelectorAll(DOMString selectors);

  //(Not implemented)Element?  find(DOMString selectors, optional (Element or sequence<Node>)? refNodes);
  //(Not implemented)NodeList  findAll(DOMString selectors, optional (Element or sequence<Node>)? refNodes);
/*};

  Mozilla extensions of various sorts
*/

  // nsIDOMDocumentXBL.  Wish we could make these [ChromeOnly], but
  // that would likely break bindings running with the page principal.
  NodeList? getAnonymousNodes(Element elt);
  Element? getAnonymousElementByAttribute(Element elt, DOMString attrName,
                                          DOMString attrValue);
  [Throws]
  void addBinding(Element elt, DOMString bindingURL);
  [Throws]
  void removeBinding(Element elt, DOMString bindingURL);
  Element? getBindingParent(Node node);
  [Throws]
  void loadBindingDocument(DOMString documentURL);

  // nsIDOMDocumentTouch
  // XXXbz I can't find the sane spec for this stuff, so just cribbing
  // from our xpidl for now.
  // XXXbz commented out for now because quickstubs can't do pref-ability
  /*
  [SetterThrows, Pref="dom.w3c_touch_events.expose"]
  attribute EventHandler ontouchstart;
  [SetterThrows, Pref="dom.w3c_touch_events.expose"]
  attribute EventHandler ontouchend;
  [SetterThrows, Pref="dom.w3c_touch_events.expose"]
  attribute EventHandler ontouchmove;
  [SetterThrows, Pref="dom.w3c_touch_events.expose"]
  attribute EventHandler ontouchenter;
  [SetterThrows, Pref="dom.w3c_touch_events.expose"]
  attribute EventHandler ontouchleave;
  [SetterThrows, Pref="dom.w3c_touch_events.expose"]
  attribute EventHandler ontouchcancel;
  [Creator, Pref="dom.w3c_touch_events.expose"]
  Touch createTouch(optional Window? view = null,
                    // Nasty hack, because we can't do EventTarget arguments yet
                    // (they would need to be non-castable, but trying to do
                    // XPConnect unwrapping with nsDOMEventTargetHelper fails).
                    // optional EventTarget? target = null,
                    optional nsISupports? target = null,
                    optional long identifier = 0,
                    optional long pageX = 0,
                    optional long pageY = 0,
                    optional long screenX = 0,
                    optional long screenY = 0,
                    optional long clientX = 0,
                    optional long clientY = 0,
                    optional long radiusX = 0,
                    optional long radiusY = 0,
                    optional float rotationAngle = 0,
                    optional float force = 0);
  // XXXbz a hack to get around the fact that we don't support variadics as
  // distinguishing arguments yet.  Once this hack is removed. we can also
  // remove the corresponding overload on nsIDocument, since Touch... and
  // sequence<Touch> look the same in the C++.
  [Creator, Pref="dom.w3c_touch_events.expose"]
  TouchList createTouchList(Touch touch, Touch... touches);
  // XXXbz and another hack for the fact that we can't usefully have optional
  // distinguishing arguments but need a working zero-arg form of
  // createTouchList().
  [Creator, Pref="dom.w3c_touch_events.expose"]
  TouchList createTouchList();
  [Creator, Pref="dom.w3c_touch_events.expose"]
  TouchList createTouchList(sequence<Touch> touches);
  */
};

Document implements XPathEvaluator;
