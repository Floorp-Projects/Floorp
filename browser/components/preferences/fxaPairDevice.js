// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { FxAccounts } = ChromeUtils.import(
  "resource://gre/modules/FxAccounts.jsm"
);
const { Weave } = ChromeUtils.import("resource://services-sync/main.js");

XPCOMUtils.defineLazyModuleGetters(this, {
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",
  FxAccountsPairingFlow: "resource://gre/modules/FxAccountsPairing.jsm",
});
const { require } = ChromeUtils.import(
  "resource://devtools/shared/Loader.jsm",
  {}
);
const QR = require("devtools/shared/qrcode/index");

// This is only for "labor illusion", see
// https://www.fastcompany.com/3061519/the-ux-secret-that-will-ruin-apps-for-you
const MIN_PAIRING_LOADING_TIME_MS = 1000;

/**
 * Communication between FxAccountsPairingFlow and gFxaPairDeviceDialog
 * is done using an emitter via the following messages:
 * <- [view:SwitchToWebContent] - Notifies the view to navigate to a specific URL.
 * <- [view:Error] - Notifies the view something went wrong during the pairing process.
 * -> [view:Closed] - Notifies the pairing module the view was closed.
 */
var gFxaPairDeviceDialog = {
  init() {
    this._resetBackgroundQR();
    FxAccounts.config
      .promiseConnectDeviceURI("pairing-modal")
      .then(connectURI => {
        document
          .getElementById("connect-another-device-link")
          .setAttribute("href", connectURI);
      });
    // We let the modal show itself before eventually showing a master-password dialog later.
    Services.tm.dispatchToMainThread(() => this.startPairingFlow());
  },

  uninit() {
    this.teardownListeners();
    this._emitter.emit("view:Closed");
  },

  async startPairingFlow() {
    this._resetBackgroundQR();
    document
      .getElementById("qrWrapper")
      .setAttribute("pairing-status", "loading");
    this._emitter = new EventEmitter();
    this.setupListeners();
    try {
      if (!Weave.Utils.ensureMPUnlocked()) {
        throw new Error("Master-password locked.");
      }
      const [, uri] = await Promise.all([
        new Promise(res => setTimeout(res, MIN_PAIRING_LOADING_TIME_MS)),
        FxAccountsPairingFlow.start({ emitter: this._emitter }),
      ]);
      const imgData = QR.encodeToDataURI(uri, "L");
      document.getElementById(
        "qrContainer"
      ).style.backgroundImage = `url("${imgData.src}")`;
      document
        .getElementById("qrWrapper")
        .setAttribute("pairing-status", "ready");
    } catch (e) {
      this.onError(e);
    }
  },

  _resetBackgroundQR() {
    // The text we encode doesn't really matter as it is un-scannable (blurry and very transparent).
    const imgData = QR.encodeToDataURI(
      "https://accounts.firefox.com/pair",
      "L"
    );
    document.getElementById(
      "qrContainer"
    ).style.backgroundImage = `url("${imgData.src}")`;
  },

  onError(err) {
    Cu.reportError(err);
    this.teardownListeners();
    document
      .getElementById("qrWrapper")
      .setAttribute("pairing-status", "error");
  },

  _switchToUrl(url) {
    const browser = window.docShell.chromeEventHandler;
    browser.loadURI(url, {
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
        {}
      ),
    });
  },

  setupListeners() {
    this._switchToWebContent = (_, url) => this._switchToUrl(url);
    this._onError = (_, error) => this.onError(error);
    this._emitter.once("view:SwitchToWebContent", this._switchToWebContent);
    this._emitter.on("view:Error", this._onError);
  },

  teardownListeners() {
    try {
      this._emitter.off("view:SwitchToWebContent", this._switchToWebContent);
      this._emitter.off("view:Error", this._onError);
    } catch (e) {
      console.warn("Error while tearing down listeners.", e);
    }
  },
};
