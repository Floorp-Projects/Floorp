/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { LoopCallsInternal } = Cu.import("resource:///modules/loop/LoopCalls.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "Chat",
                                  "resource:///modules/Chat.jsm");

let actionReceived = false;
let openChatOrig = Chat.open;

const firstCallId = 4444333221;
const secondCallId = 1001100101;

let msgHandler = function(msg) {
  if (msg.messageType &&
      msg.messageType === "action" &&
      msg.event === "terminate" &&
      msg.reason === "busy") {
    actionReceived = true;
  }
};


add_task(function* test_busy_2fxa_calls() {
  actionReceived = false;

  yield MozLoopService.promiseRegisteredWithServers(LOOP_SESSION_TYPE.FXA);

  let opened = 0;
  let windowId;
  Chat.open = function(contentWindow, origin, title, url) {
    opened++;
    windowId = url.match(/about:loopconversation\#(\d+)$/)[1];
    return windowId;
  };

  mockPushHandler.notify(1, MozLoopService.channelIDs.callsFxA);

  yield waitForCondition(() => { return actionReceived && opened > 0; }).then(() => {
    do_check_true(opened === 1, "should open only one chat window");
    do_check_true(actionReceived, "should respond with busy/reject to second call");
    LoopCalls.clearCallInProgress(windowId);
  }, () => {
    do_throw("should have opened a chat window for first call and rejected second call");
  });
});

function run_test() {
  setupFakeLoopServer();

  // Setup fake login state so we get FxA requests.
  const MozLoopServiceInternal = Cu.import("resource:///modules/loop/MozLoopService.jsm", {}).MozLoopServiceInternal;
  MozLoopServiceInternal.fxAOAuthTokenData = {
    token_type:"bearer",
    access_token:"1bad3e44b12f77a88fe09f016f6a37c42e40f974bc7a8b432bb0d2f0e37e1752",
    scope:"profile"
  };
  MozLoopServiceInternal.fxAOAuthProfile = {email: "test@example.com", uid: "abcd1234"};

  let mockWebSocket = new MockWebSocketChannel();
  mockWebSocket.defaultMsgHandler = msgHandler;
  LoopCallsInternal.mocks.webSocket = mockWebSocket;

  Services.io.offline = false;

  mockPushHandler.registrationPushURL = kEndPointUrl;

  // For each notification received from the PushServer, MozLoopService will first query
  // for any pending calls on the FxA hawk session and then again using the guest session.
  // A pair of response objects in the callsResponses array will be consumed for each
  // notification. The even calls object is for the FxA session, the odd the Guest session.

  let callsRespCount = 0;
  let callsResponses = [
    {calls: [{callId: firstCallId,
              websocketToken: "0deadbeef0",
              progressURL: "wss://localhost:5000/websocket"},
             {callId: secondCallId,
              websocketToken: "1deadbeef1",
              progressURL: "wss://localhost:5000/websocket"}]},
    {calls: [{callId: firstCallId,
              websocketToken: "0deadbeef0",
              progressURL: "wss://localhost:5000/websocket"}]},
    {calls: [{callId: secondCallId,
              websocketToken: "1deadbeef1",
              progressURL: "wss://localhost:5000/websocket"}]}
  ];

  loopServer.registerPathHandler("/registration", (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.processAsync();
    response.finish();
  });

  loopServer.registerPathHandler("/calls", (request, response) => {
    response.setStatusLine(null, 200, "OK");

    if (callsRespCount >= callsResponses.length) {
      callsRespCount = 0;
    }

    response.write(JSON.stringify(callsResponses[callsRespCount++]));
    response.processAsync();
    response.finish();
  });

  do_register_cleanup(function() {
    // Revert original Chat.open implementation
    Chat.open = openChatOrig;

    // Revert fake login state
    MozLoopServiceInternal.fxAOAuthTokenData = null;

    // clear test pref
    Services.prefs.clearUserPref("loop.seenToS");

    LoopCallsInternal.mocks.webSocket = undefined;
  });

  run_next_test();
}
