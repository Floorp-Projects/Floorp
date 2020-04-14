/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../base/content/utilityOverlay.js */
/* import-globals-from ../../../toolkit/content/preferencesBindings.js */
/* import-globals-from in-content/extensionControlled.js */

document
  .getElementById("ConnectionsDialog")
  .addEventListener("dialoghelp", window.top.openPrefsHelp);

Preferences.addAll([
  // Add network.proxy.autoconfig_url before network.proxy.type so they're
  // both initialized when network.proxy.type initialization triggers a call to
  // gConnectionsDialog.updateReloadButton().
  { id: "network.proxy.autoconfig_url", type: "string" },
  { id: "network.proxy.type", type: "int" },
  { id: "network.proxy.http", type: "string" },
  { id: "network.proxy.http_port", type: "int" },
  { id: "network.proxy.ftp", type: "string" },
  { id: "network.proxy.ftp_port", type: "int" },
  { id: "network.proxy.ssl", type: "string" },
  { id: "network.proxy.ssl_port", type: "int" },
  { id: "network.proxy.socks", type: "string" },
  { id: "network.proxy.socks_port", type: "int" },
  { id: "network.proxy.socks_version", type: "int" },
  { id: "network.proxy.socks_remote_dns", type: "bool" },
  { id: "network.proxy.no_proxies_on", type: "string" },
  { id: "network.proxy.share_proxy_settings", type: "bool" },
  { id: "signon.autologin.proxy", type: "bool" },
  { id: "pref.advanced.proxies.disable_button.reload", type: "bool" },
  { id: "network.proxy.backup.ftp", type: "string" },
  { id: "network.proxy.backup.ftp_port", type: "int" },
  { id: "network.proxy.backup.ssl", type: "string" },
  { id: "network.proxy.backup.ssl_port", type: "int" },
  { id: "network.trr.mode", type: "int" },
  { id: "network.trr.uri", type: "string" },
  { id: "network.trr.resolvers", type: "string" },
  { id: "network.trr.custom_uri", type: "string" },
]);

window.addEventListener(
  "DOMContentLoaded",
  () => {
    Preferences.get("network.proxy.type").on(
      "change",
      gConnectionsDialog.proxyTypeChanged.bind(gConnectionsDialog)
    );
    Preferences.get("network.proxy.socks_version").on(
      "change",
      gConnectionsDialog.updateDNSPref.bind(gConnectionsDialog)
    );

    Preferences.get("network.trr.uri").on("change", () => {
      gConnectionsDialog.updateDnsOverHttpsUI();
    });

    Preferences.get("network.trr.resolvers").on("change", () => {
      gConnectionsDialog.initDnsOverHttpsUI();
    });

    // XXX: We can't init the DNS-over-HTTPs UI until the onsyncfrompreference for network.trr.mode
    //      has been called. The uiReady promise will be resolved after the first call to
    //      readDnsOverHttpsMode and the subsequent call to initDnsOverHttpsUI has happened.
    gConnectionsDialog.uiReady = new Promise(resolve => {
      gConnectionsDialog._areTrrPrefsReady = false;
      gConnectionsDialog._handleTrrPrefsReady = resolve;
    }).then(() => {
      gConnectionsDialog.initDnsOverHttpsUI();
    });

    document
      .getElementById("disableProxyExtension")
      .addEventListener(
        "command",
        makeDisableControllingExtension(PREF_SETTING_TYPE, PROXY_KEY).bind(
          gConnectionsDialog
        )
      );
    gConnectionsDialog.updateProxySettingsUI();
    initializeProxyUI(gConnectionsDialog);
    gConnectionsDialog.registerSyncPrefListeners();
    document
      .getElementById("ConnectionsDialog")
      .addEventListener("beforeaccept", e =>
        gConnectionsDialog.beforeAccept(e)
      );
  },
  { once: true, capture: true }
);

