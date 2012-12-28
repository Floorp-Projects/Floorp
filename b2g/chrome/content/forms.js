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

let HTMLInputElement = Ci.nsIDOMHTMLInputElement;
let HTMLTextAreaElement = Ci.nsIDOMHTMLTextAreaElement;
let HTMLSelectElement = Ci.nsIDOMHTMLSelectElement;
let HTMLOptGroupElement = Ci.nsIDOMHTMLOptGroupElement;
let HTMLOptionElement = Ci.nsIDOMHTMLOptionElement;

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
        if (this.isTextInputElement(target) && this.isIMEDisabled())
          return;

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
            if (this.focusedElement) {
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
    switch (topic) {
      case "xpcom-shutdown":
        Services.obs.removeObserver(this, "xpcom-shutdown");
        removeMessageListener("Forms:Select:Choice", this);
        removeMessageListener("Forms:Input:Value", this);
        break;
    }
  },

  isIMEDisabled: function fa_isIMEDisabled() {
    let disabled = false;
    try {
      disabled = domWindowUtils.IMEStatus == domWindowUtils.IME_STATUS_DISABLED;
    } catch (e) {}

    return disabled;
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
    if (element.contentEditable && element.contentEditable == "true") {
      return true;
    }

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
           (element.contentEditable && element.contentEditable == "true");
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


function getJSON(element) {
  let type = element.type || "";
  let value = element.value || ""

  // Treat contenteditble element as a special text field
  if (element.contentEditable && element.contentEditable == "true") {
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

