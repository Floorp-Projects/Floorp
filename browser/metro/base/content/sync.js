/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Sync = {
  setupData: null,
  _boundOnEngineSync: null,     // Needed to unhook the observers in close().
  _boundOnServiceSync: null,
  jpake: null,
  _bundle: null,
  _loginError: false,
  _progressBar: null,
  _progressValue: 0,
  _progressMax: null,

  get _isSetup() {
    if (Weave.Status.checkSetup() == Weave.CLIENT_NOT_CONFIGURED) {
      return false;
    }
    // check for issues related to failed logins that do not have anything to
    // do with network, server, and other non-client issues. See the login
    // failure status codes in sync service.
    return (Weave.Status.login != Weave.LOGIN_FAILED_NO_USERNAME &&
            Weave.Status.login != Weave.LOGIN_FAILED_NO_PASSWORD &&
            Weave.Status.login != Weave.LOGIN_FAILED_NO_PASSPHRASE &&
            Weave.Status.login != Weave.LOGIN_FAILED_INVALID_PASSPHRASE &&
            Weave.Status.login != Weave.LOGIN_FAILED_LOGIN_REJECTED);
  },

  init: function init() {
    if (this._bundle) {
      return;
    }

    let service = Components.classes["@mozilla.org/weave/service;1"]
                                    .getService(Components.interfaces.nsISupports)
                                    .wrappedJSObject;

    if (service.ready) {
      this._init();
    } else {
      Services.obs.addObserver(this, "weave:service:ready", false);
      service.ensureLoaded();
    }
  },

#ifdef XP_WIN
  _securelySetupFromMetro: function() {
    let metroUtils = Cc["@mozilla.org/windows-metroutils;1"].
                     createInstance(Ci.nsIWinMetroUtils);
    var email = {}, password = {}, key = {};
    try {
      metroUtils.loadSyncInfo(email, password, key);
    } catch (ex) {
        return false;
    }

    let server = Weave.Service.serverURL;
    let defaultPrefs = Services.prefs.getDefaultBranch(null);
    if (server == defaultPrefs.getCharPref("services.sync.serverURL"))
      server = "";

    this.setupData = {
      account: email.value,
      password: password.value,
      synckey: key.value,
      serverURL: server
    };

    try {
      metroUtils.clearSyncInfo();
    } catch (ex) {
    }

    this.connect();
    return true;
  },
#endif

  _init: function () {
    this._bundle = Services.strings.createBundle("chrome://browser/locale/sync.properties");

    this._addListeners();

    this.setupData = { account: "", password: "" , synckey: "", serverURL: "" };

    if (this._isSetup) {
      this.loadSetupData();
    }

    // Update the state of the ui
    this._updateUI();

    this._boundOnEngineSync = this.onEngineSync.bind(this);
    this._boundOnServiceSync = this.onServiceSync.bind(this);
    this._progressBar = document.getElementById("syncsetup-progressbar");

#ifdef XP_WIN
    if (Weave.Status.checkSetup() == Weave.CLIENT_NOT_CONFIGURED) {
      this._securelySetupFromMetro();
    }
#endif
  },

  abortEasySetup: function abortEasySetup() {
    document.getElementById("syncsetup-code1").value = "....";
    document.getElementById("syncsetup-code2").value = "....";
    document.getElementById("syncsetup-code3").value = "....";
    if (!this.jpake)
      return;

    this.jpake.abort();
    this.jpake = null;
  },

  _resetScrollPosition: function _resetScrollPosition() {
    let scrollboxes = document.getElementsByClassName("syncsetup-scrollbox");
    for (let i = 0; i < scrollboxes.length; i++) {
      let sbo = scrollboxes[i].boxObject.QueryInterface(Ci.nsIScrollBoxObject);
      try {
        sbo.scrollTo(0, 0);
      } catch(e) {}
    }
  },

  open: function open() {
    let container = document.getElementById("syncsetup-container");
    if (!container.hidden)
      return;

    // Services.io.offline is lying to us, so we use the NetworkLinkService instead
    let nls = Cc["@mozilla.org/network/network-link-service;1"].getService(Ci.nsINetworkLinkService);
    if (!nls.isLinkUp) {
      Services.obs.notifyObservers(null, "browser:sync:setup:networkerror", "");
      Services.prompt.alert(window,
                             this._bundle.GetStringFromName("sync.setup.error.title"),
                             this._bundle.GetStringFromName("sync.setup.error.network"));
      return;
    }

    // Clear up any previous JPAKE codes
    this.abortEasySetup();

    // Show the connect UI
    container.hidden = false;
    document.getElementById("syncsetup-simple").hidden = false;
    document.getElementById("syncsetup-waiting").hidden = true;
    document.getElementById("syncsetup-fallback").hidden = true;

    DialogUI.pushDialog(this);

    let self = this;
    this.jpake = new Weave.JPAKEClient({
      displayPIN: function displayPIN(aPin) {
        document.getElementById("syncsetup-code1").value = aPin.slice(0, 4);
        document.getElementById("syncsetup-code2").value = aPin.slice(4, 8);
        document.getElementById("syncsetup-code3").value = aPin.slice(8);
      },

      onPairingStart: function onPairingStart() {
        document.getElementById("syncsetup-simple").hidden = true;
        document.getElementById("syncsetup-waiting").hidden = false;
      },

      onComplete: function onComplete(aCredentials) {
        self.jpake = null;

        self._progressBar.mode = "determined";
        document.getElementById("syncsetup-waiting-desc").hidden = true;
        document.getElementById("syncsetup-waiting-cancel").hidden = true;
        document.getElementById("syncsetup-waitingdownload-desc").hidden = false;
        document.getElementById("syncsetup-waiting-close").hidden = false;
        Services.obs.addObserver(self._boundOnEngineSync, "weave:engine:sync:finish", false);
        Services.obs.addObserver(self._boundOnEngineSync, "weave:engine:sync:error", false);
        Services.obs.addObserver(self._boundOnServiceSync, "weave:service:sync:finish", false);
        Services.obs.addObserver(self._boundOnServiceSync, "weave:service:sync:error", false);
        self.setupData = aCredentials;
        self.connect();
      },

      onAbort: function onAbort(aError) {
        self.jpake = null;

        if (aError == "jpake.error.userabort" || container.hidden) {
          Services.obs.notifyObservers(null, "browser:sync:setup:userabort", "");
          return;
        }

        // Automatically go to manual setup if we couldn't acquire a channel.
        let brandShortName = Strings.brand.GetStringFromName("brandShortName");
        let tryAgain = self._bundle.GetStringFromName("sync.setup.tryagain");
        let manualSetup = self._bundle.GetStringFromName("sync.setup.manual");
        let buttonFlags = Ci.nsIPrompt.BUTTON_POS_1_DEFAULT +
                         (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_0) +
                         (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_1) +
                         (Ci.nsIPrompt.BUTTON_TITLE_CANCEL    * Ci.nsIPrompt.BUTTON_POS_2);

        let button = Services.prompt.confirmEx(window,
                               self._bundle.GetStringFromName("sync.setup.error.title"),
                               self._bundle.formatStringFromName("sync.setup.error.nodata", [brandShortName], 1),
                               buttonFlags, tryAgain, manualSetup, null, "", {});
        switch (button) {
          case 0:
            // we have to build a new JPAKEClient here rather than reuse the old one
            container.hidden = true;
            self.open();
            break;
          case 1:
            self.openManual();
            break;
          case 2:
          default:
            self.close();
            break;
        }
      }
    });
    this.jpake.receiveNoPIN();
  },

  openManual: function openManual() {
    this.abortEasySetup();

    // Reset the scroll since the previous page might have been scrolled
    this._resetScrollPosition();

    document.getElementById("syncsetup-simple").hidden = true;
    document.getElementById("syncsetup-waiting").hidden = true;
    document.getElementById("syncsetup-fallback").hidden = false;

    // Push the current setup data into the UI
    if (this.setupData && "account" in this.setupData) {
      this._elements.account.value = this.setupData.account;
      this._elements.password.value = this.setupData.password;
      let pp = this.setupData.synckey;
      if (Weave.Utils.isPassphrase(pp))
        pp = Weave.Utils.hyphenatePassphrase(pp);
      this._elements.synckey.value = pp;
      if (this.setupData.serverURL && this.setupData.serverURL.length) {
        this._elements.usecustomserver.checked = true;
        this._elements.customserver.disabled = false;
        this._elements.customserver.value = this.setupData.serverURL;
      } else {
        this._elements.usecustomserver.checked = false;
        this._elements.customserver.disabled = true;
        this._elements.customserver.value = "";
      }
    }

    this.canConnect();
  },

  onEngineSync: function onEngineSync(subject, topic, data) {
    // The Clients engine syncs first. At this point we don't necessarily know
    // yet how many engines will be enabled, so we'll ignore the Clients engine
    // and evaluate how many engines are enabled when the first "real" engine
    // syncs.
    if (data == 'clients') {
      return;
    }
    if (this._progressMax == null) {
      this._progressMax = Weave.Service.engineManager.getEnabled().length;
      this._progressBar.max = this._progressMax;
    }
    this._progressValue += 1;
    this._progressBar.setAttribute("value", this._progressValue);
  },

  onServiceSync: function onServiceSync() {
    this.close();
  },

  close: function close() {
    try {
      Services.obs.removeObserver(this._boundOnEngineSync, "weave:engine:sync:finish");
      Services.obs.removeObserver(this._boundOnEngineSync, "weave:engine:sync:error");
      Services.obs.removeObserver(this._boundOnServiceSync, "weave:service:sync:finish");
      Services.obs.removeObserver(this._boundOnServiceSync, "weave:service:sync:error");
    }
    catch(e) {
      // Observers weren't registered because we never got as far as onComplete.
    }

    if (this.jpake)
      this.abortEasySetup();

    // Reset the scroll since the previous page might have been scrolled
    this._resetScrollPosition();

    // Save current setup data
    this.setupData = {
      account: this._elements.account.value.trim(),
      password: this._elements.password.value.trim(),
      synckey: Weave.Utils.normalizePassphrase(this._elements.synckey.value.trim()),
      serverURL: this._validateServer(this._elements.customserver.value.trim())
    };

    // Clear the UI so it's ready for next time
    this._elements.account.value = "";
    this._elements.password.value = "";
    this._elements.synckey.value = "";
    this._elements.usecustomserver.checked = false;
    this._elements.customserver.disabled = true;
    this._elements.customserver.value = "";
    document.getElementById("syncsetup-waiting-desc").hidden = false;
    document.getElementById("syncsetup-waiting-cancel").hidden = false;
    document.getElementById("syncsetup-waitingdownload-desc").hidden = true;
    document.getElementById("syncsetup-waiting-close").hidden = true;
    this._progressMax = null;
    this._progressValue = 0;
    this._progressBar.max = 0;
    this._progressBar.value = 0;
    this._progressBar.mode = "undetermined";

    // Close the connect UI
    document.getElementById("syncsetup-container").hidden = true;
    DialogUI.popDialog();
  },

  toggleCustomServer: function toggleCustomServer() {
    let useCustomServer = this._elements.usecustomserver.checked;
    this._elements.customserver.disabled = !useCustomServer;
    if (!useCustomServer)
      this._elements.customserver.value = "";
  },

  canConnect: function canConnect() {
    let account = this._elements.account.value;
    let password = this._elements.password.value;
    let synckey = this._elements.synckey.value;

    let disabled = !(account && password && synckey);
    document.getElementById("syncsetup-button-connect").disabled = disabled;
  },

  tryConnect: function login() {
    // If Sync is not configured, simply show the setup dialog
    if (this._loginError || Weave.Status.checkSetup() == Weave.CLIENT_NOT_CONFIGURED) {
      this.open();
      return;
    }

    // No setup data, do nothing
    if (!this.setupData)
      return;

    if (this.setupData.serverURL && this.setupData.serverURL.length)
      Weave.Service.serverURL = this.setupData.serverURL;

    // We might still be in the middle of a sync from before Sync was disabled, so
    // let's force the UI into a state that the Sync code feels comfortable
    this.observe(null, "", "");

    // Now try to re-connect. If successful, this will reset the UI into the
    // correct state automatically.
    Weave.Service.login(Weave.Service.identity.username, this.setupData.password, this.setupData.synckey);
  },

  connect: function connect(aSetupData) {
    // Use setup data to pre-configure manual fields
    if (aSetupData)
      this.setupData = aSetupData;

    // Cause the Sync system to reset internals if we seem to be switching accounts
    if (this.setupData.account != Weave.Service.identity.account)
      Weave.Service.startOver();

    // Remove any leftover connection error string
    this._elements.connect.removeAttribute("desc");

    // Reset the custom server URL, if we have one
    if (this.setupData.serverURL && this.setupData.serverURL.length)
      Weave.Service.serverURL = this.setupData.serverURL;

    // Sync will use the account value and munge it into a username, as needed
    Weave.Service.identity.account = this.setupData.account;
    Weave.Service.identity.basicPassword = this.setupData.password;
    Weave.Service.identity.syncKey = this.setupData.synckey;
    Weave.Service.persistLogin();
    Weave.Svc.Obs.notify("weave:service:setup-complete");
    this.sync();
  },

  disconnect: function disconnect() {
    this.setupData = null;
    Weave.Service.startOver();
  },

  sync: function sync() {
    Weave.Service.scheduler.scheduleNextSync(0);
  },

  _addListeners: function _addListeners() {
    let topics = ["weave:service:setup-complete",
      "weave:service:sync:start", "weave:service:sync:finish",
      "weave:service:sync:error", "weave:service:login:start",
      "weave:service:login:finish", "weave:service:login:error",
      "weave:ui:login:error",
      "weave:service:logout:finish"];

    // For each topic, add Sync the observer
    topics.forEach(function(topic) {
      Services.obs.addObserver(Sync, topic, false);
    });

    // Remove them on unload
    addEventListener("unload", function() {
      topics.forEach(function(topic) {
        Services.obs.removeObserver(Sync, topic);
      });
    }, false);
  },

  get _elements() {
    // Get all the setting nodes from the add-ons display
    let elements = {};
    let setupids = ["account", "password", "synckey", "usecustomserver", "customserver"];
    setupids.forEach(function(id) {
      elements[id] = document.getElementById("syncsetup-" + id);
    });

    let settingids = ["device", "connect", "connected", "disconnect", "lastsync", "pairdevice",
                      "errordescription", "accountinfo"];
    settingids.forEach(function(id) {
      elements[id] = document.getElementById("sync-" + id);
    });

    // Replace the getter with the collection of settings
    delete this._elements;
    return this._elements = elements;
  },

  _updateUI: function _updateUI() {
    if (this._elements == null)
      return;

    let connect = this._elements.connect;
    let connected = this._elements.connected;
    let device = this._elements.device;
    let disconnect = this._elements.disconnect;
    let lastsync = this._elements.lastsync;
    let pairdevice = this._elements.pairdevice;
    let accountinfo = this._elements.accountinfo;

    // This gets updated when an error occurs
    this._elements.errordescription.collapsed = true;

    let isConfigured = (!this._loginError && this._isSetup);

    connect.collapsed = isConfigured;
    connected.collapsed = !isConfigured;
    lastsync.collapsed = !isConfigured;
    device.collapsed = !isConfigured;
    disconnect.collapsed = !isConfigured;

    // Set the device name text edit to configured name or the auto generated
    // name if we aren't set up.
    try {
      device.value = Services.prefs.getCharPref("services.sync.client.name");
    } catch(ex) {
      device.value = Weave.Service.clientsEngine.localName || "";
    }

    // Account information header
    accountinfo.collapsed = true;
    try {
      let account = Weave.Service.identity.account;
      if (account != null && isConfigured) {
        let accountStr = this._bundle.formatStringFromName("account.label", [account], 1);
        accountinfo.textContent = accountStr;
        accountinfo.collapsed = false;
      }
    } catch (ex) {}

    // If we're already locked, a sync is in progress..
    if (Weave.Service.locked && isConfigured) {
      connect.firstChild.disabled = true;
    }

    // Show the day-of-week and time (HH:MM) of last sync
    let lastSync = Weave.Svc.Prefs.get("lastSync");
    lastsync.textContent = "";
    if (lastSync != null) {
      let syncDate = new Date(lastSync).toLocaleFormat("%A %I:%M %p");
      let dateStr = this._bundle.formatStringFromName("lastSync2.label", [syncDate], 1);
      lastsync.textContent = dateStr;
    }

    // Check the lock again on a timeout since it's set after observers notify
    setTimeout(function(self) {
      // Prevent certain actions when the service is locked
      if (Weave.Service.locked) {
        connect.firstChild.disabled = true;
      } else {
        connect.firstChild.disabled = false;
      }
    }, 100, this);
  },

  observe: function observe(aSubject, aTopic, aData) {
    if (aTopic == "weave:service:ready") {
      Services.obs.removeObserver(this, aTopic);
      this._init();
      return;
    }

    // Make sure we're online when connecting/syncing
    Util.forceOnline();

    // Can't do anything before settings are loaded
    if (this._elements == null)
      return;

    // Update the state of the ui
    this._updateUI();

    let errormsg = this._elements.errordescription;
    let accountinfo = this._elements.accountinfo;

    // Show what went wrong with login if necessary
    if (aTopic == "weave:ui:login:error") {
      this._loginError = true;
      errormsg.textContent = Weave.Utils.getErrorString(Weave.Status.login);
      errormsg.collapsed = false;
    }

    if (aTopic == "weave:service:login:finish") {
      this._loginError = false;
      // Init the setup data if we just logged in
      if (!this.setupData)
        this.loadSetupData();
    }

    // Check for a storage format update, update the user and load the Sync update page
    if (aTopic =="weave:service:sync:error") {
      errormsg.textContent = Weave.Utils.getErrorString(Weave.Status.sync);
      errormsg.collapsed = false;

      let clientOutdated = false, remoteOutdated = false;
      if (Weave.Status.sync == Weave.VERSION_OUT_OF_DATE) {
        clientOutdated = true;
      } else if (Weave.Status.sync == Weave.DESKTOP_VERSION_OUT_OF_DATE) {
        remoteOutdated = true;
      } else if (Weave.Status.service == Weave.SYNC_FAILED_PARTIAL) {
        // Some engines failed, check for per-engine compat
        for (let [engine, reason] in Iterator(Weave.Status.engines)) {
           clientOutdated = clientOutdated || reason == Weave.VERSION_OUT_OF_DATE;
           remoteOutdated = remoteOutdated || reason == Weave.DESKTOP_VERSION_OUT_OF_DATE;
        }
      }

      if (clientOutdated || remoteOutdated) {
        let brand = Services.strings.createBundle("chrome://branding/locale/brand.properties");
        let brandName = brand.GetStringFromName("brandShortName");

        let type = clientOutdated ? "client" : "remote";
        let message = this._bundle.GetStringFromName("sync.update." + type);
        message = message.replace("#1", brandName);
        message = message.replace("#2", Services.appinfo.version);
        let title = this._bundle.GetStringFromName("sync.update.title")
        let button = this._bundle.GetStringFromName("sync.update.learnmore")
        let close = this._bundle.GetStringFromName("sync.update.close")

        let flags = Services.prompt.BUTTON_POS_0 * Services.prompt.BUTTON_TITLE_IS_STRING +
                    Services.prompt.BUTTON_POS_1 * Services.prompt.BUTTON_TITLE_IS_STRING;
        let choice = Services.prompt.confirmEx(window, title, message, flags, button, close, null, null, {});
        if (choice == 0)
          Browser.addTab("https://services.mozilla.com/update/", true, Browser.selectedTab);
      }
    }
  },

  changeName: function changeName(aInput) {
    // Make sure to update to a modified name, e.g., empty-string -> default
    Weave.Service.clientsEngine.localName = aInput.value;
    aInput.value = Weave.Service.clientsEngine.localName;
  },

  _validateServer: function _validateServer(aURL) {
    let uri = Weave.Utils.makeURI(aURL);

    if (!uri && aURL)
      uri = Weave.Utils.makeURI("https://" + aURL);

    if (!uri)
      return "";
    return uri.spec;
  },

  openTutorial: function _openTutorial() {
    Sync.close();

    let formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].getService(Ci.nsIURLFormatter);
    let url = formatter.formatURLPref("app.sync.tutorialURL");
    BrowserUI.newTab(url, Browser.selectedTab);
  },

  loadSetupData: function _loadSetupData() {
    this.setupData = {};
    this.setupData.account = Weave.Service.identity.account || "";
    this.setupData.password = Weave.Service.identity.basicPassword || "";
    this.setupData.synckey = Weave.Service.identity.syncKey || "";

    let serverURL = Weave.Service.serverURL;
    let defaultPrefs = Services.prefs.getDefaultBranch(null);
    if (serverURL == defaultPrefs.getCharPref("services.sync.serverURL"))
      serverURL = "";
    this.setupData.serverURL = serverURL;
  }
};


