XPCOMUtils.defineLazyModuleGetter(this, "MozLoopService",
  "resource:///modules/loop/MozLoopService.jsm");

var server;

const kServerPushUrl = "http://localhost:3456";

// XXX should report error if Hawk-Session-Token is lexically invalid
// (not a string of 64 hex digits) to help resist other possible injection
// attacks.  For now, however, we're just checking if it's the right length.
add_test(function test_registration_handles_bogus_hawk_token() {

  var wrongSizeToken = "jdkasjkasjdlaksj";
  Services.prefs.clearUserPref("loop.hawk-session-token");

  server.registerPathHandler("/registration", (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.setHeader("Hawk-Session-Token", wrongSizeToken, false);
    response.processAsync();
    response.finish();
  });

  MozLoopService.register(function(err) {

    Assert.equal(err, "session-token-wrong-size", "Should cause an error to be" +
      " called back if the session-token is not 64 characters long");

    // for some reason, Assert.throw is misbehaving, so....
    var ex;
    try {
      Services.prefs.getCharPref("loop.hawk-session-token");
    } catch (e) {
      ex = e;
    }
    Assert.notEqual(ex, undefined, "Should not set a loop.hawk-session-token pref");

    run_next_test();
  });
});

function run_test()
{
  server = new HttpServer();
  server.start(-1);

  // Registrations and pref settings.
  gMockWebSocketChannelFactory.register();
  Services.prefs.setCharPref("services.push.serverURL", kServerPushUrl);

  Services.prefs.setCharPref("loop.server", "http://localhost:" + server.identity.primaryPort);

  do_register_cleanup(function() {
    gMockWebSocketChannelFactory.unregister();
    Services.prefs.clearUserPref("loop.hawk-session-token");
    server.stop(function() {});
  });

  run_next_test();

}
