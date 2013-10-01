/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "stable",
  "engines": {
    "Firefox": "*"
  }
};

const { Ci, Cc } = require("chrome"),
    { setTimeout } = require("./timers"),
    { emit, off } = require("./event/core"),
    { Class, obscure } = require("./core/heritage"),
    { EventTarget } = require("./event/target"),
    { ns } = require("./core/namespace"),
    { when: unload } = require("./system/unload"),
    { ignoreWindow } = require('./private-browsing/utils'),
    { getTabs, getTabContentWindow, getTabForContentWindow,
      getAllTabContentWindows } = require('./tabs/utils'),
    winUtils = require("./window/utils"),
    events = require("./system/events"),
    { iteratorSymbol, forInIterator } = require("./util/iteration");

// The selection types
const HTML = 0x01,
      TEXT = 0x02,
      DOM  = 0x03; // internal use only

// A more developer-friendly message than the caught exception when is not
// possible change a selection.
const ERR_CANNOT_CHANGE_SELECTION =
  "It isn't possible to change the selection, as there isn't currently a selection";

const selections = ns();

const Selection = Class({
  /**
   * Creates an object from which a selection can be set, get, etc. Each
   * object has an associated with a range number. Range numbers are the
   * 0-indexed counter of selection ranges as explained at
   * https://developer.mozilla.org/en/DOM/Selection.
   *
   * @param rangeNumber
   *        The zero-based range index into the selection
   */
  initialize: function initialize(rangeNumber) {
    // In order to hide the private `rangeNumber` argument from API consumers
    // while still enabling Selection getters/setters to access it, we define
    // it as non enumerable, non configurable property. While consumers still
    // may discover it they won't be able to do any harm which is good enough
    // in this case.
    Object.defineProperties(this, {
      rangeNumber: {
        enumerable: false,
        configurable: false,
        value: rangeNumber
      }
    });
  },
  get text() { return getSelection(TEXT, this.rangeNumber); },
  set text(value) { setSelection(TEXT, value, this.rangeNumber); },
  get html() { return getSelection(HTML, this.rangeNumber); },
  set html(value) { setSelection(HTML, value, this.rangeNumber); },
  get isContiguous() {

    // If there are multiple non empty ranges, the selection is definitely
    // discontiguous. It returns `false` also if there are no valid selection.
    let count = 0;
    for (let sel in selectionIterator)
      if (++count > 1)
        break;

    return count === 1;
  }
});

const selectionListener = {
  notifySelectionChanged: function (document, selection, reason) {
    if (!["SELECTALL", "KEYPRESS", "MOUSEUP"].some(function(type) reason &
      Ci.nsISelectionListener[type + "_REASON"]) || selection.toString() == "")
        return;

    this.onSelect();
  },

  onSelect: function() {
    emit(module.exports, "select");
  }
}

/**
 * Defines iterators so that discontiguous selections can be iterated.
 * Empty selections are skipped - see `safeGetRange` for further details.
 *
 * If discontiguous selections are in a text field, only the first one
 * is returned because the text field selection APIs doesn't support
 * multiple selections.
 */
function* forOfIterator() {
  let selection = getSelection(DOM);
  let count = 0;

  if (selection)
    count = selection.rangeCount || (getElementWithSelection() ? 1 : 0);

  for (let i = 0; i < count; i++) {
    let sel = Selection(i);

    if (sel.text)
      yield Selection(i);
  }
}

const selectionIteratorOptions = {
  __iterator__: forInIterator
}
selectionIteratorOptions[iteratorSymbol] = forOfIterator;
const selectionIterator = obscure(selectionIteratorOptions);

/**
 * Returns the most recent focused window.
 * if private browsing window is most recent and not supported,
 * then ignore it and return `null`, because the focused window
 * can't be targeted.
 */
function getFocusedWindow() {
  let window = winUtils.getFocusedWindow();

  return ignoreWindow(window) ? null : window;
}

/**
 * Returns the focused element in the most recent focused window
 * if private browsing window is most recent and not supported,
 * then ignore it and return `null`, because the focused element
 * can't be targeted.
 */
function getFocusedElement() {
  let element = winUtils.getFocusedElement();

  if (!element || ignoreWindow(element.ownerDocument.defaultView))
    return null;

  return element;
}

