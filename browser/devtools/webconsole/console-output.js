/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Heritage = require("sdk/core/heritage");
const XHTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

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
};

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
   * Holds the text-only representation of the message.
   * @type string
   */
  textContent: "",

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
   * TODO: remove this once bug 778766.
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
  Messages.BaseMessage.apply(this, arguments);
  this._url = url;
  this.textContent = "------ " + url;
  this.timestamp = timestamp;
};

Messages.NavigationMarker.prototype = Heritage.extend(Messages.BaseMessage.prototype,
{
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

    // This is going into the WebConsoleFrame object instance that owns
    // the ConsoleOutput object. The WebConsoleFrame owner is the WebConsole
    // object instance from hudservice.js.
    // TODO: move _addMessageLinkCallback() into ConsoleOutput once bug 778766
    // is fixed.
    this.output.owner._addMessageLinkCallback(urlnode, () => {
      this.output.owner.owner.openLink(this._url);
    });

    let render = Messages.BaseMessage.prototype.render.bind(this);
    render().element.appendChild(urlnode);
    this.element.classList.add("navigation-marker");
    this.element.url = this._url;
    this.element.appendChild(doc.createTextNode("\n"));

    return this;
  },
}); // Messages.NavigationMarker.prototype


function gSequenceId()
{
  return gSequenceId.n++;
}
gSequenceId.n = 0;

exports.ConsoleOutput = ConsoleOutput;
exports.Messages = Messages;
