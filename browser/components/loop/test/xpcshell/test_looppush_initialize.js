/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
{
  let dummyCallback = () => {};
  let mockWebSocket = new MockWebSocketChannel();
  let pushServerRequestCount = 0;

  add_test(function test_initalize_offline() {
    Services.io.offline = true;
    do_check_false(MozLoopPushHandler.initialize());
    Services.io.offline = false;
    run_next_test();
  });

  add_test(function test_initalize() {
    loopServer.registerPathHandler("/push-server-config", (request, response) => {
      // The PushHandler should retry the request for the push-server-config for
      // each of these cases without throwing an error.
      let n = 0;
      switch (++pushServerRequestCount) {
      case ++n:
        // Non-200 response
        response.setStatusLine(null, 500, "Retry");
        response.processAsync();
        response.finish();
        break;
      case ++n:
        // missing parameter
        response.setStatusLine(null, 200, "OK");
        response.write(JSON.stringify({pushServerURI: null}));
        response.processAsync();
        response.finish();
        break;
      case ++n:
        // json parse error
        response.setStatusLine(null, 200, "OK");
        response.processAsync();
        response.finish();
        break;
      case ++n:
        response.setStatusLine(null, 200, "OK");
        response.write(JSON.stringify({pushServerURI: kServerPushUrl}));
        response.processAsync();
        response.finish();

        run_next_test();
        break;
      }
    });

    do_check_true(MozLoopPushHandler.initialize({mockWebSocket: mockWebSocket}));
  });

  add_test(function test_initalize_missing_chanid() {
    Assert.throws(() => {MozLoopPushHandler.register(null, dummyCallback, dummyCallback)});
    run_next_test();
  });

  add_test(function test_initalize_missing_regcallback() {
    Assert.throws(() => {MozLoopPushHandler.register("chan-1", null, dummyCallback)});
    run_next_test();
  });

  add_test(function test_initalize_missing_notifycallback() {
    Assert.throws(() => {MozLoopPushHandler.register("chan-1", dummyCallback, null)});
    run_next_test();
  });

  add_test(function test_initalize_websocket() {
    MozLoopPushHandler.register(
      "chan-1",
      function(err, url, id) {
        Assert.equal(err, null, "Should return null for success");
        Assert.equal(url, kEndPointUrl, "Should return push server application URL");
        Assert.equal(id, "chan-1", "Should have channel id = chan-1");
        Assert.equal(mockWebSocket.uri.prePath, kServerPushUrl,
                     "Should have the url from preferences");
        Assert.equal(mockWebSocket.origin, kServerPushUrl,
                     "Should have the origin url from preferences");
        Assert.equal(mockWebSocket.protocol, "push-notification",
                     "Should have the protocol set to push-notifications");
        mockWebSocket.notify(15);
      },
      function(version, id) {
        Assert.equal(version, 15, "Should have version number 15");
        Assert.equal(id, "chan-1", "Should have channel id = chan-1");
        run_next_test();
      });
  });

  add_test(function test_register_twice_same_channel() {
    MozLoopPushHandler.register(
      "chan-2",
      function(err, url, id) {
        Assert.equal(err, null, "Should return null for success");
        Assert.equal(url, kEndPointUrl, "Should return push server application URL");
        Assert.equal(id, "chan-2", "Should have channel id = chan-2");
        Assert.equal(mockWebSocket.uri.prePath, kServerPushUrl,
                     "Should have the url from preferences");
        Assert.equal(mockWebSocket.origin, kServerPushUrl,
                     "Should have the origin url from preferences");
        Assert.equal(mockWebSocket.protocol, "push-notification",
                     "Should have the protocol set to push-notifications");

        // Register again for the same channel
        MozLoopPushHandler.register(
          "chan-2",
          function(err, url, id) {
            Assert.notEqual(err, null, "Should have returned an error");
            // Notify the first registration to make sure that still works.
            mockWebSocket.notify(16);
          },
          function(version, id) {
            Assert.ok(false, "The 2nd onNotification callback shouldn't be called");
        });
      },
      function(version, id) {
        Assert.equal(version, 16, "Should have version number 16");
        Assert.equal(id, "chan-2", "Should have channel id = chan-2");
        run_next_test();
      });
  });

  add_test(function test_reconnect_websocket() {
    MozLoopPushHandler.uaID = undefined;
    MozLoopPushHandler.registeredChannels = {}; //Do this to force a new registration callback.
    mockWebSocket.stop();
  });

  add_test(function test_reopen_websocket() {
    MozLoopPushHandler.uaID = undefined;
    MozLoopPushHandler.registeredChannels = {}; //Do this to force a new registration callback.
    mockWebSocket.serverClose();
  });

  add_test(function test_retry_registration() {
    MozLoopPushHandler.uaID = undefined;
    MozLoopPushHandler.registeredChannels = {}; //Do this to force a new registration callback.
    mockWebSocket.initRegStatus = 500;
    mockWebSocket.stop();
  });

  function run_test() {
    setupFakeLoopServer();

    Services.prefs.setIntPref("loop.retry_delay.start", 10); // 10 ms
    Services.prefs.setIntPref("loop.retry_delay.limit", 20); // 20 ms

    do_register_cleanup(function() {
      Services.prefs.clearUserPref("loop.retry_delay.start");
      Services.prefs.clearUserPref("loop.retry_delay.limit");
      Services.prefs.setCharPref("loop.server", kLoopServerUrl);
    });

    run_next_test();
  };
}
