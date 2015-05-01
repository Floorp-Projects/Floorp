/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "Chat",
                                  "resource:///modules/Chat.jsm");

let openChatOrig = Chat.open;

add_test(function test_openChatWindow_on_notification() {
  Services.prefs.setCharPref("loop.seenToS", "unseen");

  mockPushHandler.registrationPushURL = kEndPointUrl;

  MozLoopService.promiseRegisteredWithServers(LOOP_SESSION_TYPE.FXA).then(() => {
    let opened = false;
    Chat.open = function() {
      opened = true;
    };

    mockPushHandler.notify(1, MozLoopService.channelIDs.callsFxA);

    waitForCondition(() => opened).then(() => {
      do_check_true(opened, "should open a chat window");

      do_check_eq(Services.prefs.getCharPref("loop.seenToS"), "seen",
                  "should set the pref to 'seen'");

      run_next_test();
    }, () => {
      do_throw("should have opened a chat window");
    });

  });
});

function run_test() {
  setupFakeLoopServer();

  setupFakeFxAUserProfile();

  loopServer.registerPathHandler("/registration", (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.processAsync();
    response.finish();
  });
  loopServer.registerPathHandler("/calls", (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.write(JSON.stringify({calls: [{callId: 4444333221, websocketToken: "0deadbeef0"}]}));
    response.processAsync();
    response.finish();
  });

  do_register_cleanup(function() {
    // Revert original Chat.open implementation
    Chat.open = openChatOrig;

    // clear test pref
    Services.prefs.clearUserPref("loop.seenToS");
  });

  run_next_test();
}
