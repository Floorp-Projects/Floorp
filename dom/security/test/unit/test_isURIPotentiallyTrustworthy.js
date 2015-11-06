/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests the "Is origin potentially trustworthy?" logic from
 * <https://w3c.github.io/webappsec-secure-contexts/#is-origin-trustworthy>.
 */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gContentSecurityManager",
                                   "@mozilla.org/contentsecuritymanager;1",
                                   "nsIContentSecurityManager");

add_task(function* test_isURIPotentiallyTrustworthy() {
  for (let [uriSpec, expectedResult] of [
    ["http://example.com/", false],
    ["https://example.com/", true],
    ["http://localhost/", true],
    ["http://127.0.0.1/", true],
    ["file:///", true],
    ["about:config", false],
    ["urn:generic", false],
  ]) {
    let uri = NetUtil.newURI(uriSpec);
    Assert.equal(gContentSecurityManager.isURIPotentiallyTrustworthy(uri),
                 expectedResult);
  }
});
