// This file supports translating W3C tests
// to tests on auto MochiTest system with minimum changes.
// Author: Maksim Lebedev <alessarik@gmail.com>

const PARENT_ORIGIN = "http://mochi.test:8888/";

// Setup environment.
addListeners(document.getElementById("target0"));
addListeners(document.getElementById("target1"));

// Setup communication between mochitest_support_external.js.
// Function allows to initialize prerequisites before testing
// and adds some callbacks to support mochitest system.
add_result_callback((aTestObj) => {
  var message = aTestObj["name"] + " (";
  message += "Get: " + JSON.stringify(aTestObj["status"]) + ", ";
  message += "Expect: " + JSON.stringify(aTestObj["PASS"]) + ")";
  window.opener.postMessage({type: "RESULT",
                             message: message,
                             result: aTestObj["status"] === aTestObj["PASS"]},
                            PARENT_ORIGIN);
});

add_completion_callback(() => {
  window.opener.postMessage({type: "FIN"}, PARENT_ORIGIN);
});

window.addEventListener("load", () => {
  // Start testing when the document is loaded.
  window.opener.postMessage({type: "START"}, PARENT_ORIGIN);
});

function addListeners(elem) {
  if(!elem)
    return;
  var All_Events = ["pointerdown","pointerup","pointercancel","pointermove","pointerover","pointerout",
                    "pointerenter","pointerleave","gotpointercapture","lostpointercapture"];
  All_Events.forEach(function(name) {
    elem.addEventListener(name, function(event) {
      console.log('('+event.type+')-('+event.pointerType+')');
    }, false);
  });
}
