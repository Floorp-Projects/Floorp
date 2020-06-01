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

// Indexes into the "sync status" deck.
const SYNC_DISCONNECTED = 0;
const SYNC_CONNECTED = 1;

var gSyncPane = {
  get page() {
    return document.getElementById("weavePrefsDeck").selectedIndex;
  },

  set page(val) {
    document.getElementById("weavePrefsDeck").selectedIndex = val;
  },

  init() {
    this._setupEventListeners();
    this.setupEnginesUI();

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

    // it may take some time before all the promises we care about resolve, so
    // pre-load what we can from synchronous sources.
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

  _showLoadPage(xps) {
    let maybeAcct = false;
    let username = Services.prefs.getCharPref("services.sync.username", "");
    if (username) {
      document.getElementById("fxaEmailAddress").textContent = username;
      maybeAcct = true;
    }

    let cachedComputerName = Services.prefs.getStringPref(
      "identity.fxaccounts.account.device.name",
      undefined
    );
    if (cachedComputerName) {
      maybeAcct = true;
      this._populateComputerName(cachedComputerName);
    }
    this.page = maybeAcct ? FXA_PAGE_LOGGED_IN : FXA_PAGE_LOGGED_OUT;
  },

  _init() {
    Weave.Svc.Obs.add(UIState.ON_UPDATE, this.updateWeavePrefs, this);

    window.addEventListener("unload", () => {
      Weave.Svc.Obs.remove(UIState.ON_UPDATE, this.updateWeavePrefs, this);
    });

    XPCOMUtils.defineLazyGetter(this, "_accountsStringBundle", () => {
      return Services.strings.createBundle(
        "chrome://browser/locale/accounts.properties"
      );
    });

    FxAccounts.config
      .promiseConnectDeviceURI(this._getEntryPoint())
      .then(connectURI => {
        document
          .getElementById("connect-another-device")
          .setAttribute("href", connectURI);
      });
    // Links for mobile devices.
    for (let platform of ["android", "ios"]) {
      let url =
        Services.prefs.getCharPref(`identity.mobilepromo.${platform}`) +
        "sync-preferences";
      for (let elt of document.querySelectorAll(
        `.fxaMobilePromo-${platform}`
      )) {
        elt.setAttribute("href", url);
      }
    }

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
    setEventListener("syncSetup", "command", function() {
      this._chooseWhatToSync(false);
    });
    setEventListener("syncChangeOptions", "command", function() {
      this._chooseWhatToSync(true);
    });
    setEventListener("syncNow", "command", function() {
      // syncing can take a little time to send the "started" notification, so
      // pretend we already got it.
      this._updateSyncNow(true);
      Weave.Service.sync({ why: "aboutprefs" });
    });
    setEventListener("syncNow", "mouseover", function() {
      const state = UIState.get();
      // If we are currently syncing, just set the tooltip to the same as the
      // button label (ie, "Syncing...")
      let tooltiptext = state.syncing
        ? document.getElementById("syncNow").getAttribute("label")
        : window.docShell.rootTreeItem.domWindow.gSync.formatLastSyncDate(
            state.lastSync
          );
      document
        .getElementById("syncNow")
        .setAttribute("tooltiptext", tooltiptext);
    });
  },

  async _chooseWhatToSync(isAlreadySyncing) {
    // Assuming another device is syncing and we're not,
    // we update the engines selection so the correct
    // checkboxes are pre-filed.
    if (!isAlreadySyncing) {
      try {
        await Weave.Service.updateLocalEnginesState();
      } catch (err) {
        console.error("Error updating the local engines state", err);
      }
    }
    let params = {};
    if (isAlreadySyncing) {
      // If we are already syncing then we also offer to disconnect.
      params.disconnectFun = () => this.disconnectSync();
    }
    gSubDialog.open(
      "chrome://browser/content/preferences/dialogs/syncChooseWhatToSync.xhtml",
      "" /* aFeatures */,
      params /* aParams */,
      event => {
        /* aClosingCallback */
        if (!isAlreadySyncing && event.detail.button == "accept") {
          // We weren't syncing but the user has accepted the dialog - so we
          // want to start!
          fxAccounts.telemetry
            .recordConnection(["sync"], "ui")
            .then(() => {
              return Weave.Service.configure();
            })
            .catch(err => {
              console.error("Failed to enable sync", err);
            });
        }
      }
    );
  },

  _updateSyncNow(syncing) {
    let butSyncNow = document.getElementById("syncNow");
    if (syncing) {
      butSyncNow.setAttribute("label", butSyncNow.getAttribute("labelsyncing"));
      butSyncNow.removeAttribute("accesskey");
      butSyncNow.disabled = true;
    } else {
      butSyncNow.setAttribute(
        "label",
        butSyncNow.getAttribute("labelnotsyncing")
      );
      butSyncNow.setAttribute(
        "accesskey",
        butSyncNow.getAttribute("accesskeynotsyncing")
      );
      butSyncNow.disabled = false;
    }
  },

  updateWeavePrefs() {
    let service = Cc["@mozilla.org/weave/service;1"].getService(Ci.nsISupports)
      .wrappedJSObject;

    let displayNameLabel = document.getElementById("fxaDisplayName");
    let fxaEmailAddressLabels = document.querySelectorAll(
      ".l10nArgsEmailAddress"
    );
    displayNameLabel.hidden = true;

    // while we determine the fxa status pre-load what we can.
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
    for (let elt of document.querySelectorAll("needs-account-ready")) {
      elt.disabled = !syncReady;
    }

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
    // and the actual sync state.
    let eltSyncStatus = document.getElementById("syncStatus");
    eltSyncStatus.hidden = !syncReady;
    eltSyncStatus.selectedIndex = state.syncEnabled
      ? SYNC_CONNECTED
      : SYNC_DISCONNECTED;
    this._updateSyncNow(state.syncing);
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
    const url = await FxAccounts.config.promiseConnectAccountURI(
      this._getEntryPoint()
    );
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
      (await FxAccounts.config.promiseConnectAccountURI(entryPoint));
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
      .then(() => fxAccounts.getSignedInUser(), onError)
      .then(onSuccess, onError);
  },

  // Disconnect the account, including everything linked.
  unlinkFirefoxAccount(confirm) {
    window.docShell.rootTreeItem.domWindow.gSync.disconnect({ confirm });
  },

  // Disconnect sync, leaving the account connected.
  disconnectSync() {
    return window.docShell.rootTreeItem.domWindow.gSync.disconnect({
      confirm: true,
      disconnectAccount: false,
    });
  },

  pairAnotherDevice() {
    gSubDialog.open(
      "chrome://browser/content/preferences/fxaPairDevice.xhtml",
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

  // arranges to dynamically show or hide sync engine name elements based on the
  // preferences used for this engines.
  setupEnginesUI() {
    let observe = (elt, prefName) => {
      let enabled = Services.prefs.getBoolPref(prefName, false);
      if (enabled) {
        elt.removeAttribute("hidden");
      } else {
        elt.setAttribute("hidden", "true");
      }
    };

    for (let elt of document.querySelectorAll("[engine_preference]")) {
      let prefName = elt.getAttribute("engine_preference");
      let obs = observe.bind(null, elt, prefName);
      obs();
      Services.prefs.addObserver(prefName, obs);
      window.addEventListener("unload", () => {
        Services.prefs.removeObserver(prefName, obs);
      });
    }
  },
};
