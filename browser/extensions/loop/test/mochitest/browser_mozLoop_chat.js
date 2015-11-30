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
  let windowData = {
    roomToken: "fake1234",
    type: "room"
  };

  LoopRooms.open(windowData);
  Assert.ok(isAnyLoopChatOpen(), "chat window should have been opened");
});

add_task(function* test_mozLoop_hangupAllChatWindows() {
  let windowData = {
    roomToken: "fake2345",
    type: "room"
  };

  LoopRooms.open(windowData);

  yield promiseWaitForCondition(() => {
    MozLoopService.hangupAllChatWindows();
    return !isAnyLoopChatOpen();
  });

  Assert.ok(!isAnyLoopChatOpen(), "chat window should have been closed");
});
