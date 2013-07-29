// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

Components.utils.import("resource://gre/modules/Services.jsm");

let SyncFlyoutPanel = {
  init: function() {
    if (this._isInitialized) {
      Cu.reportError("Attempted to initialize SyncFlyoutPanel more than once");
      return;
    }

    this._isInitialized = true;
    this._bundle = Services.strings.createBundle("chrome://browser/locale/sync.properties");
    Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
    let self = this;

    this._elements = {};
    [
      ['outer',                 'sync-flyoutpanel'],
      ['preSetup',              'sync-presetup-container'],
      ['easySetup',             'sync-setup-container'],
      ['manualSetup',           'sync-manualsetup-container'],
      ['setupSuccess',          'sync-setupsuccess-container'],
      ['setupFailure',          'sync-setupfailure-container'],
      ['connected',             'sync-connected-container'],
      ['pairNewDevice',         'sync-pair-container'],
      ['pairSuccess',           'sync-pair-success-container'],
      ['setupCode1',            'sync-setup-code1'],
      ['setupCode2',            'sync-setup-code2'],
      ['setupCode3',            'sync-setup-code3'],
      ['setupThrobber',         'sync-setup-throbber'],
      ['account',               'sync-manualsetup-account'],
      ['password',              'sync-manualsetup-password'],
      ['syncKey',               'sync-manualsetup-syncKey'],
      ['manualSetupConnect',    'sync-manualsetup-connect'],
      ['manualSetupFailure',    'sync-manualsetup-failure'],
      ['connectedAccount',      'sync-connected-account'],
      ['deviceName',            'sync-connected-device'],
      ['lastSync',              'sync-connected-lastSynced'],
      ['connectedThrobber',     'sync-connected-throbber'],
      ['disconnectLink',        'sync-disconnect-label'],
      ['disconnectWarning',     'sync-disconnect-warning'],
      ['pairCode1',             'sync-pair-entry1'],
      ['pairCode2',             'sync-pair-entry2'],
      ['pairCode3',             'sync-pair-entry3'],
      ['pairButton',            'sync-pair-button'],
      ['pairFailureMessage',    'sync-pair-failure'],
    ].forEach(function (aContainer) {
      let [name, id] = aContainer;
      XPCOMUtils.defineLazyGetter(self._elements, name, function() {
        return document.getElementById(id);
      });
    });

    this._topmostElement = this._elements.outer;

    let xps = Components.classes["@mozilla.org/weave/service;1"]
              .getService(Components.interfaces.nsISupports)
              .wrappedJSObject;

    if (xps.ready) {
      this._onServiceReady();
    } else {
      Services.obs.addObserver(this._onServiceReady.bind(this),
                               "weave:service:ready",
                               false);
      xps.ensureLoaded();
    }
  },

  _hide: function() {
    this._elements.outer.hide();
    this.showInitialScreen();
  },

  _hideVisibleContainer: function() {
    if (this._currentlyVisibleContainer) {
      this._currentlyVisibleContainer.collapsed = true;
      delete this._currentlyVisibleContainer;
      delete this._onBackButton;
    }
  },

  _onServiceReady: function(aEvent) {
    if (aEvent) {
      Services.obs.removeObserver(this._onServiceReady, "weave:service:ready");
    }

    this.showInitialScreen();
    Services.obs.addObserver(this._onSyncStart.bind(this), "weave:service:sync:start", false);
    Services.obs.addObserver(this._onSyncEnd.bind(this), "weave:ui:sync:finish", false);
    Services.obs.addObserver(this._onSyncEnd.bind(this), "weave:ui:sync:error", false);
    Weave.Service.scheduler.scheduleNextSync(10*1000);
  },

  _onSyncStart: function() {
    this._isSyncing = true;
    this._updateConnectedPage();
  },

  _onSyncEnd: function() {
    this._isSyncing = false;
    this._updateConnectedPage();
  },

  showInitialScreen: function() {
    if (Weave.Status.checkSetup() == Weave.CLIENT_NOT_CONFIGURED) {
      this.showPreSetup();
    } else {
      this.showConnected();
    }
  },

  abortEasySetup: function() {
    if (this._setupJpake) {
      this._setupJpake.abort();
    }
    this._cleanUpEasySetup();
  },

  _cleanUpEasySetup: function() {
    this._elements.setupCode1.value = "";
    this._elements.setupCode2.value = "";
    this._elements.setupCode3.value = "";
    delete this._setupJpake;
    this._elements.setupThrobber.collapsed = true;
    this._elements.setupThrobber.enabled = false;
  },

  _updateConnectedPage: function() {
    // Show the day-of-week and time (HH:MM) of last sync
    let lastSync = Weave.Svc.Prefs.get("lastSync");
    let syncDate = '';
    if (lastSync != null) {
      syncDate = new Date(lastSync).toLocaleFormat("%A %I:%M %p");
    }

    let device = Weave.Service.clientsEngine.localName;
    let account = Weave.Service.identity.account;
    this._elements.deviceName.textContent =
      this._bundle.formatStringFromName("sync.flyout.connected.device",
                                        [device], 1);
    this._elements.connectedAccount.textContent =
      this._bundle.formatStringFromName("sync.flyout.connected.account",
                                        [account], 1);
    this._elements.lastSync.textContent =
      this._bundle.formatStringFromName("sync.flyout.connected.lastSynced",
                                        [syncDate], 1);

    if (this._currentlyVisibleContainer == this._elements.connected
     && this._isSyncing) {
      this._elements.connectedThrobber.collapsed = false;
      this._elements.connectedThrobber.enabled = true;
    } else {
      this._elements.connectedThrobber.collapsed = true;
      this._elements.connectedThrobber.enabled = false;
    }
  },

  showConnected: function() {
    // Reset state of the connected screen
    this._elements.disconnectWarning.collapsed = true;
    this._elements.disconnectLink.collapsed = false;

    this._updateConnectedPage();
    this._showContainer(this._elements.connected);
  },

  startEasySetup: function() {
    let self = this;

    this._showContainer(this._elements.easySetup);

    // Set up our back button to do the appropriate action
    this._onBackButton = function() {
      self.abortEasySetup();
      self.showInitialScreen();
    };

    this._setupJpake = new Weave.JPAKEClient({
      displayPIN: function displayPIN(aPin) {
        self._elements.setupCode1.value = aPin.slice(0, 4);
        self._elements.setupCode2.value = aPin.slice(4, 8);
        self._elements.setupCode3.value = aPin.slice(8);
      },

      onPairingStart: function onPairingStart() {
        self._elements.setupThrobber.collapsed = false;
        self._elements.setupThrobber.enabled = true;
      },

      onComplete: function onComplete(aCredentials) {
        Weave.Service.identity.account = aCredentials.account;
        Weave.Service.identity.basicPassword = aCredentials.password;
        Weave.Service.identity.syncKey = aCredentials.synckey;
        Weave.Service.serverURL = aCredentials.serverURL;
        Weave.Service.persistLogin();
        Weave.Service.scheduler.scheduleNextSync(0);

        if (self._currentlyVisibleContainer == self._elements.easySetup) {
          self.showSetupSuccess();
        }
        self._cleanUpEasySetup();
      },

      onAbort: function onAbort(aError) {
        if (aError == "jpake.error.userabort") {
          Services.obs.notifyObservers(null, "browser:sync:setup:userabort", "");
          self._cleanUpEasySetup();
          return;
        } else if (aError == "jpake.error.network") {
          Services.obs.notifyObservers(null, "browser:sync:setup:networkerror", "");
        }

        if (self._currentlyVisibleContainer == self._elements.easySetup) {
          self.showSetupFailure();
          self._cleanUpEasySetup();
        }
      }
    });

    this._setupJpake.receiveNoPIN();
  },

  _showContainer: function(aContainer) {
    this._hideVisibleContainer();
    this._currentlyVisibleContainer = aContainer;
    this._currentlyVisibleContainer.collapsed = false;
  },

  showSetupSuccess: function() {
    this._showContainer(this._elements.setupSuccess);
    this._onBackButton = this.showInitialScreen;
  },

  showSetupFailure: function() {
    this._showContainer(this._elements.setupFailure);
    this._onBackButton = this.showInitialScreen;
  },

  showPreSetup: function() {
    this._showContainer(this._elements.preSetup);
    delete this._onBackButton;
  },

  showManualSetup: function() {
    this._showContainer(this._elements.manualSetup);
    this._onBackButton = this.showInitialScreen;

    this._elements.account.value = Weave.Service.identity.account;
    this._elements.password.value = Weave.Service.identity.basicPassword;
    this._elements.syncKey.value =
             Weave.Utils.hyphenatePassphrase(Weave.Service.identity.syncKey);
    this.updateManualSetupConnectButtonState();
  },

  updateManualSetupConnectButtonState: function() {
    this._elements.manualSetupConnect.disabled = !this._elements.account.value
                                              || !this._elements.password.value
                                              || !this._elements.syncKey.value;
  },

  manualSetupConnect: function() {
    delete this._onBackButton;
    this._elements.manualSetupConnect.disabled = true;
    Weave.Service.identity.account = this._elements.account.value;
    Weave.Service.identity.basicPassword = this._elements.password.value;
    Weave.Service.identity.syncKey = Weave.Utils.normalizePassphrase(this._elements.syncKey.value);
    if (Weave.Service.login()) {
      Weave.Service.persistLogin();
      if (this._currentlyVisibleContainer == this._elements.manualSetup) {
        this.showSetupSuccess();
      }
      Weave.Service.scheduler.scheduleNextSync(0);
    } else {
      this._elements.manualSetupFailure.textContent = Weave.Utils.getErrorString(Weave.Status.login);
      this._elements.manualSetupFailure.collapsed = false;
      this.updateManualSetupConnectButtonState();
    }
  },

  onDisconnectLink: function() {
    this._elements.disconnectWarning.collapsed = false;
    this._elements.disconnectLink.collapsed = true;
  },

  onDisconnectCancel: function() {
    this._elements.disconnectWarning.collapsed = true;
    this._elements.disconnectLink.collapsed = false;
  },

  onDisconnectButton: function() {
    Weave.Service.startOver();
    this.showInitialScreen();
  },

  onPairDeviceLink: function() {
    // Reset state
    this._elements.pairCode1.value = "";
    this._elements.pairCode2.value = "";
    this._elements.pairCode3.value = "";
    this.updatePairButtonState();
    this._elements.pairFailureMessage.collapsed = true;
    this._elements.pairNewDevice.collapsed = false;

    this._showContainer(this._elements.pairNewDevice);
    this._onBackButton = this.showInitialScreen;
  },

  updatePairButtonState: function () {
    this._elements.pairButton.disabled = !this._elements.pairCode1.value
                                      || !this._elements.pairCode2.value
                                      || !this._elements.pairCode3.value;
  },

  onCancelButton: function() {
    this.showInitialContainer();
  },

  onTryAgainButton: function() {
    this.startEasySetup();
  },

  onPairButton: function() {
    this._elements.pairButton.disabled = true;
    this._elements.pairFailureMessage.collapsed = true;
    let self = this;
    this._pairJpake = new Weave.JPAKEClient({
        onPaired: function() {
          self._pairJpake.sendAndComplete({
              account: Weave.Service.identity.account,
              password: Weave.Service.identity.basicPassword,
              synckey: Weave.Service.identity.syncKey,
              serverURL: Weave.Service.serverURL
            });
        },

        onComplete: function() {
          delete self._pairJpake;
          Weave.Service.persistLogin();
          if (self._currentlyVisibleContainer == self._elements.pairNewDevice) {
            self._showContainer(self._elements.pairSuccess);
          }
          Weave.Service.scheduler.scheduleNextSync(Weave.Service.scheduler.activeInterval);
        },

        onAbort: function(error) {
          delete self._pairJpake;
          if (error == Weave.JPAKE_ERROR_USERABORT) {
            return;
          }

          self._elements.pairFailureMessage.collapsed = false;
          self.updatePairButtonState();
        }
    });

    this._pairJpake.pairWithPIN(this._elements.pairCode1.value
                              + this._elements.pairCode2.value
                              + this._elements.pairCode3.value,
                                false);
  },
};
