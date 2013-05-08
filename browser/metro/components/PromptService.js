/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// Whitelist of methods we remote - to check against malicious data.
// For example, it would be dangerous to allow content to show auth prompts.
const REMOTABLE_METHODS = {
  alert: { outParams: [] },
  alertCheck: { outParams: [4] },
  confirm: { outParams: [] },
  prompt: { outParams: [3, 5] },
  confirmEx: { outParams: [8] },
  confirmCheck: { outParams: [4] },
  select: { outParams: [5] }
};

var gPromptService = null;

function PromptService() {
  // Depending on if we are in the parent or child, prepare to remote
  // certain calls
  var appInfo = Cc["@mozilla.org/xre/app-info;1"];
  if (!appInfo || appInfo.getService(Ci.nsIXULRuntime).processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
    // Parent process
    this.inContentProcess = false;

    // Used for wakeups service. FIXME: clean up with bug 593407
    this.wrappedJSObject = this;

    // Setup listener for child messages. We don't need to call
    // addMessageListener as the wakeup service will do that for us.
    this.receiveMessage = function(aMessage) {
      var json = aMessage.json;
      switch (aMessage.name) {
        case "Prompt:Call":
          var method = aMessage.json.method;
          if (!REMOTABLE_METHODS.hasOwnProperty(method))
            throw "PromptServiceRemoter received an invalid method "+method;

          var arguments = aMessage.json.arguments;
          var ret = this[method].apply(this, arguments);
          // Return multiple return values in objects of form { value: ... },
          // and also with the actual return value at the end
          arguments.push(ret);
          return arguments;
      }
    };
  } else {
    // Child process
    this.inContentProcess = true;
  }

  gPromptService = this;
}

PromptService.prototype = {
  classID: Components.ID("{9a61149b-2276-4a0a-b79c-be994ad106cf}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPromptFactory, Ci.nsIPromptService, Ci.nsIPromptService2]),

  /* ----------  nsIPromptFactory  ---------- */

  // XXX Copied from nsPrompter.js.
  getPrompt: function getPrompt(domWin, iid) {
    if (this.inContentProcess)
      return ContentPrompt.QueryInterface(iid);

    let doc = this.getDocument();
    if (!doc) {
      let fallback = this._getFallbackService();
      return fallback.getPrompt(domWin, iid);
    }

    let p = new Prompt(domWin, doc);
    p.QueryInterface(iid);
    return p;
  },

  /* ----------  private memebers  ---------- */

  _getFallbackService: function _getFallbackService() {
    return Components.classesByID["{7ad1b327-6dfa-46ec-9234-f2a620ea7e00}"]
                     .getService(Ci.nsIPromptService);
  },

  getDocument: function getDocument() {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    return win ? win.document : null;
  },

  // nsIPromptService and nsIPromptService2 methods proxy to our Prompt class
  // if we can show in-document popups, or to the fallback service otherwise.
  callProxy: function(aMethod, aArguments) {
    let prompt;
    if (this.inContentProcess) {
      // Bring this tab to the front, so prompt appears on the right tab
      var window = aArguments[0];
      if (window && window.document) {
        var event = window.document.createEvent("Events");
        event.initEvent("DOMWillOpenModalDialog", true, true);
        let winUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIDOMWindowUtils);
        winUtils.dispatchEventToChromeOnly(window, event);
      }
      prompt = ContentPrompt;
    } else {
      let doc = this.getDocument();
      if (!doc) {
        let fallback = this._getFallbackService();
        return fallback[aMethod].apply(fallback, aArguments);
      }
      let domWin = aArguments[0];
      prompt = new Prompt(domWin, doc);
    }
    return prompt[aMethod].apply(prompt, Array.prototype.slice.call(aArguments, 1));
  },

  /* ----------  nsIPromptService  ---------- */

  alert: function() {
    return this.callProxy("alert", arguments);
  },
  alertCheck: function() {
    return this.callProxy("alertCheck", arguments);
  },
  confirm: function() {
    return this.callProxy("confirm", arguments);
  },
  confirmCheck: function() {
    return this.callProxy("confirmCheck", arguments);
  },
  confirmEx: function() {
    return this.callProxy("confirmEx", arguments);
  },
  prompt: function() {
    return this.callProxy("prompt", arguments);
  },
  promptUsernameAndPassword: function() {
    return this.callProxy("promptUsernameAndPassword", arguments);
  },
  promptPassword: function() {
    return this.callProxy("promptPassword", arguments);
  },
  select: function() {
    return this.callProxy("select", arguments);
  },

  /* ----------  nsIPromptService2  ---------- */

  promptAuth: function() {
    return this.callProxy("promptAuth", arguments);
  },
  asyncPromptAuth: function() {
    return this.callProxy("asyncPromptAuth", arguments);
  }
};

