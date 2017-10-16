// See https://bugzilla.mozilla.org/show_bug.cgi?id=1273251
"use strict"

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const XRAY_PREF = "dom.allow_named_properties_object_for_xrays";

add_task(async function() {
  let webnav = Services.appShell.createWindowlessBrowser(false);

  let docShell = webnav.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDocShell);

  docShell.createAboutBlankContentViewer(null);

  let window = webnav.getInterface(Ci.nsIDOMWindow);
  let unwrapped = Cu.waiveXrays(window);

  window.document.body.innerHTML = '<div id="foo"></div>';

  equal(Preferences.get(XRAY_PREF), 1, "Should have pref=1 by default");

  Preferences.set(XRAY_PREF, 0);
  equal(typeof window.foo, "object", "Should have named X-ray property access with pref=0");
  equal(typeof unwrapped.foo, "object", "Should always have non-X-ray named property access");

  Preferences.set(XRAY_PREF, 1);
  equal(typeof window.foo, "object", "Should have named X-ray property access with pref=1");
  equal(typeof unwrapped.foo, "object", "Should always have non-X-ray named property access");

  Preferences.set(XRAY_PREF, 2);
  equal(window.foo, undefined, "Should not have named X-ray property access with pref=2");
  equal(typeof unwrapped.foo, "object", "Should always have non-X-ray named property access");

  webnav.close();
});