var gConnectionsDialog = {
  beforeAccept(event) {
    let dnsOverHttpsResolverChoice = document.getElementById(
      "networkDnsOverHttpsResolverChoices"
    ).value;
    if (dnsOverHttpsResolverChoice == "custom") {
      let customValue = document
        .getElementById("networkCustomDnsOverHttpsInput")
        .value.trim();
      if (customValue) {
        Services.prefs.setStringPref("network.trr.uri", customValue);
      } else {
        Services.prefs.clearUserPref("network.trr.uri");
      }
    } else {
      Services.prefs.setStringPref(
        "network.trr.uri",
        dnsOverHttpsResolverChoice
      );
    }

    var proxyTypePref = Preferences.get("network.proxy.type");
    if (proxyTypePref.value == 2) {
      this.doAutoconfigURLFixup();
      return;
    }

    if (proxyTypePref.value != 1) {
      return;
    }

    var httpProxyURLPref = Preferences.get("network.proxy.http");
    var httpProxyPortPref = Preferences.get("network.proxy.http_port");
    var shareProxiesPref = Preferences.get(
      "network.proxy.share_proxy_settings"
    );

    // If the port is 0 and the proxy server is specified, focus on the port and cancel submission.
    for (let prefName of ["http", "ssl", "ftp", "socks"]) {
      let proxyPortPref = Preferences.get(
        "network.proxy." + prefName + "_port"
      );
      let proxyPref = Preferences.get("network.proxy." + prefName);
      // Only worry about ports which are currently active. If the share option is on, then ignore
      // all ports except the HTTP and SOCKS port
      if (
        proxyPref.value != "" &&
        proxyPortPref.value == 0 &&
        (prefName == "http" || prefName == "socks" || !shareProxiesPref.value)
      ) {
        document
          .getElementById("networkProxy" + prefName.toUpperCase() + "_Port")
          .focus();
        event.preventDefault();
        return;
      }
    }

    // In the case of a shared proxy preference, backup the current values and update with the HTTP value
    if (shareProxiesPref.value) {
      var proxyPrefs = ["ssl", "ftp"];
      for (var i = 0; i < proxyPrefs.length; ++i) {
        var proxyServerURLPref = Preferences.get(
          "network.proxy." + proxyPrefs[i]
        );
        var proxyPortPref = Preferences.get(
          "network.proxy." + proxyPrefs[i] + "_port"
        );
        var backupServerURLPref = Preferences.get(
          "network.proxy.backup." + proxyPrefs[i]
        );
        var backupPortPref = Preferences.get(
          "network.proxy.backup." + proxyPrefs[i] + "_port"
        );
        backupServerURLPref.value =
          backupServerURLPref.value || proxyServerURLPref.value;
        backupPortPref.value = backupPortPref.value || proxyPortPref.value;
        proxyServerURLPref.value = httpProxyURLPref.value;
        proxyPortPref.value = httpProxyPortPref.value;
      }
    }

    this.sanitizeNoProxiesPref();
  },

  checkForSystemProxy() {
    if ("@mozilla.org/system-proxy-settings;1" in Cc) {
      document.getElementById("systemPref").removeAttribute("hidden");
    }
  },

  proxyTypeChanged() {
    var proxyTypePref = Preferences.get("network.proxy.type");

    // Update http
    var httpProxyURLPref = Preferences.get("network.proxy.http");
    httpProxyURLPref.disabled = proxyTypePref.value != 1;
    var httpProxyPortPref = Preferences.get("network.proxy.http_port");
    httpProxyPortPref.disabled = proxyTypePref.value != 1;

    // Now update the other protocols
    this.updateProtocolPrefs();

    var shareProxiesPref = Preferences.get(
      "network.proxy.share_proxy_settings"
    );
    shareProxiesPref.disabled =
      proxyTypePref.value != 1 || shareProxiesPref.locked;
    var autologinProxyPref = Preferences.get("signon.autologin.proxy");
    autologinProxyPref.disabled =
      proxyTypePref.value == 0 || autologinProxyPref.locked;
    var noProxiesPref = Preferences.get("network.proxy.no_proxies_on");
    noProxiesPref.disabled = proxyTypePref.value == 0 || noProxiesPref.locked;

    var autoconfigURLPref = Preferences.get("network.proxy.autoconfig_url");
    autoconfigURLPref.disabled =
      proxyTypePref.value != 2 || autoconfigURLPref.locked;

    this.updateReloadButton();

    document.getElementById(
      "networkProxyNoneLocalhost"
    ).hidden = Services.prefs.getBoolPref(
      "network.proxy.allow_hijacking_localhost",
      false
    );
  },

  updateDNSPref() {
    var socksVersionPref = Preferences.get("network.proxy.socks_version");
    var socksDNSPref = Preferences.get("network.proxy.socks_remote_dns");
    var proxyTypePref = Preferences.get("network.proxy.type");
    var isDefinitelySocks4 =
      !socksVersionPref.disabled && socksVersionPref.value == 4;
    socksDNSPref.disabled =
      isDefinitelySocks4 || proxyTypePref.value == 0 || socksDNSPref.locked;
    return undefined;
  },

  updateReloadButton() {
    // Disable the "Reload PAC" button if the selected proxy type is not PAC or
    // if the current value of the PAC input does not match the value stored
    // in prefs.  Likewise, disable the reload button if PAC is not configured
    // in prefs.

    var typedURL = document.getElementById("networkProxyAutoconfigURL").value;
    var proxyTypeCur = Preferences.get("network.proxy.type").value;

    var pacURL = Services.prefs.getCharPref("network.proxy.autoconfig_url");
    var proxyType = Services.prefs.getIntPref("network.proxy.type");

    var disableReloadPref = Preferences.get(
      "pref.advanced.proxies.disable_button.reload"
    );
    disableReloadPref.disabled =
      proxyTypeCur != 2 || proxyType != 2 || typedURL != pacURL;
  },

  readProxyType() {
    this.proxyTypeChanged();
    return undefined;
  },

  updateProtocolPrefs() {
    var proxyTypePref = Preferences.get("network.proxy.type");
    var shareProxiesPref = Preferences.get(
      "network.proxy.share_proxy_settings"
    );
    var proxyPrefs = ["ssl", "ftp", "socks"];
    for (var i = 0; i < proxyPrefs.length; ++i) {
      var proxyServerURLPref = Preferences.get(
        "network.proxy." + proxyPrefs[i]
      );
      var proxyPortPref = Preferences.get(
        "network.proxy." + proxyPrefs[i] + "_port"
      );

      // Restore previous per-proxy custom settings, if present.
      if (proxyPrefs[i] != "socks" && !shareProxiesPref.value) {
        var backupServerURLPref = Preferences.get(
          "network.proxy.backup." + proxyPrefs[i]
        );
        var backupPortPref = Preferences.get(
          "network.proxy.backup." + proxyPrefs[i] + "_port"
        );
        if (backupServerURLPref.hasUserValue) {
          proxyServerURLPref.value = backupServerURLPref.value;
          backupServerURLPref.reset();
        }
        if (backupPortPref.hasUserValue) {
          proxyPortPref.value = backupPortPref.value;
          backupPortPref.reset();
        }
      }

      proxyServerURLPref.updateElements();
      proxyPortPref.updateElements();
      let prefIsShared = proxyPrefs[i] != "socks" && shareProxiesPref.value;
      proxyServerURLPref.disabled = proxyTypePref.value != 1 || prefIsShared;
      proxyPortPref.disabled = proxyServerURLPref.disabled;
    }
    var socksVersionPref = Preferences.get("network.proxy.socks_version");
    socksVersionPref.disabled = proxyTypePref.value != 1;
    this.updateDNSPref();
    return undefined;
  },

  readProxyProtocolPref(aProtocol, aIsPort) {
    if (aProtocol != "socks") {
      var shareProxiesPref = Preferences.get(
        "network.proxy.share_proxy_settings"
      );
      if (shareProxiesPref.value) {
        var pref = Preferences.get(
          "network.proxy.http" + (aIsPort ? "_port" : "")
        );
        return pref.value;
      }

      var backupPref = Preferences.get(
        "network.proxy.backup." + aProtocol + (aIsPort ? "_port" : "")
      );
      return backupPref.hasUserValue ? backupPref.value : undefined;
    }
    return undefined;
  },

  reloadPAC() {
    Cc["@mozilla.org/network/protocol-proxy-service;1"]
      .getService()
      .reloadPAC();
  },

  doAutoconfigURLFixup() {
    var autoURL = document.getElementById("networkProxyAutoconfigURL");
    var autoURLPref = Preferences.get("network.proxy.autoconfig_url");
    try {
      autoURLPref.value = autoURL.value = Services.uriFixup.createFixupURI(
        autoURL.value,
        0
      ).spec;
    } catch (ex) {}
  },

  sanitizeNoProxiesPref() {
    var noProxiesPref = Preferences.get("network.proxy.no_proxies_on");
    // replace substrings of ; and \n with commas if they're neither immediately
    // preceded nor followed by a valid separator character
    noProxiesPref.value = noProxiesPref.value.replace(
      /([^, \n;])[;\n]+(?![,\n;])/g,
      "$1,"
    );
    // replace any remaining ; and \n since some may follow commas, etc.
    noProxiesPref.value = noProxiesPref.value.replace(/[;\n]/g, "");
  },

  readHTTPProxyServer() {
    var shareProxiesPref = Preferences.get(
      "network.proxy.share_proxy_settings"
    );
    if (shareProxiesPref.value) {
      this.updateProtocolPrefs();
    }
    return undefined;
  },

  readHTTPProxyPort() {
    var shareProxiesPref = Preferences.get(
      "network.proxy.share_proxy_settings"
    );
    if (shareProxiesPref.value) {
      this.updateProtocolPrefs();
    }
    return undefined;
  },

  getProxyControls() {
    let controlGroup = document.getElementById("networkProxyType");
    return [
      ...controlGroup.querySelectorAll(":scope > radio"),
      ...controlGroup.querySelectorAll("label"),
      ...controlGroup.querySelectorAll("input"),
      ...controlGroup.querySelectorAll("checkbox"),
      ...document.querySelectorAll("#networkProxySOCKSVersion > radio"),
      ...document.querySelectorAll("#ConnectionsDialogPane > checkbox"),
    ];
  },

  // Update the UI to show/hide the extension controlled message for
  // proxy settings.
  async updateProxySettingsUI() {
    let isLocked = API_PROXY_PREFS.some(pref =>
      Services.prefs.prefIsLocked(pref)
    );

    function setInputsDisabledState(isControlled) {
      for (let element of gConnectionsDialog.getProxyControls()) {
        element.disabled = isControlled;
      }
      gConnectionsDialog.proxyTypeChanged();
    }

    if (isLocked) {
      // An extension can't control this setting if any pref is locked.
      hideControllingExtension(PROXY_KEY);
    } else {
      handleControllingExtension(PREF_SETTING_TYPE, PROXY_KEY).then(
        setInputsDisabledState
      );
    }
  },

  get dnsOverHttpsResolvers() {
    let rawValue = Preferences.get("network.trr.resolvers", "").value;
    // if there's no default, we'll hold its position with an empty string
    let defaultURI = Preferences.get("network.trr.uri", "").defaultValue;
    let providers = [];
    if (rawValue) {
      try {
        providers = JSON.parse(rawValue);
      } catch (ex) {
        Cu.reportError(
          `Bad JSON data in pref network.trr.resolvers: ${rawValue}`
        );
      }
    }
    if (!Array.isArray(providers)) {
      Cu.reportError(
        `Expected a JSON array in network.trr.resolvers: ${rawValue}`
      );
      providers = [];
    }
    let defaultIndex = providers.findIndex(p => p.url == defaultURI);
    if (defaultIndex == -1 && defaultURI) {
      // the default value for the pref isn't included in the resolvers list
      // so we'll make a stub for it. Without an id, we'll have to use the url as the label
      providers.unshift({ url: defaultURI });
    }
    return providers;
  },

  isDnsOverHttpsLocked() {
    return Services.prefs.prefIsLocked("network.trr.mode");
  },

  isDnsOverHttpsEnabled() {
    // values outside 1:4 are considered falsey/disabled in this context
    let trrPref = Preferences.get("network.trr.mode");
    let enabled = trrPref.value > 0 && trrPref.value < 5;
    return enabled;
  },

  readDnsOverHttpsMode() {
    // called to update checked element property to reflect current pref value
    let enabled = this.isDnsOverHttpsEnabled();
    let uriPref = Preferences.get("network.trr.uri");
    uriPref.disabled = !enabled || this.isDnsOverHttpsLocked();
    // this is the first signal we get when the prefs are available, so
    // lazy-init if appropriate
    if (!this._areTrrPrefsReady) {
      this._areTrrPrefsReady = true;
      this._handleTrrPrefsReady();
    } else {
      this.updateDnsOverHttpsUI();
    }
    return enabled;
  },

  writeDnsOverHttpsMode() {
    // called to update pref with user change
    let trrModeCheckbox = document.getElementById("networkDnsOverHttps");
    // we treat checked/enabled as mode 2
    return trrModeCheckbox.checked ? 2 : 0;
  },

  updateDnsOverHttpsUI() {
    // init and update of the UI must wait until the pref values are ready
    if (!this._areTrrPrefsReady) {
      return;
    }
    let [menu, customInput] = this.getDnsOverHttpsControls();
    let dohUIContainer = document.getElementById("dnsOverHttps-grid");
    let customURI = Preferences.get("network.trr.custom_uri").value;
    let currentURI = Preferences.get("network.trr.uri").value;
    let resolvers = this.dnsOverHttpsResolvers;
    let isCustom = menu.value == "custom";

    if (this.isDnsOverHttpsEnabled()) {
      this.toggleDnsOverHttpsUI(false);
      if (isCustom) {
        // if the current and custom_uri values mismatch, update the uri pref
        if (
          currentURI &&
          !customURI &&
          !resolvers.find(r => r.url == currentURI)
        ) {
          Services.prefs.setStringPref("network.trr.custom_uri", currentURI);
        }
      }
    } else {
      this.toggleDnsOverHttpsUI(true);
    }

    if (!menu.disabled && isCustom) {
      dohUIContainer.classList.remove("custom-container-hidden");
      customInput.disabled = false;
      customInput.scrollIntoView();
    } else {
      dohUIContainer.classList.add("custom-container-hidden");
      customInput.disabled = true;
    }

    // The height has likely changed, find our SubDialog and tell it to resize.
    requestAnimationFrame(() => {
      let dialogs = window.opener.gSubDialog._dialogs;
      let dialog = dialogs.find(d => d._frame.contentDocument == document);
      if (dialog) {
        dialog.resizeVertically();
      }
    });
  },

  getDnsOverHttpsControls() {
    return [
      document.getElementById("networkDnsOverHttpsResolverChoices"),
      document.getElementById("networkCustomDnsOverHttpsInput"),
      document.getElementById("networkDnsOverHttpsResolverChoicesLabel"),
      document.getElementById("networkCustomDnsOverHttpsInputLabel"),
    ];
  },

  toggleDnsOverHttpsUI(disabled) {
    for (let element of this.getDnsOverHttpsControls()) {
      element.disabled = disabled;
    }
  },

  initDnsOverHttpsUI() {
    let resolvers = this.dnsOverHttpsResolvers;
    let defaultURI = Preferences.get("network.trr.uri").defaultValue;
    let currentURI = Preferences.get("network.trr.uri").value;
    let menu = document.getElementById("networkDnsOverHttpsResolverChoices");

    // populate the DNS-Over-HTTPs resolver list
    menu.removeAllItems();
    for (let resolver of resolvers) {
      let item = menu.appendItem(undefined, resolver.url);
      if (resolver.url == defaultURI) {
        document.l10n.setAttributes(
          item,
          "connection-dns-over-https-url-item-default",
          {
            name: resolver.name || resolver.url,
          }
        );
      } else {
        item.label = resolver.name || resolver.url;
      }
    }
    let lastItem = menu.appendItem(undefined, "custom");
    document.l10n.setAttributes(
      lastItem,
      "connection-dns-over-https-url-custom"
    );

    // set initial selection in the resolver provider picker
    let selectedIndex = currentURI
      ? resolvers.findIndex(r => r.url == currentURI)
      : 0;
    if (selectedIndex == -1) {
      // select the last "Custom" item
      selectedIndex = menu.itemCount - 1;
    }
    menu.selectedIndex = selectedIndex;

    if (this.isDnsOverHttpsLocked()) {
      // disable all the options and the checkbox itself to disallow enabling them
      this.toggleDnsOverHttpsUI(true);
      document.getElementById("networkDnsOverHttps").disabled = true;
    } else {
      this.toggleDnsOverHttpsUI(false);
      this.updateDnsOverHttpsUI();
      document.getElementById("networkDnsOverHttps").disabled = false;
    }
  },

  registerSyncPrefListeners() {
    function setSyncFromPrefListener(element_id, callback) {
      Preferences.addSyncFromPrefListener(
        document.getElementById(element_id),
        callback
      );
    }
    function setSyncToPrefListener(element_id, callback) {
      Preferences.addSyncToPrefListener(
        document.getElementById(element_id),
        callback
      );
    }
    setSyncFromPrefListener("networkProxyType", () => this.readProxyType());
    setSyncFromPrefListener("networkProxyHTTP", () =>
      this.readHTTPProxyServer()
    );
    setSyncFromPrefListener("networkProxyHTTP_Port", () =>
      this.readHTTPProxyPort()
    );
    setSyncFromPrefListener("shareAllProxies", () =>
      this.updateProtocolPrefs()
    );
    setSyncFromPrefListener("networkProxySSL", () =>
      this.readProxyProtocolPref("ssl", false)
    );
    setSyncFromPrefListener("networkProxySSL_Port", () =>
      this.readProxyProtocolPref("ssl", true)
    );
    setSyncFromPrefListener("networkProxyFTP", () =>
      this.readProxyProtocolPref("ftp", false)
    );
    setSyncFromPrefListener("networkProxyFTP_Port", () =>
      this.readProxyProtocolPref("ftp", true)
    );
    setSyncFromPrefListener("networkProxySOCKS", () =>
      this.readProxyProtocolPref("socks", false)
    );
    setSyncFromPrefListener("networkProxySOCKS_Port", () =>
      this.readProxyProtocolPref("socks", true)
    );
    setSyncFromPrefListener("networkDnsOverHttps", () =>
      this.readDnsOverHttpsMode()
    );
    setSyncToPrefListener("networkDnsOverHttps", () =>
      this.writeDnsOverHttpsMode()
    );
  },
};
