/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

dump("###################################### forms.js loaded\n");

let Ci = Components.interfaces;
let Cc = Components.classes;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
XPCOMUtils.defineLazyServiceGetter(Services, "fm",
                                   "@mozilla.org/focus-manager;1",
                                   "nsIFocusManager");

XPCOMUtils.defineLazyGetter(this, "domWindowUtils", function () {
  return content.QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIDOMWindowUtils);
});

const RESIZE_SCROLL_DELAY = 20;

let HTMLDocument = Ci.nsIDOMHTMLDocument;
let HTMLHtmlElement = Ci.nsIDOMHTMLHtmlElement;
let HTMLBodyElement = Ci.nsIDOMHTMLBodyElement;
let HTMLIFrameElement = Ci.nsIDOMHTMLIFrameElement;
let HTMLInputElement = Ci.nsIDOMHTMLInputElement;
let HTMLTextAreaElement = Ci.nsIDOMHTMLTextAreaElement;
let HTMLSelectElement = Ci.nsIDOMHTMLSelectElement;
let HTMLOptGroupElement = Ci.nsIDOMHTMLOptGroupElement;
let HTMLOptionElement = Ci.nsIDOMHTMLOptionElement;

let FormVisibility = {
  /**
   * Searches upwards in the DOM for an element that has been scrolled.
   *
   * @param {HTMLElement} node element to start search at.
   * @return {Window|HTMLElement|Null} null when none are found window/element otherwise.
   */
  findScrolled: function fv_findScrolled(node) {
    let win = node.ownerDocument.defaultView;

    while (!(node instanceof HTMLBodyElement)) {

      // We can skip elements that have not been scrolled.
      // We only care about top now remember to add the scrollLeft
      // check if we decide to care about the X axis.
      if (node.scrollTop !== 0) {
        // the element has been scrolled so we may need to adjust
        // where we think the root element is located.
        //
        // Otherwise it may seem visible but be scrolled out of the viewport
        // inside this scrollable node.
        return node;
      } else {
        // this node does not effect where we think
        // the node is even if it is scrollable it has not hidden
        // the element we are looking for.
        node = node.parentNode;
        continue;
      }
    }

    // we also care about the window this is the more
    // common case where the content is larger then
    // the viewport/screen.
    if (win.scrollMaxX || win.scrollMaxY) {
      return win;
    }

    return null;
  },

  /**
   * Checks if "top  and "bottom" points of the position is visible.
   *
   * @param {Number} top position.
   * @param {Number} height of the element.
   * @param {Number} maxHeight of the window.
   * @return {Boolean} true when visible.
   */
  yAxisVisible: function fv_yAxisVisible(top, height, maxHeight) {
    return (top > 0 && (top + height) < maxHeight);
  },

  /**
   * Searches up through the dom for scrollable elements
   * which are not currently visible (relative to the viewport).
   *
   * @param {HTMLElement} element to start search at.
   * @param {Object} pos .top, .height and .width of element.
   */
  scrollablesVisible: function fv_scrollablesVisible(element, pos) {
    while ((element = this.findScrolled(element))) {
      if (element.window && element.self === element)
        break;

      // remember getBoundingClientRect does not care
      // about scrolling only where the element starts
      // in the document.
      let offset = element.getBoundingClientRect();

      // the top of both the scrollable area and
      // the form element itself are in the same document.
      // We  adjust the "top" so if the elements coordinates
      // are relative to the viewport in the current document.
      let adjustedTop = pos.top - offset.top;

      let visible = this.yAxisVisible(
        adjustedTop,
        pos.height,
        pos.width
      );

      if (!visible)
        return false;

      element = element.parentNode;
    }

    return true;
  },

  /**
   * Verifies the element is visible in the viewport.
   * Handles scrollable areas, frames and scrollable viewport(s) (windows).
   *
   * @param {HTMLElement} element to verify.
   * @return {Boolean} true when visible.
   */
  isVisible: function fv_isVisible(element) {
    // scrollable frames can be ignored we just care about iframes...
    let rect = element.getBoundingClientRect();
    let parent = element.ownerDocument.defaultView;

    // used to calculate the inner position of frames / scrollables.
    // The intent was to use this information to scroll either up or down.
    // scrollIntoView(true) will _break_ some web content so we can't do
    // this today. If we want that functionality we need to manually scroll
    // the individual elements.
    let pos = {
      top: rect.top,
      height: rect.height,
      width: rect.width
    };

    let visible = true;

    do {
      let frame = parent.frameElement;
      visible = visible &&
                this.yAxisVisible(pos.top, pos.height, parent.innerHeight) &&
                this.scrollablesVisible(element, pos);

      // nothing we can do about this now...
      // In the future we can use this information to scroll
      // only the elements we need to at this point as we should
      // have all the details we need to figure out how to scroll.
      if (!visible)
        return false;

      if (frame) {
        let frameRect = frame.getBoundingClientRect();

        pos.top += frameRect.top + frame.clientTop;
      }
    } while (
      (parent !== parent.parent) &&
      (parent = parent.parent)
    );

    return visible;
  }
};

