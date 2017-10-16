// See https://bugzilla.mozilla.org/show_bug.cgi?id=1273251
"use strict"

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");

add_task(async function() {
  let webnav = Services.appShell.createWindowlessBrowser(false);

  let docShell = webnav.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDocShell);

  docShell.createAboutBlankContentViewer(null);

  let window = webnav.getInterface(Ci.nsIDOMWindow);
  let unwrapped = Cu.waiveXrays(window);

  window.document.body.innerHTML = '<div id="foo"></div>';

  equal(window.foo, undefined, "Should not have named X-ray property access");
  equal(typeof unwrapped.foo, "object", "Should always have non-X-ray named property access");

  webnav.close();
});

