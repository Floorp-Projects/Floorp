/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

XPCOMUtils.defineLazyModuleGetter(this, "Chat",
                                  "resource:///modules/Chat.jsm");

let openChatOrig = Chat.open;

const firstCallId = 4444333221;
const secondCallId = 1001100101;

function test_send_busy_on_call() {
  let actionReceived = false;

  let msgHandler = function(msg) {
    if (msg.messageType &&
        msg.messageType === "action" &&
        msg.event === "terminate" &&
        msg.reason === "busy") {
      actionReceived = true;
    }
  };

  let mockWebSocket = new MockWebSocketChannel({defaultMsgHandler: msgHandler});
  Services.io.offline = false;

  MozLoopService.register(mockPushHandler, mockWebSocket).then(() => {
    let opened = 0;
    Chat.open = function() {
      opened++;
    };

    mockPushHandler.notify(1);

    waitForCondition(() => {return actionReceived && opened > 0}).then(() => {
      do_check_true(opened === 1, "should open only one chat window");
      do_check_true(actionReceived, "should respond with busy/reject to second call");
      MozLoopService.releaseCallData(firstCallId);
      run_next_test();
    }, () => {
      do_throw("should have opened a chat window for first call and rejected second call");
    });

  });
}

add_test(test_send_busy_on_call); //FXA call accepted, Guest call rejected
add_test(test_send_busy_on_call); //No FXA call, first Guest call accepted, second rejected

function run_test()
{
  setupFakeLoopServer();

  // For each notification received from the PushServer, MozLoopService will first query
  // for any pending calls on the FxA hawk session and then again using the guest session.
  // A pair of response objects in the callsResponses array will be consumed for each
  // notification. The even calls object is for the FxA session, the odd the Guest session.

  let callsRespCount = 0;
  let callsResponses = [
    {calls: [{callId: firstCallId,
              websocketToken: "0deadbeef0",
              progressURL: "wss://localhost:5000/websocket"}]},
    {calls: [{callId: secondCallId,
              websocketToken: "1deadbeef1",
              progressURL: "wss://localhost:5000/websocket"}]},

    {calls: []},
    {calls: [{callId: firstCallId,
              websocketToken: "0deadbeef0",
              progressURL: "wss://localhost:5000/websocket"},
             {callId: secondCallId,
              websocketToken: "1deadbeef1",
              progressURL: "wss://localhost:5000/websocket"}]},
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

    // clear test pref
    Services.prefs.clearUserPref("loop.seenToS");
  });

  run_next_test();
}