let FormAssistant = {
  init: function fa_init() {
    addEventListener("focus", this, true, false);
    addEventListener("blur", this, true, false);
    addEventListener("resize", this, true, false);
    addEventListener("submit", this, true, false);
    addEventListener("pagehide", this, true, false);
    addEventListener("input", this, true, false);
    addEventListener("keydown", this, true, false);
    addMessageListener("Forms:Select:Choice", this);
    addMessageListener("Forms:Input:Value", this);
    addMessageListener("Forms:Select:Blur", this);
    addMessageListener("Forms:SetSelectionRange", this);
  },

  ignoredInputTypes: new Set([
    'button', 'file', 'checkbox', 'radio', 'reset', 'submit', 'image'
  ]),

  isKeyboardOpened: false,
  selectionStart: -1,
  selectionEnd: -1,
  scrollIntoViewTimeout: null,
  _focusedElement: null,
  _documentEncoder: null,

  get focusedElement() {
    if (this._focusedElement && Cu.isDeadWrapper(this._focusedElement))
      this._focusedElement = null;

    return this._focusedElement;
  },

  set focusedElement(val) {
    this._focusedElement = val;
  },

  setFocusedElement: function fa_setFocusedElement(element) {
    if (element === this.focusedElement)
      return;

    if (this.focusedElement) {
      this.focusedElement.removeEventListener('mousedown', this);
      this.focusedElement.removeEventListener('mouseup', this);
      if (!element) {
        this.focusedElement.blur();
      }
    }

    this._documentEncoder = null;

    if (element) {
      element.addEventListener('mousedown', this);
      element.addEventListener('mouseup', this);
      if (isContentEditable(element)) {
        this._documentEncoder = getDocumentEncoder(element);
      }
    }

    this.focusedElement = element;
  },

  get documentEncoder() {
    return this._documentEncoder;
  },

  handleEvent: function fa_handleEvent(evt) {
    let target = evt.target;

    let range = null;
    switch (evt.type) {
      case "focus":
        if (!target) {
          break;
        }

        if (target instanceof HTMLDocument || target == content) {
          break;
        }

        if (isContentEditable(target)) {
          this.showKeyboard(this.getTopLevelEditable(target));
          this.updateSelection();
          break;
        }

        if (this.isFocusableElement(target)) {
          this.showKeyboard(target);
          this.updateSelection();
        }
        break;

      case "pagehide":
        // We are only interested to the pagehide event from the root document.
        if (target && target != content.document) {
          break;
        }
        // fall through
      case "blur":
      case "submit":
        if (this.focusedElement) {
          this.hideKeyboard();
          this.selectionStart = -1;
          this.selectionEnd = -1;
        }
        break;

      case 'mousedown':
        // We only listen for this event on the currently focused element.
        // When the mouse goes down, note the cursor/selection position
        this.updateSelection();
        break;

      case 'mouseup':
        // We only listen for this event on the currently focused element.
        // When the mouse goes up, see if the cursor has moved (or the
        // selection changed) since the mouse went down. If it has, we
        // need to tell the keyboard about it
        range = getSelectionRange(this.focusedElement);
        if (range[0] !== this.selectionStart ||
            range[1] !== this.selectionEnd) {
          this.sendKeyboardState(this.focusedElement);
          this.updateSelection();
        }
        break;

      case "resize":
        if (!this.isKeyboardOpened)
          return;

        if (this.scrollIntoViewTimeout) {
          content.clearTimeout(this.scrollIntoViewTimeout);
          this.scrollIntoViewTimeout = null;
        }

        // We may receive multiple resize events in quick succession, so wait
        // a bit before scrolling the input element into view.
        if (this.focusedElement) {
          this.scrollIntoViewTimeout = content.setTimeout(function () {
            this.scrollIntoViewTimeout = null;
            if (this.focusedElement && !FormVisibility.isVisible(this.focusedElement)) {
              this.focusedElement.scrollIntoView(false);
            }
          }.bind(this), RESIZE_SCROLL_DELAY);
        }
        break;

      case "input":
        // When the text content changes, notify the keyboard
        this.updateSelection();
        break;

      case "keydown":
        // We use 'setTimeout' to wait until the input element accomplishes the
        // change in selection range
        content.setTimeout(function() {
          this.updateSelection();
        }.bind(this), 0);
        break;
    }
  },

  receiveMessage: function fa_receiveMessage(msg) {
    let target = this.focusedElement;
    if (!target) {
      return;
    }

    let json = msg.json;
    switch (msg.name) {
      case "Forms:Input:Value": {
        target.value = json.value;

        let event = content.document.createEvent('HTMLEvents');
        event.initEvent('input', true, false);
        target.dispatchEvent(event);
        break;
      }

      case "Forms:Select:Choice":
        let options = target.options;
        let valueChanged = false;
        if ("index" in json) {
          if (options.selectedIndex != json.index) {
            options.selectedIndex = json.index;
            valueChanged = true;
          }
        } else if ("indexes" in json) {
          for (let i = 0; i < options.length; i++) {
            let newValue = (json.indexes.indexOf(i) != -1);
            if (options.item(i).selected != newValue) {
              options.item(i).selected = newValue;
              valueChanged = true;
            }
          }
        }

        // only fire onchange event if any selected option is changed
        if (valueChanged) {
          let event = content.document.createEvent('HTMLEvents');
          event.initEvent('change', true, true);
          target.dispatchEvent(event);
        }
        break;

      case "Forms:Select:Blur": {
        this.setFocusedElement(null);
        break;
      }

      case "Forms:SetSelectionRange":  {
        let start = json.selectionStart;
        let end =  json.selectionEnd;
        setSelectionRange(target, start, end);
        this.updateSelection();
        break;
      }
    }
  },

  showKeyboard: function fa_showKeyboard(target) {
    if (this.isKeyboardOpened)
      return;

    if (target instanceof HTMLOptionElement)
      target = target.parentNode;

    this.setFocusedElement(target);

    let kbOpened = this.sendKeyboardState(target);
    if (this.isTextInputElement(target))
      this.isKeyboardOpened = kbOpened;
  },

  hideKeyboard: function fa_hideKeyboard() {
    sendAsyncMessage("Forms:Input", { "type": "blur" });
    this.isKeyboardOpened = false;
    this.setFocusedElement(null);
  },

  isFocusableElement: function fa_isFocusableElement(element) {
    if (element instanceof HTMLSelectElement ||
        element instanceof HTMLTextAreaElement)
      return true;

    if (element instanceof HTMLOptionElement &&
        element.parentNode instanceof HTMLSelectElement)
      return true;

    return (element instanceof HTMLInputElement &&
            !this.ignoredInputTypes.has(element.type));
  },

  isTextInputElement: function fa_isTextInputElement(element) {
    return element instanceof HTMLInputElement ||
           element instanceof HTMLTextAreaElement ||
           isContentEditable(element);
  },

  getTopLevelEditable: function fa_getTopLevelEditable(element) {
    function retrieveTopLevelEditable(element) {
      // Retrieve the top element that is editable
      if (element instanceof HTMLHtmlElement)
        element = element.ownerDocument.body;
      else if (element instanceof HTMLDocument)
        element = element.body;

      while (element && !isContentEditable(element))
        element = element.parentNode;

      // Return the container frame if we are into a nested editable frame
      if (element &&
          element instanceof HTMLBodyElement &&
          element.ownerDocument.defaultView != content.document.defaultView)
        return element.ownerDocument.defaultView.frameElement;
    }

    if (element instanceof HTMLIFrameElement)
      return element;

    return retrieveTopLevelEditable(element) || element;
  },

  sendKeyboardState: function(element) {
    // FIXME/bug 729623: work around apparent bug in the IME manager
    // in gecko.
    let readonly = element.getAttribute("readonly");
    if (readonly) {
      return false;
    }

    sendAsyncMessage("Forms:Input", getJSON(element));
    return true;
  },

  // Notify when the selection range changes
  updateSelection: function fa_updateSelection() {
    let range =  getSelectionRange(this.focusedElement);
    if (range[0] != this.selectionStart || range[1] != this.selectionEnd) {
      this.selectionStart = range[0];
      this.selectionEnd = range[1];
      sendAsyncMessage("Forms:SelectionChange", {
        selectionStart: range[0],
        selectionEnd: range[1]
      });
    }
  }
};

