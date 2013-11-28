/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");

loader.lazyImporter(this, "VariablesView", "resource:///modules/devtools/VariablesView.jsm");

const Heritage = require("sdk/core/heritage");
const XHTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const STRINGS_URI = "chrome://browser/locale/devtools/webconsole.properties";

const WebConsoleUtils = require("devtools/toolkit/webconsole/utils").Utils;
const l10n = new WebConsoleUtils.l10n(STRINGS_URI);

// Constants for compatibility with the Web Console output implementation before
// bug 778766.
// TODO: remove these once bug 778766 is fixed.
const COMPAT = {
  // The various categories of messages.
  CATEGORIES: {
    NETWORK: 0,
    CSS: 1,
    JS: 2,
    WEBDEV: 3,
    INPUT: 4,
    OUTPUT: 5,
    SECURITY: 6,
  },

  // The possible message severities.
  SEVERITIES: {
    ERROR: 0,
    WARNING: 1,
    INFO: 2,
    LOG: 3,
  },

  // The preference keys to use for each category/severity combination, indexed
  // first by category (rows) and then by severity (columns).
  //
  // Most of these rather idiosyncratic names are historical and predate the
  // division of message type into "category" and "severity".
  PREFERENCE_KEYS: [
    // Error        Warning       Info    Log
    [ "network",    "netwarn",    null,   "networkinfo", ],  // Network
    [ "csserror",   "cssparser",  null,   null,          ],  // CSS
    [ "exception",  "jswarn",     null,   "jslog",       ],  // JS
    [ "error",      "warn",       "info", "log",         ],  // Web Developer
    [ null,         null,         null,   null,          ],  // Input
    [ null,         null,         null,   null,          ],  // Output
    [ "secerror",   "secwarn",    null,   null,          ],  // Security
  ],

  // The fragment of a CSS class name that identifies each category.
  CATEGORY_CLASS_FRAGMENTS: [ "network", "cssparser", "exception", "console",
                              "input", "output", "security" ],

  // The fragment of a CSS class name that identifies each severity.
  SEVERITY_CLASS_FRAGMENTS: [ "error", "warn", "info", "log" ],

  // The indent of a console group in pixels.
  GROUP_INDENT: 12,
};

// A map from the console API call levels to the Web Console severities.
const CONSOLE_API_LEVELS_TO_SEVERITIES = {
  error: "error",
  warn: "warning",
  info: "info",
  log: "log",
  trace: "log",
  debug: "log",
  dir: "log",
  group: "log",
  groupCollapsed: "log",
  groupEnd: "log",
  time: "log",
  timeEnd: "log"
};

// Array of known message source URLs we need to hide from output.
const IGNORED_SOURCE_URLS = ["debugger eval code", "self-hosted"];

// The maximum length of strings to be displayed by the Web Console.
const MAX_LONG_STRING_LENGTH = 200000;


/**
 * The ConsoleOutput object is used to manage output of messages in the Web
 * Console.
 *
 * @constructor
 * @param object owner
 *        The console output owner. This usually the WebConsoleFrame instance.
 *        Any other object can be used, as long as it has the following
 *        properties and methods:
 *          - window
 *          - document
 *          - outputMessage(category, methodOrNode[, methodArguments])
 *            TODO: this is needed temporarily, until bug 778766 is fixed.
 */
function ConsoleOutput(owner)
{
  this.owner = owner;
  this._onFlushOutputMessage = this._onFlushOutputMessage.bind(this);
}