// Implementation of nsIPrompt that just forwards to the parent process.
let ContentPrompt = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPrompt]),

  sendMessage: function sendMessage(aMethod) {
    let args = Array.prototype.slice.call(arguments);
    args[0] = null; // No need to pass "window" argument to the prompt service.

    // We send all prompts as sync, even alert (which has no important
    // return value), as otherwise program flow will continue, and the
    // script can theoretically show several alerts at once. In particular
    // this can lead to a bug where you cannot click the earlier one, which
    // is now hidden by a new one (and Fennec is helplessly frozen).
    var json = { method: aMethod, arguments: args };
    var response = this.messageManager.sendSyncMessage("Prompt:Call", json)[0];

    // Args copying - for methods that have out values
    REMOTABLE_METHODS[aMethod].outParams.forEach(function(i) {
      args[i].value = response[i].value;
    });
    return response.pop(); // final return value was given at the end
  }
};

XPCOMUtils.defineLazyServiceGetter(ContentPrompt, "messageManager",
  "@mozilla.org/childprocessmessagemanager;1", Ci.nsISyncMessageSender);

// Add remotable methods to ContentPrompt.
for (let [method, _] in Iterator(REMOTABLE_METHODS)) {
  ContentPrompt[method] = ContentPrompt.sendMessage.bind(ContentPrompt, method);
}

function Prompt(aDomWin, aDocument) {
  this._domWin = aDomWin;
  this._doc = aDocument;
}

