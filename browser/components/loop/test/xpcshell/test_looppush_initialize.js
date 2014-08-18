/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
{
  add_test(function test_initalize_offline() {
    Services.io.offline = true;

    MozLoopPushHandler.initialize(function(err) {
      Assert.equal(err, "offline", "Should error with 'offline' when offline");

      Services.io.offline = false;
      run_next_test();
    });
  });

  let mockWebSocket = new MockWebSocketChannel();

  add_test(function test_initalize_websocket() {
    MozLoopPushHandler.initialize(
      function(err, url) {
        Assert.equal(err, null, "Should return null for success");
        Assert.equal(url, kEndPointUrl, "Should return push server application URL");
        Assert.equal(mockWebSocket.uri.prePath, kServerPushUrl,
                     "Should have the url from preferences");
        Assert.equal(mockWebSocket.origin, kServerPushUrl,
                     "Should have the origin url from preferences");
        Assert.equal(mockWebSocket.protocol, "push-notification",
                     "Should have the protocol set to push-notifications");
        mockWebSocket.notify(15);
      },
      function(version) {
        Assert.equal(version, 15, "Should have version number 15");
        run_next_test();
      }, 
      mockWebSocket);
  });

  add_test(function test_reconnect_websocket() {
    MozLoopPushHandler.uaID = undefined;
    MozLoopPushHandler.pushUrl = undefined; //Do this to force a new registration callback.
    mockWebSocket.stop();
  });

  add_test(function test_reopen_websocket() {
    MozLoopPushHandler.uaID = undefined;
    MozLoopPushHandler.pushUrl = undefined; //Do this to force a new registration callback.
    mockWebSocket.serverClose();
  });

  add_test(function test_retry_registration() {
    MozLoopPushHandler.uaID = undefined;
    MozLoopPushHandler.pushUrl = undefined; //Do this to force a new registration callback.
    mockWebSocket.initRegStatus = 500;
    mockWebSocket.stop();
  });

  function run_test() {
    setupFakeLoopServer();

    Services.prefs.setCharPref("services.push.serverURL", kServerPushUrl);
    Services.prefs.setIntPref("loop.retry_delay.start", 10); // 10 ms
    Services.prefs.setIntPref("loop.retry_delay.limit", 20); // 20 ms

    run_next_test();
  };
}