/**
 * Returns the current selection from most recent content window. Depending on
 * the specified |type|, the value returned can be a string of text, stringified
 * HTML, or a DOM selection object as described at
 * https://developer.mozilla.org/en/DOM/Selection.
 *
 * @param type
 *        Specifies the return type of the selection. Valid values are the one
 *        of the constants HTML, TEXT, or DOM.
 *
 * @param rangeNumber
 *        Specifies the zero-based range index of the returned selection.
 */
function getSelection(type, rangeNumber) {
  let window, selection;
  try {
    window = getFocusedWindow();
    selection = window.getSelection();
  }
  catch (e) {
    return null;
  }

  // Get the selected content as the specified type
  if (type == DOM) {
    return selection;
  }
  else if (type == TEXT) {
    let range = safeGetRange(selection, rangeNumber);

    if (range)
      return range.toString();

    let node = getElementWithSelection();

    if (!node)
      return null;

    return node.value.substring(node.selectionStart, node.selectionEnd);
  }
  else if (type == HTML) {
    let range = safeGetRange(selection, rangeNumber);
    // Another way, but this includes the xmlns attribute for all elements in
    // Gecko 1.9.2+ :
    // return Cc["@mozilla.org/xmlextras/xmlserializer;1"].
    //   createInstance(Ci.nsIDOMSerializer).serializeToSTring(range.
    //     cloneContents());
    if (!range)
      return null;

    let node = window.document.createElement("span");
    node.appendChild(range.cloneContents());
    return node.innerHTML;
  }

  throw new Error("Type " + type + " is unrecognized.");
}

/**
 * Sets the current selection of the most recent content document by changing
 * the existing selected text/HTML range to the specified value.
 *
 * @param val
 *        The value for the new selection
 *
 * @param rangeNumber
 *        The zero-based range index of the selection to be set
 *
 */
function setSelection(type, val, rangeNumber) {
  // Make sure we have a window context & that there is a current selection.
  // Selection cannot be set unless there is an existing selection.
  let window, selection;

  try {
    window = getFocusedWindow();
    selection = window.getSelection();
  }
  catch (e) {
    throw new Error(ERR_CANNOT_CHANGE_SELECTION);
  }

  let range = safeGetRange(selection, rangeNumber);

  if (range) {
    let fragment;

    if (type === HTML)
      fragment = range.createContextualFragment(val);
    else {
      fragment = range.createContextualFragment("");
      fragment.textContent = val;
    }

    range.deleteContents();
    range.insertNode(fragment);
  }
  else {
    let node = getElementWithSelection();

    if (!node)
      throw new Error(ERR_CANNOT_CHANGE_SELECTION);

    let { value, selectionStart, selectionEnd } = node;

    let newSelectionEnd = selectionStart + val.length;

    node.value = value.substring(0, selectionStart) +
                  val +
                  value.substring(selectionEnd, value.length);

    node.setSelectionRange(selectionStart, newSelectionEnd);
  }
}

/**
 * Returns the specified range in a selection without throwing an exception.
 *
 * @param selection
 *        A selection object as described at
 *         https://developer.mozilla.org/en/DOM/Selection
 *
 * @param [rangeNumber]
 *        Specifies the zero-based range index of the returned selection.
 *        If it's not provided the function will return the first non empty
 *        range, if any.
 */
function safeGetRange(selection, rangeNumber) {
  try {
    let { rangeCount } = selection;
    let range = null;

    if (typeof rangeNumber === "undefined")
      rangeNumber = 0;
    else
      rangeCount = rangeNumber + 1;

    for (; rangeNumber < rangeCount; rangeNumber++ ) {
      range = selection.getRangeAt(rangeNumber);

      if (range && range.toString())
        break;

      range = null;
    }

    return range;
  }
  catch (e) {
    return null;
  }
}

/**
 * Returns a reference of the DOM's active element for the window given, if it
 * supports the text field selection API and has a text selected.
 *
 * Note:
 *   we need this method because window.getSelection doesn't return a selection
 *   for text selected in a form field (see bug 85686)
 */
function getElementWithSelection() {
  let element = getFocusedElement();

  if (!element)
    return null;

  let { value, selectionStart, selectionEnd } = element;

  let hasSelection = typeof value === "string" &&
                      !isNaN(selectionStart) &&
                      !isNaN(selectionEnd) &&
                      selectionStart !== selectionEnd;

  return hasSelection ? element : null;
}

