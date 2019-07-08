/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests the "Is origin potentially trustworthy?" logic from
 * <https://w3c.github.io/webappsec-secure-contexts/#is-origin-trustworthy>.
 */

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gScriptSecurityManager",
  "@mozilla.org/scriptsecuritymanager;1",
  "nsIScriptSecurityManager"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gContentSecurityManager",
  "@mozilla.org/contentsecuritymanager;1",
  "nsIContentSecurityManager"
);

Services.prefs.setCharPref(
  "dom.securecontext.whitelist",
  "example.net,example.org"
);

add_task(async function test_isOriginPotentiallyTrustworthy() {
  for (let [uriSpec, expectedResult] of [
    ["http://example.com/", false],
    ["https://example.com/", true],
    ["http://localhost/", true],
    ["http://127.0.0.1/", true],
    ["file:///", true],
    ["resource:///", true],
    ["moz-extension://", true],
    ["wss://example.com/", true],
    ["about:config", false],
    ["http://example.net/", true],
    ["ws://example.org/", true],
    ["chrome://example.net/content/messenger.xul", false],
    ["http://1234567890abcdef.onion/", false],
  ]) {
    let uri = NetUtil.newURI(uriSpec);
    let principal = gScriptSecurityManager.createCodebasePrincipal(uri, {});
    Assert.equal(
      gContentSecurityManager.isOriginPotentiallyTrustworthy(principal),
      expectedResult
    );
  }
  // And now let's test whether .onion sites are properly treated when
  // whitelisted, see bug 1382359.
  Services.prefs.setBoolPref("dom.securecontext.whitelist_onions", true);
  let uri = NetUtil.newURI("http://1234567890abcdef.onion/");
  let principal = gScriptSecurityManager.createCodebasePrincipal(uri, {});
  Assert.equal(
    gContentSecurityManager.isOriginPotentiallyTrustworthy(principal),
    true
  );
});
