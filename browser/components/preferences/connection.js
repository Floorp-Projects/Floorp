/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gConnectionsDialog = {
  beforeAccept: function ()
  {
    var proxyTypePref = document.getElementById("network.proxy.type");
    if (proxyTypePref.value == 2) {
      this.doAutoconfigURLFixup();
      return true;
    }

    if (proxyTypePref.value != 1)
      return true;

    var httpProxyURLPref = document.getElementById("network.proxy.http");
    var httpProxyPortPref = document.getElementById("network.proxy.http_port");
    var shareProxiesPref = document.getElementById("network.proxy.share_proxy_settings");

    // If the port is 0 and the proxy server is specified, focus on the port and cancel submission.
    for (let prefName of ["http","ssl","ftp","socks"]) {
      let proxyPortPref = document.getElementById("network.proxy." + prefName + "_port");
      let proxyPref = document.getElementById("network.proxy." + prefName);
      // Only worry about ports which are currently active. If the share option is on, then ignore
      // all ports except the HTTP port
      if (proxyPref.value != "" && proxyPortPref.value == 0 &&
            (prefName == "http" || !shareProxiesPref.value)) {
        document.getElementById("networkProxy" + prefName.toUpperCase() + "_Port").focus();
        return false;
      }
    }

    // In the case of a shared proxy preference, backup the current values and update with the HTTP value
    if (shareProxiesPref.value) {
      var proxyPrefs = ["ssl", "ftp", "socks"];
      for (var i = 0; i < proxyPrefs.length; ++i) {
        var proxyServerURLPref = document.getElementById("network.proxy." + proxyPrefs[i]);
        var proxyPortPref = document.getElementById("network.proxy." + proxyPrefs[i] + "_port");
        var backupServerURLPref = document.getElementById("network.proxy.backup." + proxyPrefs[i]);
        var backupPortPref = document.getElementById("network.proxy.backup." + proxyPrefs[i] + "_port");
        backupServerURLPref.value = backupServerURLPref.value || proxyServerURLPref.value;
        backupPortPref.value = backupPortPref.value || proxyPortPref.value;
        proxyServerURLPref.value = httpProxyURLPref.value;
        proxyPortPref.value = httpProxyPortPref.value;
      }
    }
    
    this.sanitizeNoProxiesPref();
    
    return true;
  },

  checkForSystemProxy: function ()
  {
    if ("@mozilla.org/system-proxy-settings;1" in Components.classes)
      document.getElementById("systemPref").removeAttribute("hidden");
  },
  
  proxyTypeChanged: function ()
  {
    var proxyTypePref = document.getElementById("network.proxy.type");

    // Update http
    var httpProxyURLPref = document.getElementById("network.proxy.http");
    httpProxyURLPref.disabled = proxyTypePref.value != 1;
    var httpProxyPortPref = document.getElementById("network.proxy.http_port");
    httpProxyPortPref.disabled = proxyTypePref.value != 1;

    // Now update the other protocols
    this.updateProtocolPrefs();

    var shareProxiesPref = document.getElementById("network.proxy.share_proxy_settings");
    shareProxiesPref.disabled = proxyTypePref.value != 1;
    var autologinProxyPref = document.getElementById("signon.autologin.proxy");
    autologinProxyPref.disabled = proxyTypePref.value == 0;
    var noProxiesPref = document.getElementById("network.proxy.no_proxies_on");
    noProxiesPref.disabled = proxyTypePref.value != 1;

    var autoconfigURLPref = document.getElementById("network.proxy.autoconfig_url");
    autoconfigURLPref.disabled = proxyTypePref.value != 2;

    this.updateReloadButton();
  },
  
  updateDNSPref: function ()
  {
    var socksVersionPref = document.getElementById("network.proxy.socks_version");
    var socksDNSPref = document.getElementById("network.proxy.socks_remote_dns");
    var proxyTypePref = document.getElementById("network.proxy.type");
    var isDefinitelySocks4 = !socksVersionPref.disabled && socksVersionPref.value == 4;
    socksDNSPref.disabled = (isDefinitelySocks4 || proxyTypePref.value == 0);
    return undefined;
  },
  
  updateReloadButton: function ()
  {
    // Disable the "Reload PAC" button if the selected proxy type is not PAC or
    // if the current value of the PAC textbox does not match the value stored
    // in prefs.  Likewise, disable the reload button if PAC is not configured
    // in prefs.

    var typedURL = document.getElementById("networkProxyAutoconfigURL").value;
    var proxyTypeCur = document.getElementById("network.proxy.type").value;

    var prefs =
        Components.classes["@mozilla.org/preferences-service;1"].
        getService(Components.interfaces.nsIPrefBranch);
    var pacURL = prefs.getCharPref("network.proxy.autoconfig_url");
    var proxyType = prefs.getIntPref("network.proxy.type");

    var disableReloadPref =
        document.getElementById("pref.advanced.proxies.disable_button.reload");
    disableReloadPref.disabled =
        (proxyTypeCur != 2 || proxyType != 2 || typedURL != pacURL);
  },
  
  readProxyType: function ()
  {
    this.proxyTypeChanged();
    return undefined;
  },
  
  updateProtocolPrefs: function ()
  {
    var proxyTypePref = document.getElementById("network.proxy.type");
    var shareProxiesPref = document.getElementById("network.proxy.share_proxy_settings");
    var proxyPrefs = ["ssl", "ftp", "socks"];
    for (var i = 0; i < proxyPrefs.length; ++i) {
      var proxyServerURLPref = document.getElementById("network.proxy." + proxyPrefs[i]);
      var proxyPortPref = document.getElementById("network.proxy." + proxyPrefs[i] + "_port");
      
      // Restore previous per-proxy custom settings, if present. 
      if (!shareProxiesPref.value) {
        var backupServerURLPref = document.getElementById("network.proxy.backup." + proxyPrefs[i]);
        var backupPortPref = document.getElementById("network.proxy.backup." + proxyPrefs[i] + "_port");
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
      proxyServerURLPref.disabled = proxyTypePref.value != 1 || shareProxiesPref.value;
      proxyPortPref.disabled = proxyServerURLPref.disabled;
    }
    var socksVersionPref = document.getElementById("network.proxy.socks_version");
    socksVersionPref.disabled = proxyTypePref.value != 1 || shareProxiesPref.value;
    this.updateDNSPref();
    return undefined;
  },
  
  readProxyProtocolPref: function (aProtocol, aIsPort)
  {
    var shareProxiesPref = document.getElementById("network.proxy.share_proxy_settings");
    if (shareProxiesPref.value) {
      var pref = document.getElementById("network.proxy.http" + (aIsPort ? "_port" : ""));    
      return pref.value;
    }
    
    var backupPref = document.getElementById("network.proxy.backup." + aProtocol + (aIsPort ? "_port" : ""));
    return backupPref.hasUserValue ? backupPref.value : undefined;
  },

  reloadPAC: function ()
  {
    Components.classes["@mozilla.org/network/protocol-proxy-service;1"].
        getService().reloadPAC();
  },
  
  doAutoconfigURLFixup: function ()
  {
    var autoURL = document.getElementById("networkProxyAutoconfigURL");
    var autoURLPref = document.getElementById("network.proxy.autoconfig_url");
    var URIFixup = Components.classes["@mozilla.org/docshell/urifixup;1"]
                             .getService(Components.interfaces.nsIURIFixup);
    try {
      autoURLPref.value = autoURL.value = URIFixup.createFixupURI(autoURL.value, 0).spec;
    } catch(ex) {}
  },

  sanitizeNoProxiesPref: function()
  {
    var noProxiesPref = document.getElementById("network.proxy.no_proxies_on");
    // replace substrings of ; and \n with commas if they're neither immediately
    // preceded nor followed by a valid separator character
    noProxiesPref.value = noProxiesPref.value.replace(/([^, \n;])[;\n]+(?![,\n;])/g, '$1,');
    // replace any remaining ; and \n since some may follow commas, etc. 
    noProxiesPref.value = noProxiesPref.value.replace(/[;\n]/g, '');
  },
  
  readHTTPProxyServer: function ()
  {
    var shareProxiesPref = document.getElementById("network.proxy.share_proxy_settings");
    if (shareProxiesPref.value)
      this.updateProtocolPrefs();
    return undefined;
  },
  
  readHTTPProxyPort: function ()
  {
    var shareProxiesPref = document.getElementById("network.proxy.share_proxy_settings");
    if (shareProxiesPref.value)
      this.updateProtocolPrefs();
    return undefined;
  }
};
