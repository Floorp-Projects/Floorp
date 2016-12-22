/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://services-sync/main.js");
Components.utils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "FxAccountsCommon", function() {
  return Components.utils.import("resource://gre/modules/FxAccountsCommon.js", {});
});

XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
  "resource://gre/modules/FxAccounts.jsm");

const PAGE_NO_ACCOUNT = 0;
const PAGE_HAS_ACCOUNT = 1;
const PAGE_NEEDS_UPDATE = 2;
const FXA_PAGE_LOGGED_OUT = 3;
const FXA_PAGE_LOGGED_IN = 4;

// Indexes into the "login status" deck.
// We are in a successful verified state - everything should work!
const FXA_LOGIN_VERIFIED = 0;
// We have logged in to an unverified account.
const FXA_LOGIN_UNVERIFIED = 1;
// We are logged in locally, but the server rejected our credentials.
const FXA_LOGIN_FAILED = 2;

var gSyncPane = {
  prefArray: ["engine.bookmarks", "engine.passwords", "engine.prefs",
              "engine.tabs", "engine.history"],

  get page() {
    return document.getElementById("weavePrefsDeck").selectedIndex;
  },

  set page(val) {
    document.getElementById("weavePrefsDeck").selectedIndex = val;
  },

  get _usingCustomServer() {
    return Weave.Svc.Prefs.isSet("serverURL");
  },

  needsUpdate() {
    this.page = PAGE_NEEDS_UPDATE;
    let label = document.getElementById("loginError");
    label.textContent = Weave.Utils.getErrorString(Weave.Status.login);
    label.className = "error";
  },

  init() {
    this._setupEventListeners();

    // If the Service hasn't finished initializing, wait for it.
    let xps = Components.classes["@mozilla.org/weave/service;1"]
                                .getService(Components.interfaces.nsISupports)
                                .wrappedJSObject;

    if (xps.ready) {
      this._init();
      return;
    }

    // it may take some time before we can determine what provider to use
    // and the state of that provider, so show the "please wait" page.
    this._showLoadPage(xps);

    let onUnload = function() {
      window.removeEventListener("unload", onUnload, false);
      try {
        Services.obs.removeObserver(onReady, "weave:service:ready");
      } catch (e) {}
    };

    let onReady = function() {
      Services.obs.removeObserver(onReady, "weave:service:ready");
      window.removeEventListener("unload", onUnload, false);
      this._init();
    }.bind(this);

    Services.obs.addObserver(onReady, "weave:service:ready", false);
    window.addEventListener("unload", onUnload, false);

    xps.ensureLoaded();
  },

  _showLoadPage(xps) {
    let username;
    try {
      username = Services.prefs.getCharPref("services.sync.username");
    } catch (e) {}
    if (!username) {
      this.page = FXA_PAGE_LOGGED_OUT;
    } else if (xps.fxAccountsEnabled) {
      // Use cached values while we wait for the up-to-date values
      let cachedComputerName;
      try {
        cachedComputerName = Services.prefs.getCharPref("services.sync.client.name");
      }
      catch (e) {
        cachedComputerName = "";
      }
      document.getElementById("fxaEmailAddress1").textContent = username;
      this._populateComputerName(cachedComputerName);
      this.page = FXA_PAGE_LOGGED_IN;
    } else { // Old Sync
      this.page = PAGE_HAS_ACCOUNT;
    }
  },

  _init() {
    let topics = ["weave:service:login:error",
                  "weave:service:login:finish",
                  "weave:service:start-over:finish",
                  "weave:service:setup-complete",
                  "weave:service:logout:finish",
                  FxAccountsCommon.ONVERIFIED_NOTIFICATION,
                  FxAccountsCommon.ONLOGIN_NOTIFICATION,
                  FxAccountsCommon.ON_PROFILE_CHANGE_NOTIFICATION,
                  ];
    // Add the observers now and remove them on unload
    // XXXzpao This should use Services.obs.* but Weave's Obs does nice handling
    //        of `this`. Fix in a followup. (bug 583347)
    topics.forEach(function(topic) {
      Weave.Svc.Obs.add(topic, this.updateWeavePrefs, this);
    }, this);

    window.addEventListener("unload", function() {
      topics.forEach(function(topic) {
        Weave.Svc.Obs.remove(topic, this.updateWeavePrefs, this);
      }, gSyncPane);
    }, false);

    XPCOMUtils.defineLazyGetter(this, '_stringBundle', () => {
      return Services.strings.createBundle("chrome://browser/locale/preferences/preferences.properties");
    });

    XPCOMUtils.defineLazyGetter(this, '_accountsStringBundle', () => {
      return Services.strings.createBundle("chrome://browser/locale/accounts.properties");
    });

    let url = Services.prefs.getCharPref("identity.mobilepromo.android") + "sync-preferences";
    document.getElementById("fxaMobilePromo-android").setAttribute("href", url);
    document.getElementById("fxaMobilePromo-android-hasFxaAccount").setAttribute("href", url);
    url = Services.prefs.getCharPref("identity.mobilepromo.ios") + "sync-preferences";
    document.getElementById("fxaMobilePromo-ios").setAttribute("href", url);
    document.getElementById("fxaMobilePromo-ios-hasFxaAccount").setAttribute("href", url);

    document.getElementById("tosPP-small-ToS").setAttribute("href", gSyncUtils.tosURL);
    document.getElementById("tosPP-normal-ToS").setAttribute("href", gSyncUtils.tosURL);
    document.getElementById("tosPP-small-PP").setAttribute("href", gSyncUtils.privacyPolicyURL);
    document.getElementById("tosPP-normal-PP").setAttribute("href", gSyncUtils.privacyPolicyURL);

    fxAccounts.promiseAccountsManageURI(this._getEntryPoint()).then(accountsManageURI => {
      document.getElementById("verifiedManage").setAttribute("href", accountsManageURI);
    });

    this.updateWeavePrefs();

    this._initProfileImageUI();
  },

  _toggleComputerNameControls(editMode) {
    let textbox = document.getElementById("fxaSyncComputerName");
    textbox.disabled = !editMode;
    document.getElementById("fxaChangeDeviceName").hidden = editMode;
    document.getElementById("fxaCancelChangeDeviceName").hidden = !editMode;
    document.getElementById("fxaSaveChangeDeviceName").hidden = !editMode;
  },

  _focusComputerNameTextbox() {
    let textbox = document.getElementById("fxaSyncComputerName");
    let valLength = textbox.value.length;
    textbox.focus();
    textbox.setSelectionRange(valLength, valLength);
  },

  _blurComputerNameTextbox() {
    document.getElementById("fxaSyncComputerName").blur();
  },

  _focusAfterComputerNameTextbox() {
    // Focus the most appropriate element that's *not* the "computer name" box.
    Services.focus.moveFocus(window,
                             document.getElementById("fxaSyncComputerName"),
                             Services.focus.MOVEFOCUS_FORWARD, 0);
  },

  _updateComputerNameValue(save) {
    if (save) {
      let textbox = document.getElementById("fxaSyncComputerName");
      Weave.Service.clientsEngine.localName = textbox.value;
    }
    this._populateComputerName(Weave.Service.clientsEngine.localName);
  },

  _setupEventListeners() {
    function setEventListener(aId, aEventType, aCallback)
    {
      document.getElementById(aId)
              .addEventListener(aEventType, aCallback.bind(gSyncPane));
    }

    setEventListener("noAccountSetup", "click", function(aEvent) {
      aEvent.stopPropagation();
      gSyncPane.openSetup(null);
    });
    setEventListener("noAccountPair", "click", function(aEvent) {
      aEvent.stopPropagation();
      gSyncPane.openSetup('pair');
    });
    setEventListener("syncChangePassword", "command",
      () => gSyncUtils.changePassword());
    setEventListener("syncResetPassphrase", "command",
      () => gSyncUtils.resetPassphrase());
    setEventListener("syncReset", "command", gSyncPane.resetSync);
    setEventListener("syncAddDeviceLabel", "click", function() {
      gSyncPane.openAddDevice();
      return false;
    });
    setEventListener("syncEnginesList", "select", function() {
      if (this.selectedCount)
        this.clearSelection();
    });
    setEventListener("syncComputerName", "change", function(e) {
      gSyncUtils.changeName(e.target);
    });
    setEventListener("fxaChangeDeviceName", "command", function() {
      this._toggleComputerNameControls(true);
      this._focusComputerNameTextbox();
    });
    setEventListener("fxaCancelChangeDeviceName", "command", function() {
      // We explicitly blur the textbox because of bug 75324, then after
      // changing the state of the buttons, force focus to whatever the focus
      // manager thinks should be next (which on the mac, depends on an OSX
      // keyboard access preference)
      this._blurComputerNameTextbox();
      this._toggleComputerNameControls(false);
      this._updateComputerNameValue(false);
      this._focusAfterComputerNameTextbox();
    });
    setEventListener("fxaSaveChangeDeviceName", "command", function() {
      // Work around bug 75324 - see above.
      this._blurComputerNameTextbox();
      this._toggleComputerNameControls(false);
      this._updateComputerNameValue(true);
      this._focusAfterComputerNameTextbox();
    });
    setEventListener("unlinkDevice", "click", function() {
      gSyncPane.startOver(true);
      return false;
    });
    setEventListener("loginErrorUpdatePass", "click", function() {
      gSyncPane.updatePass();
      return false;
    });
    setEventListener("loginErrorResetPass", "click", function() {
      gSyncPane.resetPass();
      return false;
    });
    setEventListener("loginErrorStartOver", "click", function() {
      gSyncPane.startOver(true);
      return false;
    });
    setEventListener("noFxaSignUp", "command", function() {
      gSyncPane.signUp();
      return false;
    });
    setEventListener("noFxaSignIn", "command", function() {
      gSyncPane.signIn();
      return false;
    });
    setEventListener("fxaUnlinkButton", "command", function() {
      gSyncPane.unlinkFirefoxAccount(true);
    });
    setEventListener("verifyFxaAccount", "command",
      gSyncPane.verifyFirefoxAccount);
    setEventListener("unverifiedUnlinkFxaAccount", "command", function() {
      /* no warning as account can't have previously synced */
      gSyncPane.unlinkFirefoxAccount(false);
    });
    setEventListener("rejectReSignIn", "command",
      gSyncPane.reSignIn);
    setEventListener("rejectUnlinkFxaAccount", "command", function() {
      gSyncPane.unlinkFirefoxAccount(true);
    });
    setEventListener("fxaSyncComputerName", "keypress", function(e) {
      if (e.keyCode == KeyEvent.DOM_VK_RETURN) {
        document.getElementById("fxaSaveChangeDeviceName").click();
      } else if (e.keyCode == KeyEvent.DOM_VK_ESCAPE) {
        document.getElementById("fxaCancelChangeDeviceName").click();
      }
    });
  },

  _initProfileImageUI() {
    try {
      if (Services.prefs.getBoolPref("identity.fxaccounts.profile_image.enabled")) {
        document.getElementById("fxaProfileImage").hidden = false;
      }
    } catch (e) { }
  },

  updateWeavePrefs() {
    let service = Components.classes["@mozilla.org/weave/service;1"]
                  .getService(Components.interfaces.nsISupports)
                  .wrappedJSObject;
    // service.fxAccountsEnabled is false iff sync is already configured for
    // the legacy provider.
    if (service.fxAccountsEnabled) {
      let displayNameLabel = document.getElementById("fxaDisplayName");
      let fxaEmailAddress1Label = document.getElementById("fxaEmailAddress1");
      fxaEmailAddress1Label.hidden = false;
      displayNameLabel.hidden = true;

      let profileInfoEnabled;
      try {
        profileInfoEnabled = Services.prefs.getBoolPref("identity.fxaccounts.profile_image.enabled");
      } catch (ex) {}

      // determine the fxa status...
      this._showLoadPage(service);

      fxAccounts.getSignedInUser().then(data => {
        if (!data) {
          this.page = FXA_PAGE_LOGGED_OUT;
          return false;
        }
        this.page = FXA_PAGE_LOGGED_IN;
        // We are logged in locally, but maybe we are in a state where the
        // server rejected our credentials (eg, password changed on the server)
        let fxaLoginStatus = document.getElementById("fxaLoginStatus");
        let syncReady;
        // Not Verfied implies login error state, so check that first.
        if (!data.verified) {
          fxaLoginStatus.selectedIndex = FXA_LOGIN_UNVERIFIED;
          syncReady = false;
        // So we think we are logged in, so login problems are next.
        // (Although if the Sync identity manager is still initializing, we
        // ignore login errors and assume all will eventually be good.)
        // LOGIN_FAILED_LOGIN_REJECTED explicitly means "you must log back in".
        // All other login failures are assumed to be transient and should go
        // away by themselves, so aren't reflected here.
        } else if (Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED) {
          fxaLoginStatus.selectedIndex = FXA_LOGIN_FAILED;
          syncReady = false;
        // Else we must be golden (or in an error state we expect to magically
        // resolve itself)
        } else {
          fxaLoginStatus.selectedIndex = FXA_LOGIN_VERIFIED;
          syncReady = true;
        }
        fxaEmailAddress1Label.textContent = data.email;
        document.getElementById("fxaEmailAddress2").textContent = data.email;
        document.getElementById("fxaEmailAddress3").textContent = data.email;
        this._populateComputerName(Weave.Service.clientsEngine.localName);
        let engines = document.getElementById("fxaSyncEngines")
        for (let checkbox of engines.querySelectorAll("checkbox")) {
          checkbox.disabled = !syncReady;
        }
        document.getElementById("fxaChangeDeviceName").disabled = !syncReady;

        // Clear the profile image (if any) of the previously logged in account.
        document.getElementById("fxaProfileImage").style.removeProperty("list-style-image");

        // If the account is verified the next promise in the chain will
        // fetch profile data.
        return data.verified;
      }).then(isVerified => {
        if (isVerified) {
          return fxAccounts.getSignedInUserProfile();
        }
        return null;
      }).then(data => {
        let fxaLoginStatus = document.getElementById("fxaLoginStatus");
        if (data && profileInfoEnabled) {
          if (data.displayName) {
            fxaLoginStatus.setAttribute("hasName", true);
            displayNameLabel.hidden = false;
            displayNameLabel.textContent = data.displayName;
          } else {
            fxaLoginStatus.removeAttribute("hasName");
          }
          if (data.avatar) {
            let bgImage = "url(\"" + data.avatar + "\")";
            let profileImageElement = document.getElementById("fxaProfileImage");
            profileImageElement.style.listStyleImage = bgImage;

            let img = new Image();
            img.onerror = () => {
              // Clear the image if it has trouble loading. Since this callback is asynchronous
              // we check to make sure the image is still the same before we clear it.
              if (profileImageElement.style.listStyleImage === bgImage) {
                profileImageElement.style.removeProperty("list-style-image");
              }
            };
            img.src = data.avatar;
          }
        } else {
          fxaLoginStatus.removeAttribute("hasName");
        }
      }, err => {
        FxAccountsCommon.log.error(err);
      }).catch(err => {
        // If we get here something's really busted
        Cu.reportError(String(err));
      });

    // If fxAccountEnabled is false and we are in a "not configured" state,
    // then fxAccounts is probably fully disabled rather than just unconfigured,
    // so handle this case.  This block can be removed once we remove support
    // for fxAccounts being disabled.
    } else if (Weave.Status.service == Weave.CLIENT_NOT_CONFIGURED ||
               Weave.Svc.Prefs.get("firstSync", "") == "notReady") {
      this.page = PAGE_NO_ACCOUNT;
    // else: sync was previously configured for the legacy provider, so we
    // make the "old" panels available.
    } else if (Weave.Status.login == Weave.LOGIN_FAILED_INVALID_PASSPHRASE ||
               Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED) {
      this.needsUpdate();
    } else {
      this.page = PAGE_HAS_ACCOUNT;
      document.getElementById("accountName").textContent = Weave.Service.identity.account;
      document.getElementById("syncComputerName").value = Weave.Service.clientsEngine.localName;
      document.getElementById("tosPP-normal").hidden = this._usingCustomServer;
    }
  },

  startOver(showDialog) {
    if (showDialog) {
      let flags = Services.prompt.BUTTON_POS_0 * Services.prompt.BUTTON_TITLE_IS_STRING +
                  Services.prompt.BUTTON_POS_1 * Services.prompt.BUTTON_TITLE_CANCEL +
                  Services.prompt.BUTTON_POS_1_DEFAULT;
      let buttonChoice =
        Services.prompt.confirmEx(window,
                                  this._stringBundle.GetStringFromName("syncUnlink.title"),
                                  this._stringBundle.GetStringFromName("syncUnlink.label"),
                                  flags,
                                  this._stringBundle.GetStringFromName("syncUnlinkConfirm.label"),
                                  null, null, null, {});

      // If the user selects cancel, just bail
      if (buttonChoice == 1)
        return;
    }

    Weave.Service.startOver();
    this.updateWeavePrefs();
  },

  updatePass() {
    if (Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED)
      gSyncUtils.changePassword();
    else
      gSyncUtils.updatePassphrase();
  },

  resetPass() {
    if (Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED)
      gSyncUtils.resetPassword();
    else
      gSyncUtils.resetPassphrase();
  },

  _getEntryPoint() {
    let params = new URLSearchParams(document.URL.split("#")[0].split("?")[1] || "");
    return params.get("entrypoint") || "preferences";
  },

  _openAboutAccounts(action) {
    let entryPoint = this._getEntryPoint();
    let params = new URLSearchParams();
    if (action) {
      params.set("action", action);
    }
    params.set("entrypoint", entryPoint);

    this.replaceTabWithUrl("about:accounts?" + params);
  },

  /**
   * Invoke the Sync setup wizard.
   *
   * @param wizardType
   *        Indicates type of wizard to launch:
   *          null    -- regular set up wizard
   *          "pair"  -- pair a device first
   *          "reset" -- reset sync
   */
  openSetup(wizardType) {
    let service = Components.classes["@mozilla.org/weave/service;1"]
                  .getService(Components.interfaces.nsISupports)
                  .wrappedJSObject;

    if (service.fxAccountsEnabled) {
      this._openAboutAccounts();
    } else {
      let win = Services.wm.getMostRecentWindow("Weave:AccountSetup");
      if (win)
        win.focus();
      else {
        window.openDialog("chrome://browser/content/sync/setup.xul",
                          "weaveSetup", "centerscreen,chrome,resizable=no",
                          wizardType);
      }
    }
  },

  openContentInBrowser(url, options) {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    if (!win) {
      // no window to use, so use _openLink to create a new one.  We don't
      // always use that as it prefers to open a new window rather than use
      // an existing one.
      gSyncUtils._openLink(url);
      return;
    }
    win.switchToTabHavingURI(url, true, options);
  },

  // Replace the current tab with the specified URL.
  replaceTabWithUrl(url) {
    // Get the <browser> element hosting us.
    let browser = window.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebNavigation)
                        .QueryInterface(Ci.nsIDocShell)
                        .chromeEventHandler;
    // And tell it to load our URL.
    browser.loadURI(url);
  },

  signUp() {
    this._openAboutAccounts("signup");
  },

  signIn() {
    this._openAboutAccounts("signin");
  },

  reSignIn() {
    this._openAboutAccounts("reauth");
  },


  clickOrSpaceOrEnterPressed(event) {
    // Note: charCode is deprecated, but 'char' not yet implemented.
    // Replace charCode with char when implemented, see Bug 680830
    return ((event.type == "click" && event.button == 0) ||
            (event.type == "keypress" &&
             (event.charCode == KeyEvent.DOM_VK_SPACE || event.keyCode == KeyEvent.DOM_VK_RETURN)));
  },

  openChangeProfileImage(event) {
    if (this.clickOrSpaceOrEnterPressed(event)) {
      fxAccounts.promiseAccountsChangeProfileURI(this._getEntryPoint(), "avatar")
          .then(url => {
        this.openContentInBrowser(url, {
          replaceQueryString: true
        });
      });
      // Prevent page from scrolling on the space key.
      event.preventDefault();
    }
  },

  openManageFirefoxAccount(event) {
    if (this.clickOrSpaceOrEnterPressed(event)) {
      this.manageFirefoxAccount();
      // Prevent page from scrolling on the space key.
      event.preventDefault();
    }
  },

  manageFirefoxAccount() {
    fxAccounts.promiseAccountsManageURI(this._getEntryPoint())
      .then(url => {
        this.openContentInBrowser(url, {
          replaceQueryString: true
        });
      });
  },

  verifyFirefoxAccount() {
    let showVerifyNotification = (data) => {
      let isError = !data;
      let maybeNot = isError ? "Not" : "";
      let sb = this._accountsStringBundle;
      let title = sb.GetStringFromName("verification" + maybeNot + "SentTitle");
      let email = !isError && data ? data.email : "";
      let body = sb.formatStringFromName("verification" + maybeNot + "SentBody", [email], 1);
      new Notification(title, { body })
    }

    let onError = () => {
      showVerifyNotification();
    };

    let onSuccess = data => {
      if (data) {
        showVerifyNotification(data);
      } else {
        onError();
      }
    };

    fxAccounts.resendVerificationEmail()
      .then(fxAccounts.getSignedInUser, onError)
      .then(onSuccess, onError);
  },

  openOldSyncSupportPage() {
    let url = Services.urlFormatter.formatURLPref("app.support.baseURL") + "old-sync";
    this.openContentInBrowser(url);
  },

  unlinkFirefoxAccount(confirm) {
    if (confirm) {
      // We use a string bundle shared with aboutAccounts.
      let sb = Services.strings.createBundle("chrome://browser/locale/syncSetup.properties");
      let disconnectLabel = sb.GetStringFromName("disconnect.label");
      let title = sb.GetStringFromName("disconnect.verify.title");
      let body = sb.GetStringFromName("disconnect.verify.bodyHeading") +
                 "\n\n" +
                 sb.GetStringFromName("disconnect.verify.bodyText");
      let ps = Services.prompt;
      let buttonFlags = (ps.BUTTON_POS_0 * ps.BUTTON_TITLE_IS_STRING) +
                        (ps.BUTTON_POS_1 * ps.BUTTON_TITLE_CANCEL) +
                        ps.BUTTON_POS_1_DEFAULT;

      let factory = Cc["@mozilla.org/prompter;1"]
                      .getService(Ci.nsIPromptFactory);
      let prompt = factory.getPrompt(window, Ci.nsIPrompt);
      let bag = prompt.QueryInterface(Ci.nsIWritablePropertyBag2);
      bag.setPropertyAsBool("allowTabModal", true);

      let pressed = prompt.confirmEx(title, body, buttonFlags,
                                     disconnectLabel, null, null, null, {});

      if (pressed != 0) { // 0 is the "continue" button
        return;
      }
    }
    fxAccounts.signOut().then(() => {
      this.updateWeavePrefs();
    });
  },

  openAddDevice() {
    if (!Weave.Utils.ensureMPUnlocked())
      return;

    let win = Services.wm.getMostRecentWindow("Sync:AddDevice");
    if (win)
      win.focus();
    else
      window.openDialog("chrome://browser/content/sync/addDevice.xul",
                        "syncAddDevice", "centerscreen,chrome,resizable=no");
  },

  resetSync() {
    this.openSetup("reset");
  },

  _populateComputerName(value) {
    let textbox = document.getElementById("fxaSyncComputerName");
    if (!textbox.hasAttribute("placeholder")) {
      textbox.setAttribute("placeholder",
                           Weave.Utils.getDefaultDeviceName());
    }
    textbox.value = value;
  },
};
