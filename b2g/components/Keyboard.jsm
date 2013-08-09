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
    'SetValue', 'RemoveFocus', 'SetSelectedOption', 'SetSelectedOptions',
    'SetSelectionRange', 'ReplaceSurroundingText', 'ShowInputMethodPicker',
    'SwitchToNextInputMethod', 'HideInputMethod',
    'GetText', 'SendKey', 'GetContext'
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
    Services.obs.addObserver(this, 'oop-frameloader-crashed', false);

    for (let name of this._messageNames)
      ppmm.addMessageListener('Keyboard:' + name, this);
  },

  observe: function keyboardObserve(subject, topic, data) {
    let frameLoader = subject.QueryInterface(Ci.nsIFrameLoader);
    let mm = frameLoader.messageManager;

    if (topic == 'oop-frameloader-crashed') {
      if (this.messageManager == mm) {
        // The application has been closed unexpectingly. Let's tell the
        // keyboard app that the focus has been lost.
        ppmm.broadcastAsyncMessage('Keyboard:FocusChange', { 'type': 'blur' });
      }
    } else {
      mm.addMessageListener('Forms:Input', this);
      mm.addMessageListener('Forms:SelectionChange', this);
      mm.addMessageListener('Forms:GetText:Result:OK', this);
      mm.addMessageListener('Forms:GetText:Result:Error', this);
      mm.addMessageListener('Forms:SetSelectionRange:Result:OK', this);
      mm.addMessageListener('Forms:ReplaceSurroundingText:Result:OK', this);
      mm.addMessageListener('Forms:SendKey:Result:OK', this);
      mm.addMessageListener('Forms:SequenceError', this);
      mm.addMessageListener('Forms:GetContext:Result:OK', this);

      // When not running apps OOP, we need to load forms.js here since this
      // won't happen from dom/ipc/preload.js
      try {
         if (Services.prefs.getBoolPref("dom.ipc.tabs.disabled") === true) {
           mm.loadFrameScript(kFormsFrameScript, true);
        }
      } catch (e) {
         dump('Error loading ' + kFormsFrameScript + ' as frame script: ' + e + '\n');
      }
    }
  },

  receiveMessage: function keyboardReceiveMessage(msg) {
    // If we get a 'Keyboard:XXX' message, check that the sender has the
    // keyboard permission.
    if (msg.name.indexOf("Keyboard:") != -1) {
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
        this.handleFocusChange(msg);
        break;
      case 'Forms:SelectionChange':
      case 'Forms:GetText:Result:OK':
      case 'Forms:GetText:Result:Error':
      case 'Forms:SetSelectionRange:Result:OK':
      case 'Forms:ReplaceSurroundingText:Result:OK':
      case 'Forms:SendKey:Result:OK':
      case 'Forms:SequenceError':
      case 'Forms:GetContext:Result:OK':
        let name = msg.name.replace(/^Forms/, 'Keyboard');
        this.forwardEvent(name, msg);
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
      case 'Keyboard:SetSelectionRange':
        this.setSelectionRange(msg);
        break;
      case 'Keyboard:ReplaceSurroundingText':
        this.replaceSurroundingText(msg);
        break;
      case 'Keyboard:SwitchToNextInputMethod':
        this.switchToNextInputMethod();
        break;
      case 'Keyboard:ShowInputMethodPicker':
        this.showInputMethodPicker();
        break;
      case 'Keyboard:GetText':
        this.getText(msg);
        break;
      case 'Keyboard:SendKey':
        this.sendKey(msg);
        break;
      case 'Keyboard:GetContext':
        this.getContext(msg);
        break;
    }
  },

  forwardEvent: function keyboardForwardEvent(newEventName, msg) {
    this.messageManager = msg.target.QueryInterface(Ci.nsIFrameLoaderOwner)
                             .frameLoader.messageManager;

    ppmm.broadcastAsyncMessage(newEventName, msg.data);
  },

  handleFocusChange: function keyboardHandleFocusChange(msg) {
    this.forwardEvent('Keyboard:FocusChange', msg);

    let browser = Services.wm.getMostRecentWindow("navigator:browser");

    browser.shell.sendChromeEvent({
      type: 'inputmethod-contextchange',
      inputType: msg.data.type
    });
  },

  setSelectedOption: function keyboardSetSelectedOption(msg) {
    this.messageManager.sendAsyncMessage('Forms:Select:Choice', msg.data);
  },

  setSelectedOptions: function keyboardSetSelectedOptions(msg) {
    this.messageManager.sendAsyncMessage('Forms:Select:Choice', msg.data);
  },

  setSelectionRange: function keyboardSetSelectionRange(msg) {
    this.messageManager.sendAsyncMessage('Forms:SetSelectionRange', msg.data);
  },

  setValue: function keyboardSetValue(msg) {
    this.messageManager.sendAsyncMessage('Forms:Input:Value', msg.data);
  },

  removeFocus: function keyboardRemoveFocus() {
    this.messageManager.sendAsyncMessage('Forms:Select:Blur', {});
  },

  replaceSurroundingText: function keyboardReplaceSurroundingText(msg) {
    this.messageManager.sendAsyncMessage('Forms:ReplaceSurroundingText',
                                         msg.data);
  },

  showInputMethodPicker: function keyboardShowInputMethodPicker() {
    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    browser.shell.sendChromeEvent({
      type: "inputmethod-showall"
    });
  },

  switchToNextInputMethod: function keyboardSwitchToNextInputMethod() {
    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    browser.shell.sendChromeEvent({
      type: "inputmethod-next"
    });
  },

  getText: function keyboardGetText(msg) {
    this.messageManager.sendAsyncMessage('Forms:GetText', msg.data);
  },

  sendKey: function keyboardSendKey(msg) {
    this.messageManager.sendAsyncMessage('Forms:Input:SendKey', msg.data);
  },

  getContext: function keyboardGetContext(msg) {
    this.messageManager.sendAsyncMessage('Forms:GetContext', msg.data);
  }
};

Keyboard.init();