ConsoleOutput.prototype = {
  /**
   * The output container.
   * @type DOMElement
   */
  get element() {
    return this.owner.outputNode;
  },

  /**
   * The document that holds the output.
   * @type DOMDocument
   */
  get document() {
    return this.owner.document;
  },

  /**
   * The DOM window that holds the output.
   * @type Window
   */
  get window() {
    return this.owner.window;
  },

  /**
   * Getter for the debugger WebConsoleClient.
   * @type object
   */
  get webConsoleClient() {
    return this.owner.webConsoleClient;
  },

  /**
   * Add a message to output.
   *
   * @param object ...args
   *        Any number of Message objects.
   * @return this
   */
  addMessage: function(...args)
  {
    for (let msg of args) {
      msg.init(this);
      this.owner.outputMessage(msg._categoryCompat, this._onFlushOutputMessage,
                               [msg]);
    }
    return this;
  },

  /**
   * Message renderer used for compatibility with the current Web Console output
   * implementation. This method is invoked for every message object that is
   * flushed to output. The message object is initialized and rendered, then it
   * is displayed.
   *
   * TODO: remove this method once bug 778766 is fixed.
   *
   * @private
   * @param object message
   *        The message object to render.
   * @return DOMElement
   *         The message DOM element that can be added to the console output.
   */
  _onFlushOutputMessage: function(message)
  {
    return message.render().element;
  },

  /**
   * Get an array of selected messages. This list is based on the text selection
   * start and end points.
   *
   * @param number [limit]
   *        Optional limit of selected messages you want. If no value is given,
   *        all of the selected messages are returned.
   * @return array
   *         Array of DOM elements for each message that is currently selected.
   */
  getSelectedMessages: function(limit)
  {
    let selection = this.window.getSelection();
    if (selection.isCollapsed) {
      return [];
    }

    if (selection.containsNode(this.element, true)) {
      return Array.slice(this.element.children);
    }

    let anchor = this.getMessageForElement(selection.anchorNode);
    let focus = this.getMessageForElement(selection.focusNode);
    if (!anchor || !focus) {
      return [];
    }

    let start, end;
    if (anchor.timestamp > focus.timestamp) {
      start = focus;
      end = anchor;
    } else {
      start = anchor;
      end = focus;
    }

    let result = [];
    let current = start;
    while (current) {
      result.push(current);
      if (current == end || (limit && result.length == limit)) {
        break;
      }
      current = current.nextSibling;
    }
    return result;
  },

  /**
   * Find the DOM element of a message for any given descendant.
   *
   * @param DOMElement elem
   *        The element to start the search from.
   * @return DOMElement|null
   *         The DOM element of the message, if any.
   */
  getMessageForElement: function(elem)
  {
    while (elem && elem.parentNode) {
      if (elem.classList && elem.classList.contains("message")) {
        return elem;
      }
      elem = elem.parentNode;
    }
    return null;
  },

  /**
   * Select all messages.
   */
  selectAllMessages: function()
  {
    let selection = this.window.getSelection();
    selection.removeAllRanges();
    let range = this.document.createRange();
    range.selectNodeContents(this.element);
    selection.addRange(range);
  },

  /**
   * Add a message to the selection.
   *
   * @param DOMElement elem
   *        The message element to select.
   */
  selectMessage: function(elem)
  {
    let selection = this.window.getSelection();
    selection.removeAllRanges();
    let range = this.document.createRange();
    range.selectNodeContents(elem);
    selection.addRange(range);
  },

  /**
   * Open an URL in a new tab.
   * @see WebConsole.openLink() in hudservice.js
   */
  openLink: function()
  {
    this.owner.owner.openLink.apply(this.owner.owner, arguments);
  },

  /**
   * Open the variables view to inspect an object actor.
   * @see JSTerm.openVariablesView() in webconsole.js
   */
  openVariablesView: function()
  {
    this.owner.jsterm.openVariablesView.apply(this.owner.jsterm, arguments);
  },

  /**
   * Destroy this ConsoleOutput instance.
   */
  destroy: function()
  {
    this.owner = null;
  },
}; // ConsoleOutput.prototype

/**
 * Message objects container.
 * @type object
 */
let Messages = {};

/**
 * The BaseMessage object is used for all types of messages. Every kind of
 * message should use this object as its base.
 *
 * @constructor
 */
Messages.BaseMessage = function()
{
  this.widgets = new Set();
  this._onClickAnchor = this._onClickAnchor.bind(this);
  this._repeatID = { uid: gSequenceId() };
  this.textContent = "";
};

