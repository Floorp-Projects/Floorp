/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_test(function test_initalize_offline() {
  Services.io.offline = true;

  MozLoopPushHandler.initialize(function(err) {
    Assert.equal(err, "offline", "Should error with 'offline' when offline");

    Services.io.offline = false;
    run_next_test();
  });
});

add_test(function test_initalize_websocket() {
  let mockWebSocket = new MockWebSocketChannel();

  MozLoopPushHandler.initialize(function(err) {
    Assert.equal(err, null, "Should return null for success");

    Assert.equal(mockWebSocket.uri.prePath, kServerPushUrl,
                 "Should have the url from preferences");
    Assert.equal(mockWebSocket.origin, kServerPushUrl,
                 "Should have the origin url from preferences");
    Assert.equal(mockWebSocket.protocol, "push-notification",
                 "Should have the protocol set to push-notifications");

    run_next_test();
  }, function() {}, mockWebSocket);
});

function run_test() {
  Services.prefs.setCharPref("services.push.serverURL", kServerPushUrl);

  run_next_test();
};
