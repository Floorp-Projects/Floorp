/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Weave.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Edward Lee <edilee@mozilla.com>
 *   Mike Connor <mconnor@mozilla.com>
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *   Paul O’Shannessy <paul@oshannessy.com>
 *   Richard Newman <rnewman@mozilla.com>
 *   Allison Naaktgeboren <ally@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

// page consts

const PAIR_PAGE                     = 0;
const INTRO_PAGE                    = 1;
const NEW_ACCOUNT_START_PAGE        = 2;
const EXISTING_ACCOUNT_CONNECT_PAGE = 3;
const EXISTING_ACCOUNT_LOGIN_PAGE   = 4;
const OPTIONS_PAGE                  = 5;
const OPTIONS_CONFIRM_PAGE          = 6;

// Broader than we'd like, but after this changed from api-secure.recaptcha.net
// we had no choice. At least we only do this for the duration of setup.
// See discussion in Bugs 508112 and 653307.
const RECAPTCHA_DOMAIN = "https://www.google.com";

const PIN_PART_LENGTH = 4;

Cu.import("resource://services-sync/main.js");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/PluralForm.jsm");


function setVisibility(element, visible) {
  element.style.visibility = visible ? "visible" : "hidden";
}

var gSyncSetup = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),

  captchaBrowser: null,
  wizard: null,
  _disabledSites: [],

  status: {
    password: false,
    email: false,
    server: false
  },

  get _remoteSites() [Weave.Service.serverURL, RECAPTCHA_DOMAIN],

  get _usingMainServers() {
    if (this._settingUpNew)
      return document.getElementById("server").selectedIndex == 0;
    return document.getElementById("existingServer").selectedIndex == 0;
  },

  init: function () {
    let obs = [
      ["weave:service:changepph:finish", "onResetPassphrase"],
      ["weave:service:login:start",  "onLoginStart"],
      ["weave:service:login:error",  "onLoginEnd"],
      ["weave:service:login:finish", "onLoginEnd"]];

    // Add the observers now and remove them on unload
    let self = this;
    let addRem = function(add) {
      obs.forEach(function([topic, func]) {
        //XXXzpao This should use Services.obs.* but Weave's Obs does nice handling
        //        of `this`. Fix in a followup. (bug 583347)
        if (add)
          Weave.Svc.Obs.add(topic, self[func], self);
        else
          Weave.Svc.Obs.remove(topic, self[func], self);
      });
    };
    addRem(true);
    window.addEventListener("unload", function() addRem(false), false);

    window.setTimeout(function () {
      // Force Service to be loaded so that engines are registered.
      // See Bug 670082.
      Weave.Service;
    }, 0);

    this.captchaBrowser = document.getElementById("captcha");

    this.wizardType = null;
    if (window.arguments && window.arguments[0]) {
      this.wizardType = window.arguments[0];
    }
    switch (this.wizardType) {
      case null:
        this.wizard.pageIndex = INTRO_PAGE;
        // Fall through!
      case "pair":
        this.captchaBrowser.addProgressListener(this);
        Weave.Svc.Prefs.set("firstSync", "notReady");
        break;
      case "reset":
        this._resettingSync = true;
        this.wizard.pageIndex = OPTIONS_PAGE;
        break;
    }

    this.wizard.getButton("extra1").label =
      this._stringBundle.GetStringFromName("button.syncOptions.label");

    // Remember these values because the options pages change them temporarily.
    this._nextButtonLabel = this.wizard.getButton("next").label;
    this._nextButtonAccesskey = this.wizard.getButton("next")
                                           .getAttribute("accesskey");
    this._backButtonLabel = this.wizard.getButton("back").label;
    this._backButtonAccesskey = this.wizard.getButton("back")
                                           .getAttribute("accesskey");
  },

  startNewAccountSetup: function () {
    if (!Weave.Utils.ensureMPUnlocked())
      return false;
    this._settingUpNew = true;
    this.wizard.pageIndex = NEW_ACCOUNT_START_PAGE;
  },

  useExistingAccount: function () {
    if (!Weave.Utils.ensureMPUnlocked())
      return false;
    this._settingUpNew = false;
    if (this.wizardType == "pair") {
      // We're already pairing, so there's no point in pairing again.
      // Go straight to the manual login page.
      this.wizard.pageIndex = EXISTING_ACCOUNT_LOGIN_PAGE;
    } else {
      this.wizard.pageIndex = EXISTING_ACCOUNT_CONNECT_PAGE;
    }
  },

  resetPassphrase: function resetPassphrase() {
    // Apply the existing form fields so that
    // Weave.Service.changePassphrase() has the necessary credentials.
    Weave.Service.account = document.getElementById("existingAccountName").value;
    Weave.Service.password = document.getElementById("existingPassword").value;

    // Generate a new passphrase so that Weave.Service.login() will
    // actually do something.
    let passphrase = Weave.Utils.generatePassphrase();
    Weave.Service.passphrase = passphrase;

    // Only open the dialog if username + password are actually correct.
    Weave.Service.login();
    if ([Weave.LOGIN_FAILED_INVALID_PASSPHRASE,
         Weave.LOGIN_FAILED_NO_PASSPHRASE,
         Weave.LOGIN_SUCCEEDED].indexOf(Weave.Status.login) == -1) {
      return;
    }

    // Hide any errors about the passphrase, we know it's not right.
    let feedback = document.getElementById("existingPassphraseFeedbackRow");
    feedback.hidden = true;
    let el = document.getElementById("existingPassphrase");
    el.value = Weave.Utils.hyphenatePassphrase(passphrase);

    // changePassphrase() will sync, make sure we set the "firstSync" pref
    // according to the user's pref.
    Weave.Svc.Prefs.reset("firstSync");
    this.setupInitialSync();
    gSyncUtils.resetPassphrase(true);
  },

  onResetPassphrase: function () {
    document.getElementById("existingPassphrase").value = 
      Weave.Utils.hyphenatePassphrase(Weave.Service.passphrase);
    this.checkFields();
    this.wizard.advance();
  },

  onLoginStart: function () {
    this.toggleLoginFeedback(false);
  },

  onLoginEnd: function () {
    this.toggleLoginFeedback(true);
  },

  sendCredentialsAfterSync: function () {
    let send = function() {
      Services.obs.removeObserver("weave:service:sync:finish", send);
      Services.obs.removeObserver("weave:service:sync:error", send);
      let credentials = {account:   Weave.Service.account,
                         password:  Weave.Service.password,
                         synckey:   Weave.Service.passphrase,
                         serverURL: Weave.Service.serverURL};
      this._jpakeclient.sendAndComplete(credentials);
    }.bind(this);
    Services.obs.addObserver("weave:service:sync:finish", send, false);
    Services.obs.addObserver("weave:service:sync:error", send, false);
  },

  toggleLoginFeedback: function (stop) {
    document.getElementById("login-throbber").hidden = stop;
    let password = document.getElementById("existingPasswordFeedbackRow");
    let server = document.getElementById("existingServerFeedbackRow");
    let passphrase = document.getElementById("existingPassphraseFeedbackRow");

    if (!stop || (Weave.Status.login == Weave.LOGIN_SUCCEEDED)) {
      password.hidden = server.hidden = passphrase.hidden = true;
      return;
    }

    let feedback;
    switch (Weave.Status.login) {
      case Weave.LOGIN_FAILED_NETWORK_ERROR:
      case Weave.LOGIN_FAILED_SERVER_ERROR:
        feedback = server;
        break;
      case Weave.LOGIN_FAILED_LOGIN_REJECTED:
      case Weave.LOGIN_FAILED_NO_USERNAME:
      case Weave.LOGIN_FAILED_NO_PASSWORD:
        feedback = password;
        break;
      case Weave.LOGIN_FAILED_INVALID_PASSPHRASE:
        feedback = passphrase;
        break;
    }
    this._setFeedbackMessage(feedback, false, Weave.Status.login);
  },

  setupInitialSync: function () {
    let action = document.getElementById("mergeChoiceRadio").selectedItem.id;
    switch (action) {
      case "resetClient":
        // if we're not resetting sync, we don't need to explicitly
        // call resetClient
        if (!this._resettingSync)
          return;
        // otherwise, fall through
      case "wipeClient":
      case "wipeRemote":
        Weave.Svc.Prefs.set("firstSync", action);
        break;
    }
  },

  // fun with validation!
  checkFields: function () {
    this.wizard.canAdvance = this.readyToAdvance();
  },

  readyToAdvance: function () {
    switch (this.wizard.pageIndex) {
      case INTRO_PAGE:
        return false;
      case NEW_ACCOUNT_START_PAGE:
        for (let i in this.status) {
          if (!this.status[i])
            return false;
        }
        if (this._usingMainServers)
          return document.getElementById("tos").checked;

        return true;
      case EXISTING_ACCOUNT_LOGIN_PAGE:
        let hasUser = document.getElementById("existingAccountName").value != "";
        let hasPass = document.getElementById("existingPassword").value != "";
        let hasKey = document.getElementById("existingPassphrase").value != "";

        if (hasUser && hasPass && hasKey) {
          if (this._usingMainServers)
            return true;

          if (this._validateServer(document.getElementById("existingServer"))) {
            return true;
          }
        }
        return false;
    }
    // Default, e.g. wizard's special page -1 etc.
    return true;
  },

  onPINInput: function onPINInput(textbox) {
    if (textbox && textbox.value.length == PIN_PART_LENGTH) {
      this.nextFocusEl[textbox.id].focus();
    }
    this.wizard.canAdvance = (this.pin1.value.length == PIN_PART_LENGTH &&
                              this.pin2.value.length == PIN_PART_LENGTH &&
                              this.pin3.value.length == PIN_PART_LENGTH);
  },

  onEmailInput: function () {
    // Check account validity when the user stops typing for 1 second.
    if (this._checkAccountTimer)
      window.clearTimeout(this._checkAccountTimer);
    this._checkAccountTimer = window.setTimeout(function () {
      gSyncSetup.checkAccount();
    }, 1000);
  },

  checkAccount: function() {
    delete this._checkAccountTimer;
    let value = Weave.Utils.normalizeAccount(
      document.getElementById("weaveEmail").value);
    if (!value) {
      this.status.email = false;
      this.checkFields();
      return;
    }

    let re = /^(([^<>()[\]\\.,;:\s@\"]+(\.[^<>()[\]\\.,;:\s@\"]+)*)|(\".+\"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/;
    let feedback = document.getElementById("emailFeedbackRow");
    let valid = re.test(value);

    let str = "";
    if (!valid) {
      str = "invalidEmail.label";
    } else {
      let availCheck = Weave.Service.checkAccount(value);
      valid = availCheck == "available";
      if (!valid) {
        if (availCheck == "notAvailable")
          str = "usernameNotAvailable.label";
        else
          str = availCheck;
      }
    }

    this._setFeedbackMessage(feedback, valid, str);
    this.status.email = valid;
    if (valid)
      Weave.Service.account = value;
    this.checkFields();
  },

  onPasswordChange: function () {
    let password = document.getElementById("weavePassword");
    let pwconfirm = document.getElementById("weavePasswordConfirm");
    let [valid, errorString] = gSyncUtils.validatePassword(password, pwconfirm);

    let feedback = document.getElementById("passwordFeedbackRow");
    this._setFeedback(feedback, valid, errorString);

    this.status.password = valid;
    this.checkFields();
  },

  onPageShow: function() {
    switch (this.wizard.pageIndex) {
      case PAIR_PAGE:
        this.wizard.getButton("back").hidden = true;
        this.wizard.getButton("extra1").hidden = true;
        this.onPINInput();
        this.pin1.focus();
        break;
      case INTRO_PAGE:
        // We may not need the captcha in the Existing Account branch of the
        // wizard. However, we want to preload it to avoid any flickering while
        // the Create Account page is shown.
        this.loadCaptcha();
        this.wizard.getButton("next").hidden = true;
        this.wizard.getButton("back").hidden = true;
        this.wizard.getButton("extra1").hidden = true;
        this.checkFields();
        break;
      case NEW_ACCOUNT_START_PAGE:
        this.wizard.getButton("extra1").hidden = false;
        this.wizard.getButton("next").hidden = false;
        this.wizard.getButton("back").hidden = false;
        this.onServerCommand();
        this.wizard.canRewind = true;
        this.checkFields();
        break;
      case EXISTING_ACCOUNT_CONNECT_PAGE:
        Weave.Svc.Prefs.set("firstSync", "existingAccount");
        this.wizard.getButton("next").hidden = false;
        this.wizard.getButton("back").hidden = false;
        this.wizard.getButton("extra1").hidden = false;
        this.wizard.canAdvance = false;
        this.wizard.canRewind = true;
        this.startEasySetup();
        break;
      case EXISTING_ACCOUNT_LOGIN_PAGE:
        this.wizard.getButton("next").hidden = false;
        this.wizard.getButton("back").hidden = false;
        this.wizard.getButton("extra1").hidden = false;
        this.wizard.canRewind = true;
        this.checkFields();
        break;
      case OPTIONS_PAGE:
        this.wizard.canRewind = false;
        this.wizard.canAdvance = true;
        if (!this._resettingSync) {
          this.wizard.getButton("next").label =
            this._stringBundle.GetStringFromName("button.syncOptionsDone.label");
          this.wizard.getButton("next").removeAttribute("accesskey");
        }
        this.wizard.getButton("next").hidden = false;
        this.wizard.getButton("back").hidden = true;
        this.wizard.getButton("cancel").hidden = !this._resettingSync;
        this.wizard.getButton("extra1").hidden = true;
        document.getElementById("syncComputerName").value = Weave.Clients.localName;
        document.getElementById("syncOptions").collapsed = this._resettingSync;
        document.getElementById("mergeOptions").collapsed = this._settingUpNew;
        break;
      case OPTIONS_CONFIRM_PAGE:
        this.wizard.canRewind = true;
        this.wizard.canAdvance = true;
        this.wizard.getButton("back").label =
          this._stringBundle.GetStringFromName("button.syncOptionsCancel.label");
        this.wizard.getButton("back").removeAttribute("accesskey");
        this.wizard.getButton("back").hidden = this._resettingSync;
        this.wizard.getButton("next").hidden = false;
        this.wizard.getButton("finish").hidden = true;
        break;
    }
  },

  onWizardAdvance: function () {
    // Check pageIndex so we don't prompt before the Sync setup wizard appears.
    // This is a fallback in case the Master Password gets locked mid-wizard.
    if ((this.wizard.pageIndex >= 0) &&
        !Weave.Utils.ensureMPUnlocked()) {
      return false;
    }

    switch (this.wizard.pageIndex) {
      case PAIR_PAGE:
        this.startPairing();
        return false;
      case NEW_ACCOUNT_START_PAGE:
        // If the user selects Next (e.g. by hitting enter) when we haven't
        // executed the delayed checks yet, execute them immediately.
        if (this._checkAccountTimer) {
          this.checkAccount();
        }
        if (this._checkServerTimer) {
          this.checkServer();
        }
        if (!this.wizard.canAdvance) {
          return false;
        }

        let doc = this.captchaBrowser.contentDocument;
        let getField = function getField(field) {
          let node = doc.getElementById("recaptcha_" + field + "_field");
          return node && node.value;
        };

        // Display throbber
        let feedback = document.getElementById("captchaFeedback");
        let image = feedback.firstChild;
        let label = image.nextSibling;
        image.setAttribute("status", "active");
        label.value = this._stringBundle.GetStringFromName("verifying.label");
        setVisibility(feedback, true);

        let password = document.getElementById("weavePassword").value;
        let email = Weave.Utils.normalizeAccount(
          document.getElementById("weaveEmail").value);
        let challenge = getField("challenge");
        let response = getField("response");

        let error = Weave.Service.createAccount(email, password,
                                                challenge, response);

        if (error == null) {
          Weave.Service.account = email;
          Weave.Service.password = password;
          Weave.Service.passphrase = Weave.Utils.generatePassphrase();
          this._handleNoScript(false);
          Weave.Svc.Prefs.set("firstSync", "newAccount");
          this.wizardFinish();
          return false;
        }

        image.setAttribute("status", "error");
        label.value = Weave.Utils.getErrorString(error);
        return false;
      case EXISTING_ACCOUNT_LOGIN_PAGE:
        Weave.Service.account = Weave.Utils.normalizeAccount(
          document.getElementById("existingAccountName").value);
        Weave.Service.password = document.getElementById("existingPassword").value;
        let pp = document.getElementById("existingPassphrase").value;
        Weave.Service.passphrase = Weave.Utils.normalizePassphrase(pp);
        if (Weave.Service.login()) {
          this.wizardFinish();
        }
        return false;
      case OPTIONS_PAGE:
        let desc = document.getElementById("mergeChoiceRadio").selectedIndex;
        // No confirmation needed on new account setup or merge option
        // with existing account.
        if (this._settingUpNew || (!this._resettingSync && desc == 0))
          return this.returnFromOptions();
        return this._handleChoice();
      case OPTIONS_CONFIRM_PAGE:
        if (this._resettingSync) {
          this.wizardFinish();
          return false;
        }
        return this.returnFromOptions();
    }
    return true;
  },

  onWizardBack: function () {
    switch (this.wizard.pageIndex) {
      case NEW_ACCOUNT_START_PAGE:
      case EXISTING_ACCOUNT_LOGIN_PAGE:
        this.wizard.pageIndex = INTRO_PAGE;
        return false;
      case EXISTING_ACCOUNT_CONNECT_PAGE:
        this.abortEasySetup();
        this.wizard.pageIndex = INTRO_PAGE;
        return false;
      case EXISTING_ACCOUNT_LOGIN_PAGE:
        // If we were already pairing on entry, we went straight to the manual
        // login page. If subsequently we go back, return to the page that lets
        // us choose whether we already have an account.
        if (this.wizardType == "pair") {
          this.wizard.pageIndex = INTRO_PAGE;
          return false;
        }
        return true;
      case OPTIONS_CONFIRM_PAGE:
        // Backing up from the confirmation page = resetting first sync to merge.
        document.getElementById("mergeChoiceRadio").selectedIndex = 0;
        return this.returnFromOptions();
    }
    return true;
  },

  wizardFinish: function () {
    this.setupInitialSync();

    if (this.wizardType == "pair") {
      this.completePairing();
    }

    if (!this._resettingSync) {
      function isChecked(element) {
        return document.getElementById(element).hasAttribute("checked");
      }

      let prefs = ["engine.bookmarks", "engine.passwords", "engine.history",
                   "engine.tabs", "engine.prefs", "engine.addons"];
      for (let i = 0;i < prefs.length;i++) {
        Weave.Svc.Prefs.set(prefs[i], isChecked(prefs[i]));
      }
      this._handleNoScript(false);
      if (Weave.Svc.Prefs.get("firstSync", "") == "notReady")
        Weave.Svc.Prefs.reset("firstSync");

      Weave.Service.persistLogin();
      Weave.Svc.Obs.notify("weave:service:setup-complete");

      gSyncUtils.openFirstSyncProgressPage();
    }
    Weave.Utils.nextTick(Weave.Service.sync, Weave.Service);
    window.close();
  },

  onWizardCancel: function () {
    if (this._resettingSync)
      return;

    this.abortEasySetup();
    this._handleNoScript(false);
    Weave.Service.startOver();
  },

  onSyncOptions: function () {
    this._beforeOptionsPage = this.wizard.pageIndex;
    this.wizard.pageIndex = OPTIONS_PAGE;
  },

  returnFromOptions: function() {
    this.wizard.getButton("next").label = this._nextButtonLabel;
    this.wizard.getButton("next").setAttribute("accesskey",
                                               this._nextButtonAccesskey);
    this.wizard.getButton("back").label = this._backButtonLabel;
    this.wizard.getButton("back").setAttribute("accesskey",
                                               this._backButtonAccesskey);
    this.wizard.getButton("cancel").hidden = false;
    this.wizard.getButton("extra1").hidden = false;
    this.wizard.pageIndex = this._beforeOptionsPage;
    return false;
  },

  startPairing: function startPairing() {
    this.pairDeviceErrorRow.hidden = true;
    // When onAbort is called, Weave may already be gone.
    const JPAKE_ERROR_USERABORT = Weave.JPAKE_ERROR_USERABORT;

    let self = this;
    let jpakeclient = this._jpakeclient = new Weave.JPAKEClient({
      onPaired: function onPaired() {
        self.wizard.pageIndex = INTRO_PAGE;
      },
      onComplete: function onComplete() {
        // This method will never be called since SendCredentialsController
        // will take over after the wizard completes.
      },
      onAbort: function onAbort(error) {
        delete self._jpakeclient;

        // Aborted by user, ignore. The window is almost certainly going to close
        // or is already closed.
        if (error == JPAKE_ERROR_USERABORT) {
          return;
        }

        self.pairDeviceErrorRow.hidden = false;
        self.pairDeviceThrobber.hidden = true;
        self.pin1.value = self.pin2.value = self.pin3.value = "";
        self.pin1.disabled = self.pin2.disabled = self.pin3.disabled = false;
        if (self.wizard.pageIndex == PAIR_PAGE) {
          self.pin1.focus();
        }
      }
    });
    this.pairDeviceThrobber.hidden = false;
    this.pin1.disabled = this.pin2.disabled = this.pin3.disabled = true;
    this.wizard.canAdvance = false;

    let pin = this.pin1.value + this.pin2.value + this.pin3.value;
    let expectDelay = true;
    jpakeclient.pairWithPIN(pin, expectDelay);
  },

  completePairing: function completePairing() {
    if (!this._jpakeclient) {
      // The channel was aborted while we were setting up the account
      // locally. XXX TODO should we do anything here, e.g. tell
      // the user on the last wizard page that it's ok, they just
      // have to pair again?
      return;
    }
    let controller = new Weave.SendCredentialsController(this._jpakeclient);
    this._jpakeclient.controller = controller;
  },

  startEasySetup: function () {
    // Don't do anything if we have a client already (e.g. we went to
    // Sync Options and just came back).
    if (this._jpakeclient)
      return;

    // When onAbort is called, Weave may already be gone
    const JPAKE_ERROR_USERABORT = Weave.JPAKE_ERROR_USERABORT;

    let self = this;
    this._jpakeclient = new Weave.JPAKEClient({
      displayPIN: function displayPIN(pin) {
        document.getElementById("easySetupPIN1").value = pin.slice(0, 4);
        document.getElementById("easySetupPIN2").value = pin.slice(4, 8);
        document.getElementById("easySetupPIN3").value = pin.slice(8);
      },

      onPairingStart: function onPairingStart() {},

      onComplete: function onComplete(credentials) {
        Weave.Service.account = credentials.account;
        Weave.Service.password = credentials.password;
        Weave.Service.passphrase = credentials.synckey;
        Weave.Service.serverURL = credentials.serverURL;
        gSyncSetup.wizardFinish();
      },

      onAbort: function onAbort(error) {
        delete self._jpakeclient;

        // Ignore if wizard is aborted.
        if (error == JPAKE_ERROR_USERABORT)
          return;

        // Automatically go to manual setup if we couldn't acquire a channel.
        if (error == Weave.JPAKE_ERROR_CHANNEL) {
          self.wizard.pageIndex = EXISTING_ACCOUNT_LOGIN_PAGE;
          return;
        }

        // Restart on all other errors.
        self.startEasySetup();
      }
    });
    this._jpakeclient.receiveNoPIN();
  },

  abortEasySetup: function () {
    document.getElementById("easySetupPIN1").value = "";
    document.getElementById("easySetupPIN2").value = "";
    document.getElementById("easySetupPIN3").value = "";
    if (!this._jpakeclient)
      return;

    this._jpakeclient.abort();
    delete this._jpakeclient;
  },

  manualSetup: function () {
    this.abortEasySetup();
    this.wizard.pageIndex = EXISTING_ACCOUNT_LOGIN_PAGE;
  },

  // _handleNoScript is needed because it blocks the captcha. So we temporarily
  // allow the necessary sites so that we can verify the user is in fact a human.
  // This was done with the help of Giorgio (NoScript author). See bug 508112.
  _handleNoScript: function (addExceptions) {
    // if NoScript isn't installed, or is disabled, bail out.
    let ns = Cc["@maone.net/noscript-service;1"];
    if (ns == null)
      return;

    ns = ns.getService().wrappedJSObject;
    if (addExceptions) {
      this._remoteSites.forEach(function(site) {
        site = ns.getSite(site);
        if (!ns.isJSEnabled(site)) {
          this._disabledSites.push(site); // save status
          ns.setJSEnabled(site, true); // allow site
        }
      }, this);
    }
    else {
      this._disabledSites.forEach(function(site) {
        ns.setJSEnabled(site, false);
      });
      this._disabledSites = [];
    }
  },

  onExistingServerCommand: function () {
    let control = document.getElementById("existingServer");
    if (control.selectedIndex == 0) {
      control.removeAttribute("editable");
      Weave.Svc.Prefs.reset("serverURL");
    } else {
      control.setAttribute("editable", "true");
      // Force a style flush to ensure that the binding is attached.
      control.clientTop;
      control.value = "";
      control.inputField.focus();
    }
    document.getElementById("existingServerFeedbackRow").hidden = true;
    this.checkFields();
  },

  onExistingServerInput: function () {
    // Check custom server validity when the user stops typing for 1 second.
    if (this._existingServerTimer)
      window.clearTimeout(this._existingServerTimer);
    this._existingServerTimer = window.setTimeout(function () {
      gSyncSetup.checkFields();
    }, 1000);
  },

  onServerCommand: function () {
    setVisibility(document.getElementById("TOSRow"), this._usingMainServers);
    let control = document.getElementById("server");
    if (!this._usingMainServers) {
      control.setAttribute("editable", "true");
      // Force a style flush to ensure that the binding is attached.
      control.clientTop;
      control.value = "";
      control.inputField.focus();
      // checkServer() will call checkAccount() and checkFields().
      this.checkServer();
      return;
    }
    control.removeAttribute("editable");
    Weave.Svc.Prefs.reset("serverURL");
    if (this._settingUpNew) {
      this.loadCaptcha();
    }
    this.checkAccount();
    this.status.server = true;
    document.getElementById("serverFeedbackRow").hidden = true;
    this.checkFields();
  },

  onServerInput: function () {
    // Check custom server validity when the user stops typing for 1 second.
    if (this._checkServerTimer)
      window.clearTimeout(this._checkServerTimer);
    this._checkServerTimer = window.setTimeout(function () {
      gSyncSetup.checkServer();
    }, 1000);
  },

  checkServer: function () {
    delete this._checkServerTimer;
    let el = document.getElementById("server");
    let valid = false;
    let feedback = document.getElementById("serverFeedbackRow");
    let str = "";
    if (el.value) {
      valid = this._validateServer(el);
      let str = valid ? "" : "serverInvalid.label";
      this._setFeedbackMessage(feedback, valid, str);
    }
    else
      this._setFeedbackMessage(feedback, true);

    // Recheck account against the new server.
    if (valid)
      this.checkAccount();

    this.status.server = valid;
    this.checkFields();
  },

  _validateServer: function (element) {
    let valid = false;
    let val = element.value;
    if (!val)
      return false;

    let uri = Weave.Utils.makeURI(val);

    if (!uri)
      uri = Weave.Utils.makeURI("https://" + val);

    if (uri && this._settingUpNew) {
      function isValid(uri) {
        Weave.Service.serverURL = uri.spec;
        let check = Weave.Service.checkAccount("a");
        return (check == "available" || check == "notAvailable");
      }

      if (uri.schemeIs("http")) {
        uri.scheme = "https";
        if (isValid(uri))
          valid = true;
        else
          // setting the scheme back to http
          uri.scheme = "http";
      }
      if (!valid)
        valid = isValid(uri);

      if (valid) {
        this.loadCaptcha();
      }
    }
    else if (uri) {
      valid = true;
      Weave.Service.serverURL = uri.spec;
    }

    if (valid)
      element.value = Weave.Service.serverURL;
    else
      Weave.Svc.Prefs.reset("serverURL");

    return valid;
  },

  _handleChoice: function () {
    let desc = document.getElementById("mergeChoiceRadio").selectedIndex;
    document.getElementById("chosenActionDeck").selectedIndex = desc;
    switch (desc) {
      case 1:
        if (this._case1Setup)
          break;

        let places_db = PlacesUtils.history
                                   .QueryInterface(Ci.nsPIPlacesDatabase)
                                   .DBConnection;
        if (Weave.Engines.get("history").enabled) {
          let daysOfHistory = 0;
          let stm = places_db.createStatement(
            "SELECT ROUND(( " +
              "strftime('%s','now','localtime','utc') - " +
              "( " +
                "SELECT visit_date FROM moz_historyvisits " +
                "ORDER BY visit_date ASC LIMIT 1 " +
                ")/1000000 " +
              ")/86400) AS daysOfHistory ");

          if (stm.step())
            daysOfHistory = stm.getInt32(0);
          // Support %S for historical reasons (see bug 600141)
          document.getElementById("historyCount").value =
            PluralForm.get(daysOfHistory,
                           this._stringBundle.GetStringFromName("historyDaysCount.label"))
                      .replace("%S", daysOfHistory)
                      .replace("#1", daysOfHistory);
        } else {
          document.getElementById("historyCount").hidden = true;
        }

        if (Weave.Engines.get("bookmarks").enabled) {
          let bookmarks = 0;
          let stm = places_db.createStatement(
            "SELECT count(*) AS bookmarks " +
            "FROM moz_bookmarks b " +
            "LEFT JOIN moz_bookmarks t ON " +
            "b.parent = t.id WHERE b.type = 1 AND t.parent <> :tag");
          stm.params.tag = PlacesUtils.tagsFolderId;
          if (stm.executeStep())
            bookmarks = stm.row.bookmarks;
          // Support %S for historical reasons (see bug 600141)
          document.getElementById("bookmarkCount").value =
            PluralForm.get(bookmarks,
                           this._stringBundle.GetStringFromName("bookmarksCount.label"))
                      .replace("%S", bookmarks)
                      .replace("#1", bookmarks);
        } else {
          document.getElementById("bookmarkCount").hidden = true;
        }

        if (Weave.Engines.get("passwords").enabled) {
          let logins = Services.logins.getAllLogins({});
          // Support %S for historical reasons (see bug 600141)
          document.getElementById("passwordCount").value =
            PluralForm.get(logins.length,
                           this._stringBundle.GetStringFromName("passwordsCount.label"))
                      .replace("%S", logins.length)
                      .replace("#1", logins.length);
        } else {
          document.getElementById("passwordCount").hidden = true;
        }

        if (!Weave.Engines.get("prefs").enabled) {
          document.getElementById("prefsWipe").hidden = true;
        }

        this._case1Setup = true;
        break;
      case 2:
        if (this._case2Setup)
          break;
        let count = 0;
        function appendNode(label) {
          let box = document.getElementById("clientList");
          let node = document.createElement("label");
          node.setAttribute("value", label);
          node.setAttribute("class", "data indent");
          box.appendChild(node);
        }

        for each (let name in Weave.Clients.stats.names) {
          // Don't list the current client
          if (name == Weave.Clients.localName)
            continue;

          // Only show the first several client names
          if (++count <= 5)
            appendNode(name);
        }
        if (count > 5) {
          // Support %S for historical reasons (see bug 600141)
          let label =
            PluralForm.get(count - 5,
                           this._stringBundle.GetStringFromName("additionalClientCount.label"))
                      .replace("%S", count - 5)
                      .replace("#1", count - 5);
          appendNode(label);
        }
        this._case2Setup = true;
        break;
    }

    return true;
  },

  // sets class and string on a feedback element
  // if no property string is passed in, we clear label/style
  _setFeedback: function (element, success, string) {
    element.hidden = success || !string;
    let classname = success ? "success" : "error";
    let image = element.getElementsByAttribute("class", "statusIcon")[0];
    image.setAttribute("status", classname);
    let label = element.getElementsByAttribute("class", "status")[0];
    label.value = string;
  },

  // shim
  _setFeedbackMessage: function (element, success, string) {
    let str = "";
    if (string) {
      try {
        str = this._stringBundle.GetStringFromName(string);
      } catch(e) {}

      if (!str)
        str = Weave.Utils.getErrorString(string);
    }
    this._setFeedback(element, success, str);
  },

  loadCaptcha: function loadCaptcha() {
    let captchaURI = Weave.Service.miscAPI + "captcha_html";
    // First check for NoScript and whitelist the right sites.
    this._handleNoScript(true);
    if (this.captchaBrowser.currentURI.spec != captchaURI) {
      this.captchaBrowser.loadURI(captchaURI);
    }
  },

  onStateChange: function(webProgress, request, stateFlags, status) {
    // We're only looking for the end of the frame load
    if ((stateFlags & Ci.nsIWebProgressListener.STATE_STOP) == 0)
      return;
    if ((stateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) == 0)
      return;
    if ((stateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW) == 0)
      return;

    // If we didn't find a captcha, assume it's not needed and don't show it.
    let responseStatus = request.QueryInterface(Ci.nsIHttpChannel).responseStatus;
    setVisibility(this.captchaBrowser, responseStatus != 404);
    //XXX TODO we should really log any responseStatus other than 200
  },
  onProgressChange: function() {},
  onStatusChange: function() {},
  onSecurityChange: function() {},
  onLocationChange: function () {}
};

// Define lazy getters for various XUL elements.
//
// onWizardAdvance() and onPageShow() are run before init(), so we'll even
// define things that will almost certainly be used (like 'wizard') as a lazy
// getter here.
["wizard",
 "pin1",
 "pin2",
 "pin3",
 "pairDeviceErrorRow",
 "pairDeviceThrobber"].forEach(function (id) {
  XPCOMUtils.defineLazyGetter(gSyncSetup, id, function() {
    return document.getElementById(id);
  });
});
XPCOMUtils.defineLazyGetter(gSyncSetup, "nextFocusEl", function () {
  return {pin1: this.pin2,
          pin2: this.pin3,
          pin3: this.wizard.getButton("next")};
});
XPCOMUtils.defineLazyGetter(gSyncSetup, "_stringBundle", function() {
  return Services.strings.createBundle("chrome://browser/locale/syncSetup.properties");
});
