// This file supports translating W3C tests
// to tests on auto MochiTest system with minimum changes.
// Author: Maksim Lebedev <alessarik@gmail.com>

// Function allows to prepare our tests after load document
addEventListener("load", function(event) {
  console.log("OnLoad internal document");
  addListeners(document.getElementById("target0"));
  addListeners(document.getElementById("target1"));
  preExecute();
}, false);

// Function allows to initialize prerequisites before testing
// and adds some callbacks to support mochitest system.
function preExecute() {
  add_result_callback(parent.result_function);
  add_completion_callback(parent.completion_function);
  parent.turnOnPointerEvents(window.callExecute);
}

// The main function allows to execute tests in auto mode
function callExecute() {
  console.log("Run 'executeTest' function");
  if(!!parent.executeTest)
    parent.executeTest(window);
  else
    parent.is(!!parent.executeTest, true, "parent-document should have function 'executeTest'");
}

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
