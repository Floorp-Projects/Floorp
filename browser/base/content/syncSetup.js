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

const INTRO_PAGE                    = 0;
const NEW_ACCOUNT_START_PAGE        = 1;
const NEW_ACCOUNT_PP_PAGE           = 2;
const NEW_ACCOUNT_CAPTCHA_PAGE      = 3;
const EXISTING_ACCOUNT_LOGIN_PAGE   = 4;
const EXISTING_ACCOUNT_PP_PAGE      = 5;
const OPTIONS_PAGE                  = 6;
const OPTIONS_CONFIRM_PAGE          = 7;
const SETUP_SUCCESS_PAGE            = 8;

Cu.import("resource://services-sync/main.js");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PluralForm.jsm");

var gSyncSetup = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),

  captchaBrowser: null,
  wizard: null,
  _disabledSites: [],
  _remoteSites: [Weave.Service.serverURL, "https://api-secure.recaptcha.net"],

  status: {
    password: false,
    email: false,
    server: false
  },
  _haveSyncKeyBackup: false,

  get _usingMainServers() {
    if (this._settingUpNew)
      return document.getElementById("serverType").selectedItem.value == "main";

    return document.getElementById("existingServerType").selectedItem.value == "main";
  },

  init: function () {
    let obs = [
      ["weave:service:changepph:finish", "onResetPassphrase"],
      ["weave:service:verify-login:start",  "onLoginStart"],
      ["weave:service:verify-login:error",  "onLoginEnd"],
      ["weave:service:verify-login:finish", "onLoginEnd"]];

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

    this.captchaBrowser = document.getElementById("captcha");
    this.wizard = document.getElementById("accountSetup");

    if (window.arguments && window.arguments[0] == true) {
      // we're resetting sync
      this._resettingSync = true;
      this.wizard.pageIndex = OPTIONS_PAGE;
    }
    else {
      this.wizard.canAdvance = false;
      this.captchaBrowser.addProgressListener(this);
      Weave.Svc.Prefs.set("firstSync", "notReady");
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
    this._settingUpNew = true;
    this.wizard.pageIndex = NEW_ACCOUNT_START_PAGE;
  },

  useExistingAccount: function () {
    this._settingUpNew = false;
    this.wizard.pageIndex = EXISTING_ACCOUNT_LOGIN_PAGE;
  },

  onResetPassphrase: function () {
    document.getElementById("existingPassphrase").value = Weave.Service.passphrase;
    this.wizard.advance();
  },

  onLoginStart: function () {
    this.toggleLoginFeedback(false);
  },

  onLoginEnd: function () {
    this.toggleLoginFeedback(true);
  },

  toggleLoginFeedback: function (stop) {
    switch (this.wizard.pageIndex) {
      case EXISTING_ACCOUNT_LOGIN_PAGE:
        document.getElementById("connect-throbber").hidden = stop;
        let feedback = document.getElementById("existingPasswordFeedbackRow");
        if (stop) {
          let success = Weave.Status.login == Weave.LOGIN_SUCCEEDED ||
                        Weave.Status.login == Weave.LOGIN_FAILED_INVALID_PASSPHRASE;
          this._setFeedbackMessage(feedback, success, Weave.Status.login);
        }
        else
          this._setFeedbackMessage(feedback, true);
        break;
      case EXISTING_ACCOUNT_PP_PAGE:
        document.getElementById("passphrase-throbber").hidden = stop;
        feedback = document.getElementById("existingPassphraseFeedbackBox");
        if (stop) {
          let success = Weave.Status.login == Weave.LOGIN_SUCCEEDED;
          this._setFeedbackMessage(feedback, success, Weave.Status.login);
          document.getElementById("passphraseHelpBox").hidden = success;
        }
        else
          this._setFeedbackMessage(feedback, true);

        break;
    }
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
        for (i in this.status) {
          if (!this.status[i])
            return false;
        }
        if (this._usingMainServers)
          return document.getElementById("tos").checked;

        return true;
      case NEW_ACCOUNT_PP_PAGE:
        return this._haveSyncKeyBackup && this.checkPassphrase();
      case EXISTING_ACCOUNT_LOGIN_PAGE:
        let hasUser = document.getElementById("existingAccountName").value != "";
        let hasPass = document.getElementById("existingPassword").value != "";
        if (hasUser && hasPass) {
          if (this._usingMainServers)
            return true;

          if (this._validateServer(document.getElementById("existingServerURL"), false))
            return true;
        }
        return false;
      case EXISTING_ACCOUNT_PP_PAGE:
        return document.getElementById("existingPassphrase").value != "";
    }
    // we probably shouldn't get here
    return true;
  },

  onEmailChange: function () {
    let value = document.getElementById("weaveEmail").value;
    if (!value) {
      this.status.email = false;
      this.checkFields();
      return;
    }
    // Do this async to avoid blocking the widget while we go to the server.
    window.setTimeout(function() {
      gSyncSetup.checkAccount(value);
    }, 0);
  },

  checkAccount: function(value) {
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
    let valid, str;
    if (password.value == document.getElementById("weavePassphrase").value) {
      // xxxmpc - hack, sigh
      valid = false;
      errorString = Weave.Utils.getErrorString("change.password.pwSameAsSyncKey");
    }
    else {
      let pwconfirm = document.getElementById("weavePasswordConfirm");
      [valid, errorString] = gSyncUtils.validatePassword(password, pwconfirm);
    }

    let feedback = document.getElementById("passwordFeedbackRow");
    this._setFeedback(feedback, valid, errorString);

    this.status.password = valid;
    this.checkFields();
  },

  onPassphraseChange: function () {
    // Ignore if there's no actual change from the generated one.
    let el = document.getElementById("weavePassphrase");
    if (gSyncUtils.normalizePassphrase(el.value) == Weave.Service.passphrase) {
      el = document.getElementById("generatePassphraseButton");
      el.hidden = true;
      this._haveCustomSyncKey = false;
      return;
    }

    this._haveSyncKeyBackup = true;
    this._haveCustomSyncKey = true;
    el = document.getElementById("generatePassphraseButton");
    el.hidden = false;
    this.checkFields();
  },

  onPassphraseGenerate: function () {
    let passphrase = gSyncUtils.generatePassphrase();
    Weave.Service.passphrase = passphrase;
    let el = document.getElementById("weavePassphrase");
    el.value = gSyncUtils.hyphenatePassphrase(passphrase);

    el = document.getElementById("generatePassphraseButton");
    el.hidden = true;
    document.getElementById("passphraseStrengthRow").hidden = true;
    let feedback = document.getElementById("passphraseFeedbackRow");
    this._setFeedback(feedback, true, "");
  },

  afterBackup: function () {
    this._haveSyncKeyBackup = true;
    this.checkFields();
  },

  checkPassphrase: function () {
    let el1 = document.getElementById("weavePassphrase");
    let valid, str;
    // xxxmpc - hack, sigh
    if (el1.value == document.getElementById("weavePassword").value) {
      valid = false;
      str = Weave.Utils.getErrorString("change.passphrase.ppSameAsPassword");
    }
    else {
      [valid, str] = gSyncUtils.validatePassphrase(el1);
    }

    let feedback = document.getElementById("passphraseFeedbackRow");
    this._setFeedback(feedback, valid, str);
    if (!valid)
      return valid;

    // Display passphrase strength
    let pp = document.getElementById("weavePassphrase").value;
    let bits = Weave.Utils.passphraseStrength(pp);
    let meter = document.getElementById("passphraseStrength");
    meter.value = bits;
    // The generated 20 character passphrase has an entropy of 94 bits
    // which we consider "strong".
    if (bits > 94)
      meter.className = "strong";
    else if (bits > 47)
      meter.className = "medium";
    else
      meter.className = "";
    document.getElementById("passphraseStrengthRow").hidden = false;
    return valid;
  },

  onPageShow: function() {
    switch (this.wizard.pageIndex) {
      case INTRO_PAGE:
        this.wizard.getButton("next").hidden = true;
        this.wizard.getButton("back").hidden = true;
        this.wizard.getButton("extra1").hidden = true;
        break;
      case NEW_ACCOUNT_PP_PAGE:
        let el = document.getElementById("weavePassphrase");
        el.blur();
        if (!el.value)
          this.onPassphraseGenerate();
        this.checkFields();
        break;
      case NEW_ACCOUNT_START_PAGE:
        this.wizard.getButton("extra1").hidden = false;
        this.onServerChange();
        // fall through
      case EXISTING_ACCOUNT_LOGIN_PAGE:
        this.checkFields();
        this.wizard.getButton("next").hidden = false;
        this.wizard.getButton("back").hidden = false;
        this.wizard.getButton("extra1").hidden = false;
        this.wizard.canRewind = true;
        break;
      case SETUP_SUCCESS_PAGE:
        this.wizard.canRewind = false;
        this.wizard.getButton("back").hidden = true;
        this.wizard.getButton("next").hidden = true;
        this.wizard.getButton("cancel").hidden = true;
        this.wizard.getButton("finish").hidden = false;
        this._handleSuccess();
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
    if (!this.wizard.pageIndex)
      return true;

    switch (this.wizard.pageIndex) {
      case NEW_ACCOUNT_CAPTCHA_PAGE:
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
        feedback.hidden = false;

        let password = document.getElementById("weavePassword").value;
        let email    = document.getElementById("weaveEmail").value;
        let challenge = getField("challenge");
        let response = getField("response");

        let error = Weave.Service.createAccount(email, password,
                                                challenge, response);

        if (error == null) {
          Weave.Service.account = email;
          Weave.Service.password = password;
          this._handleNoScript(false);
          this.wizard.pageIndex = SETUP_SUCCESS_PAGE;
          return false;
        }

        image.setAttribute("status", "error");
        label.value = Weave.Utils.getErrorString(error);
        return false;
      case NEW_ACCOUNT_PP_PAGE:
        if (this._haveCustomSyncKey)
          Weave.Service.passphrase = document.getElementById("weavePassphrase").value;
        // Time to load the captcha.
        // First check for NoScript and whitelist the right sites.
        this._handleNoScript(true);
        this.captchaBrowser.loadURI(Weave.Service.miscAPI + "captcha_html");
        break;
      case EXISTING_ACCOUNT_LOGIN_PAGE:
        Weave.Service.account = document.getElementById("existingAccountName").value;
        Weave.Service.password = document.getElementById("existingPassword").value;
        Weave.Service.passphrase = document.getElementById("existingPassphrase").value;
        // verifyLogin() will likely return false because we probably don't
        // have a passphrase yet (unless the user already entered it
        // and hit the back button).
        if (!Weave.Service.verifyLogin()
            && Weave.Status.login != Weave.LOGIN_FAILED_NO_PASSPHRASE
            && Weave.Status.login != Weave.LOGIN_FAILED_INVALID_PASSPHRASE) {
          let feedback = document.getElementById("existingPasswordFeedbackRow");
          this._setFeedbackMessage(feedback, false, Weave.Status.login);
          return false;
        }
        break;
      case EXISTING_ACCOUNT_PP_PAGE:
        let pp = document.getElementById("existingPassphrase").value;
        Weave.Service.passphrase = gSyncUtils.normalizePassphrase(pp);
        if (Weave.Service.login())
          this.wizard.pageIndex = SETUP_SUCCESS_PAGE;
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
          this.onWizardFinish();
          window.close();
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
      case EXISTING_ACCOUNT_PP_PAGE: // no idea wtf is up here, but meh!
        this.wizard.pageIndex = EXISTING_ACCOUNT_LOGIN_PAGE;
        return false;
      case OPTIONS_CONFIRM_PAGE:
        // Backing up from the confirmation page = resetting first sync to merge.
        document.getElementById("mergeChoiceRadio").selectedIndex = 0;
        return this.returnFromOptions();
    }
    return true;
  },

  onWizardFinish: function () {
    this.setupInitialSync();

    if (!this._resettingSync) {
      function isChecked(element) {
        return document.getElementById(element).hasAttribute("checked");
      }

      let prefs = ["engine.bookmarks", "engine.passwords", "engine.history", "engine.tabs", "engine.prefs"];
      for (let i = 0;i < prefs.length;i++) {
        Weave.Svc.Prefs.set(prefs[i], isChecked(prefs[i]));
      }
      this._handleNoScript(false);
      if (Weave.Svc.Prefs.get("firstSync", "") == "notReady")
        Weave.Svc.Prefs.reset("firstSync");

      Weave.Service.persistLogin();
      Weave.Svc.Obs.notify("weave:service:setup-complete");
      if (this._settingUpNew)
        gSyncUtils.openFirstClientFirstrun();
      else
        gSyncUtils.openAddedClientFirstrun();
    }

    if (!Weave.Service.isLoggedIn)
      Weave.Service.login();

    Weave.Service.syncOnIdle(1);
  },

  onWizardCancel: function () {
    if (this._resettingSync)
      return;

    if (this.wizard.pageIndex == SETUP_SUCCESS_PAGE) {
      this.onWizardFinish();
      return;
    }
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

  onServerChange: function () {
    if (this.wizard.pageIndex == EXISTING_ACCOUNT_LOGIN_PAGE) {
      if (this._usingMainServers)
        Weave.Svc.Prefs.reset("serverURL");
      document.getElementById("existingServerRow").hidden = this._usingMainServers;
      this.checkFields();
      return;
    }

    document.getElementById("serverRow").hidden = this._usingMainServers;
    document.getElementById("TOSRow").hidden = !this._usingMainServers;
    let valid = false;
    let feedback = document.getElementById("serverFeedbackRow");

    if (this._usingMainServers) {
      Weave.Svc.Prefs.reset("serverURL");
      valid = true;
      feedback.hidden = true;
    }
    else {
      let el = document.getElementById("weaveServerURL");
      let str = "";
      if (el.value) {
        valid = this._validateServer(el, true);
        let str = valid ? "" : "serverInvalid.label";
        this._setFeedbackMessage(feedback, valid, str);
      }
      else
        this._setFeedbackMessage(feedback, true);
    }

    // Recheck account against the new server.
    if (valid)
      this.onEmailChange();

    this.status.server = valid;
    this.checkFields();
  },

  // xxxmpc - checkRemote is a hack, we can't verify a minimal server is live
  // without auth, so we won't validate in the existing-server case.
  _validateServer: function (element, checkRemote) {
    let valid = false;
    let val = element.value;
    if (!val)
      return false;

    let uri = Weave.Utils.makeURI(val);

    if (!uri)
      uri = Weave.Utils.makeURI("https://" + val);

    if (uri && checkRemote) {
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

  _handleSuccess: function() {
    let self = this;
    function fill(id, string)
      document.getElementById(id).firstChild.nodeValue =
        string ? self._stringBundle.GetStringFromName(string) : "";

    fill("firstSyncAction", "");
    fill("firstSyncActionWarning", "");
    if (this._settingUpNew) {
      fill("firstSyncAction", "newAccount.action.label");
      fill("firstSyncActionChange", "newAccount.change.label");
      return;
    }
    fill("firstSyncActionChange", "existingAccount.change.label");
    let action = document.getElementById("mergeChoiceRadio").selectedItem.id;
    let id = action == "resetClient" ? "firstSyncAction" : "firstSyncActionWarning";
    fill(id, action + ".change.label");
  },

  _handleChoice: function () {
    let desc = document.getElementById("mergeChoiceRadio").selectedIndex;
    document.getElementById("chosenActionDeck").selectedIndex = desc;
    switch (desc) {
      case 1:
        if (this._case1Setup)
          break;

        // history
        let db = Weave.Svc.History.DBConnection;

        let daysOfHistory = 0;
        let stm = db.createStatement(
          "SELECT ROUND(( " +
            "strftime('%s','now','localtime','utc') - " +
            "( " +
              "SELECT visit_date FROM moz_historyvisits " +
              "UNION ALL " +
              "SELECT visit_date FROM moz_historyvisits_temp " +
              "ORDER BY visit_date ASC LIMIT 1 " +
              ")/1000000 " +
            ")/86400) AS daysOfHistory ");

        if (stm.step())
          daysOfHistory = stm.getInt32(0);
        document.getElementById("historyCount").value =
          PluralForm.get(daysOfHistory,
                         this._stringBundle.GetStringFromName("historyDaysCount.label"))
                             .replace("%S", daysOfHistory);

        // bookmarks
        let bookmarks = 0;
        stm = db.createStatement(
          "SELECT count(*) AS bookmarks " +
          "FROM moz_bookmarks b " +
          "LEFT JOIN moz_bookmarks t ON " +
          "b.parent = t.id WHERE b.type = 1 AND t.parent <> :tag");
        stm.params.tag = Weave.Svc.Bookmark.tagsFolder;
        if (stm.executeStep())
          bookmarks = stm.row.bookmarks;
        document.getElementById("bookmarkCount").value =
          PluralForm.get(bookmarks,
                         this._stringBundle.GetStringFromName("bookmarksCount.label"))
                             .replace("%S", bookmarks);

        // passwords
        let logins = Weave.Svc.Login.getAllLogins({});
        document.getElementById("passwordCount").value =
          PluralForm.get(logins.length,
                         this._stringBundle.GetStringFromName("passwordsCount.label"))
                             .replace("%S", logins.length);
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
          let label =
            PluralForm.get(count - 5,
                           this._stringBundle.GetStringFromName("additionalClientCount.label"))
                               .replace("%S", count - 5);
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
    let class = success ? "success" : "error";
    let image = element.firstChild.nextSibling.firstChild;
    image.setAttribute("status", class);
    let label = image.nextSibling;
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


  onStateChange: function(webProgress, request, stateFlags, status) {
    // We're only looking for the end of the frame load
    if ((stateFlags & Ci.nsIWebProgressListener.STATE_STOP) == 0)
      return;
    if ((stateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) == 0)
      return;
    if ((stateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW) == 0)
      return;

    // If we didn't find the captcha, assume it's not needed and move on
    if (request.QueryInterface(Ci.nsIHttpChannel).responseStatus == 404)
      this.onWizardAdvance();
  },
  onProgressChange: function() {},
  onStatusChange: function() {},
  onSecurityChange: function() {},
  onLocationChange: function () {}
}

// onWizardAdvance() and onPageShow() are run before init(), so we'll set
// wizard & _stringBundle up as lazy getters.
XPCOMUtils.defineLazyGetter(gSyncSetup, "wizard", function() {
  return document.getElementById("accountSetup");
});
XPCOMUtils.defineLazyGetter(gSyncSetup, "_stringBundle", function() {
  return Services.strings.createBundle("chrome://browser/locale/syncSetup.properties");
});