FormAssistant.init();

function isContentEditable(element) {
  if (!element) {
    return false;
  }

  if (element.isContentEditable || element.designMode == "on")
    return true;

  // If a body element is editable and the body is the child of an
  // iframe we can assume this is an advanced HTML editor
  if (element instanceof HTMLIFrameElement &&
      element.contentDocument &&
      (element.contentDocument.body.isContentEditable ||
       element.contentDocument.designMode == "on"))
    return true;

  return element.ownerDocument && element.ownerDocument.designMode == "on";
}

function getJSON(element) {
  let type = element.type || "";
  let value = element.value || "";
  let max = element.max || "";
  let min = element.min || "";

  // Treat contenteditble element as a special text area field
  if (isContentEditable(element)) {
    type = "textarea";
    value = getContentEditableText(element);
  }

  // Until the input type=date/datetime/range have been implemented
  // let's return their real type even if the platform returns 'text'
  let attributeType = element.getAttribute("type") || "";

  if (attributeType) {
    var typeLowerCase = attributeType.toLowerCase(); 
    switch (typeLowerCase) {
      case "datetime":
      case "datetime-local":
      case "range":
        type = typeLowerCase;
        break;
    }
  }

  // Gecko has some support for @inputmode but behind a preference and
  // it is disabled by default.
  // Gaia is then using @x-inputmode has its proprietary way to set
  // inputmode for fields. This shouldn't be used outside of pre-installed
  // apps because the attribute is going to disappear as soon as a definitive
  // solution will be find.
  let inputmode = element.getAttribute('x-inputmode');
  if (inputmode) {
    inputmode = inputmode.toLowerCase();
  } else {
    inputmode = '';
  }

  let range = getSelectionRange(element);

  return {
    "type": type.toLowerCase(),
    "choices": getListForElement(element),
    "value": value,
    "inputmode": inputmode,
    "selectionStart": range[0],
    "selectionEnd": range[1],
    "max": max,
    "min": min
  };
}