Messages.BaseMessage.prototype = {
  /**
   * Reference to the ConsoleOutput owner.
   *
   * @type object|null
   *       This is |null| if the message is not yet initialized.
   */
  output: null,

  /**
   * Reference to the parent message object, if this message is in a group or if
   * it is otherwise owned by another message.
   *
   * @type object|null
   */
  parent: null,

  /**
   * Message DOM element.
   *
   * @type DOMElement|null
   *       This is |null| if the message is not yet rendered.
   */
  element: null,

  /**
   * Tells if this message is visible or not.
   * @type boolean
   */
  get visible() {
    return this.element && this.element.parentNode;
  },

  /**
   * The owner DOM document.
   * @type DOMElement
   */
  get document() {
    return this.output.document;
  },

  /**
   * Holds the text-only representation of the message.
   * @type string
   */
  textContent: null,

  /**
   * Set of widgets included in this message.
   * @type Set
   */
  widgets: null,

  // Properties that allow compatibility with the current Web Console output
  // implementation.
  _categoryCompat: null,
  _severityCompat: null,
  _categoryNameCompat: null,
  _severityNameCompat: null,
  _filterKeyCompat: null,

  /**
   * Object that is JSON-ified and used as a non-unique ID for tracking
   * duplicate messages.
   * @private
   * @type object
   */
  _repeatID: null,

  /**
   * Initialize the message.
   *
   * @param object output
   *        The ConsoleOutput owner.
   * @param object [parent=null]
   *        Optional: a different message object that owns this instance.
   * @return this
   */
  init: function(output, parent=null)
  {
    this.output = output;
    this.parent = parent;
    return this;
  },

  /**
   * Non-unique ID for this message object used for tracking duplicate messages.
   * Different message kinds can identify themselves based their own criteria.
   *
   * @return string
   */
  getRepeatID: function()
  {
    return JSON.stringify(this._repeatID);
  },

  /**
   * Render the message. After this method is invoked the |element| property
   * will point to the DOM element of this message.
   * @return this
   */
  render: function()
  {
    if (!this.element) {
      this.element = this._renderCompat();
    }
    return this;
  },

  /**
   * Prepare the message container for the Web Console, such that it is
   * compatible with the current implementation.
   * TODO: remove this once bug 778766 is fixed.
   *
   * @private
   * @return Element
   *         The DOM element that wraps the message.
   */
  _renderCompat: function()
  {
    let doc = this.output.document;
    let container = doc.createElementNS(XHTML_NS, "div");
    container.id = "console-msg-" + gSequenceId();
    container.className = "message";
    container.category = this._categoryCompat;
    container.severity = this._severityCompat;
    container.setAttribute("category", this._categoryNameCompat);
    container.setAttribute("severity", this._severityNameCompat);
    container.setAttribute("filter", this._filterKeyCompat);
    container.clipboardText = this.textContent;
    container.timestamp = this.timestamp;
    container._messageObject = this;

    return container;
  },

  /**
   * Add a click callback to a given DOM element.
   *
   * @private
   * @param Element element
   *        The DOM element to which you want to add a click event handler.
   * @param function [callback=this._onClickAnchor]
   *        Optional click event handler. The default event handler is
   *        |this._onClickAnchor|.
   */
  _addLinkCallback: function(element, callback = this._onClickAnchor)
  {
    // This is going into the WebConsoleFrame object instance that owns
    // the ConsoleOutput object. The WebConsoleFrame owner is the WebConsole
    // object instance from hudservice.js.
    // TODO: move _addMessageLinkCallback() into ConsoleOutput once bug 778766
    // is fixed.
    this.output.owner._addMessageLinkCallback(element, callback);
  },

  /**
   * The default |click| event handler for links in the output. This function
   * opens the anchor's link in a new tab.
   *
   * @private
   * @param Event event
   *        The DOM event that invoked this function.
   */
  _onClickAnchor: function(event)
  {
    this.output.openLink(event.target.href);
  },
}; // Messages.BaseMessage.prototype


/**
 * The NavigationMarker is used to show a page load event.
 *
 * @constructor
 * @extends Messages.BaseMessage
 * @param string url
 *        The URL to display.
 * @param number timestamp
 *        The message date and time, milliseconds elapsed since 1 January 1970
 *        00:00:00 UTC.
 */
Messages.NavigationMarker = function(url, timestamp)
{
  Messages.BaseMessage.call(this);
  this._url = url;
  this.textContent = "------ " + url;
  this.timestamp = timestamp;
};

