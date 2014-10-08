/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

XPCOMUtils.defineLazyModuleGetter(this, "Chat",
                                  "resource:///modules/Chat.jsm");
let openChatOrig = Chat.open;

const contact = {
  name: [ "Mr Smith" ],
  email: [{
    type: "home",
    value: "fakeEmail",
    pref: true
  }]
};

add_task(function test_startDirectCall_opens_window() {
  let openedUrl;
  Chat.open = function(contentWindow, origin, title, url) {
    openedUrl = url;
  };

  MozLoopService.startDirectCall(contact, "audio-video");

  do_check_true(!!openedUrl, "should open a chat window");

  // Stop the busy kicking in for following tests.
  let callId = openedUrl.match(/about:loopconversation\#outgoing\/(.*)/)[1];
  MozLoopService.releaseCallData(callId);
});

add_task(function test_startDirectCall_getCallData() {
  let openedUrl;
  Chat.open = function(contentWindow, origin, title, url) {
    openedUrl = url;
  };

  MozLoopService.startDirectCall(contact, "audio-video");

  let callId = openedUrl.match(/about:loopconversation\#outgoing\/(.*)/)[1];

  let callData = MozLoopService.getCallData(callId);

  do_check_eq(callData.callType, "audio-video", "should have the correct call type");
  do_check_eq(callData.contact, contact, "should have the contact details");

  // Stop the busy kicking in for following tests.
  MozLoopService.releaseCallData(callId);
});

function run_test() {
  do_register_cleanup(function() {
    // Revert original Chat.open implementation
    Chat.open = openChatOrig;
  });

  run_next_test();
}
