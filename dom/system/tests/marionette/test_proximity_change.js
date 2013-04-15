/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

let receivedEvent = false;
let expectedEvent;

function enableProximityListener() {
  // Setup device proximity event listener, expect defaults
  log("Enabling 'deviceproximity' event listener.");

  // Bug 814043: Device proximity event 'min' and 'max' attributes incorrect
  // Until that is fixed, expect 1:0:1 instead of 1:0:0
  // expectedEvent = new DeviceProximityEvent("deviceproximity",
  //     {value:1, min:0, max:0});
  expectedEvent = new DeviceProximityEvent("deviceproximity",
      {value:1, min:0, max:1});

  window.addEventListener('deviceproximity', listener);
  log("Waiting for device proximity event.");
  waitFor(changeProximity, function() {
    return(receivedEvent);
  });
}

function listener(event) {
  // Received proximity update
  log("Received 'deviceproximity' event via listener (value:"
      + event.value + " min:" + event.min + " max:" + event.max + ").");
  // Verify event values are as expected
  is(event.value, expectedEvent.value, "value");
  is(event.min, expectedEvent.min, "min");
  is(event.max, expectedEvent.max, "max");
  receivedEvent = true;
}

function changeProximity() {
  // Change emulator's proximity and verify event attributes
  let newValue = "7:3:15";

  // Bug 814043: Device proximity event 'min' and 'max' attributes won't change
  // Until fixed, expect proximity event min to be '0' and max to be '1' always
  // expectedEvent = new DeviceProximityEvent("deviceproximity",
  //     {value: 7, min: 3, max: 15});
  expectedEvent = new DeviceProximityEvent("deviceproximity",
       {value:7, min:0, max:1});

  // Setup handler and verify 'ondeviceproximity' event
  window.ondeviceproximity = function(event) {
    log("Received 'ondeviceproximity' event via handler (value:"
        + event.value + " min:" + event.min + " max:"
        + event.max + ").");
    is(event.value, expectedEvent.value, "value");
    is(event.min, expectedEvent.min, "min");
    is(event.max, expectedEvent.max, "max");
    // Turn off event handler and listener
    window.ondeviceproximity = null;
    window.removeEventListener('deviceproximity', listener);
    restoreProximity();
  };

  log("Sending emulator command to fake proximity change (" + newValue + ").");
  runEmulatorCmd("sensor set proximity " + newValue, function(result) {
    log("Emulator callback.");
  });
}

function restoreProximity() {
  // Set the emulator's proximity value back to original
  newValue = "1:0:0";
  log("Sending emulator command to restore proximity (" + newValue + ").");
  runEmulatorCmd("sensor set proximity " + newValue, function(result) {
    cleanUp();
  });
}

function cleanUp() {
  finish();
}

// Start the test
enableProximityListener();
