// This file supports translating W3C tests
// to tests on auto MochiTest system with minimum changes.
// Author: Maksim Lebedev <alessarik@gmail.com>

// Function allows to prepare our tests after load document
addEventListener("load", function(event) {
  console.log("OnLoad external document");
  prepareTest();
}, false);

// Function allows to initialize prerequisites before testing
function prepareTest() {
  SimpleTest.waitForExplicitFinish();
  SimpleTest.requestCompleteLog();
  turnOnPointerEvents(startTest);
}

function setImplicitPointerCapture(capture, callback) {
  console.log("SET dom.w3c_pointer_events.implicit_capture as " + capture);
  SpecialPowers.pushPrefEnv({
    "set": [
      ["dom.w3c_pointer_events.implicit_capture", capture]
    ]
  }, callback);
}

function turnOnPointerEvents(callback) {
  console.log("SET dom.w3c_pointer_events.enabled as TRUE");
  console.log("SET layout.css.touch_action.enabled as TRUE");
  SpecialPowers.pushPrefEnv({
    "set": [
      ["dom.w3c_pointer_events.enabled", true],
      ["layout.css.touch_action.enabled", true]
    ]
  }, callback);
}

// Helper function to send MouseEvent with different parameters
function sendMouseEvent(int_win, elemId, mouseEventType, params) {
  var elem = int_win.document.getElementById(elemId);
  if(!!elem) {
    var rect = elem.getBoundingClientRect();
    var eventObj = {type: mouseEventType};
    if(params && "button" in params)
      eventObj.button = params.button;
    if(params && "inputSource" in params)
      eventObj.inputSource = params.inputSource;
    if(params && "buttons" in params)
      eventObj.buttons = params.buttons;

    // Default to the center of the target element but we can still send to a
    // position outside of the target element.
    var offsetX = params && "offsetX" in params ? params.offsetX : rect.width / 2;
    var offsetY = params && "offsetY" in params ? params.offsetY : rect.height / 2;

    console.log(elemId, eventObj);
    synthesizeMouse(elem, offsetX, offsetY, eventObj, int_win);

  } else {
    is(!!elem, true, "Document should have element with id: " + elemId);
  }
}

// Helper function to send TouchEvent with different parameters
function sendTouchEvent(int_win, elemId, touchEventType, params) {
  var elem = int_win.document.getElementById(elemId);
  if(!!elem) {
    var rect = elem.getBoundingClientRect();
    var eventObj = {type: touchEventType};

    // Default to the center of the target element but we can still send to a
    // position outside of the target element.
    var offsetX = params && "offsetX" in params ? params.offsetX : rect.width / 2;
    var offsetY = params && "offsetY" in params ? params.offsetY : rect.height / 2;

    console.log(elemId, eventObj);
    synthesizeTouch(elem, offsetX, offsetY, eventObj, int_win);
  } else {
    is(!!elem, true, "Document should have element with id: " + elemId);
  }
}

// Helper function to run Point Event test in a new tab.
function runTestInNewWindow(aFile) {
  var testURL = location.href.substring(0, location.href.lastIndexOf('/') + 1) + aFile;
  var testWindow = window.open(testURL, "_blank");

  window.addEventListener("message", function(aEvent) {
    switch(aEvent.data.type) {
      case "START":
        turnOnPointerEvents(() => {
          executeTest(testWindow);
        });
        return;
      case "RESULT":
        ok(aEvent.data.result, aEvent.data.message);
        return;
      case "FIN":
        testWindow.close();
        SimpleTest.finish();
        return;
    }
  });
}