Messages.NavigationMarker.prototype = Heritage.extend(Messages.BaseMessage.prototype,
{
  /**
   * The address of the loading page.
   * @private
   * @type string
   */
  _url: null,

  /**
   * Message timestamp.
   *
   * @type number
   *       Milliseconds elapsed since 1 January 1970 00:00:00 UTC.
   */
  timestamp: 0,

  _categoryCompat: COMPAT.CATEGORIES.NETWORK,
  _severityCompat: COMPAT.SEVERITIES.LOG,
  _categoryNameCompat: "network",
  _severityNameCompat: "info",
  _filterKeyCompat: "networkinfo",

  /**
   * Prepare the DOM element for this message.
   * @return this
   */
  render: function()
  {
    if (this.element) {
      return this;
    }

    let url = this._url;
    let pos = url.indexOf("?");
    if (pos > -1) {
      url = url.substr(0, pos);
    }

    let doc = this.output.document;
    let urlnode = doc.createElementNS(XHTML_NS, "a");
    urlnode.className = "url";
    urlnode.textContent = url;
    urlnode.title = this._url;
    urlnode.href = this._url;
    urlnode.draggable = false;
    this._addLinkCallback(urlnode);

    let render = Messages.BaseMessage.prototype.render.bind(this);
    render().element.appendChild(urlnode);
    this.element.classList.add("navigation-marker");
    this.element.url = this._url;
    this.element.appendChild(doc.createTextNode("\n"));

    return this;
  },
}); // Messages.NavigationMarker.prototype


/**
 * The Simple message is used to show any basic message in the Web Console.
 *
 * @constructor
 * @extends Messages.BaseMessage
 * @param string|Node|function message
 *        The message to display.
 * @param object [options]
 *        Options for this message:
 *        - category: (string) category that this message belongs to. Defaults
 *        to no category.
 *        - severity: (string) severity of the message. Defaults to no severity.
 *        - timestamp: (number) date and time when the message was recorded.
 *        Defaults to |Date.now()|.
 *        - link: (string) if provided, the message will be wrapped in an anchor
 *        pointing to the given URL here.
 *        - linkCallback: (function) if provided, the message will be wrapped in
 *        an anchor. The |linkCallback| function will be added as click event
 *        handler.
 *        - location: object that tells the message source: url, line, column
 *        and lineText.
 *        - className: (string) additional element class names for styling
 *        purposes.
 *        - private: (boolean) mark this as a private message.
 *        - filterDuplicates: (boolean) true if you do want this message to be
 *        filtered as a potential duplicate message, false otherwise.
 */
Messages.Simple = function(message, options = {})
{
  Messages.BaseMessage.call(this);

  this.category = options.category;
  this.severity = options.severity;
  this.location = options.location;
  this.timestamp = options.timestamp || Date.now();
  this.private = !!options.private;

  this._message = message;
  this._className = options.className;
  this._link = options.link;
  this._linkCallback = options.linkCallback;
  this._filterDuplicates = options.filterDuplicates;
};

