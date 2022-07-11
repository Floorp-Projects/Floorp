const contract = "@mozilla.org/network/protocol/about;1?what=newtab";
const am = Cc[contract].getService(Ci.nsIAboutModule);
const uri = Services.io.newURI("about:newtab");

function run_test() {
  test_AS_enabled_flags();
}

// Activity Stream, however, is e10s-capable, and should advertise it.
function test_AS_enabled_flags() {
  let flags = am.getURIFlags(uri);

  ok(flags & Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD);
}
