const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");

const contract = "@mozilla.org/network/protocol/about;1?what=newtab";
const am = Cc[contract].getService(Ci.nsIAboutModule);
const uri = Services.io.newURI("about:newtab");

function run_test() {
  test_AS_enabled_flags();
  test_AS_disabled_flags();
}

// Since tiles isn't e10s capable, it shouldn't advertise that it can load in
// the child.
function test_AS_disabled_flags() {
  Services.prefs.setBoolPref("browser.newtabpage.activity-stream.enabled",
    false);

  let flags = am.getURIFlags(uri);

  ok(!(flags & Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD));
}

// Activity Stream, however, is e10s-capable, and should advertise it.
function test_AS_enabled_flags() {
  Services.prefs.setBoolPref("browser.newtabpage.activity-stream.enabled",
    true);

  let flags = am.getURIFlags(uri);

  ok(flags & Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD);
}