Messages.Simple.prototype = Heritage.extend(Messages.BaseMessage.prototype,
{
  /**
   * Message category.
   * @type string
   */
  category: null,

  /**
   * Message severity.
   * @type string
   */
  severity: null,

  /**
   * Message source location. Properties: url, line, column, lineText.
   * @type object
   */
  location: null,

  /**
   * Tells if this message comes from a private browsing context.
   * @type boolean
   */
  private: false,

  /**
   * Custom class name for the DOM element of the message.
   * @private
   * @type string
   */
  _className: null,

  /**
   * Message link - if this message is clicked then this URL opens in a new tab.
   * @private
   * @type string
   */
  _link: null,

  /**
   * Message click event handler.
   * @private
   * @type function
   */
  _linkCallback: null,

  /**
   * Tells if this message should be checked if it is a duplicate of another
   * message or not.
   */
  _filterDuplicates: false,

  /**
   * The raw message displayed by this Message object. This can be a function,
   * DOM node or a string.
   *
   * @private
   * @type mixed
   */
  _message: null,

  _afterMessage: null,
  _objectActors: null,
  _groupDepthCompat: 0,

  /**
   * Message timestamp.
   *
   * @type number
   *       Milliseconds elapsed since 1 January 1970 00:00:00 UTC.
   */
  timestamp: 0,

  get _categoryCompat() {
    return this.category ?
           COMPAT.CATEGORIES[this.category.toUpperCase()] : null;
  },
  get _severityCompat() {
    return this.severity ?
           COMPAT.SEVERITIES[this.severity.toUpperCase()] : null;
  },
  get _categoryNameCompat() {
    return this.category ?
           COMPAT.CATEGORY_CLASS_FRAGMENTS[this._categoryCompat] : null;
  },
  get _severityNameCompat() {
    return this.severity ?
           COMPAT.SEVERITY_CLASS_FRAGMENTS[this._severityCompat] : null;
  },

  get _filterKeyCompat() {
    return this._categoryCompat !== null && this._severityCompat !== null ?
           COMPAT.PREFERENCE_KEYS[this._categoryCompat][this._severityCompat] :
           null;
  },

  init: function()
  {
    Messages.BaseMessage.prototype.init.apply(this, arguments);
    this._groupDepthCompat = this.output.owner.groupDepth;
    this._initRepeatID();
    return this;
  },

  _initRepeatID: function()
  {
    if (!this._filterDuplicates) {
      return;
    }

    // Add the properties we care about for identifying duplicate messages.
    let rid = this._repeatID;
    delete rid.uid;

    rid.category = this.category;
    rid.severity = this.severity;
    rid.private = this.private;
    rid.location = this.location;
    rid.link = this._link;
    rid.linkCallback = this._linkCallback + "";
    rid.className = this._className;
    rid.groupDepth = this._groupDepthCompat;
    rid.textContent = "";
  },

  getRepeatID: function()
  {
    // No point in returning a string that includes other properties when there
    // is a unique ID.
    if (this._repeatID.uid) {
      return JSON.stringify({ uid: this._repeatID.uid });
    }

    return JSON.stringify(this._repeatID);
  },

  render: function()
  {
    if (this.element) {
      return this;
    }

    let timestamp = new Widgets.MessageTimestamp(this, this.timestamp).render();

    let icon = this.document.createElementNS(XHTML_NS, "span");
    icon.className = "icon";

    let body = this._renderBody();
    this._repeatID.textContent += "|" + body.textContent;

    let repeatNode = this._renderRepeatNode();
    let location = this._renderLocation();

    Messages.BaseMessage.prototype.render.call(this);
    if (this._className) {
      this.element.className += " " + this._className;
    }

    this.element.appendChild(timestamp.element);
    this.element.appendChild(icon);
    this.element.appendChild(body);
    if (repeatNode) {
      this.element.appendChild(repeatNode);
    }
    if (location) {
      this.element.appendChild(location);
    }
    this.element.appendChild(this.document.createTextNode("\n"));

    this.element.clipboardText = this.element.textContent;

    if (this.private) {
      this.element.setAttribute("private", true);
    }

    if (this._afterMessage) {
      this.element._outputAfterNode = this._afterMessage.element;
      this._afterMessage = null;
    }

    // TODO: handle object releasing in a more elegant way once all console
    // messages use the new API - bug 778766.
    this.element._objectActors = this._objectActors;
    this._objectActors = null;

    return this;
  },

  /**
   * Render the message body DOM element.
   * @private
   * @return Element
   */
  _renderBody: function()
  {
    let body = this.document.createElementNS(XHTML_NS, "span");
    body.className = "body devtools-monospace";

    let anchor, container = body;
    if (this._link || this._linkCallback) {
      container = anchor = this.document.createElementNS(XHTML_NS, "a");
      anchor.href = this._link || "#";
      anchor.draggable = false;
      this._addLinkCallback(anchor, this._linkCallback);
      body.appendChild(anchor);
    }

    if (typeof this._message == "function") {
      container.appendChild(this._message(this));
    } else if (this._message instanceof Ci.nsIDOMNode) {
      container.appendChild(this._message);
    } else {
      container.textContent = this._message;
    }

    return body;
  },

  /**
   * Render the repeat bubble DOM element part of the message.
   * @private
   * @return Element
   */
  _renderRepeatNode: function()
  {
    if (!this._filterDuplicates) {
      return null;
    }

    let repeatNode = this.document.createElementNS(XHTML_NS, "span");
    repeatNode.setAttribute("value", "1");
    repeatNode.className = "repeats";
    repeatNode.textContent = 1;
    repeatNode._uid = this.getRepeatID();
    return repeatNode;
  },

  /**
   * Render the message source location DOM element.
   * @private
   * @return Element
   */
  _renderLocation: function()
  {
    if (!this.location) {
      return null;
    }

    let {url, line} = this.location;
    if (IGNORED_SOURCE_URLS.indexOf(url) != -1) {
      return null;
    }

    // The ConsoleOutput owner is a WebConsoleFrame instance from webconsole.js.
    // TODO: move createLocationNode() into this file when bug 778766 is fixed.
    return this.output.owner.createLocationNode(url, line);
  },
}); // Messages.Simple.prototype