function getListForElement(element) {
  if (!(element instanceof HTMLSelectElement))
    return null;

  let optionIndex = 0;
  let result = {
    "multiple": element.multiple,
    "choices": []
  };

  // Build up a flat JSON array of the choices.
  // In HTML, it's possible for select element choices to be under a
  // group header (but not recursively). We distinguish between headers
  // and entries using the boolean "list.group".
  let children = element.children;
  for (let i = 0; i < children.length; i++) {
    let child = children[i];

    if (child instanceof HTMLOptGroupElement) {
      result.choices.push({
        "group": true,
        "text": child.label || child.firstChild.data,
        "disabled": child.disabled
      });

      let subchildren = child.children;
      for (let j = 0; j < subchildren.length; j++) {
        let subchild = subchildren[j];
        result.choices.push({
          "group": false,
          "inGroup": true,
          "text": subchild.text,
          "disabled": child.disabled || subchild.disabled,
          "selected": subchild.selected,
          "optionIndex": optionIndex++
        });
      }
    } else if (child instanceof HTMLOptionElement) {
      result.choices.push({
        "group": false,
        "inGroup": false,
        "text": child.text,
        "disabled": child.disabled,
        "selected": child.selected,
        "optionIndex": optionIndex++
      });
    }
  }

  return result;
};

