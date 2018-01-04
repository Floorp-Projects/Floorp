/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

Components.utils.import("resource://services-sync/main.js");
Components.utils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "FxAccountsCommon", function() {
  return Components.utils.import("resource://gre/modules/FxAccountsCommon.js", {});
});

XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
  "resource://gre/modules/FxAccounts.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "UIState",
  "resource://services-sync/UIState.jsm");

const FXA_PAGE_LOGGED_OUT = 0;
const FXA_PAGE_LOGGED_IN = 1;

// Indexes into the "login status" deck.
// We are in a successful verified state - everything should work!
const FXA_LOGIN_VERIFIED = 0;
// We have logged in to an unverified account.
const FXA_LOGIN_UNVERIFIED = 1;
// We are logged in locally, but the server rejected our credentials.
const FXA_LOGIN_FAILED = 2;

var gSyncPane = {
  get page() {
    return document.getElementById("weavePrefsDeck").selectedIndex;
  },

  set page(val) {
    document.getElementById("weavePrefsDeck").selectedIndex = val;
  },

  init() {
    this._setupEventListeners();
    this._adjustForPrefs();

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
      window.removeEventListener("unload", onUnload);
      try {
        Services.obs.removeObserver(onReady, "weave:service:ready");
      } catch (e) { }
    };

    let onReady = () => {
      Services.obs.removeObserver(onReady, "weave:service:ready");
      window.removeEventListener("unload", onUnload);
      this._init();
    };

    Services.obs.addObserver(onReady, "weave:service:ready");
    window.addEventListener("unload", onUnload);

    xps.ensureLoaded();
  },

  // make whatever tweaks we need based on preferences.
  _adjustForPrefs() {
    // These 2 engines are unique in that there are prefs that make the
    // entire engine unavailable (which is distinct from "disabled").
    let enginePrefs = [
      ["services.sync.engine.addresses.available", "engine.addresses"],
      ["services.sync.engine.creditcards.available", "engine.creditcards"],
    ];
    let numHidden = 0;
    for (let [availablePref, prefName] of enginePrefs) {
      if (!Services.prefs.getBoolPref(availablePref)) {
        let checkbox = document.querySelector("[preference=\"" + prefName + "\"]");
        checkbox.hidden = true;
        numHidden += 1;
      }
    }
    // If we hid both, the list of prefs is unbalanced, so move "history" to
    // the second column. (If we only moved one, it's still unbalanced, but
    // there's an odd number of engines so that can't be avoided)
    if (numHidden == 2) {
      let history = document.querySelector("[preference=\"engine.history\"]");
      let addons = document.querySelector("[preference=\"engine.addons\"]");
      addons.parentNode.insertBefore(history, addons);
    }
  },

  _showLoadPage(xps) {
    let username = Services.prefs.getCharPref("services.sync.username", "");
    if (!username) {
      this.page = FXA_PAGE_LOGGED_OUT;
      return;
    }

    // Use cached values while we wait for the up-to-date values
    let cachedComputerName = Services.prefs.getCharPref("services.sync.client.name", "");
    document.querySelector(".fxaEmailAddress").value = username;
    this._populateComputerName(cachedComputerName);
    this.page = FXA_PAGE_LOGGED_IN;
  },

  _init() {
    // Add the observers now and remove them on unload
    // XXXzpao This should use Services.obs.* but Weave's Obs does nice handling
    //        of `this`. Fix in a followup. (bug 583347)
    Weave.Svc.Obs.add(UIState.ON_UPDATE, this.updateWeavePrefs, this);

    window.addEventListener("unload", () => {
      Weave.Svc.Obs.remove(UIState.ON_UPDATE, this.updateWeavePrefs, this);
    });

    XPCOMUtils.defineLazyGetter(this, "_accountsStringBundle", () => {
      return Services.strings.createBundle("chrome://browser/locale/accounts.properties");
    });

    let url = Services.prefs.getCharPref("identity.mobilepromo.android") + "sync-preferences";
    document.getElementById("fxaMobilePromo-android").setAttribute("href", url);
    document.getElementById("fxaMobilePromo-android-hasFxaAccount").setAttribute("href", url);
    url = Services.prefs.getCharPref("identity.mobilepromo.ios") + "sync-preferences";
    document.getElementById("fxaMobilePromo-ios").setAttribute("href", url);
    document.getElementById("fxaMobilePromo-ios-hasFxaAccount").setAttribute("href", url);

    document.getElementById("tosPP-small-ToS").setAttribute("href", Weave.Svc.Prefs.get("fxa.termsURL"));
    document.getElementById("tosPP-small-PP").setAttribute("href", Weave.Svc.Prefs.get("fxa.privacyURL"));

    fxAccounts.promiseAccountsSignUpURI(this._getEntryPoint()).then(signUpURI => {
      document.getElementById("noFxaSignUp").setAttribute("href", signUpURI);
    });

    this.updateWeavePrefs();

    // Notify observers that the UI is now ready
    Services.obs.notifyObservers(window, "sync-pane-loaded");
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
    function setEventListener(aId, aEventType, aCallback) {
      document.getElementById(aId)
        .addEventListener(aEventType, aCallback.bind(gSyncPane));
    }

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

  updateWeavePrefs() {
    let service = Components.classes["@mozilla.org/weave/service;1"]
      .getService(Components.interfaces.nsISupports)
      .wrappedJSObject;

    let displayNameLabel = document.getElementById("fxaDisplayName");
    let fxaEmailAddressLabels = document.querySelectorAll(".fxaEmailAddress");
    displayNameLabel.hidden = true;

    // determine the fxa status...
    this._showLoadPage(service);

    let state = UIState.get();
    if (state.status == UIState.STATUS_NOT_CONFIGURED) {
      this.page = FXA_PAGE_LOGGED_OUT;
      return;
    }
    this.page = FXA_PAGE_LOGGED_IN;
    // We are logged in locally, but maybe we are in a state where the
    // server rejected our credentials (eg, password changed on the server)
    let fxaLoginStatus = document.getElementById("fxaLoginStatus");
    let syncReady = false; // Is sync able to actually sync?
    // We need to check error states that need a re-authenticate to resolve
    // themselves first.
    if (state.status == UIState.STATUS_LOGIN_FAILED) {
      fxaLoginStatus.selectedIndex = FXA_LOGIN_FAILED;
    } else if (state.status == UIState.STATUS_NOT_VERIFIED) {
      fxaLoginStatus.selectedIndex = FXA_LOGIN_UNVERIFIED;
    } else {
      // We must be golden (or in an error state we expect to magically
      // resolve itself)
      fxaLoginStatus.selectedIndex = FXA_LOGIN_VERIFIED;
      syncReady = true;
    }
    fxaEmailAddressLabels.forEach((label) => {
      label.value = state.email;
    });
    this._populateComputerName(Weave.Service.clientsEngine.localName);
    let engines = document.getElementById("fxaSyncEngines");
    for (let checkbox of engines.querySelectorAll("checkbox")) {
      checkbox.disabled = !syncReady;
    }
    document.getElementById("fxaChangeDeviceName").disabled = !syncReady;

    // Clear the profile image (if any) of the previously logged in account.
    document.querySelector("#fxaLoginVerified > .fxaProfileImage").style.removeProperty("list-style-image");

    if (state.displayName) {
      fxaLoginStatus.setAttribute("hasName", true);
      displayNameLabel.hidden = false;
      displayNameLabel.textContent = state.displayName;
    } else {
      fxaLoginStatus.removeAttribute("hasName");
    }
    if (state.avatarURL) {
      let bgImage = "url(\"" + state.avatarURL + "\")";
      let profileImageElement = document.querySelector("#fxaLoginVerified > .fxaProfileImage");
      profileImageElement.style.listStyleImage = bgImage;

      let img = new Image();
      img.onerror = () => {
        // Clear the image if it has trouble loading. Since this callback is asynchronous
        // we check to make sure the image is still the same before we clear it.
        if (profileImageElement.style.listStyleImage === bgImage) {
          profileImageElement.style.removeProperty("list-style-image");
        }
      };
      img.src = state.avatarURL;
    }
    // The "manage account" link embeds the uid, so we need to update this
    // if the account state changes.
    fxAccounts.promiseAccountsManageURI(this._getEntryPoint()).then(accountsManageURI => {
      document.getElementById("verifiedManage").setAttribute("href", accountsManageURI);
    });
  },

  _getEntryPoint() {
    let params = new URLSearchParams(document.URL.split("#")[0].split("?")[1] || "");
    return params.get("entrypoint") || "preferences";
  },

  openContentInBrowser(url, options) {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    if (!win) {
      openUILinkIn(url, "tab");
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

  async signIn() {
    const url = await fxAccounts.promiseAccountsSignInURI(this._getEntryPoint());
    this.replaceTabWithUrl(url);
  },

  async reSignIn() {
    // There's a bit of an edge-case here - we might be forcing reauth when we've
    // lost the FxA account data - in which case we'll not get a URL as the re-auth
    // URL embeds account info and the server endpoint complains if we don't
    // supply it - So we just use the regular "sign in" URL in that case.
    let entryPoint = this._getEntryPoint();
    const url = (await fxAccounts.promiseAccountsForceSigninURI(entryPoint)) ||
                (await fxAccounts.promiseAccountsSignInURI(entryPoint));
    this.replaceTabWithUrl(url);
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
            replaceQueryString: true,
            triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
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
          replaceQueryString: true,
          triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
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
      new Notification(title, { body });
    };

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

  unlinkFirefoxAccount(confirm) {
    if (confirm) {
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

  _populateComputerName(value) {
    let textbox = document.getElementById("fxaSyncComputerName");
    if (!textbox.hasAttribute("placeholder")) {
      textbox.setAttribute("placeholder",
        Weave.Utils.getDefaultDeviceName());
    }
    textbox.value = value;
  },
};
