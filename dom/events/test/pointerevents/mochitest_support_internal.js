// This file supports translating W3C tests
// to tests on auto MochiTest system with minimum changes.
// Author: Maksim Lebedev <alessarik@gmail.com>

const PARENT_ORIGIN = "http://mochi.test:8888/";

// Since web platform tests don't check pointerId, we have to use some heuristic
// to test them. and thus pointerIds are send to mochitest_support_external.js
// before we start sending synthesized widget events. Here, we avoid using
// default values used in Gecko to insure everything works as expected.
const POINTER_MOUSE_ID = 7;
const POINTER_PEN_ID   = 8;
const POINTER_TOUCH_ID = 9; // Extend for multiple touch points if needed.

// Setup environment.
addListeners(document.getElementById("target0"));
addListeners(document.getElementById("target1"));

// Setup communication between mochitest_support_external.js.
// Function allows to initialize prerequisites before testing
// and adds some callbacks to support mochitest system.
function resultCallback(aTestObj) {
  var message = aTestObj["name"] + " (";
  message += "Get: " + JSON.stringify(aTestObj["status"]) + ", ";
  message += "Expect: " + JSON.stringify(aTestObj["PASS"]) + ")";
  window.opener.postMessage({type: "RESULT",
                             message: message,
                             result: aTestObj["status"] === aTestObj["PASS"]},
                            PARENT_ORIGIN);
}

add_result_callback(resultCallback);
add_completion_callback(() => {
  window.opener.postMessage({type: "FIN"}, PARENT_ORIGIN);
});

window.addEventListener("load", () => {
  // Start testing.
  var startMessage = {
    type: "START",
    message: {
      mouseId: POINTER_MOUSE_ID,
      penId:   POINTER_PEN_ID,
      touchId: POINTER_TOUCH_ID
    }
  }
  window.opener.postMessage(startMessage, PARENT_ORIGIN);
});

function addListeners(elem) {
  if(!elem)
    return;
  var All_Events = ["pointerdown","pointerup","pointercancel","pointermove","pointerover","pointerout",
                    "pointerenter","pointerleave","gotpointercapture","lostpointercapture"];
  All_Events.forEach(function(name) {
    elem.addEventListener(name, function(event) {
      console.log('('+event.type+')-('+event.pointerType+')');

      // Perform checks only for trusted events.
      if (!event.isTrusted) {
        return;
      }

      // Compute the desired event.pointerId from event.pointerType.
      var pointerId = {
        mouse: POINTER_MOUSE_ID,
        pen:   POINTER_PEN_ID,
        touch: POINTER_TOUCH_ID
      }[event.pointerType];

      // Compare the pointerId.
      resultCallback({
        name:   "Mismatched event.pointerId recieved.",
        status: event.pointerId,
        PASS:   pointerId
      });

    }, false);
  });
}
