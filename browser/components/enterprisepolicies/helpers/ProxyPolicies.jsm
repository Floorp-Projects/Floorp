/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

const PREF_LOGLEVEL = "browser.policies.loglevel";

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm", {});
  return new ConsoleAPI({
    prefix: "ProxyPolicies.jsm",
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.jsm for details.
    maxLogLevel: "error",
    maxLogLevelPref: PREF_LOGLEVEL,
  });
});

// Don't use const here because this is acessed by
// tests through the BackstagePass object.
var PROXY_TYPES_MAP = new Map([
  ["none", Ci.nsIProtocolProxyService.PROXYCONFIG_DIRECT],
  ["system", Ci.nsIProtocolProxyService.PROXYCONFIG_SYSTEM],
  ["manual", Ci.nsIProtocolProxyService.PROXYCONFIG_MANUAL],
  ["autoDetect", Ci.nsIProtocolProxyService.PROXYCONFIG_WPAD],
  ["autoConfig", Ci.nsIProtocolProxyService.PROXYCONFIG_PAC],
]);

var EXPORTED_SYMBOLS = [ "ProxyPolicies" ];

var ProxyPolicies = {
  configureProxySettings(param, setPref) {
    if (param.Mode) {
      setPref("network.proxy.type", PROXY_TYPES_MAP.get(param.Mode));
    }

    if (param.AutoConfigURL) {
      setPref("network.proxy.autoconfig_url", param.AutoConfigURL.href);
    }

    if (param.UseProxyForDNS !== undefined) {
      setPref("network.proxy.socks_remote_dns", param.UseProxyForDNS);
    }

    if (param.AutoLogin !== undefined) {
      setPref("signon.autologin.proxy", param.AutoLogin);
    }

    if (param.SOCKSVersion !== undefined) {
      if (param.SOCKSVersion != 4 && param.SOCKSVersion != 5) {
        log.error("Invalid SOCKS version");
      } else {
        setPref("network.proxy.socks_version", param.SOCKSVersion);
      }
    }

    if (param.Passthrough !== undefined) {
      setPref("network.proxy.no_proxies_on", param.Passthrough);
    }

    if (param.UseHTTPProxyForAllProtocols !== undefined) {
      setPref("network.proxy.share_proxy_settings", param.UseHTTPProxyForAllProtocols);
    }

    function setProxyHostAndPort(type, address) {
      let url;
      try {
        // Prepend https just so we can use the URL parser
        // instead of parsing manually.
        url = new URL(`https://${address}`);
      } catch (e) {
        log.error(`Invalid address for ${type} proxy: ${address}`);
        return;
      }

      setPref(`network.proxy.${type}`, url.hostname);
      if (url.port) {
        setPref(`network.proxy.${type}_port`, Number(url.port));
      }
    }

    if (param.HTTPProxy) {
      setProxyHostAndPort("http", param.HTTPProxy);

      // network.proxy.share_proxy_settings is a UI feature, not handled by the
      // network code. That pref only controls if the checkbox is checked, and
      // then we must manually set the other values.
      if (param.UseHTTPProxyForAllProtocols) {
        param.FTPProxy = param.SSLProxy = param.SOCKSProxy = param.HTTPProxy;
      }
    }

    if (param.FTPProxy) {
      setProxyHostAndPort("ftp", param.FTPProxy);
    }

    if (param.SSLProxy) {
      setProxyHostAndPort("ssl", param.SSLProxy);
    }

    if (param.SOCKSProxy) {
      setProxyHostAndPort("socks", param.SOCKSProxy);
    }
  }
};