Prompt.prototype = {
  _domWin: null,
  _doc: null,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPrompt, Ci.nsIAuthPrompt, Ci.nsIAuthPrompt2]),

  /* ---------- internal methods ---------- */

  openDialog: function openDialog(aSrc, aParams) {
    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    return browser.DialogUI.importModal(this._domWin, aSrc, aParams);
  },

  _setupPrompt: function setupPrompt(aDoc, aType, aTitle, aText, aCheck) {
    aDoc.getElementById("prompt-" + aType + "-title").appendChild(aDoc.createTextNode(aTitle));
    aDoc.getElementById("prompt-" + aType + "-message").appendChild(aDoc.createTextNode(aText));

    if (aCheck && aCheck.msg) {
      aDoc.getElementById("prompt-" + aType + "-checkbox").checked = aCheck.value;
      this.setLabelForNode(aDoc.getElementById("prompt-" + aType + "-checkbox"), aCheck.msg);
      aDoc.getElementById("prompt-" + aType + "-checkbox").removeAttribute("collapsed");
    }
  },

  commonPrompt: function commonPrompt(aTitle, aText, aValue, aCheckMsg, aCheckState, isPassword) {
    var params = new Object();
    params.result = false;
    params.checkbox = aCheckState;
    params.value = aValue;

    let dialog = this.openDialog("chrome://browser/content/prompt/prompt.xul", params);
    let doc = this._doc;
    this._setupPrompt(doc, "prompt", aTitle, aText, {value: aCheckState.value, msg: aCheckMsg});
    doc.getElementById("prompt-prompt-textbox").value = aValue.value;
    if (isPassword)
      doc.getElementById("prompt-prompt-textbox").type = "password";

    dialog.waitForClose();
    return params.result;
  },

  //
  // Copied from chrome://global/content/commonDialog.js
  //
  setLabelForNode: function setLabelForNode(aNode, aLabel) {
    // This is for labels which may contain embedded access keys.
    // If we end in (&X) where X represents the access key, optionally preceded
    // by spaces and/or followed by the ':' character, store the access key and
    // remove the access key placeholder + leading spaces from the label.
    // Otherwise a character preceded by one but not two &s is the access key.
    // Store it and remove the &.

    // Note that if you change the following code, see the comment of
    // nsTextBoxFrame::UpdateAccessTitle.

    if (!aLabel)
      return;

    var accessKey = null;
    if (/ *\(\&([^&])\)(:)?$/.test(aLabel)) {
      aLabel = RegExp.leftContext + RegExp.$2;
      accessKey = RegExp.$1;
    } else if (/^(.*[^&])?\&(([^&]).*$)/.test(aLabel)) {
      aLabel = RegExp.$1 + RegExp.$2;
      accessKey = RegExp.$3;
    }

    // && is the magic sequence to embed an & in your label.
    aLabel = aLabel.replace(/\&\&/g, "&");
    if (aNode instanceof Ci.nsIDOMXULLabelElement) {
      aNode.setAttribute("value", aLabel);
    } else if (aNode instanceof Ci.nsIDOMXULDescriptionElement) {
      let text = aNode.ownerDocument.createTextNode(aLabel);
      aNode.appendChild(text);
    } else {    // Set text for other xul elements
      aNode.setAttribute("label", aLabel);
    }

    // XXXjag bug 325251
    // Need to set this after aNode.setAttribute("value", aLabel);
    if (accessKey)
      aNode.setAttribute("accesskey", accessKey);
  },

  /*
   * ---------- interface disambiguation ----------
   *
   * XXX Copied from nsPrompter.js.
   *
   * nsIPrompt and nsIAuthPrompt share 3 method names with slightly
   * different arguments. All but prompt() have the same number of
   * arguments, so look at the arg types to figure out how we're being
   * called. :-(
   */
  prompt: function prompt() {
    if (gPromptService.inContentProcess)
      return gPromptService.callProxy("prompt", [null].concat(Array.prototype.slice.call(arguments)));

    // also, the nsIPrompt flavor has 5 args instead of 6.
    if (typeof arguments[2] == "object")
      return this.nsIPrompt_prompt.apply(this, arguments);
    else
      return this.nsIAuthPrompt_prompt.apply(this, arguments);
  },

  promptUsernameAndPassword: function promptUsernameAndPassword() {
    // Both have 6 args, so use types.
    if (typeof arguments[2] == "object")
      return this.nsIPrompt_promptUsernameAndPassword.apply(this, arguments);
    else
      return this.nsIAuthPrompt_promptUsernameAndPassword.apply(this, arguments);
  },

  promptPassword: function promptPassword() {
    // Both have 5 args, so use types.
    if (typeof arguments[2] == "object")
      return this.nsIPrompt_promptPassword.apply(this, arguments);
    else
      return this.nsIAuthPrompt_promptPassword.apply(this, arguments);
  },

  /* ----------  nsIPrompt  ---------- */

  alert: function alert(aTitle, aText) {
    let dialog = this.openDialog("chrome://browser/content/prompt/alert.xul", null);
    let doc = this._doc;
    this._setupPrompt(doc, "alert", aTitle, aText);

    dialog.waitForClose();
  },

  alertCheck: function alertCheck(aTitle, aText, aCheckMsg, aCheckState) {
    let dialog = this.openDialog("chrome://browser/content/prompt/alert.xul", aCheckState);
    let doc = this._doc;
    this._setupPrompt(doc, "alert", aTitle, aText, {value: aCheckState.value, msg: aCheckMsg});
    dialog.waitForClose();
  },

  confirm: function confirm(aTitle, aText) {
    var params = new Object();
    params.result = false;

    let dialog = this.openDialog("chrome://browser/content/prompt/confirm.xul", params);
    let doc = this._doc;
    this._setupPrompt(doc, "confirm", aTitle, aText);

    dialog.waitForClose();
    return params.result;
  },

  confirmCheck: function confirmCheck(aTitle, aText, aCheckMsg, aCheckState) {
    var params = new Object();
    params.result = false;
    params.checkbox = aCheckState;

    let dialog = this.openDialog("chrome://browser/content/prompt/confirm.xul", params);
    let doc = this._doc;
    this._setupPrompt(doc, "confirm", aTitle, aText, {value: aCheckState.value, msg: aCheckMsg});

    dialog.waitForClose();
    return params.result;
  },

  confirmEx: function confirmEx(aTitle, aText, aButtonFlags, aButton0,
                      aButton1, aButton2, aCheckMsg, aCheckState) {

    let numButtons = 0;
    let titles = [aButton0, aButton1, aButton2];

    let defaultButton = 0;
    if (aButtonFlags & Ci.nsIPromptService.BUTTON_POS_1_DEFAULT)
      defaultButton = 1;
    if (aButtonFlags & Ci.nsIPromptService.BUTTON_POS_2_DEFAULT)
      defaultButton = 2;

    var params = {
      result: false,
      checkbox: aCheckState,
      defaultButton: defaultButton
    }

    let dialog = this.openDialog("chrome://browser/content/prompt/confirm.xul", params);
    let doc = this._doc;
    this._setupPrompt(doc, "confirm", aTitle, aText, {value: aCheckState.value, msg: aCheckMsg});

    let bbox = doc.getElementById("prompt-confirm-buttons-box");
    while (bbox.lastChild)
      bbox.removeChild(bbox.lastChild);

    for (let i = 0; i < 3; i++) {
      let bTitle = null;
      switch (aButtonFlags & 0xff) {
        case Ci.nsIPromptService.BUTTON_TITLE_OK :
          bTitle = PromptUtils.getLocaleString("OK");
        break;
        case Ci.nsIPromptService.BUTTON_TITLE_CANCEL :
          bTitle = PromptUtils.getLocaleString("Cancel");
        break;
        case Ci.nsIPromptService.BUTTON_TITLE_YES :
          bTitle = PromptUtils.getLocaleString("Yes");
        break;
        case Ci.nsIPromptService.BUTTON_TITLE_NO :
          bTitle = PromptUtils.getLocaleString("No");
        break;
        case Ci.nsIPromptService.BUTTON_TITLE_SAVE :
          bTitle = PromptUtils.getLocaleString("Save");
        break;
        case Ci.nsIPromptService.BUTTON_TITLE_DONT_SAVE :
          bTitle = PromptUtils.getLocaleString("DontSave");
        break;
        case Ci.nsIPromptService.BUTTON_TITLE_REVERT :
          bTitle = PromptUtils.getLocaleString("Revert");
        break;
        case Ci.nsIPromptService.BUTTON_TITLE_IS_STRING :
          bTitle = titles[i];
        break;
      }

      if (bTitle) {
        let button = doc.createElement("button");
        this.setLabelForNode(button, bTitle);
        if (i == defaultButton) {
          button.setAttribute("command", "cmd_ok");
        }
        else {
          button.setAttribute("oncommand",
            "document.getElementById('prompt-confirm-dialog').PromptHelper.closeConfirm(" + i + ")");
        }
        bbox.appendChild(button);
      }

      aButtonFlags >>= 8;
    }

    dialog.waitForClose();
    return params.result;
  },

  nsIPrompt_prompt: function nsIPrompt_prompt(aTitle, aText, aValue, aCheckMsg, aCheckState) {
    return this.commonPrompt(aTitle, aText, aValue, aCheckMsg, aCheckState, false);
  },

  nsIPrompt_promptPassword: function nsIPrompt_promptPassword(
      aTitle, aText, aPassword, aCheckMsg, aCheckState) {
    return this.commonPrompt(aTitle, aText, aPassword, aCheckMsg, aCheckState, true);
  },

  nsIPrompt_promptUsernameAndPassword: function nsIPrompt_promptUsernameAndPassword(
      aTitle, aText, aUsername, aPassword, aCheckMsg, aCheckState) {
    var params = new Object();
    params.result = false;
    params.checkbox = aCheckState;
    params.user = aUsername;
    params.password = aPassword;

    let dialog = this.openDialog("chrome://browser/content/prompt/promptPassword.xul", params);
    let doc = this._doc;
    this._setupPrompt(doc, "password", aTitle, aText, {value: aCheckState.value, msg: aCheckMsg});

    doc.getElementById("prompt-password-user").value = aUsername.value;
    doc.getElementById("prompt-password-password").value = aPassword.value;

    dialog.waitForClose();
    return params.result;
  },

  select: function select(aTitle, aText, aCount, aSelectList, aOutSelection) {
    var params = new Object();
    params.result = false;
    params.selection = aOutSelection;

    let dialog = this.openDialog("chrome://browser/content/prompt/select.xul", params);
    let doc = this._doc;
    this._setupPrompt(doc, "select", aTitle, aText);

    let list = doc.getElementById("prompt-select-list");
    for (let i = 0; i < aCount; i++)
      list.appendItem(aSelectList[i], null, null);

    // select the first one
    list.selectedIndex = 0;

    dialog.waitForClose();
    return params.result;
  },

  /* ----------  nsIAuthPrompt  ---------- */

  nsIAuthPrompt_prompt : function (title, text, passwordRealm, savePassword, defaultText, result) {
    // TODO: Port functions from nsLoginManagerPrompter.js to here
    if (defaultText)
      result.value = defaultText;
    return this.nsIPrompt_prompt(title, text, result, null, {});
  },

  nsIAuthPrompt_promptUsernameAndPassword : function (aTitle, aText, aPasswordRealm, aSavePassword, aUser, aPass) {
    return nsIAuthPrompt_loginPrompt(aTitle, aText, aPasswordRealm, aSavePassword, aUser, aPass);
  },

  nsIAuthPrompt_promptPassword : function (aTitle, aText, aPasswordRealm, aSavePassword, aPass) {
    return nsIAuthPrompt_loginPrompt(aTitle, aText, aPasswordRealm, aSavePassword, null, aPass);
  },

  nsIAuthPrompt_loginPrompt: function(aTitle, aPasswordRealm, aSavePassword, aUser, aPass) {
    let checkMsg = null;
    let check = { value: false };
    let [hostname, realm, aUser] = PromptUtils.getHostnameAndRealm(aPasswordRealm);

    let canSave = PromptUtils.canSaveLogin(hostname, aSavePassword);
    if (canSave) {
      // Look for existing logins.
      let foundLogins = PromptUtils.pwmgr.findLogins({}, hostname, null, realm);
      [checkMsg, check] = PromptUtils.getUsernameAndPassword(foundLogins, aUser, aPass);
    }

    let ok = false;
    if (aUser)
      ok = this.nsIPrompt_promptUsernameAndPassword(aTitle, aText, aUser, aPass, checkMsg, check);
    else
      ok = this.nsIPrompt_promptPassword(aTitle, aText, aPass, checkMsg, check);

    if (ok && canSave && check.value)
      PromptUtils.savePassword(hostname, realm, aUser, aPass);

    return ok;  },

  /* ----------  nsIAuthPrompt2  ---------- */

  promptAuth: function promptAuth(aChannel, aLevel, aAuthInfo) {
    let checkMsg = null;
    let check = { value: false };
    let message = PromptUtils.makeDialogText(aChannel, aAuthInfo);
    let [username, password] = PromptUtils.getAuthInfo(aAuthInfo);
    let [hostname, httpRealm] = PromptUtils.getAuthTarget(aChannel, aAuthInfo);
    let foundLogins = PromptUtils.pwmgr.findLogins({}, hostname, null, httpRealm);

    let canSave = PromptUtils.canSaveLogin(hostname, null);
    if (canSave)
      [checkMsg, check] = PromptUtils.getUsernameAndPassword(foundLogins, username, password);

    if (username.value && password.value) {
      PromptUtils.setAuthInfo(aAuthInfo, username.value, password.value);
    }

    let canAutologin = false;
    if (aAuthInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY &&
        !(aAuthInfo.flags & Ci.nsIAuthInformation.PREVIOUS_FAILED) &&
        Services.prefs.getBoolPref("signon.autologin.proxy"))
      canAutologin = true;

    let ok = canAutologin;
    if (!ok && aAuthInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD)
      ok = this.nsIPrompt_promptPassword(null, message, password, checkMsg, check);
    else if (!ok)
      ok = this.nsIPrompt_promptUsernameAndPassword(null, message, username, password, checkMsg, check);

    PromptUtils.setAuthInfo(aAuthInfo, username.value, password.value);

    if (ok && canSave && check.value)
      PromptUtils.savePassword(foundLogins, username, password, hostname, httpRealm);

    return ok;
  },

  _asyncPrompts: {},
  _asyncPromptInProgress: false,

  _doAsyncPrompt : function() {
    if (this._asyncPromptInProgress)
      return;

    // Find the first prompt key we have in the queue
    let hashKey = null;
    for (hashKey in this._asyncPrompts)
      break;

    if (!hashKey)
      return;

    // If login manger has logins for this host, defer prompting if we're
    // already waiting on a master password entry.
    let prompt = this._asyncPrompts[hashKey];
    let prompter = prompt.prompter;
    let [hostname, httpRealm] = PromptUtils.getAuthTarget(prompt.channel, prompt.authInfo);
    let foundLogins = PromptUtils.pwmgr.findLogins({}, hostname, null, httpRealm);
    if (foundLogins.length > 0 && PromptUtils.pwmgr.uiBusy)
      return;

    this._asyncPromptInProgress = true;
    prompt.inProgress = true;

    let self = this;

    let runnable = {
      run: function() {
        let ok = false;
        try {
          ok = prompter.promptAuth(prompt.channel, prompt.level, prompt.authInfo);
        } catch (e) {
          Cu.reportError("_doAsyncPrompt:run: " + e + "\n");
        }

        delete self._asyncPrompts[hashKey];
        prompt.inProgress = false;
        self._asyncPromptInProgress = false;

        for each (let consumer in prompt.consumers) {
          if (!consumer.callback)
            // Not having a callback means that consumer didn't provide it
            // or canceled the notification
            continue;

          try {
            if (ok)
              consumer.callback.onAuthAvailable(consumer.context, prompt.authInfo);
            else
              consumer.callback.onAuthCancelled(consumer.context, true);
          } catch (e) { /* Throw away exceptions caused by callback */ }
        }
        self._doAsyncPrompt();
      }
    }

    Services.tm.mainThread.dispatch(runnable, Ci.nsIThread.DISPATCH_NORMAL);
  },

  asyncPromptAuth: function asyncPromptAuth(aChannel, aCallback, aContext, aLevel, aAuthInfo) {
    let cancelable = null;
    try {
      // If the user submits a login but it fails, we need to remove the
      // notification bar that was displayed. Conveniently, the user will
      // be prompted for authentication again, which brings us here.
      //this._removeLoginNotifications();

      cancelable = {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
        callback: aCallback,
        context: aContext,
        cancel: function() {
          this.callback.onAuthCancelled(this.context, false);
          this.callback = null;
          this.context = null;
        }
      };
      let [hostname, httpRealm] = PromptUtils.getAuthTarget(aChannel, aAuthInfo);
      let hashKey = aLevel + "|" + hostname + "|" + httpRealm;
      let asyncPrompt = this._asyncPrompts[hashKey];
      if (asyncPrompt) {
        asyncPrompt.consumers.push(cancelable);
        return cancelable;
      }

      asyncPrompt = {
        consumers: [cancelable],
        channel: aChannel,
        authInfo: aAuthInfo,
        level: aLevel,
        inProgress : false,
        prompter: this
      }

      this._asyncPrompts[hashKey] = asyncPrompt;
      this._doAsyncPrompt();
    } catch (e) {
      Cu.reportError("PromptService: " + e + "\n");
      throw e;
    }
    return cancelable;
  }
};

