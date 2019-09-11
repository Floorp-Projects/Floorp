/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

var { Weave } = ChromeUtils.import("resource://services-sync/main.js");
var { FxAccounts, fxAccounts } = ChromeUtils.import(
  "resource://gre/modules/FxAccounts.jsm"
);
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "FxAccountsCommon", function() {
  return ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js", {});
});

XPCOMUtils.defineLazyModuleGetters(this, {
  SyncDisconnect: "resource://services-sync/SyncDisconnect.jsm",
  UIState: "resource://services-sync/UIState.jsm",
});

const FXA_PAGE_LOGGED_OUT = 0;
const FXA_PAGE_LOGGED_IN = 1;

// Indexes into the "login status" deck.
// We are in a successful verified state - everything should work!
const FXA_LOGIN_VERIFIED = 0;
// We have logged in to an unverified account.
const FXA_LOGIN_UNVERIFIED = 1;
// We are logged in locally, but the server rejected our credentials.
const FXA_LOGIN_FAILED = 2;

Preferences.addAll([
  { id: "services.sync.engine.addons", type: "bool" },
  { id: "services.sync.engine.bookmarks", type: "bool" },
  { id: "services.sync.engine.history", type: "bool" },
  { id: "services.sync.engine.tabs", type: "bool" },
  { id: "services.sync.engine.prefs", type: "bool" },
  { id: "services.sync.engine.passwords", type: "bool" },
  { id: "services.sync.engine.addresses", type: "bool" },
  { id: "services.sync.engine.creditcards", type: "bool" },
]);

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

    document
      .getElementById("weavePrefsDeck")
      .removeAttribute("data-hidden-from-search");

    // If the Service hasn't finished initializing, wait for it.
    let xps = Cc["@mozilla.org/weave/service;1"].getService(Ci.nsISupports)
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
      } catch (e) {}
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
      [
        "services.sync.engine.addresses.available",
        "services.sync.engine.addresses",
      ],
      [
        "services.sync.engine.creditcards.available",
        "services.sync.engine.creditcards",
      ],
    ];
    let numHidden = 0;
    for (let [availablePref, prefName] of enginePrefs) {
      if (!Services.prefs.getBoolPref(availablePref)) {
        let checkbox = document.querySelector(
          '[preference="' + prefName + '"]'
        );
        checkbox.hidden = true;
        numHidden += 1;
      }
    }
    // If we hid both, the list of prefs is unbalanced, so move "history" to
    // the second column. (If we only moved one, it's still unbalanced, but
    // there's an odd number of engines so that can't be avoided)
    if (numHidden == 2) {
      let history = document.querySelector(
        '[preference="services.sync.engine.history"]'
      );
      let addons = document.querySelector(
        '[preference="services.sync.engine.addons"]'
      );
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
    let cachedComputerName = Services.prefs.getCharPref(
      "services.sync.client.name",
      ""
    );
    document.getElementById("fxaEmailAddress").textContent = username;
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
      return Services.strings.createBundle(
        "chrome://browser/locale/accounts.properties"
      );
    });

    // Links for mobile devices before the user is logged in.
    let url =
      Services.prefs.getCharPref("identity.mobilepromo.android") +
      "sync-preferences";
    document.getElementById("fxaMobilePromo-android").setAttribute("href", url);
    url =
      Services.prefs.getCharPref("identity.mobilepromo.ios") +
      "sync-preferences";
    document.getElementById("fxaMobilePromo-ios").setAttribute("href", url);

    // Links for mobile devices shown after the user is logged in.
    FxAccounts.config
      .promiseConnectDeviceURI(this._getEntryPoint())
      .then(connectURI => {
        document
          .getElementById("connect-another-device")
          .setAttribute("href", connectURI);
      });

    FxAccounts.config
      .promiseManageDevicesURI(this._getEntryPoint())
      .then(manageURI => {
        document
          .getElementById("manage-devices")
          .setAttribute("href", manageURI);
      });

    FxAccounts.config.promiseLegalTermsURI().then(uri => {
      document.getElementById("tosPP-small-ToS").setAttribute("href", uri);
    });

    FxAccounts.config.promiseLegalPrivacyURI().then(uri => {
      document.getElementById("tosPP-small-PP").setAttribute("href", uri);
    });

    FxAccounts.config
      .promiseSignUpURI(this._getEntryPoint())
      .then(signUpURI => {
        document.getElementById("noFxaSignUp").setAttribute("href", signUpURI);
      });

    this.updateWeavePrefs();

    // Notify observers that the UI is now ready
    Services.obs.notifyObservers(window, "sync-pane-loaded");

    // document.location.search is empty, so we simply match on `action=pair`.
    if (
      location.href.includes("action=pair") &&
      location.hash == "#sync" &&
      UIState.get().status == UIState.STATUS_SIGNED_IN
    ) {
      gSyncPane.pairAnotherDevice();
    }
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
    Services.focus.moveFocus(
      window,
      document.getElementById("fxaSyncComputerName"),
      Services.focus.MOVEFOCUS_FORWARD,
      0
    );
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
      document
        .getElementById(aId)
        .addEventListener(aEventType, aCallback.bind(gSyncPane));
    }

    setEventListener("openChangeProfileImage", "click", function(event) {
      gSyncPane.openChangeProfileImage(event);
    });
    setEventListener("openChangeProfileImage", "keypress", function(event) {
      gSyncPane.openChangeProfileImage(event);
    });
    setEventListener("verifiedManage", "keypress", function(event) {
      gSyncPane.openManageFirefoxAccount(event);
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
    setEventListener("noFxaSignIn", "command", function() {
      gSyncPane.signIn();
      return false;
    });
    setEventListener("fxaUnlinkButton", "command", function() {
      gSyncPane.unlinkFirefoxAccount(true);
    });
    setEventListener(
      "verifyFxaAccount",
      "command",
      gSyncPane.verifyFirefoxAccount
    );
    setEventListener("unverifiedUnlinkFxaAccount", "command", function() {
      /* no warning as account can't have previously synced */
      gSyncPane.unlinkFirefoxAccount(false);
    });
    setEventListener("rejectReSignIn", "command", gSyncPane.reSignIn);
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
    let service = Cc["@mozilla.org/weave/service;1"].getService(Ci.nsISupports)
      .wrappedJSObject;

    let displayNameLabel = document.getElementById("fxaDisplayName");
    let fxaEmailAddressLabels = document.querySelectorAll(
      ".l10nArgsEmailAddress"
    );
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
    fxaEmailAddressLabels.forEach(label => {
      let l10nAttrs = document.l10n.getAttributes(label);
      document.l10n.setAttributes(label, l10nAttrs.id, { email: state.email });
    });
    document.getElementById("fxaEmailAddress").textContent = state.email;

    this._populateComputerName(Weave.Service.clientsEngine.localName);
    let engines = document.getElementById("fxaSyncEngines");
    for (let checkbox of engines.querySelectorAll("checkbox")) {
      checkbox.disabled = !syncReady;
    }
    document.getElementById("fxaChangeDeviceName").disabled = !syncReady;

    // Clear the profile image (if any) of the previously logged in account.
    document
      .querySelector("#fxaLoginVerified > .fxaProfileImage")
      .style.removeProperty("list-style-image");

    if (state.displayName) {
      fxaLoginStatus.setAttribute("hasName", true);
      displayNameLabel.hidden = false;
      document.getElementById("fxaDisplayNameHeading").textContent =
        state.displayName;
    } else {
      fxaLoginStatus.removeAttribute("hasName");
    }
    if (state.avatarURL && !state.avatarIsDefault) {
      let bgImage = 'url("' + state.avatarURL + '")';
      let profileImageElement = document.querySelector(
        "#fxaLoginVerified > .fxaProfileImage"
      );
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
    FxAccounts.config
      .promiseManageURI(this._getEntryPoint())
      .then(accountsManageURI => {
        document
          .getElementById("verifiedManage")
          .setAttribute("href", accountsManageURI);
      });
    let isUnverified = state.status == UIState.STATUS_NOT_VERIFIED;
    // The mobile promo links - which one is shown depends on the number of devices.
    let isMultiDevice = Weave.Service.clientsEngine.stats.numClients > 1;
    document.getElementById("connect-another-device").hidden = isUnverified;
    document.getElementById("manage-devices").hidden =
      isUnverified || !isMultiDevice;
  },

  _getEntryPoint() {
    let params = new URLSearchParams(
      document.URL.split("#")[0].split("?")[1] || ""
    );
    return params.get("entrypoint") || "preferences";
  },

  openContentInBrowser(url, options) {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    if (!win) {
      openTrustedLinkIn(url, "tab");
      return;
    }
    win.switchToTabHavingURI(url, true, options);
  },

  // Replace the current tab with the specified URL.
  replaceTabWithUrl(url) {
    // Get the <browser> element hosting us.
    let browser = window.docShell.chromeEventHandler;
    // And tell it to load our URL.
    browser.loadURI(url, {
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
        {}
      ),
    });
  },

  async signIn() {
    const url = await FxAccounts.config.promiseSignInURI(this._getEntryPoint());
    this.replaceTabWithUrl(url);
  },

  async reSignIn() {
    // There's a bit of an edge-case here - we might be forcing reauth when we've
    // lost the FxA account data - in which case we'll not get a URL as the re-auth
    // URL embeds account info and the server endpoint complains if we don't
    // supply it - So we just use the regular "sign in" URL in that case.
    let entryPoint = this._getEntryPoint();
    const url =
      (await FxAccounts.config.promiseForceSigninURI(entryPoint)) ||
      (await FxAccounts.config.promiseSignInURI(entryPoint));
    this.replaceTabWithUrl(url);
  },

  clickOrSpaceOrEnterPressed(event) {
    // Note: charCode is deprecated, but 'char' not yet implemented.
    // Replace charCode with char when implemented, see Bug 680830
    return (
      (event.type == "click" && event.button == 0) ||
      (event.type == "keypress" &&
        (event.charCode == KeyEvent.DOM_VK_SPACE ||
          event.keyCode == KeyEvent.DOM_VK_RETURN))
    );
  },

  openChangeProfileImage(event) {
    if (this.clickOrSpaceOrEnterPressed(event)) {
      FxAccounts.config
        .promiseChangeAvatarURI(this._getEntryPoint())
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
    FxAccounts.config.promiseManageURI(this._getEntryPoint()).then(url => {
      this.openContentInBrowser(url, {
        replaceQueryString: true,
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      });
    });
  },

  verifyFirefoxAccount() {
    let showVerifyNotification = data => {
      let isError = !data;
      let maybeNot = isError ? "Not" : "";
      let sb = this._accountsStringBundle;
      let title = sb.GetStringFromName("verification" + maybeNot + "SentTitle");
      let email = !isError && data ? data.email : "";
      let body = sb.formatStringFromName(
        "verification" + maybeNot + "SentBody",
        [email]
      );
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

    fxAccounts
      .resendVerificationEmail()
      .then(fxAccounts.getSignedInUser, onError)
      .then(onSuccess, onError);
  },

  unlinkFirefoxAccount(confirm) {
    if (confirm) {
      gSubDialog.open(
        "chrome://browser/content/preferences/in-content/syncDisconnect.xul",
        "resizable=no" /* aFeatures */,
        null /* aParams */,
        event => {
          /* aClosingCallback */
          if (event.detail.button == "accept") {
            this.updateWeavePrefs();
          }
        }
      );
    } else {
      // no confirmation implies no data removal, so just disconnect - but
      // we still disconnect via the SyncDisconnect module for consistency.
      SyncDisconnect.disconnect().finally(() => this.updateWeavePrefs());
    }
  },

  pairAnotherDevice() {
    gSubDialog.open(
      "chrome://browser/content/preferences/in-content/fxaPairDevice.xul",
      "resizable=no" /* aFeatures */,
      null /* aParams */,
      null /* aClosingCallback */
    );
  },

  _populateComputerName(value) {
    let textbox = document.getElementById("fxaSyncComputerName");
    if (!textbox.hasAttribute("placeholder")) {
      textbox.setAttribute(
        "placeholder",
        fxAccounts.device.getDefaultLocalName()
      );
    }
    textbox.value = value;
  },
};
