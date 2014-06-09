/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

XPCOMUtils.defineLazyModuleGetter(this, "Chat",
                                  "resource:///modules/Chat.jsm");

var openChatOrig = Chat.open;

add_test(function test_get_do_not_disturb() {
  Services.prefs.setBoolPref("loop.do_not_disturb", false);

  do_check_false(MozLoopService.doNotDisturb);

  Services.prefs.setBoolPref("loop.do_not_disturb", true);

  do_check_true(MozLoopService.doNotDisturb);

  run_next_test();
});

add_test(function test_set_do_not_disturb() {
  Services.prefs.setBoolPref("loop.do_not_disturb", false);

  MozLoopService.doNotDisturb = true;

  do_check_true(Services.prefs.getBoolPref("loop.do_not_disturb"));

  run_next_test();
});

add_test(function test_do_not_disturb_disabled_should_open_chat_window() {
  MozLoopService.doNotDisturb = false;

  MozLoopService.register().then(() => {
    let webSocket = gMockWebSocketChannelFactory.createdInstances[0];

    let opened = false;
    Chat.open = function() {
      opened = true;
    };

    webSocket.notify(1);

    do_check_true(opened, "should open a chat window");

    run_next_test();
  });
});

add_test(function test_do_not_disturb_enabled_shouldnt_open_chat_window() {
  MozLoopService.doNotDisturb = true;

  MozLoopService.register().then(() => {
    let webSocket = gMockWebSocketChannelFactory.createdInstances[0];

    let opened = false;
    Chat.open = function() {
      opened = true;
    };

    webSocket.notify(1);

    do_check_false(opened, "should not open a chat window");

    run_next_test();
  });
});

function run_test()
{
  setupFakeLoopServer();

  loopServer.registerPathHandler("/registration", (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.processAsync();
    response.finish();
  });

  // Registrations and pref settings.
  gMockWebSocketChannelFactory.register();

  do_register_cleanup(function() {
    gMockWebSocketChannelFactory.unregister();

    // Revert original Chat.open implementation
    Chat.open = openChatOrig;

    // clear test pref
    Services.prefs.clearUserPref("loop.do_not_disturb");
  });

  run_next_test();
}