let PromptUtils = {
  getLocaleString: function pu_getLocaleString(aKey, aService) {
    if (aService == "passwdmgr")
      return this.passwdBundle.GetStringFromName(aKey);

    return this.bundle.GetStringFromName(aKey);
  },

  get pwmgr() {
    delete this.pwmgr;
    return this.pwmgr = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
  },

  getHostnameAndRealm: function pu_getHostnameAndRealm(aRealmString) {
    let httpRealm = /^.+ \(.+\)$/;
    if (httpRealm.test(aRealmString))
      return [null, null, null];

    let uri = Services.io.newURI(aRealmString, null, null);
    let pathname = "";

    if (uri.path != "/")
      pathname = uri.path;

    let formattedHostname = this._getFormattedHostname(uri);
    return [formattedHostname, formattedHostname + pathname, uri.username];
  },

  canSaveLogin: function pu_canSaveLogin(aHostname, aSavePassword) {
    let canSave = !this._inPrivateBrowsing && this.pwmgr.getLoginSavingEnabled(aHostname)
    if (aSavePassword)
      canSave = canSave && (aSavePassword == Ci.nsIAuthPrompt.SAVE_PASSWORD_PERMANENTLY)
    return canSave;
  },

  getUsernameAndPassword: function pu_getUsernameAndPassword(aFoundLogins, aUser, aPass) {
    let checkLabel = null;
    let check = { value: false };
    let selectedLogin;

    checkLabel = this.getLocaleString("rememberPassword", "passwdmgr");

    // XXX Like the original code, we can't deal with multiple
    // account selection. (bug 227632)
    if (aFoundLogins.length > 0) {
      selectedLogin = aFoundLogins[0];

      // If the caller provided a username, try to use it. If they
      // provided only a password, this will try to find a password-only
      // login (or return null if none exists).
      if (aUser.value)
        selectedLogin = this.findLogin(aFoundLogins, "username", aUser.value);

      if (selectedLogin) {
        check.value = true;
        aUser.value = selectedLogin.username;
        // If the caller provided a password, prefer it.
        if (!aPass.value)
          aPass.value = selectedLogin.password;
      }
    }

    return [checkLabel, check];
  },

  findLogin: function pu_findLogin(aLogins, aName, aValue) {
    for (let i = 0; i < aLogins.length; i++)
      if (aLogins[i][aName] == aValue)
        return aLogins[i];
    return null;
  },

  savePassword: function pu_savePassword(aLogins, aUser, aPass, aHostname, aRealm) {
    let selectedLogin = this.findLogin(aLogins, "username", aUser.value);

    // If we didn't find an existing login, or if the username
    // changed, save as a new login.
    if (!selectedLogin) {
      // add as new
      var newLogin = Cc["@mozilla.org/login-manager/loginInfo;1"].createInstance(Ci.nsILoginInfo);
      newLogin.init(aHostname, null, aRealm, aUser.value, aPass.value, "", "");
      this.pwmgr.addLogin(newLogin);
    } else if (aPass.value != selectedLogin.password) {
      // update password
      this.updateLogin(selectedLogin, aPass.value);
    } else {
      this.updateLogin(selectedLogin);
    }
  },

  updateLogin: function pu_updateLogin(aLogin, aPassword) {
    let now = Date.now();
    let propBag = Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag);
    if (aPassword) {
      propBag.setProperty("password", aPassword);
      // Explicitly set the password change time here (even though it would
      // be changed automatically), to ensure that it's exactly the same
      // value as timeLastUsed.
      propBag.setProperty("timePasswordChanged", now);
    }
    propBag.setProperty("timeLastUsed", now);
    propBag.setProperty("timesUsedIncrement", 1);

    this.pwmgr.modifyLogin(aLogin, propBag);
  },

  // JS port of http://mxr.mozilla.org/mozilla-central/source/embedding/components/windowwatcher/src/nsPrompt.cpp#388
  makeDialogText: function pu_makeDialogText(aChannel, aAuthInfo) {
    let isProxy    = (aAuthInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY);
    let isPassOnly = (aAuthInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD);

    let username = aAuthInfo.username;
    let [displayHost, realm] = this.getAuthTarget(aChannel, aAuthInfo);

    // Suppress "the site says: $realm" when we synthesized a missing realm.
    if (!aAuthInfo.realm && !isProxy)
    realm = "";

    // Trim obnoxiously long realms.
    if (realm.length > 150) {
      realm = realm.substring(0, 150);
      // Append "..." (or localized equivalent).
      realm += this.ellipsis;
    }

    let text;
    if (isProxy)
      text = this.bundle.formatStringFromName("EnterLoginForProxy", [realm, displayHost], 2);
    else if (isPassOnly)
      text = this.bundle.formatStringFromName("EnterPasswordFor", [username, displayHost], 2);
    else if (!realm)
      text = this.bundle.formatStringFromName("EnterUserPasswordFor", [displayHost], 1);
    else
      text = this.bundle.formatStringFromName("EnterLoginForRealm", [realm, displayHost], 2);

    return text;
  },

  // JS port of http://mxr.mozilla.org/mozilla-central/source/embedding/components/windowwatcher/public/nsPromptUtils.h#89
  getAuthHostPort: function pu_getAuthHostPort(aChannel, aAuthInfo) {
    let uri = aChannel.URI;
    let res = { host: null, port: -1 };
    if (aAuthInfo.flags & aAuthInfo.AUTH_PROXY) {
      let proxy = aChannel.QueryInterface(Ci.nsIProxiedChannel);
      res.host = proxy.proxyInfo.host;
      res.port = proxy.proxyInfo.port;
    } else {
      res.host = uri.host;
      res.port = uri.port;
    }
    return res;
  },

  getAuthTarget : function pu_getAuthTarget(aChannel, aAuthInfo) {
    let hostname, realm;
    // If our proxy is demanding authentication, don't use the
    // channel's actual destination.
    if (aAuthInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY) {
        if (!(aChannel instanceof Ci.nsIProxiedChannel))
          throw "proxy auth needs nsIProxiedChannel";

      let info = aChannel.proxyInfo;
      if (!info)
        throw "proxy auth needs nsIProxyInfo";

      // Proxies don't have a scheme, but we'll use "moz-proxy://"
      // so that it's more obvious what the login is for.
      let idnService = Cc["@mozilla.org/network/idn-service;1"].getService(Ci.nsIIDNService);
      hostname = "moz-proxy://" + idnService.convertUTF8toACE(info.host) + ":" + info.port;
      realm = aAuthInfo.realm;
      if (!realm)
        realm = hostname;

      return [hostname, realm];
    }
    hostname = this.getFormattedHostname(aChannel.URI);

    // If a HTTP WWW-Authenticate header specified a realm, that value
    // will be available here. If it wasn't set or wasn't HTTP, we'll use
    // the formatted hostname instead.
    realm = aAuthInfo.realm;
    if (!realm)
      realm = hostname;

    return [hostname, realm];
  },

  getAuthInfo : function pu_getAuthInfo(aAuthInfo) {
    let flags = aAuthInfo.flags;
    let username = {value: ""};
    let password = {value: ""};

    if (flags & Ci.nsIAuthInformation.NEED_DOMAIN && aAuthInfo.domain)
      username.value = aAuthInfo.domain + "\\" + aAuthInfo.username;
    else
      username.value = aAuthInfo.username;

    password.value = aAuthInfo.password

    return [username, password];
  },

  setAuthInfo : function (aAuthInfo, username, password) {
    var flags = aAuthInfo.flags;
    if (flags & Ci.nsIAuthInformation.NEED_DOMAIN) {
      // Domain is separated from username by a backslash
      var idx = username.indexOf("\\");
      if (idx == -1) {
        aAuthInfo.username = username;
      } else {
        aAuthInfo.domain   =  username.substring(0, idx);
        aAuthInfo.username =  username.substring(idx+1);
      }
    } else {
      aAuthInfo.username = username;
    }
    aAuthInfo.password = password;
  },

  getFormattedHostname : function pu_getFormattedHostname(uri) {
    let scheme = uri.scheme;
    let hostname = scheme + "://" + uri.host;

    // If the URI explicitly specified a port, only include it when
    // it's not the default. (We never want "http://foo.com:80")
    port = uri.port;
    if (port != -1) {
      let handler = Services.io.getProtocolHandler(scheme);
      if (port != handler.defaultPort)
        hostname += ":" + port;
    }
    return hostname;
  },
};

XPCOMUtils.defineLazyGetter(PromptUtils, "passwdBundle", function () {
  return Services.strings.createBundle("chrome://browser/locale/passwordmgr.properties");
});

XPCOMUtils.defineLazyGetter(PromptUtils, "bundle", function () {
  return Services.strings.createBundle("chrome://global/locale/commonDialogs.properties");
});

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PromptService]);
