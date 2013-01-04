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
    addMessageListener("Forms:Select:Choice", this);
    addMessageListener("Forms:Input:Value", this);
    addMessageListener("Forms:Select:Blur", this);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  ignoredInputTypes: new Set([
    'button', 'file', 'checkbox', 'radio', 'reset', 'submit', 'image'
  ]),

  isKeyboardOpened: false,
  selectionStart: 0,
  selectionEnd: 0,
  scrollIntoViewTimeout: null,
  _focusedElement: null,

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

    if (element) {
      element.addEventListener('mousedown', this);
      element.addEventListener('mouseup', this);
    }

    this.focusedElement = element;
  },

  handleEvent: function fa_handleEvent(evt) {
    let focusedElement = this.focusedElement;
    let target = evt.target;

    switch (evt.type) {
      case "focus":
        if (target && isContentEditable(target)) {
          this.showKeyboard(this.getTopLevelEditable(target));
          break;
        }

        if (target && this.isFocusableElement(target))
          this.showKeyboard(target);
        break;

      case "blur":
        if (this.focusedElement)
          this.hideKeyboard();
        break;

      case 'mousedown':
        // We only listen for this event on the currently focused element.
        // When the mouse goes down, note the cursor/selection position
        this.selectionStart = this.focusedElement.selectionStart;
        this.selectionEnd = this.focusedElement.selectionEnd;
        break;

      case 'mouseup':
        // We only listen for this event on the currently focused element.
        // When the mouse goes up, see if the cursor has moved (or the
        // selection changed) since the mouse went down. If it has, we
        // need to tell the keyboard about it
        if (this.focusedElement.selectionStart !== this.selectionStart ||
            this.focusedElement.selectionEnd !== this.selectionEnd) {
          this.sendKeyboardState(this.focusedElement);
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
    }
  },

  observe: function fa_observe(subject, topic, data) {
    Services.obs.removeObserver(this, "xpcom-shutdown");
    removeMessageListener("Forms:Select:Choice", this);
    removeMessageListener("Forms:Input:Value", this);
  },

  showKeyboard: function fa_showKeyboard(target) {
    if (this.isKeyboardOpened)
      return;

    if (target instanceof HTMLOptionElement)
      target = target.parentNode;

    let kbOpened = this.sendKeyboardState(target);
    if (this.isTextInputElement(target))
      this.isKeyboardOpened = kbOpened;

    this.setFocusedElement(target);
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
  }
};

FormAssistant.init();


function isContentEditable(element) {
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
  let value = element.value || ""

  // Treat contenteditble element as a special text field
  if (isContentEditable(element)) {
    type = "text";
    value = element.textContent;
  }

  // Until the input type=date/datetime/time have been implemented
  // let's return their real type even if the platform returns 'text'
  // Related to Bug 777279 - Implement <input type=time>
  let attributeType = element.getAttribute("type") || "";

  if (attributeType) {
    var typeLowerCase = attributeType.toLowerCase(); 
    switch (typeLowerCase) {
      case "time":
      case "datetime":
      case "datetime-local":
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

  return {
    "type": type.toLowerCase(),
    "choices": getListForElement(element),
    "value": value,
    "inputmode": inputmode,
    "selectionStart": element.selectionStart,
    "selectionEnd": element.selectionEnd
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

