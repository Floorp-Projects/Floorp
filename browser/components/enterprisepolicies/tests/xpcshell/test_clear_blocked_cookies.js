/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const HOSTNAME_DOMAIN = "browser_policy_clear_blocked_cookies.com";
const ORIGIN_DOMAIN = "browser_policy_clear_blocked_cookies.org";

add_task(async function setup() {
  const expiry = Date.now() + 24 * 60 * 60;
  Services.cookies.add(HOSTNAME_DOMAIN, "/", "secure", "true", true, false, false, expiry, {}, Ci.nsICookie2.SAMESITE_UNSET);
  Services.cookies.add(HOSTNAME_DOMAIN, "/", "insecure", "true", false, false, false, expiry, {}, Ci.nsICookie2.SAMESITE_UNSET);
  Services.cookies.add(ORIGIN_DOMAIN, "/", "secure", "true", true, false, false, expiry, {}, Ci.nsICookie2.SAMESITE_UNSET);
  Services.cookies.add(ORIGIN_DOMAIN, "/", "insecure", "true", false, false, false, expiry, {}, Ci.nsICookie2.SAMESITE_UNSET);
  Services.cookies.add("example.net", "/", "secure", "true", true, false, false, expiry, {}, Ci.nsICookie2.SAMESITE_UNSET);
  await setupPolicyEngineWithJson({
    "policies": {
      "Cookies": {
        "Block": [
          `http://${HOSTNAME_DOMAIN}`,
          `https://${ORIGIN_DOMAIN}:8080`,
        ],
      },
    },
  });
});

function retrieve_all_cookies(host) {
  const values = [];
  for (let cookie of Services.cookies.getCookiesFromHost(host, {})) {
    values.push({
      host: cookie.host,
      name: cookie.name,
      path: cookie.path,
    });
  }
  return values;
}

add_task(async function test_cookies_for_blocked_sites_cleared() {
  const cookies = {
    hostname: retrieve_all_cookies(HOSTNAME_DOMAIN),
    origin: retrieve_all_cookies(ORIGIN_DOMAIN),
    keep: retrieve_all_cookies("example.net"),
  };
  const expected = {
    hostname: [],
    origin: [],
    keep: [
      {host: "example.net",
       name: "secure",
       path: "/"},
    ],
  };
  equal(JSON.stringify(cookies), JSON.stringify(expected),
     "All stored cookies for blocked origins should be cleared");
});

add_task(function teardown() {
  for (let host of [HOSTNAME_DOMAIN, ORIGIN_DOMAIN, "example.net"]) {
    Services.cookies.removeCookiesWithOriginAttributes("{}", host);
  }
});
