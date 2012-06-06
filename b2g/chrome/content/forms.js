/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


let Ci = Components.interfaces;
let Cc = Components.classes;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

dump("###################################### forms.js loaded\n");

let HTMLSelectElement = Ci.nsIDOMHTMLSelectElement;
let HTMLOptGroupElement = Ci.nsIDOMHTMLOptGroupElement;
let HTMLOptionElement = Ci.nsIDOMHTMLOptionElement;

let FormAssistant = {
  init: function fa_init() {
    addEventListener("focus", this, true, false);
    addEventListener("click", this, true, false);
    addEventListener("blur", this, true, false);
    addMessageListener("Forms:Select:Choice", this);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  currentTarget: null,
  handleEvent: function fa_handleEvent(evt) {
    switch (evt.type) {
      case "click":
      case "focus": {
        content.setTimeout(function(self) {
          let target = evt.target;
          if (target instanceof HTMLSelectElement) { 
            sendAsyncMessage("Forms:Input", self._getJSON(target));
            self.currentTarget = target;
          } else if (target instanceof HTMLOptionElement &&
                     target.parentNode instanceof HTMLSelectElement) {
            target = target.parentNode;
            sendAsyncMessage("Forms:Input", self._getJSON(target));
            self.currentTarget = target;
          }
        }, 0, this);
        break;
      }

      case "blur": {
        let target = this.currentTarget;
        if (!target)
          return;

        this.currentTarget = null;

        sendAsyncMessage("Forms:Input", { "type": "blur" });

        let e = target.ownerDocument.createEvent("Events");
        e.initEvent("change", true, true, target.ownerDocument.defaultView,
                    0, false, false, false, false, null);

        content.setTimeout(function() {
          target.dispatchEvent(evt);
        }, 0);

        break;
      }
    }
  },

  receiveMessage: function fa_receiveMessage(msg) {
    let json = msg.json;
    switch (msg.name) {
      case "Forms:Select:Choice":
        if (!this.currentTarget)
          return;

        let options = this.currentTarget.options;
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
    Services.obs.removeObserver(this, "xpcom-shutdown");
    removeMessageListener("Forms:Select:Choice", this);
  },

  _getJSON: function fa_getJSON(element) {
    let choices = getListForElement(element);
    return {
      type: element.type.toLowerCase(),
      choices: choices
    };
  }
};

FormAssistant.init();

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