/**
 * The Extended message.
 *
 * @constructor
 * @extends Messages.Simple
 * @param array messagePieces
 *        The message to display given as an array of elements. Each array
 *        element can be a DOM node, function, ObjectActor, LongString or
 *        a string.
 * @param object [options]
 *        Options for rendering this message:
 *        - quoteStrings: boolean that tells if you want strings to be wrapped
 *        in quotes or not.
 */
Messages.Extended = function(messagePieces, options = {})
{
  Messages.Simple.call(this, null, options);

  this._messagePieces = messagePieces;

  if ("quoteStrings" in options) {
    this._quoteStrings = options.quoteStrings;
  }

  this._repeatID.quoteStrings = this._quoteStrings;
  this._repeatID.messagePieces = messagePieces + "";
  this._repeatID.actors = new Set(); // using a set to avoid duplicates
};

Messages.Extended.prototype = Heritage.extend(Messages.Simple.prototype,
{
  /**
   * The message pieces displayed by this message instance.
   * @private
   * @type array
   */
  _messagePieces: null,

  /**
   * Boolean that tells if the strings displayed in this message are wrapped.
   * @private
   * @type boolean
   */
  _quoteStrings: true,

  getRepeatID: function()
  {
    if (this._repeatID.uid) {
      return JSON.stringify({ uid: this._repeatID.uid });
    }

    // Sets are not stringified correctly. Temporarily switching to an array.
    let actors = this._repeatID.actors;
    this._repeatID.actors = [...actors];
    let result = JSON.stringify(this._repeatID);
    this._repeatID.actors = actors;
    return result;
  },

  render: function()
  {
    let result = this.document.createDocumentFragment();

    for (let i = 0; i < this._messagePieces.length; i++) {
      let separator = i > 0 ? this._renderBodyPieceSeparator() : null;
      if (separator) {
        result.appendChild(separator);
      }

      let piece = this._messagePieces[i];
      result.appendChild(this._renderBodyPiece(piece));
    }

    this._message = result;
    this._messagePieces = null;
    return Messages.Simple.prototype.render.call(this);
  },

  /**
   * Render the separator between the pieces of the message.
   *
   * @private
   * @return Element
   */
  _renderBodyPieceSeparator: function() { return null; },

  /**
   * Render one piece/element of the message array.
   *
   * @private
   * @param mixed piece
   *        Message element to display - this can be a LongString, ObjectActor,
   *        DOM node or a function to invoke.
   * @return Element
   */
  _renderBodyPiece: function(piece)
  {
    if (piece instanceof Ci.nsIDOMNode) {
      return piece;
    }
    if (typeof piece == "function") {
      return piece(this);
    }

    let isPrimitive = VariablesView.isPrimitive({ value: piece });
    let isActorGrip = WebConsoleUtils.isActorGrip(piece);

    if (isActorGrip) {
      this._repeatID.actors.add(piece.actor);

      if (!isPrimitive) {
        let widget = new Widgets.JSObject(this, piece).render();
        return widget.element;
      }
      if (piece.type == "longString") {
        let widget = new Widgets.LongString(this, piece).render();
        return widget.element;
      }
    }

    let result = this.document.createDocumentFragment();
    if (!isPrimitive || (!this._quoteStrings && typeof piece == "string")) {
      result.textContent = piece;
    } else {
      result.textContent = VariablesView.getString(piece);
    }

    return result;
  },
}); // Messages.Extended.prototype



