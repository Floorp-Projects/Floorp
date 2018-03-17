/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function checkPref(prefName, expectedValue, expectedLockedness) {
  let prefType, prefValue;
  switch (typeof(expectedValue)) {
    case "boolean":
      prefType = Services.prefs.PREF_BOOL;
      prefValue = Services.prefs.getBoolPref(prefName);
      break;

    case "number":
      prefType = Services.prefs.PREF_INT;
      prefValue = Services.prefs.getIntPref(prefName);
      break;

    case "string":
      prefType = Services.prefs.PREF_STRING;
      prefValue = Services.prefs.getStringPref(prefName);
      break;
  }

  if (expectedLockedness !== undefined) {
    is(Services.prefs.prefIsLocked(prefName), expectedLockedness, `Pref ${prefName} is correctly locked`);
  }
  is(Services.prefs.getPrefType(prefName), prefType, `Pref ${prefName} has the correct type`);
  is(prefValue, expectedValue, `Pref ${prefName} has the correct value`);
}

add_task(async function test_proxy_modes() {
  // Checks that every Mode value translates correctly to the expected pref value
  let { PROXY_TYPES_MAP } = ChromeUtils.import("resource:///modules/policies/ProxyPolicies.jsm", {});
  for (let [mode, expectedValue] of PROXY_TYPES_MAP) {
    await setupPolicyEngineWithJson({
      "policies": {
        "Proxy": {
          "Mode": mode,
          "Locked": true
        }
      }
    });
    checkPref("network.proxy.type", expectedValue, true);
  }
});

add_task(async function test_proxy_boolean_settings() {
  // Tests that both false and true values are correctly set and locked
  await setupPolicyEngineWithJson({
    "policies": {
      "Proxy": {
        "UseProxyForDNS": false,
        "AutoLogin": false,
      }
    }
  });

  checkPref("network.proxy.socks_remote_dns", false);
  checkPref("signon.autologin.proxy", false);

  await setupPolicyEngineWithJson({
    "policies": {
      "Proxy": {
        "UseProxyForDNS": true,
        "AutoLogin": true,
      }
    }
  });

  checkPref("network.proxy.socks_remote_dns", true);
  checkPref("signon.autologin.proxy", true);
});

add_task(async function test_proxy_socks_and_passthrough() {
  await setupPolicyEngineWithJson({
    "policies": {
      "Proxy": {
        "SOCKSVersion": 4,
        "Passthrough": "a, b, c"
      }
    }
  });

  checkPref("network.proxy.socks_version", 4);
  checkPref("network.proxy.no_proxies_on", "a, b, c");
});

add_task(async function test_proxy_addresses() {
  function checkProxyPref(proxytype, address, port) {
    checkPref(`network.proxy.${proxytype}`, address);
    checkPref(`network.proxy.${proxytype}_port`, port);
  }

  await setupPolicyEngineWithJson({
    "policies": {
      "Proxy": {
        "HTTPProxy": "http.proxy.example.com:10",
        "FTPProxy": "ftp.proxy.example.com:20",
        "SSLProxy": "ssl.proxy.example.com:30",
        "SOCKSProxy": "socks.proxy.example.com:40",
      }
    }
  });

  checkProxyPref("http", "http.proxy.example.com", 10);
  checkProxyPref("ftp", "ftp.proxy.example.com", 20);
  checkProxyPref("ssl", "ssl.proxy.example.com", 30);
  checkProxyPref("socks", "socks.proxy.example.com", 40);

  // Do the same, but now use the UseHTTPProxyForAllProtocols option
  // and check that it takes effect.
  await setupPolicyEngineWithJson({
    "policies": {
      "Proxy": {
        "HTTPProxy": "http.proxy.example.com:10",
        "FTPProxy": "ftp.proxy.example.com:20",
        "SSLProxy": "ssl.proxy.example.com:30",
        "SOCKSProxy": "socks.proxy.example.com:40",
        "UseHTTPProxyForAllProtocols": true
      }
    }
  });


  checkProxyPref("http", "http.proxy.example.com", 10);
  checkProxyPref("ftp", "http.proxy.example.com", 10);
  checkProxyPref("ssl", "http.proxy.example.com", 10);
  checkProxyPref("socks", "http.proxy.example.com", 10);
});
