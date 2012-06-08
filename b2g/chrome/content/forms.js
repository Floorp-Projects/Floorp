/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

dump("###################################### forms.js loaded\n");

"use strict";

let Ci = Components.interfaces;
let Cc = Components.classes;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyServiceGetter(Services, "fm",
                                   "@mozilla.org/focus-manager;1",
                                   "nsIFocusManager");

let HTMLInputElement = Ci.nsIDOMHTMLInputElement;
let HTMLTextAreaElement = Ci.nsIDOMHTMLTextAreaElement;
let HTMLSelectElement = Ci.nsIDOMHTMLSelectElement;
let HTMLOptGroupElement = Ci.nsIDOMHTMLOptGroupElement;
let HTMLOptionElement = Ci.nsIDOMHTMLOptionElement;

let FormAssistant = {
  init: function fa_init() {
    addEventListener("focus", this, true, false);
    addEventListener("keypress", this, true, false);
    addEventListener("mousedown", this, true, false);
    addEventListener("resize", this, true, false);
    addEventListener("click", this, true, false);
    addEventListener("blur", this, true, false);
    addMessageListener("Forms:Select:Choice", this);
    addMessageListener("Forms:Input:Value", this);
    Services.obs.addObserver(this, "ime-enabled-state-changed", false);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  isKeyboardOpened: false,
  previousTarget : null,
  handleEvent: function fa_handleEvent(evt) {
    let previousTarget = this.previousTarget;
    let target = evt.target;

    switch (evt.type) {
      case "focus":
        this.previousTarget = Services.fm.focusedElement;
        break;

      case "blur":
        if (!target)
          return;
        this.previousTarget = null;

        if (target instanceof HTMLSelectElement ||
            (target instanceof HTMLOptionElement && target.parentNode instanceof HTMLSelectElement)) {

          sendAsyncMessage("Forms:Input", { "type": "blur" });
        }
        break;

      case 'resize':
        if (!this.isKeyboardOpened)
          return;

        Services.fm.focusedElement.scrollIntoView(false);
        break;

      case "mousedown":
        if (evt.target != target || this.isKeyboardOpened)
          return;

        if (!(evt.target instanceof HTMLInputElement  ||
              evt.target instanceof HTMLTextAreaElement))
          return;

        this.isKeyboardOpened = this.tryShowIme(evt.target);
        break;

      case "keypress":
        if (evt.keyCode != evt.DOM_VK_ESCAPE || !this.isKeyboardOpened)
          return;

        sendAsyncMessage("Forms:Input", { "type": "blur" });
        this.isKeyboardOpened = false;

        evt.preventDefault();
        evt.stopPropagation();
        break;

      case "click":
        content.setTimeout(function showIMEForSelect() {
          if (evt.target instanceof HTMLSelectElement) { 
            sendAsyncMessage("Forms:Input", getJSON(evt.target));
          } else if (evt.target instanceof HTMLOptionElement &&
                     evt.target.parentNode instanceof HTMLSelectElement) {
            sendAsyncMessage("Forms:Input", getJSON(evt.target.parentNode));
          }
        });
        break;
    }
  },

  receiveMessage: function fa_receiveMessage(msg) {
    let target = Services.fm.focusedElement;
    if (!target)
      return;

    let json = msg.json;
    switch (msg.name) {
      case "Forms:Input:Value":
        target.value = json.value;
        break;

      case "Forms:Select:Choice":
        let options = target.options;
        if ("index" in json) {
          options.item(json.index).selected = true;
        } else if ("indexes" in json) {
          for (let i = 0; i < options.length; i++) {
            options.item(i).selected = (json.indexes.indexOf(i) != -1);
          }
        }
        break;
    }
  },

  observe: function fa_observe(subject, topic, data) {
    switch (topic) {
      case "ime-enabled-state-changed":
        let isOpen = this.isKeyboardOpened;
        let shouldOpen = parseInt(data);
        if (shouldOpen && !isOpen) {
          let target = Services.fm.focusedElement;

          if (!target || !this.tryShowIme(target)) {
            this.previousTarget = null;
            return;
          } else {
            this.previousTarget = target;
          }
        } else if (!shouldOpen && isOpen) {
          sendAsyncMessage("Forms:Input", { "type": "blur" });
        }
        this.isKeyboardOpened = shouldOpen;
        break;

      case "xpcom-shutdown":
        Services.obs.removeObserver(this, "ime-enabled-state-changed", false);
        Services.obs.removeObserver(this, "xpcom-shutdown");
        removeMessageListener("Forms:Select:Choice", this);
        break;
    }
  },

  tryShowIme: function(element) {
    // FIXME/bug 729623: work around apparent bug in the IME manager
    // in gecko.
    let readonly = element.getAttribute("readonly");
    if (readonly)
      return false;

    sendAsyncMessage("Forms:Input", getJSON(element));
    return true;
  }
};

FormAssistant.init();


function getJSON(element) {
  let type = element.type;
  // FIXME/bug 344616 is input type="number"
  // Until then, let's return 'number' even if the platform returns 'text'
  let attributeType = element.getAttribute("type") || "";
  if (attributeType && attributeType.toLowerCase() === "number")
    type = "number";

  return {
    "type": type.toLowerCase(),
    "choices": getListForElement(element)
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