/**
 * The JavaScriptEvalOutput message.
 *
 * @constructor
 * @extends Messages.Extended
 * @param object evalResponse
 *        The evaluation response packet received from the server.
 * @param string [errorMessage]
 *        Optional error message to display.
 */
Messages.JavaScriptEvalOutput = function(evalResponse, errorMessage)
{
  let severity = "log", msg, quoteStrings = true;

  if (errorMessage) {
    severity = "error";
    msg = errorMessage;
    quoteStrings = false;
  } else {
    msg = evalResponse.result;
  }

  let options = {
    timestamp: evalResponse.timestamp,
    category: "output",
    severity: severity,
    quoteStrings: quoteStrings,
  };
  Messages.Extended.call(this, [msg], options);
};

Messages.JavaScriptEvalOutput.prototype = Messages.Extended.prototype;

/**
 * The ConsoleGeneric message is used for console API calls.
 *
 * @constructor
 * @extends Messages.Extended
 * @param object packet
 *        The Console API call packet received from the server.
 */
Messages.ConsoleGeneric = function(packet)
{
  let options = {
    timestamp: packet.timeStamp,
    category: "webdev",
    severity: CONSOLE_API_LEVELS_TO_SEVERITIES[packet.level],
    private: packet.private,
    filterDuplicates: true,
    location: {
      url: packet.filename,
      line: packet.lineNumber,
    },
  };
  Messages.Extended.call(this, packet.arguments, options);
  this._repeatID.consoleApiLevel = packet.level;
};

Messages.ConsoleGeneric.prototype = Heritage.extend(Messages.Extended.prototype,
{
  _renderBodyPieceSeparator: function()
  {
    return this.document.createTextNode(" ");
  },
}); // Messages.ConsoleGeneric.prototype


let Widgets = {};

/**
 * The base widget class.
 *
 * @constructor
 * @param object message
 *        The owning message.
 */
Widgets.BaseWidget = function(message)
{
  this.message = message;
};

Widgets.BaseWidget.prototype = {
  /**
   * The owning message object.
   * @type object
   */
  message: null,

  /**
   * The DOM element of the rendered widget.
   * @type Element
   */
  element: null,

  /**
   * Getter for the DOM document that holds the output.
   * @type Document
   */
  get document() {
    return this.message.document;
  },

  /**
   * The ConsoleOutput instance that owns this widget instance.
   */
  get output() {
    return this.message.output;
  },

  /**
   * Render the widget DOM element.
   * @return this
   */
  render: function() { },

  /**
   * Destroy this widget instance.
   */
  destroy: function() { },
};

/**
 * The timestamp widget.
 *
 * @constructor
 * @param object message
 *        The owning message.
 * @param number timestamp
 *        The UNIX timestamp to display.
 */
Widgets.MessageTimestamp = function(message, timestamp)
{
  Widgets.BaseWidget.call(this, message);
  this.timestamp = timestamp;
};

Widgets.MessageTimestamp.prototype = Heritage.extend(Widgets.BaseWidget.prototype,
{
  /**
   * The UNIX timestamp.
   * @type number
   */
  timestamp: 0,

  render: function()
  {
    if (this.element) {
      return this;
    }

    this.element = this.document.createElementNS(XHTML_NS, "span");
    this.element.className = "timestamp devtools-monospace";
    this.element.textContent = l10n.timestampString(this.timestamp) + " ";

    // Apply the current group by indenting appropriately.
    // TODO: remove this once bug 778766 is fixed.
    this.element.style.marginRight = this.message._groupDepthCompat *
                                     COMPAT.GROUP_INDENT + "px";

    return this;
  },
}); // Widgets.MessageTimestamp.prototype


/**
 * The JavaScript object widget.
 *
 * @constructor
 * @param object message
 *        The owning message.
 * @param object objectActor
 *        The ObjectActor to display.
 */
Widgets.JSObject = function(message, objectActor)
{
  Widgets.BaseWidget.call(this, message);
  this.objectActor = objectActor;
  this._onClick = this._onClick.bind(this);
};

