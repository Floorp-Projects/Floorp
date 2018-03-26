/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function setup() {
  const expiry = Date.now() + 24 * 60 * 60;
  Services.cookies.add("example.com", "/", "secure", "true", true, false, false, expiry, {});
  Services.cookies.add("example.com", "/", "insecure", "true", false, false, false, expiry, {});
  Services.cookies.add("example.org", "/", "secure", "true", true, false, false, expiry, {});
  Services.cookies.add("example.org", "/", "insecure", "true", false, false, false, expiry, {});
  Services.cookies.add("example.net", "/", "secure", "true", true, false, false, expiry, {});
  await setupPolicyEngineWithJson({
    "policies": {
      "Cookies": {
        "Block": [
          "http://example.com",
          "https://example.org:8080"
        ]
      }
    }
  });
});

function retrieve_all_cookies(host) {
  const values = [];
  const cookies = Services.cookies.getCookiesFromHost(host, {});
  while (cookies.hasMoreElements()) {
    const cookie = cookies.getNext().QueryInterface(Ci.nsICookie);
    values.push({
      host: cookie.host,
      name: cookie.name,
      path: cookie.path
    });
  }
  return values;
}

add_task(async function test_cookies_for_blocked_sites_cleared() {
  const cookies = {
    hostname: retrieve_all_cookies("example.com"),
    origin: retrieve_all_cookies("example.org"),
    keep: retrieve_all_cookies("example.net")
  };
  const expected = {
    hostname: [],
    origin: [],
    keep: [
      {host: "example.net",
       name: "secure",
       path: "/"}
    ]
  };
  is(JSON.stringify(cookies), JSON.stringify(expected),
     "All stored cookies for blocked origins should be cleared");
});

add_task(function teardown() {
  for (let host of ["example.com", "example.org", "example.net"]) {
    Services.cookies.removeCookiesWithOriginAttributes("{}", host);
  }
});
