/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

this.EXPORTED_SYMBOLS = ['Keyboard'];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;
const kFormsFrameScript = 'chrome://browser/content/forms.js';

Cu.import('resource://gre/modules/Services.jsm');
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
  "@mozilla.org/parentprocessmessagemanager;1", "nsIMessageBroadcaster");

let Keyboard = {
  _messageManager: null,
  _messageNames: [
    'SetValue', 'RemoveFocus', 'SetSelectedOption', 'SetSelectedOptions'
  ],

  get messageManager() {
    if (this._messageManager && !Cu.isDeadWrapper(this._messageManager))
      return this._messageManager;

    throw Error('no message manager set');
  },

  set messageManager(mm) {
    this._messageManager = mm;
  },

  init: function keyboardInit() {
    Services.obs.addObserver(this, 'in-process-browser-or-app-frame-shown', false);
    Services.obs.addObserver(this, 'remote-browser-frame-shown', false);

    for (let name of this._messageNames)
      ppmm.addMessageListener('Keyboard:' + name, this);
  },

  observe: function keyboardObserve(subject, topic, data) {
    let frameLoader = subject.QueryInterface(Ci.nsIFrameLoader);
    let mm = frameLoader.messageManager;
    mm.addMessageListener('Forms:Input', this);
  },

  receiveMessage: function keyboardReceiveMessage(msg) {
    // If we get a 'Keyboard:XXX' message, check that the sender has the
    // keyboard permission.
    if (msg.name != 'Forms:Input') {
      let mm;
      try {
        mm = msg.target.QueryInterface(Ci.nsIFrameLoaderOwner)
                       .frameLoader.messageManager;
      } catch(e) {
        mm = msg.target;
      }

      // That should never happen.
      if (!mm) {
        dump("!! No message manager found for " + msg.name);
        return;
      }

      if (!mm.assertPermission("keyboard")) {
        dump("Keyboard message " + msg.name +
        " from a content process with no 'keyboard' privileges.");
        return;
      }
    }

    switch (msg.name) {
      case 'Forms:Input':
        this.handleFormsInput(msg);
        break;
      case 'Keyboard:SetValue':
        this.setValue(msg);
        break;
      case 'Keyboard:RemoveFocus':
        this.removeFocus();
        break;
      case 'Keyboard:SetSelectedOption':
        this.setSelectedOption(msg);
        break;
      case 'Keyboard:SetSelectedOptions':
        this.setSelectedOption(msg);
        break;
    }
  },

  handleFormsInput: function keyboardHandleFormsInput(msg) {
    this.messageManager = msg.target.QueryInterface(Ci.nsIFrameLoaderOwner)
                             .frameLoader.messageManager;

    ppmm.broadcastAsyncMessage('Keyboard:FocusChange', msg.data);
  },

  setSelectedOption: function keyboardSetSelectedOption(msg) {
    this.messageManager.sendAsyncMessage('Forms:Select:Choice', msg.data);
  },

  setSelectedOptions: function keyboardSetSelectedOptions(msg) {
    this.messageManager.sendAsyncMessage('Forms:Select:Choice', msg.data);
  },

  setValue: function keyboardSetValue(msg) {
    this.messageManager.sendAsyncMessage('Forms:Input:Value', msg.data);
  },

  removeFocus: function keyboardRemoveFocus() {
    this.messageManager.sendAsyncMessage('Forms:Select:Blur', {});
  }
};

Keyboard.init();