const PIN_PART_LENGTH = 4;

let SyncPairDevice = {
  jpake: null,

  open: function open() {
    this.code1.setAttribute("maxlength", PIN_PART_LENGTH);
    this.code2.setAttribute("maxlength", PIN_PART_LENGTH);
    this.code3.setAttribute("maxlength", PIN_PART_LENGTH);
    this.nextFocusEl = {code1: this.code2,
                        code2: this.code3,
                        code3: this.connectbutton};

    document.getElementById("syncpair-container").hidden = false;
    DialogUI.pushDialog(this);
    this.code1.focus();

    // Kick off a sync. That way the server will have the most recent data from
    // this computer and it will show up immediately on the new device.
    Weave.SyncScheduler.scheduleNextSync(0);
  },

  close: function close() {
    this.code1.value = this.code2.value = this.code3.value = "";
    this.code1.disabled = this.code2.disabled = this.code3.disabled = false;
    this.connectbutton.disabled = true;
    if (this.jpake) {
      this.jpake.abort();
      this.jpake = null;
    }
    document.getElementById("syncpair-container").hidden = true;
    DialogUI.popDialog();
  },

  onTextBoxInput: function onTextBoxInput(textbox) {
    if (textbox && textbox.value.length == PIN_PART_LENGTH) {
      let name = textbox.id.split("-")[1];
      this.nextFocusEl[name].focus();
    }

    this.connectbutton.disabled =
      !(this.code1.value.length == PIN_PART_LENGTH &&
        this.code2.value.length == PIN_PART_LENGTH &&
        this.code3.value.length == PIN_PART_LENGTH);
  },

  connect: function connect() {
    let self = this;
    let jpake = this.jpake = new Weave.JPAKEClient({
      onPaired: function onPaired() {
        let credentials = {account:   Weave.Service.identity.account,
                           password:  Weave.Service.identity.basicPassword,
                           synckey:   Weave.Service.identity.syncKey,
                           serverURL: Weave.Service.serverURL};
        jpake.sendAndComplete(credentials);
      },
      onComplete: function onComplete() {
        self.jpake = null;
        self.close();

        // Schedule a Sync for soonish to fetch the data uploaded by the
        // device with which we just paired.
        Weave.SyncScheduler.scheduleNextSync(Weave.SyncScheduler.activeInterval);
      },
      onAbort: function onAbort(error) {
        self.jpake = null;

        // Aborted by user, ignore.
        if (error == Weave.JPAKE_ERROR_USERABORT) {
          return;
        }

        self.code1.value = self.code2.value = self.code3.value = "";
        self.code1.disabled = self.code2.disabled = self.code3.disabled = false;
        self.code1.focus();
      }
    });
    this.code1.disabled = this.code2.disabled = this.code3.disabled = true;
    this.connectbutton.disabled = true;

    let pin = this.code1.value + this.code2.value + this.code3.value;
    let expectDelay = false;
    jpake.pairWithPIN(pin, expectDelay);
  }
};
["code1", "code2", "code3", "connectbutton"].forEach(function (id) {
  XPCOMUtils.defineLazyGetter(SyncPairDevice, id, function() {
    return document.getElementById("syncpair-" + id);
  });
});
