/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This file contains tests for various chat window helpers in MozLoopService.
 */
"use strict";

const { Chat } = Cu.import("resource:///modules/Chat.jsm", {});
function isAnyLoopChatOpen() {
  return [...Chat.chatboxes].some(({ src }) => /^about:loopconversation#/.test(src));
}

add_task(MozLoopService.initialize.bind(MozLoopService));

add_task(function* test_mozLoop_nochat() {
  Assert.ok(!isAnyLoopChatOpen(), "there should be no chat windows yet");
});

add_task(function* test_mozLoop_openchat() {
  let windowId = yield LoopRooms.open("fake1234");
  Assert.ok(isAnyLoopChatOpen(), "chat window should have been opened");

  let chatboxesForRoom = [...Chat.chatboxes].filter(chatbox =>
    chatbox.src == MozLoopServiceInternal.getChatURL(windowId));
  Assert.strictEqual(chatboxesForRoom.length, 1, "Only one chatbox should be open");
});

add_task(function* test_mozLoop_hangupAllChatWindows() {
  yield LoopRooms.open("fake2345");

  yield promiseWaitForCondition(() => {
    MozLoopService.hangupAllChatWindows();
    return !isAnyLoopChatOpen();
  });

  Assert.ok(!isAnyLoopChatOpen(), "chat window should have been closed");
});

add_task(function* test_mozLoop_hangupOnClose() {
  let roomToken = "fake1234";

  let hangupNowCalls = [];
  LoopAPI.stubMessageHandlers({
    HangupNow: function(message, reply) {
      hangupNowCalls.push(message);
      reply();
    }
  });

  let windowId = yield LoopRooms.open(roomToken);

  yield promiseWaitForCondition(() => {
    MozLoopService.hangupAllChatWindows();
    return !isAnyLoopChatOpen();
  });

  Assert.greaterOrEqual(hangupNowCalls.length, 1, "HangupNow handler should be called at least once");
  Assert.deepEqual(hangupNowCalls.pop(), {
    name: "HangupNow",
    data: [roomToken, windowId]
  }, "Messages should be the same");

  LoopAPI.restore();
});
