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

// Helper function to run Point Event test in a new tab.
function runTestInNewWindow(aFile) {
  var w = window.open('', "_blank");
  w.is = function(a, b, msg) { return is(a, b, aFile + " | " + msg); };
  w.ok = function(cond, name, diag) { return ok(cond, aFile + " | " + name, diag); };
  w.location = location.href.substring(0, location.href.lastIndexOf('/') + 1) + aFile;

  w.testContext = {
    result_callback: (aTestObj) => {
      if(aTestObj["status"] != aTestObj["PASS"]) {
        console.log(aTestObj["status"] + " = " + aTestObj["PASS"] + ". " + aTestObj["name"]);
      }
      is(aTestObj["status"], aTestObj["PASS"], aTestObj["name"]);
    },

    completion_callback: () => {
      if (!!w.testContext.executionPromise) {
        // We need to wait tests done and execute finished then we can close the window
        w.testContext.executionPromise.then(() => {
          w.close();
          SimpleTest.finish();
        });        
      } else {
        // execute may synchronous trigger tests done. In that case executionPromise
        // is not yet assigned 
        w.close();
        SimpleTest.finish();
      }
    },

    execute: (aWindow) => {
      turnOnPointerEvents(() => {
        w.testContext.executionPromise = new Promise((aResolve, aReject) => {
          executeTest(aWindow);
          aResolve();
        });
      });
    }
  };
  return w;
}
