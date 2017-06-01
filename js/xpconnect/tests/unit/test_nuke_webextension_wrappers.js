// See https://bugzilla.mozilla.org/show_bug.cgi?id=1273251

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://testing-common/TestUtils.jsm");

let aps = Cc["@mozilla.org/addons/policy-service;1"].getService(Ci.nsIAddonPolicyService).wrappedJSObject;

let oldAddonIdCallback = aps.setExtensionURIToAddonIdCallback(uri => uri.host);
do_register_cleanup(() => {
  aps.setExtensionURIToAddonIdCallback(oldAddonIdCallback);
});

function getWindowlessBrowser(url) {
  let ssm = Services.scriptSecurityManager;

  let uri = NetUtil.newURI(url);
  // TODO: Remove when addonId origin attribute is removed.
  let attrs = {addonId: uri.host};

  let principal = ssm.createCodebasePrincipal(uri, attrs);

  let webnav = Services.appShell.createWindowlessBrowser(false);

  let docShell = webnav.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDocShell);

  docShell.createAboutBlankContentViewer(principal);

  return webnav;
}

add_task(function*() {
  let ssm = Services.scriptSecurityManager;

  let webnavA = getWindowlessBrowser("moz-extension://foo/a.html");
  let webnavB = getWindowlessBrowser("moz-extension://foo/b.html");

  let winA = Cu.waiveXrays(webnavA.document.defaultView);
  let winB = Cu.waiveXrays(webnavB.document.defaultView);

  winB.winA = winA;
  winB.eval(`winA.thing = {foo: "bar"};`);

  let getThing = winA.eval(String(() => {
    try {
      return thing.foo;
    } catch (e) {
      return String(e);
    }
  }));

  // Check that the object can be accessed normally before windowB is closed.
  equal(getThing(), "bar");

  webnavB.close();

  // Wrappers are destroyed asynchronously, so wait for that to happen.
  yield TestUtils.topicObserved("inner-window-destroyed");

  // Check that it can't be accessed after he window has been closed.
  let result = getThing();
  ok(/dead object/.test(result),
     `Result should show a dead wrapper error: ${result}`);

  webnavA.close();
});