/**
 * Adds the Selection Listener to the content's window given
 */
function addSelectionListener(window) {
  let selection = window.getSelection();

  // Don't add the selection's listener more than once to the same window,
  // if the selection object is the same
  if ("selection" in selections(window) && selections(window).selection === selection)
    return;

  // We ensure that the current selection is an instance of
  // `nsISelectionPrivate` before working on it, in case is `null`.
  //
  // If it's `null` it's likely too early to add the listener, and we demand
  // that operation to `document-shown` - it can easily happens for frames
  if (selection instanceof Ci.nsISelectionPrivate)
    selection.addSelectionListener(selectionListener);

  // nsISelectionListener implementation seems not fire a notification if
  // a selection is in a text field, therefore we need to add a listener to
  // window.onselect, that is fired only for text fields.
  // For consistency, we add it only when the nsISelectionListener is added.
  //
  // https://developer.mozilla.org/en/DOM/window.onselect
  window.addEventListener("select", selectionListener.onSelect, true);

  selections(window).selection = selection;
};

/**
 * Removes the Selection Listener to the content's window given
 */
function removeSelectionListener(window) {
  // Don't remove the selection's listener to a window that wasn't handled.
  if (!("selection" in selections(window)))
    return;

  let selection = window.getSelection();
  let isSameSelection = selection === selections(window).selection;

  // Before remove the listener, we ensure that the current selection is an
  // instance of `nsISelectionPrivate` (it could be `null`), and that is still
  // the selection we managed for this window (it could be detached).
  if (selection instanceof Ci.nsISelectionPrivate && isSameSelection)
    selection.removeSelectionListener(selectionListener);

  window.removeEventListener("select", selectionListener.onSelect, true);

  delete selections(window).selection;
};

function onContent(event) {
  let window = event.subject.defaultView;

  // We are not interested in documents without valid defaultView (e.g. XML)
  // that aren't in a tab (e.g. Panel); or in private windows
   if (window && getTabForContentWindow(window) && !ignoreWindow(window)) {
    addSelectionListener(window);
  }
}

// Adds Selection listener to new documents
// Note that strong reference is needed for documents that are loading slowly or
// where the server didn't close the connection (e.g. "comet").
events.on("document-element-inserted", onContent, true);

// Adds Selection listeners to existing documents
getAllTabContentWindows().forEach(addSelectionListener);

// When a document is not visible anymore the selection object is detached, and
// a new selection object is created when it becomes visible again.
// That makes the previous selection's listeners added previously totally
// useless – the listeners are not notified anymore.
// To fix that we're listening for `document-shown` event in order to add
// the listeners to the new selection object created.
//
// See bug 665386 for further details.

function onShown(event) {
  let window = event.subject.defaultView;

  // We are not interested in documents without valid defaultView.
  // For example XML documents don't have windows and we don't yet support them.
  if (!window)
    return;

  // We want to handle only the windows where we added selection's listeners
  if ("selection" in selections(window)) {
    let currentSelection = window.getSelection();
    let { selection } = selections(window);

    // If the current selection for the window given is different from the one
    // stored in the namespace, we need to add the listeners again, and replace
    // the previous selection in our list with the new one.
    //
    // Notice that we don't have to remove the listeners from the old selection,
    // because is detached. An attempt to remove the listener, will raise an
    // error (see http://mxr.mozilla.org/mozilla-central/source/layout/generic/nsSelection.cpp#5343 )
    //
    // We ensure that the current selection is an instance of
    // `nsISelectionPrivate` before working on it, in case is `null`.
    if (currentSelection instanceof Ci.nsISelectionPrivate &&
      currentSelection !== selection) {

      window.addEventListener("select", selectionListener.onSelect, true);
      currentSelection.addSelectionListener(selectionListener);
      selections(window).selection = currentSelection;
    }
  }
}

events.on("document-shown", onShown, true);

// Removes Selection listeners when the add-on is unloaded
unload(function(){
  getAllTabContentWindows().forEach(removeSelectionListener);

  events.off("document-element-inserted", onContent);
  events.off("document-shown", onShown);

  off(exports);
});

const selection = Class({
  extends: EventTarget,
  implements: [ Selection, selectionIterator ]
})();

module.exports = selection;
