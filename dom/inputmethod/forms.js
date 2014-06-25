/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
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
// In content editable node, when there are hidden elements such as <br>, it
// may need more than one (usually less than 3 times) move/extend operations
// to change the selection range. If we cannot change the selection range
// with more than 20 opertations, we are likely being blocked and cannot change
// the selection range any more.
const MAX_BLOCKED_COUNT = 20;

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
    addEventListener("beforeunload", this, true, false);
    addEventListener("input", this, true, false);
    addEventListener("keydown", this, true, false);
    addEventListener("keyup", this, true, false);
    addMessageListener("Forms:Select:Choice", this);
    addMessageListener("Forms:Input:Value", this);
    addMessageListener("Forms:Select:Blur", this);
    addMessageListener("Forms:SetSelectionRange", this);
    addMessageListener("Forms:ReplaceSurroundingText", this);
    addMessageListener("Forms:GetText", this);
    addMessageListener("Forms:Input:SendKey", this);
    addMessageListener("Forms:GetContext", this);
    addMessageListener("Forms:SetComposition", this);
    addMessageListener("Forms:EndComposition", this);
  },

  ignoredInputTypes: new Set([
    'button', 'file', 'checkbox', 'radio', 'reset', 'submit', 'image',
    'range'
  ]),

  isKeyboardOpened: false,
  selectionStart: -1,
  selectionEnd: -1,
  textBeforeCursor: "",
  textAfterCursor: "",
  scrollIntoViewTimeout: null,
  _focusedElement: null,
  _focusCounter: 0, // up one for every time we focus a new element
  _observer: null,
  _documentEncoder: null,
  _editor: null,
  _editing: false,

  get focusedElement() {
    if (this._focusedElement && Cu.isDeadWrapper(this._focusedElement))
      this._focusedElement = null;

    return this._focusedElement;
  },

  set focusedElement(val) {
    this._focusCounter++;
    this._focusedElement = val;
  },

  setFocusedElement: function fa_setFocusedElement(element) {
    let self = this;

    if (element === this.focusedElement)
      return;

    if (this.focusedElement) {
      this.focusedElement.removeEventListener('mousedown', this);
      this.focusedElement.removeEventListener('mouseup', this);
      this.focusedElement.removeEventListener('compositionend', this);
      if (this._observer) {
        this._observer.disconnect();
        this._observer = null;
      }
      if (!element) {
        this.focusedElement.blur();
      }
    }

    this._documentEncoder = null;
    if (this._editor) {
      // When the nsIFrame of the input element is reconstructed by
      // CSS restyling, the editor observers are removed. Catch
      // [nsIEditor.removeEditorObserver] failure exception if that
      // happens.
      try {
        this._editor.removeEditorObserver(this);
      } catch (e) {}
      this._editor = null;
    }

    if (element) {
      element.addEventListener('mousedown', this);
      element.addEventListener('mouseup', this);
      element.addEventListener('compositionend', this);
      if (isContentEditable(element)) {
        this._documentEncoder = getDocumentEncoder(element);
      }
      this._editor = getPlaintextEditor(element);
      if (this._editor) {
        // Add a nsIEditorObserver to monitor the text content of the focused
        // element.
        this._editor.addEditorObserver(this);
      }

      // If our focusedElement is removed from DOM we want to handle it properly
      let MutationObserver = element.ownerDocument.defaultView.MutationObserver;
      this._observer = new MutationObserver(function(mutations) {
        var del = [].some.call(mutations, function(m) {
          return [].some.call(m.removedNodes, function(n) {
            return n.contains(element);
          });
        });
        if (del && element === self.focusedElement) {
          // item was deleted, fake a blur so all state gets set correctly
          self.handleEvent({ target: element, type: "blur" });
        }
      });

      this._observer.observe(element.ownerDocument.body, {
        childList: true,
        subtree: true
      });
    }

    this.focusedElement = element;
  },

  get documentEncoder() {
    return this._documentEncoder;
  },

  // Get the nsIPlaintextEditor object of current input field.
  get editor() {
    return this._editor;
  },

  // Implements nsIEditorObserver get notification when the text content of
  // current input field has changed.
  EditAction: function fa_editAction() {
    if (this._editing) {
      return;
    }
    this.sendKeyboardState(this.focusedElement);
  },

  handleEvent: function fa_handleEvent(evt) {
    let target = evt.target;

    let range = null;
    switch (evt.type) {
      case "focus":
        if (!target) {
          break;
        }

        // Focusing on Window, Document or iFrame should focus body
        if (target instanceof HTMLHtmlElement) {
          target = target.document.body;
        } else if (target instanceof HTMLDocument) {
          target = target.body;
        } else if (target instanceof HTMLIFrameElement) {
          target = target.contentDocument ? target.contentDocument.body
                                          : null;
        }

        if (!target) {
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
      case "beforeunload":
        // We are only interested to the pagehide and beforeunload events from
        // the root document.
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
         if (!this.focusedElement) {
          break;
        }

        // We only listen for this event on the currently focused element.
        // When the mouse goes down, note the cursor/selection position
        this.updateSelection();
        break;

      case 'mouseup':
        if (!this.focusedElement) {
          break;
        }

        // We only listen for this event on the currently focused element.
        // When the mouse goes up, see if the cursor has moved (or the
        // selection changed) since the mouse went down. If it has, we
        // need to tell the keyboard about it
        range = getSelectionRange(this.focusedElement);
        if (range[0] !== this.selectionStart ||
            range[1] !== this.selectionEnd) {
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
              scrollSelectionOrElementIntoView(this.focusedElement);
            }
          }.bind(this), RESIZE_SCROLL_DELAY);
        }
        break;

      case "input":
        if (this.focusedElement) {
          // When the text content changes, notify the keyboard
          this.updateSelection();
        }
        break;

      case "keydown":
        if (!this.focusedElement) {
          break;
        }

        CompositionManager.endComposition('');

        // We use 'setTimeout' to wait until the input element accomplishes the
        // change in selection range.
        content.setTimeout(function() {
          this.updateSelection();
        }.bind(this), 0);
        break;

      case "keyup":
        if (!this.focusedElement) {
          break;
        }

        CompositionManager.endComposition('');

        break;

      case "compositionend":
        if (!this.focusedElement) {
          break;
        }

        CompositionManager.onCompositionEnd();
        break;
    }
  },

  receiveMessage: function fa_receiveMessage(msg) {
    let target = this.focusedElement;
    let json = msg.json;

    // To not break mozKeyboard contextId is optional
    if ('contextId' in json &&
        json.contextId !== this._focusCounter &&
        json.requestId) {
      // Ignore messages that are meant for a previously focused element
      sendAsyncMessage("Forms:SequenceError", {
        requestId: json.requestId,
        error: "Expected contextId " + this._focusCounter +
               " but was " + json.contextId
      });
      return;
    }

    if (!target) {
      switch (msg.name) {
      case "Forms:GetText":
        sendAsyncMessage("Forms:GetText:Result:Error", {
          requestId: json.requestId,
          error: "No focused element"
        });
        break;
      }
      return;
    }

    this._editing = true;
    switch (msg.name) {
      case "Forms:Input:Value": {
        CompositionManager.endComposition('');

        target.value = json.value;

        let event = target.ownerDocument.createEvent('HTMLEvents');
        event.initEvent('input', true, false);
        target.dispatchEvent(event);
        break;
      }

      case "Forms:Input:SendKey":
        CompositionManager.endComposition('');

        this._editing = true;
        let doKeypress = domWindowUtils.sendKeyEvent('keydown', json.keyCode,
                                  json.charCode, json.modifiers);
        if (doKeypress) {
          domWindowUtils.sendKeyEvent('keypress', json.keyCode,
                                  json.charCode, json.modifiers);
        }

        if(!json.repeat) {
          domWindowUtils.sendKeyEvent('keyup', json.keyCode,
                                    json.charCode, json.modifiers);
        }

        this._editing = false;

        if (json.requestId && doKeypress) {
          sendAsyncMessage("Forms:SendKey:Result:OK", {
            requestId: json.requestId
          });
        }
        else if (json.requestId && !doKeypress) {
          sendAsyncMessage("Forms:SendKey:Result:Error", {
            requestId: json.requestId,
            error: "Keydown event got canceled"
          });
        }
        break;

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
          let event = target.ownerDocument.createEvent('HTMLEvents');
          event.initEvent('change', true, true);
          target.dispatchEvent(event);
        }
        break;

      case "Forms:Select:Blur": {
        this.setFocusedElement(null);
        break;
      }

      case "Forms:SetSelectionRange":  {
        CompositionManager.endComposition('');

        let start = json.selectionStart;
        let end =  json.selectionEnd;

        if (!setSelectionRange(target, start, end)) {
          if (json.requestId) {
            sendAsyncMessage("Forms:SetSelectionRange:Result:Error", {
              requestId: json.requestId,
              error: "failed"
            });
          }
          break;
        }

        this.updateSelection();

        if (json.requestId) {
          sendAsyncMessage("Forms:SetSelectionRange:Result:OK", {
            requestId: json.requestId,
            selectioninfo: this.getSelectionInfo()
          });
        }
        break;
      }

      case "Forms:ReplaceSurroundingText": {
        CompositionManager.endComposition('');

        let selectionRange = getSelectionRange(target);
        if (!replaceSurroundingText(target,
                                    json.text,
                                    selectionRange[0],
                                    selectionRange[1],
                                    json.offset,
                                    json.length)) {
          if (json.requestId) {
            sendAsyncMessage("Forms:ReplaceSurroundingText:Result:Error", {
              requestId: json.requestId,
              error: "failed"
            });
          }
          break;
        }

        if (json.requestId) {
          sendAsyncMessage("Forms:ReplaceSurroundingText:Result:OK", {
            requestId: json.requestId,
            selectioninfo: this.getSelectionInfo()
          });
        }
        break;
      }

      case "Forms:GetText": {
        let value = isContentEditable(target) ? getContentEditableText(target)
                                              : target.value;

        if (json.offset && json.length) {
          value = value.substr(json.offset, json.length);
        }
        else if (json.offset) {
          value = value.substr(json.offset);
        }

        sendAsyncMessage("Forms:GetText:Result:OK", {
          requestId: json.requestId,
          text: value
        });
        break;
      }

      case "Forms:GetContext": {
        let obj = getJSON(target, this._focusCounter);
        sendAsyncMessage("Forms:GetContext:Result:OK", obj);
        break;
      }

      case "Forms:SetComposition": {
        CompositionManager.setComposition(target, json.text, json.cursor,
                                          json.clauses);
        sendAsyncMessage("Forms:SetComposition:Result:OK", {
          requestId: json.requestId,
        });
        break;
      }

      case "Forms:EndComposition": {
        CompositionManager.endComposition(json.text);
        sendAsyncMessage("Forms:EndComposition:Result:OK", {
          requestId: json.requestId,
        });
        break;
      }
    }
    this._editing = false;

  },

  showKeyboard: function fa_showKeyboard(target) {
    if (this.focusedElement === target)
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
      while (element && !isContentEditable(element))
        element = element.parentNode;

      return element;
    }

    return retrieveTopLevelEditable(element) || element;
  },

  sendKeyboardState: function(element) {
    // FIXME/bug 729623: work around apparent bug in the IME manager
    // in gecko.
    let readonly = element.getAttribute("readonly");
    if (readonly) {
      return false;
    }

    sendAsyncMessage("Forms:Input", getJSON(element, this._focusCounter));
    return true;
  },

  getSelectionInfo: function fa_getSelectionInfo() {
    let element = this.focusedElement;
    let range =  getSelectionRange(element);

    let text = isContentEditable(element) ? getContentEditableText(element)
                                          : element.value;

    let textAround = getTextAroundCursor(text, range);

    let changed = this.selectionStart !== range[0] ||
      this.selectionEnd !== range[1] ||
      this.textBeforeCursor !== textAround.before ||
      this.textAfterCursor !== textAround.after;

    this.selectionStart = range[0];
    this.selectionEnd = range[1];
    this.textBeforeCursor = textAround.before;
    this.textAfterCursor = textAround.after;

    return {
      selectionStart: range[0],
      selectionEnd: range[1],
      textBeforeCursor: textAround.before,
      textAfterCursor: textAround.after,
      changed: changed
    };
  },

  // Notify when the selection range changes
  updateSelection: function fa_updateSelection() {
    if (!this.focusedElement) {
      return;
    }
    let selectionInfo = this.getSelectionInfo();
    if (selectionInfo.changed) {
      sendAsyncMessage("Forms:SelectionChange", this.getSelectionInfo());
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

  return element.ownerDocument && element.ownerDocument.designMode == "on";
}

function isPlainTextField(element) {
  if (!element) {
    return false;
  }

  return element instanceof HTMLTextAreaElement ||
         (element instanceof HTMLInputElement &&
          element.mozIsTextField(false));
}

function getJSON(element, focusCounter) {
  // <input type=number> has a nested anonymous <input type=text> element that
  // takes focus on behalf of the number control when someone tries to focus
  // the number control. If |element| is such an anonymous text control then we
  // need it's number control here in order to get the correct 'type' etc.:
  element = element.ownerNumberControl || element;

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
  let textAround = getTextAroundCursor(value, range);

  return {
    "contextId": focusCounter,

    "type": type.toLowerCase(),
    "choices": getListForElement(element),
    "value": value,
    "inputmode": inputmode,
    "selectionStart": range[0],
    "selectionEnd": range[1],
    "max": max,
    "min": min,
    "lang": element.lang || "",
    "textBeforeCursor": textAround.before,
    "textAfterCursor": textAround.after
  };
}

function getTextAroundCursor(value, range) {
  let textBeforeCursor = range[0] < 100 ?
    value.substr(0, range[0]) :
    value.substr(range[0] - 100, 100);

  let textAfterCursor = range[1] + 100 > value.length ?
    value.substr(range[0], value.length) :
    value.substr(range[0], range[1] - range[0] + 100);

  return {
    before: textBeforeCursor,
    after: textAfterCursor
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
              Ci.nsIDocumentEncoder.OutputDropInvisibleBreak |
              // Bug 902847. Don't trim trailing spaces of a line.
              Ci.nsIDocumentEncoder.OutputDontRemoveLineEndingSpaces |
              Ci.nsIDocumentEncoder.OutputLFLineBreak |
              Ci.nsIDocumentEncoder.OutputNonTextContentAsPlaceholder;
  encoder.init(element.ownerDocument, "text/plain", flags);
  return encoder;
}

// Get the visible content text of a content editable element
function getContentEditableText(element) {
  if (!element || !isContentEditable(element)) {
    return null;
  }

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
  if (isPlainTextField(element)) {
    // Get the selection range of <input> and <textarea> elements
    start = element.selectionStart;
    end = element.selectionEnd;
  } else if (isContentEditable(element)){
    // Get the selection range of contenteditable elements
    let win = element.ownerDocument.defaultView;
    let sel = win.getSelection();
    if (sel && sel.rangeCount > 0) {
      start = getContentEditableSelectionStart(element, sel);
      end = start + getContentEditableSelectionLength(element, sel);
    } else {
      dump("Failed to get window.getSelection()\n");
    }
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
  let isTextField = isPlainTextField(element);

  // Check the parameters

  if (!isTextField && !isContentEditable(element)) {
    // Skip HTMLOptionElement and HTMLSelectElement elements, as they don't
    // support the operation of setSelectionRange
    return false;
  }

  let text = isTextField ? element.value : getContentEditableText(element);
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

  if (isTextField) {
    // Set the selection range of <input> and <textarea> elements
    element.setSelectionRange(start, end, "forward");
    return true;
  } else {
    // set the selection range of contenteditable elements
    let win = element.ownerDocument.defaultView;
    let sel = win.getSelection();

    // Move the caret to the start position
    sel.collapse(element, 0);
    for (let i = 0; i < start; i++) {
      sel.modify("move", "forward", "character");
    }

    // Avoid entering infinite loop in case we cannot change the selection
    // range. See bug https://bugzilla.mozilla.org/show_bug.cgi?id=978918
    let oldStart = getContentEditableSelectionStart(element, sel);
    let counter = 0;
    while (oldStart < start) {
      sel.modify("move", "forward", "character");
      let newStart = getContentEditableSelectionStart(element, sel);
      if (oldStart == newStart) {
        counter++;
        if (counter > MAX_BLOCKED_COUNT) {
          return false;
        }
      } else {
        counter = 0;
        oldStart = newStart;
      }
    }

    // Extend the selection to the end position
    for (let i = start; i < end; i++) {
      sel.modify("extend", "forward", "character");
    }

    // Avoid entering infinite loop in case we cannot change the selection
    // range. See bug https://bugzilla.mozilla.org/show_bug.cgi?id=978918
    counter = 0;
    let selectionLength = end - start;
    let oldSelectionLength = getContentEditableSelectionLength(element, sel);
    while (oldSelectionLength  < selectionLength) {
      sel.modify("extend", "forward", "character");
      let newSelectionLength = getContentEditableSelectionLength(element, sel);
      if (oldSelectionLength == newSelectionLength ) {
        counter++;
        if (counter > MAX_BLOCKED_COUNT) {
          return false;
        }
      } else {
        counter = 0;
        oldSelectionLength = newSelectionLength;
      }
    }
    return true;
  }
}

/**
 * Scroll the given element into view.
 *
 * Calls scrollSelectionIntoView for contentEditable elements.
 */
function scrollSelectionOrElementIntoView(element) {
  let editor = getPlaintextEditor(element);
  if (editor) {
    editor.selectionController.scrollSelectionIntoView(
      Ci.nsISelectionController.SELECTION_NORMAL,
      Ci.nsISelectionController.SELECTION_FOCUS_REGION,
      Ci.nsISelectionController.SCROLL_SYNCHRONOUS);
  } else {
      element.scrollIntoView(false);
  }
}

// Get nsIPlaintextEditor object from an input field
function getPlaintextEditor(element) {
  let editor = null;
  // Get nsIEditor
  if (isPlainTextField(element)) {
    // Get from the <input> and <textarea> elements
    editor = element.QueryInterface(Ci.nsIDOMNSEditableElement).editor;
  } else if (isContentEditable(element)) {
    // Get from content editable element
    let win = element.ownerDocument.defaultView;
    let editingSession = win.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIWebNavigation)
                            .QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIEditingSession);
    if (editingSession) {
      editor = editingSession.getEditorForWindow(win);
    }
  }
  if (editor) {
    editor.QueryInterface(Ci.nsIPlaintextEditor);
  }
  return editor;
}

function replaceSurroundingText(element, text, selectionStart, selectionEnd,
                                offset, length) {
  let editor = FormAssistant.editor;
  if (!editor) {
    return false;
  }

  // Check the parameters.
  let start = selectionStart + offset;
  if (start < 0) {
    start = 0;
  }
  if (length < 0) {
    length = 0;
  }
  let end = start + length;

  if (selectionStart != start || selectionEnd != end) {
    // Change selection range before replacing.
    if (!setSelectionRange(element, start, end)) {
      return false;
    }
  }

  if (start != end) {
    // Delete the selected text.
    editor.deleteSelection(Ci.nsIEditor.ePrevious, Ci.nsIEditor.eStrip);
  }

  if (text) {
    // We don't use CR but LF
    // see https://bugzilla.mozilla.org/show_bug.cgi?id=902847
    text = text.replace(/\r/g, '\n');
    // Insert the text to be replaced with.
    editor.insertText(text);
  }
  return true;
}

let CompositionManager =  {
  _isStarted: false,
  _text: '',
  _clauseAttrMap: {
    'raw-input':
      Ci.nsICompositionStringSynthesizer.ATTR_RAWINPUT,
    'selected-raw-text':
      Ci.nsICompositionStringSynthesizer.ATTR_SELECTEDRAWTEXT,
    'converted-text':
      Ci.nsICompositionStringSynthesizer.ATTR_CONVERTEDTEXT,
    'selected-converted-text':
      Ci.nsICompositionStringSynthesizer.ATTR_SELECTEDCONVERTEDTEXT
  },

  setComposition: function cm_setComposition(element, text, cursor, clauses) {
    // Check parameters.
    if (!element) {
      return;
    }
    let len = text.length;
    if (cursor > len) {
      cursor = len;
    }
    let clauseLens = [];
    let clauseAttrs = [];
    if (clauses) {
      let remainingLength = len;
      for (let i = 0; i < clauses.length; i++) {
        if (clauses[i]) {
          let clauseLength = clauses[i].length || 0;
          // Make sure the total clauses length is not bigger than that of the
          // composition string.
          if (clauseLength > remainingLength) {
            clauseLength = remainingLength;
          }
          remainingLength -= clauseLength;
          clauseLens.push(clauseLength);
          clauseAttrs.push(this._clauseAttrMap[clauses[i].selectionType] ||
                           Ci.nsICompositionStringSynthesizer.ATTR_RAWINPUT);
        }
      }
      // If the total clauses length is less than that of the composition
      // string, extend the last clause to the end of the composition string.
      if (remainingLength > 0) {
        clauseLens[clauseLens.length - 1] += remainingLength;
      }
    } else {
      clauseLens.push(len);
      clauseAttrs.push(Ci.nsICompositionStringSynthesizer.ATTR_RAWINPUT);
    }

    // Start composition if need to.
    if (!this._isStarted) {
      this._isStarted = true;
      domWindowUtils.sendCompositionEvent('compositionstart', '', '');
      this._text = '';
    }

    // Update the composing text.
    if (this._text !== text) {
      this._text = text;
      domWindowUtils.sendCompositionEvent('compositionupdate', text, '');
    }
    let compositionString = domWindowUtils.createCompositionStringSynthesizer();
    compositionString.setString(text);
    for (var i = 0; i < clauseLens.length; i++) {
      compositionString.appendClause(clauseLens[i], clauseAttrs[i]);
    }
    if (cursor >= 0) {
      compositionString.setCaret(cursor, 0);
    }
    compositionString.dispatchEvent();
  },

  endComposition: function cm_endComposition(text) {
    if (!this._isStarted) {
      return;
    }
    // Update the composing text.
    if (this._text !== text) {
      domWindowUtils.sendCompositionEvent('compositionupdate', text, '');
    }
    let compositionString = domWindowUtils.createCompositionStringSynthesizer();
    compositionString.setString(text);
    // Set the cursor position to |text.length| so that the text will be
    // committed before the cursor position.
    compositionString.setCaret(text.length, 0);
    compositionString.dispatchEvent();
    domWindowUtils.sendCompositionEvent('compositionend', text, '');
    this._text = '';
    this._isStarted = false;
  },

  // Composition ends due to external actions.
  onCompositionEnd: function cm_onCompositionEnd() {
    if (!this._isStarted) {
      return;
    }

    this._text = '';
    this._isStarted = false;
  }
};
