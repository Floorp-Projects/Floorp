/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
const fakeEventType = "foo";

function testEventTarget(event) {
  if (event.target !== null || event.currentTarget !== null) {
    throw new Error("Event has a non-null target!");
  }
  postMessage(event.data);
}

addEventListener(fakeEventType, function(event) {
  throw new Error("Trusted event listener received untrusted event!");
}, false);

addEventListener(fakeEventType, function(event) {
  if (event.target !== self || event.currentTarget !== self) {
    throw new Error("Fake event has bad target!");
  }
  if (event.isTrusted) {
    throw new Error("Event should be untrusted!");
  }
  event.stopImmediatePropagation();
  postMessage(event.data);
}, false, true);

addEventListener(fakeEventType, function(event) {
  throw new Error("This shouldn't get called because of stopImmediatePropagation.");
}, false, true);

var count = 0;
onmessage = function(event) {
  if (event.target !== self || event.currentTarget !== self) {
    throw new Error("Event has bad target!");
  }

  if (!count++) {
    var exception;
    try {
      self.dispatchEvent(event);
    }
    catch(e) {
      exception = e;
    }

    if (!exception) {
      throw new Error("Recursive dispatch didn't fail!");
    }

    event.initMessageEvent(fakeEventType, event.bubbles, event.cancelable,
                           event.data, "*", null);
    self.dispatchEvent(event);
    return;
  }

  setTimeout(testEventTarget, 0, event);
};
