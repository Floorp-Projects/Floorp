/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

this.EXPORTED_SYMBOLS = ['Keyboard'];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
  "@mozilla.org/parentprocessmessagemanager;1", "nsIMessageBroadcaster");

this.Keyboard = {
  _formMM: null,     // The current web page message manager.
  _keyboardMM: null, // The keyboard app message manager.
  _messageNames: [
    'SetValue', 'RemoveFocus', 'SetSelectedOption', 'SetSelectedOptions',
    'SetSelectionRange', 'ReplaceSurroundingText', 'ShowInputMethodPicker',
    'SwitchToNextInputMethod', 'HideInputMethod',
    'GetText', 'SendKey', 'GetContext',
    'SetComposition', 'EndComposition',
    'Register', 'Unregister'
  ],

  get formMM() {
    if (this._formMM && !Cu.isDeadWrapper(this._formMM))
      return this._formMM;

    return null;
  },

  set formMM(mm) {
    this._formMM = mm;
  },

  sendToForm: function(name, data) {
    try {
      this.formMM.sendAsyncMessage(name, data);
    } catch(e) { }
  },

  sendToKeyboard: function(name, data) {
    try {
      this._keyboardMM.sendAsyncMessage(name, data);
    } catch(e) { }
  },

  init: function keyboardInit() {
    Services.obs.addObserver(this, 'inprocess-browser-shown', false);
    Services.obs.addObserver(this, 'remote-browser-shown', false);
    Services.obs.addObserver(this, 'oop-frameloader-crashed', false);

    for (let name of this._messageNames)
      ppmm.addMessageListener('Keyboard:' + name, this);
  },

  observe: function keyboardObserve(subject, topic, data) {
    let frameLoader = subject.QueryInterface(Ci.nsIFrameLoader);
    let mm = frameLoader.messageManager;

    if (topic == 'oop-frameloader-crashed') {
      if (this.formMM == mm) {
        // The application has been closed unexpectingly. Let's tell the
        // keyboard app that the focus has been lost.
        this.sendToKeyboard('Keyboard:FocusChange', { 'type': 'blur' });
      }
    } else {
      // Ignore notifications that aren't from a BrowserOrApp
      if (!frameLoader.ownerIsBrowserOrAppFrame) {
        return;
      }
      this.initFormsFrameScript(mm);
    }
  },

  initFormsFrameScript: function(mm) {
    mm.addMessageListener('Forms:Input', this);
    mm.addMessageListener('Forms:SelectionChange', this);
    mm.addMessageListener('Forms:GetText:Result:OK', this);
    mm.addMessageListener('Forms:GetText:Result:Error', this);
    mm.addMessageListener('Forms:SetSelectionRange:Result:OK', this);
    mm.addMessageListener('Forms:ReplaceSurroundingText:Result:OK', this);
    mm.addMessageListener('Forms:SendKey:Result:OK', this);
    mm.addMessageListener('Forms:SendKey:Result:Error', this);
    mm.addMessageListener('Forms:SequenceError', this);
    mm.addMessageListener('Forms:GetContext:Result:OK', this);
    mm.addMessageListener('Forms:SetComposition:Result:OK', this);
    mm.addMessageListener('Forms:EndComposition:Result:OK', this);
  },

  receiveMessage: function keyboardReceiveMessage(msg) {
    // If we get a 'Keyboard:XXX' message, check that the sender has the
    // input permission.
    let mm;
    let isKeyboardRegistration = msg.name == "Keyboard:Register" ||
                                 msg.name == "Keyboard:Unregister";
    if (msg.name.indexOf("Keyboard:") != -1) {
      if (!this.formMM && !isKeyboardRegistration) {
        return;
      }

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

      let testing = false;
      try {
        testing = Services.prefs.getBoolPref("dom.mozInputMethod.testing");
      } catch (e) {
      }
      if (!isKeyboardRegistration && !testing &&
          !mm.assertPermission("input")) {
        dump("Keyboard message " + msg.name +
        " from a content process with no 'input' privileges.");
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
      case 'Forms:SendKey:Result:Error':
      case 'Forms:SequenceError':
      case 'Forms:GetContext:Result:OK':
      case 'Forms:SetComposition:Result:OK':
      case 'Forms:EndComposition:Result:OK':
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
      case 'Keyboard:SetComposition':
        this.setComposition(msg);
        break;
      case 'Keyboard:EndComposition':
        this.endComposition(msg);
        break;
      case 'Keyboard:Register':
        this._keyboardMM = mm;
        break;
      case 'Keyboard:Unregister':
        this._keyboardMM = null;
        break;
    }
  },

  forwardEvent: function keyboardForwardEvent(newEventName, msg) {
    this.formMM = msg.target.QueryInterface(Ci.nsIFrameLoaderOwner)
                            .frameLoader.messageManager;

    this.sendToKeyboard(newEventName, msg.data);
  },

  handleFocusChange: function keyboardHandleFocusChange(msg) {
    this.forwardEvent('Keyboard:FocusChange', msg);

    // Chrome event, used also to render value selectors; that's why we need
    // the info about choices / min / max here as well...
    this.sendChromeEvent({
      type: 'inputmethod-contextchange',
      inputType: msg.data.type,
      value: msg.data.value,
      choices: JSON.stringify(msg.data.choices),
      min: msg.data.min,
      max: msg.data.max
    });
  },

  setSelectedOption: function keyboardSetSelectedOption(msg) {
    this.sendToForm('Forms:Select:Choice', msg.data);
  },

  setSelectedOptions: function keyboardSetSelectedOptions(msg) {
    this.sendToForm('Forms:Select:Choice', msg.data);
  },

  setSelectionRange: function keyboardSetSelectionRange(msg) {
    this.sendToForm('Forms:SetSelectionRange', msg.data);
  },

  setValue: function keyboardSetValue(msg) {
    this.sendToForm('Forms:Input:Value', msg.data);
  },

  removeFocus: function keyboardRemoveFocus() {
    this.sendToForm('Forms:Select:Blur', {});
  },

  replaceSurroundingText: function keyboardReplaceSurroundingText(msg) {
    this.sendToForm('Forms:ReplaceSurroundingText', msg.data);
  },

  showInputMethodPicker: function keyboardShowInputMethodPicker() {
    this.sendChromeEvent({
      type: "inputmethod-showall"
    });
  },

  switchToNextInputMethod: function keyboardSwitchToNextInputMethod() {
    this.sendChromeEvent({
      type: "inputmethod-next"
    });
  },

  getText: function keyboardGetText(msg) {
    this.sendToForm('Forms:GetText', msg.data);
  },

  sendKey: function keyboardSendKey(msg) {
    this.sendToForm('Forms:Input:SendKey', msg.data);
  },

  getContext: function keyboardGetContext(msg) {
    if (this._layouts) {
      this.sendToKeyboard('Keyboard:LayoutsChange', this._layouts);
    }

    this.sendToForm('Forms:GetContext', msg.data);
  },

  setComposition: function keyboardSetComposition(msg) {
    this.sendToForm('Forms:SetComposition', msg.data);
  },

  endComposition: function keyboardEndComposition(msg) {
    this.sendToForm('Forms:EndComposition', msg.data);
  },

  /**
   * Get the number of keyboard layouts active from keyboard_manager
   */
  _layouts: null,
  setLayouts: function keyboardSetLayoutCount(layouts) {
    // The input method plugins may not have loaded yet,
    // cache the layouts so on init we can respond immediately instead
    // of going back and forth between keyboard_manager
    this._layouts = layouts;

    this.sendToKeyboard('Keyboard:LayoutsChange', layouts);
  },

  sendChromeEvent: function(event) {
    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    if (browser && browser.shell) {
      browser.shell.sendChromeEvent(event);;
    }
  }
};

this.Keyboard.init();
