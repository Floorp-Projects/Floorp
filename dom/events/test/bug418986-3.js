SimpleTest.waitForExplicitFinish();

// The main testing function.
var test = function (isContent) {
  // Each definition is [eventType, prefSetting]
  // Where we are setting the "privacy.resistFingerprinting" pref.
  let eventDefs = [["mousedown", true],
                   ["mouseup", true],
                   ["mousedown", false],
                   ["mouseup", false]];

  let testCounter = 0;

  // Declare ahead of time.
  let setup;

  // This function is called when the event handler fires.
  let handleEvent = function (event, prefVal) {
    let resisting = prefVal && isContent;
    if (resisting) {
      is(event.screenX, event.clientX, "event.screenX and event.clientX should be the same");
      is(event.screenY, event.clientY, "event.screenY and event.clientY should be the same");
    } else {
      // We can't be sure about X coordinates not being equal, but we can test Y.
      isnot(event.screenY, event.clientY, "event.screenY !== event.clientY");
    }
    ++testCounter;
    if (testCounter < eventDefs.length) {
      nextTest();
    } else {
      SimpleTest.finish();
    }
  };

  // In this function, we set up the nth div and event handler,
  // and then synthesize a mouse event in the div, to test
  // whether the resulting events resist fingerprinting by
  // suppressing absolute screen coordinates.
  nextTest = function () {
    let [eventType, prefVal] = eventDefs[testCounter];
    SpecialPowers.pushPrefEnv({set:[["privacy.resistFingerprinting", prefVal]]},
      function () {
        // The following code creates a new div for each event in eventDefs,
        // attaches a listener to listen for the event, and then generates
        // a fake event at the center of the div.
        let div = document.createElement("div");
        div.style.width = "10px";
        div.style.height = "10px";
        div.style.backgroundColor = "red";
        // Name the div after the event we're listening for.
        div.id = eventType;
        document.getElementById("body").appendChild(div);
        // Seems we can't add an event listener in chrome unless we run
        // it in a later task.
        window.setTimeout(function() {
          div.addEventListener(eventType, event => handleEvent(event, prefVal), false);
          // For some reason, the following synthesizeMouseAtCenter call only seems to run if we
          // wrap it in a window.setTimeout(..., 0).
          window.setTimeout(function () {
            synthesizeMouseAtCenter(div, {type : eventType});
          }, 0);
        }, 0);
      });
  };

  // Now run by starting with the 0th event.
  nextTest();

};