// Create a plain text document encode from the focused element.
function getDocumentEncoder(element) {
  let encoder = Cc["@mozilla.org/layout/documentEncoder;1?type=text/plain"]
                .createInstance(Ci.nsIDocumentEncoder);
  let flags = Ci.nsIDocumentEncoder.SkipInvisibleContent |
              Ci.nsIDocumentEncoder.OutputRaw |
              Ci.nsIDocumentEncoder.OutputLFLineBreak |
              Ci.nsIDocumentEncoder.OutputDropInvisibleBreak;
  encoder.init(element.ownerDocument, "text/plain", flags);
  return encoder;
}

// Get the visible content text of a content editable element
function getContentEditableText(element) {
  let doc = element.ownerDocument;
  let range = doc.createRange();
  range.selectNodeContents(element);
  let encoder = FormAssistant.documentEncoder;
  encoder.setRange(range);
  return encoder.encodeToString();
}

function getSelectionRange(element) {
  let start = 0;
  let end = 0;
  if (element instanceof HTMLInputElement ||
      element instanceof HTMLTextAreaElement) {
    // Get the selection range of <input> and <textarea> elements
    start = element.selectionStart;
    end = element.selectionEnd;
  } else if (isContentEditable(element)){
    // Get the selection range of contenteditable elements
    let win = element.ownerDocument.defaultView;
    let sel = win.getSelection();
    start = getContentEditableSelectionStart(element, sel);
    end = start + getContentEditableSelectionLength(element, sel);
   }
   return [start, end];
 }

function getContentEditableSelectionStart(element, selection) {
  let doc = element.ownerDocument;
  let range = doc.createRange();
  range.setStart(element, 0);
  range.setEnd(selection.anchorNode, selection.anchorOffset);
  let encoder = FormAssistant.documentEncoder;
  encoder.setRange(range);
  return encoder.encodeToString().length;
}

function getContentEditableSelectionLength(element, selection) {
  let encoder = FormAssistant.documentEncoder;
  encoder.setRange(selection.getRangeAt(0));
  return encoder.encodeToString().length;
}

function setSelectionRange(element, start, end) {
  let isPlainTextField = element instanceof HTMLInputElement ||
                        element instanceof HTMLTextAreaElement;

  // Check the parameters

  if (!isPlainTextField && !isContentEditable(element)) {
    // Skip HTMLOptionElement and HTMLSelectElement elements, as they don't
    // support the operation of setSelectionRange
    return;
  }

  let text = isPlainTextField ? element.value : getContentEditableText(element);
  let length = text.length;
  if (start < 0) {
    start = 0;
  }
  if (end > length) {
    end = length;
  }
  if (start > end) {
    start = end;
  }

  if (isPlainTextField) {
    // Set the selection range of <input> and <textarea> elements
    element.setSelectionRange(start, end, "forward");
  } else {
    // set the selection range of contenteditable elements
    let win = element.ownerDocument.defaultView;
    let sel = win.getSelection();

    // Move the caret to the start position
    sel.collapse(element, 0);
    for (let i = 0; i < start; i++) {
      sel.modify("move", "forward", "character");
    }

    while (getContentEditableSelectionStart(element, sel) < start) {
      sel.modify("move", "forward", "character");
    }

    // Extend the selection to the end position
    for (let i = start; i < end; i++) {
      sel.modify("extend", "forward", "character");
    }

    let selectionLength = end - start;
    while (getContentEditableSelectionLength(element, sel) < selectionLength) {
      sel.modify("extend", "forward", "character");
    }
  }
}