Widgets.JSObject.prototype = Heritage.extend(Widgets.BaseWidget.prototype,
{
  /**
   * The ObjectActor displayed by the widget.
   * @type object
   */
  objectActor: null,

  render: function()
  {
    if (this.element) {
      return this;
    }

    let anchor = this.element = this.document.createElementNS(XHTML_NS, "a");
    anchor.href = "#";
    anchor.draggable = false;
    anchor.textContent = VariablesView.getString(this.objectActor);
    this.message._addLinkCallback(anchor, this._onClick);

    return this;
  },

  /**
   * The click event handler for objects shown inline.
   * @private
   */
  _onClick: function()
  {
    this.output.openVariablesView({
      label: this.element.textContent,
      objectActor: this.objectActor,
      autofocus: true,
    });
  },
}); // Widgets.JSObject.prototype

/**
 * The long string widget.
 *
 * @constructor
 * @param object message
 *        The owning message.
 * @param object longStringActor
 *        The LongStringActor to display.
 */
Widgets.LongString = function(message, longStringActor)
{
  Widgets.BaseWidget.call(this, message);
  this.longStringActor = longStringActor;
  this._onClick = this._onClick.bind(this);
  this._onSubstring = this._onSubstring.bind(this);
};

Widgets.LongString.prototype = Heritage.extend(Widgets.BaseWidget.prototype,
{
  /**
   * The LongStringActor displayed by the widget.
   * @type object
   */
  longStringActor: null,

  render: function()
  {
    if (this.element) {
      return this;
    }

    let result = this.element = this.document.createElementNS(XHTML_NS, "span");
    result.className = "longString";
    this._renderString(this.longStringActor.initial);
    result.appendChild(this._renderEllipsis());

    return this;
  },

  /**
   * Render the long string in the widget element.
   * @private
   * @param string str
   *        The string to display.
   */
  _renderString: function(str)
  {
    if (this.message._quoteStrings) {
      this.element.textContent = VariablesView.getString(str);
    } else {
      this.element.textContent = str;
    }
  },

  /**
   * Render the anchor ellipsis that allows the user to expand the long string.
   *
   * @private
   * @return Element
   */
  _renderEllipsis: function()
  {
    let ellipsis = this.document.createElementNS(XHTML_NS, "a");
    ellipsis.className = "longStringEllipsis";
    ellipsis.textContent = l10n.getStr("longStringEllipsis");
    ellipsis.href = "#";
    ellipsis.draggable = false;
    this.message._addLinkCallback(ellipsis, this._onClick);

    return ellipsis;
  },

  /**
   * The click event handler for the ellipsis shown after the short string. This
   * function expands the element to show the full string.
   * @private
   */
  _onClick: function()
  {
    let longString = this.output.webConsoleClient.longString(this.longStringActor);
    let toIndex = Math.min(longString.length, MAX_LONG_STRING_LENGTH);

    longString.substring(longString.initial.length, toIndex, this._onSubstring);
  },

  /**
   * The longString substring response callback.
   *
   * @private
   * @param object response
   *        Response packet.
   */
  _onSubstring: function(response)
  {
    if (response.error) {
      Cu.reportError("LongString substring failure: " + response.error);
      return;
    }

    this.element.lastChild.remove();
    this.element.classList.remove("longString");

    this._renderString(this.longStringActor.initial + response.substring);

    this.output.owner.emit("messages-updated", new Set([this.message.element]));

    let toIndex = Math.min(this.longStringActor.length, MAX_LONG_STRING_LENGTH);
    if (toIndex != this.longStringActor.length) {
      this._logWarningAboutStringTooLong();
    }
  },

  /**
   * Inform user that the string he tries to view is too long.
   * @private
   */
  _logWarningAboutStringTooLong: function()
  {
    let msg = new Messages.Simple(l10n.getStr("longStringTooLong"), {
      category: "output",
      severity: "warning",
    });
    this.output.addMessage(msg);
  },
}); // Widgets.LongString.prototype


function gSequenceId()
{
  return gSequenceId.n++;
}
gSequenceId.n = 0;

exports.ConsoleOutput = ConsoleOutput;
exports.Messages = Messages;
exports.Widgets = Widgets;
