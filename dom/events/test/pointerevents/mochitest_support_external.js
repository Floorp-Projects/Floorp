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

// Function checks that test should have status PASS
function result_function(testObj) {
if(testObj["status"] != testObj["PASS"])
  console.log(testObj["status"] + " = " + testObj["PASS"] + ". " + testObj["name"]);
  is(testObj["status"], testObj["PASS"], testObj["name"]);
}

// Function allows to correct finish test in mochitest system
function completion_function() {
  console.log("w3c tests have been finished");
  if(!SimpleTest._stopOnLoad) {
    console.log("Finishing Mochitest system");
    SimpleTest.finish();
  }
}

// Helper function to send PointerEvent with different parameters
function sendPointerEvent(int_win, elemId, pointerEventType, inputSource, params) {
  var elem = int_win.document.getElementById(elemId);
  if(!!elem) {
    var rect = elem.getBoundingClientRect();
    var eventObj = {type: pointerEventType, inputSource: inputSource};
    if(params && "button" in params)
      eventObj.button = params.button;
    if(params && "isPrimary" in params)
      eventObj.isPrimary = params.isPrimary;
    else if(MouseEvent.MOZ_SOURCE_MOUSE == inputSource)
      eventObj.isPrimary = true;
    console.log(elemId, eventObj);
    var salt = ("pointermove" == pointerEventType) ? 1 : 2;
    synthesizePointer(elem, rect.width*salt/5, rect.height/2, eventObj, int_win);
  } else {
    is(!!elem, true, "Document should have element with id: " + elemId);
  }
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
    console.log(elemId, eventObj);
    synthesizeMouse(elem, rect.width/4, rect.height/2, eventObj, int_win);
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
    console.log(elemId, eventObj);
    synthesizeTouch(elem, rect.width/4, rect.height/2, eventObj, int_win);
  } else {
    is(!!elem, true, "Document should have element with id: " + elemId);
  }
}
