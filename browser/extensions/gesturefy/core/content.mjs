import { isEmbeddedFrame, isEditableInput, isScrollableY, scrollToY, getClosestElement, getTargetData } from "/core/utils/commons.mjs";

import ConfigManager from "/core/helpers/config-manager.mjs";

import MouseGestureController from "/core/controllers/mouse-gesture-controller.mjs";

import RockerGestureController from "/core/controllers/rocker-gesture-controller.mjs";

import WheelGestureController from "/core/controllers/wheel-gesture-controller.mjs";

import PatternConstructor from "/core/utils/pattern-constructor.mjs";

import MouseGestureView from "/core/views/mouse-gesture-view/mouse-gesture-view.mjs";

import PopupCommandView from "/core/views/popup-command-view/popup-command-view.mjs";

import ListenerObserver from "/core/helpers/listener-detach-observer.mjs";

import "/core/helpers/user-script-runner.mjs";


// global variable containing the hierarchy of target html elements for scripts injected by commands
window.TARGET = null;

// expose commons functions to scripts injected by commands like scrollTo
window.isEditableInput = isEditableInput;
window.isScrollableY = isScrollableY;
window.scrollToY = scrollToY;
window.getClosestElement = getClosestElement;

const IS_EMBEDDED_FRAME = isEmbeddedFrame();

const Config = new ConfigManager("local", browser.runtime.getURL("resources/json/defaults.json"));
      Config.autoUpdate = true;
      Config.loaded.then(main);
      Config.addEventListener("change", main);

// re-run main function if event listeners got removed
// this is a workaround for https://bugzilla.mozilla.org/show_bug.cgi?id=1726978
ListenerObserver.onDetach.addListener(main);

// setup pattern extractor
const patternConstructor = new PatternConstructor(0.12, 10);

// Define mouse gesture controller event listeners and connect them to the mouse gesture interface methods
// Also sends the appropriate messages to the background script
// movementX/Y cannot be used because the events returned by getCoalescedEvents() contain wrong values (Firefox Bug)

// The conversation to css screen coordinates via window.mozInnerScreenX is required
// to calculate the proper position in the main frame for coordinates from embedded frames
// (required for popup commands and gesture interface)

MouseGestureController.addEventListener("start", (event, events) => {
  // expose target to global variable
  window.TARGET = event.target;
  // get coalesced events
  const coalescedEvents = [];
  // fallback if getCoalescedEvents is not defined + https://bugzilla.mozilla.org/show_bug.cgi?id=1450692
  for (const event of events) {
    if (event.getCoalescedEvents) coalescedEvents.push(...event.getCoalescedEvents());
  }
  if (!coalescedEvents.length) coalescedEvents.push(...events);

  // build gesture pattern
  for (const event of coalescedEvents) {
    patternConstructor.addPoint(event.clientX, event.clientY);
  }

  // handle mouse gesture interface
  if (Config.get("Settings.Gesture.Trace.display") || Config.get("Settings.Gesture.Command.display")) {
    // if the gesture is not performed inside a child frame or an element in the frame is in fullscreen mode (issue #148)
    // then display the mouse gesture ui in this frame, else redirect the events to the top frame
    if (!IS_EMBEDDED_FRAME || document.fullscreenElement) {
      MouseGestureView.initialize(event.clientX, event.clientY);
    }
    else {
      browser.runtime.sendMessage({
        subject: "mouseGestureViewInitialize",
        data: {
          x: event.clientX + window.mozInnerScreenX,
          y: event.clientY + window.mozInnerScreenY
        }
      });
    }

    // handle mouse gesture interface trace update
    if (Config.get("Settings.Gesture.Trace.display")) {
      if (!IS_EMBEDDED_FRAME || document.fullscreenElement) {
        const points = coalescedEvents.map(event => ({ x: event.clientX, y: event.clientY }));
        MouseGestureView.updateGestureTrace(points);
      }
      else {
        // map points to screen wide css coordinates
        const points = coalescedEvents.map(event => ({
          x: event.clientX + window.mozInnerScreenX,
          y: event.clientY + window.mozInnerScreenY
        }));
        browser.runtime.sendMessage({
          subject: "mouseGestureViewUpdateGestureTrace",
          data: {
            points: points
          }
        });
      }
    }
  }
});


MouseGestureController.addEventListener("update", (event, events) => {
  // get coalesced events
  // fallback if getCoalescedEvents is not defined + https://bugzilla.mozilla.org/show_bug.cgi?id=1450692
  const coalescedEvents = event.getCoalescedEvents ? event.getCoalescedEvents() : [];
  if (!coalescedEvents.length) coalescedEvents.push(event);

  // build gesture pattern
  for (const event of coalescedEvents) {
    const patternChange = patternConstructor.addPoint(event.clientX, event.clientY);

    if (patternChange && Config.get("Settings.Gesture.Command.display")) {
      // send current pattern to background script
      const response = browser.runtime.sendMessage({
        subject: "gestureChange",
        data: patternConstructor.getPattern()
      });
      // update command in gesture view
      response.then(MouseGestureView.updateGestureCommand);
    }
  }

  // handle mouse gesture interface update
  if (Config.get("Settings.Gesture.Trace.display")) {
    if (!IS_EMBEDDED_FRAME || document.fullscreenElement) {
      const points = coalescedEvents.map(event => ({ x: event.clientX, y: event.clientY }));
      MouseGestureView.updateGestureTrace(points);
    }
    else {
      // map points to global screen wide coordinates
      const points = coalescedEvents.map(event => ({
        x: event.clientX + window.mozInnerScreenX,
        y: event.clientY + window.mozInnerScreenY
      }));
      browser.runtime.sendMessage({
        subject: "mouseGestureViewUpdateGestureTrace",
        data: {
          points: points
        }
      });
    }
  }
});


MouseGestureController.addEventListener("abort", (events) => {
  // close mouse gesture interface
  if (Config.get("Settings.Gesture.Trace.display") || Config.get("Settings.Gesture.Command.display")) {
    if (!IS_EMBEDDED_FRAME || document.fullscreenElement) MouseGestureView.terminate();
    else browser.runtime.sendMessage({
      subject: "mouseGestureViewTerminate"
    });
  }
  // clear pattern
  patternConstructor.clear();
});


MouseGestureController.addEventListener("end", (event, events) => {
  // close mouse gesture interface
  if (Config.get("Settings.Gesture.Trace.display") || Config.get("Settings.Gesture.Command.display")) {
    if (!IS_EMBEDDED_FRAME || document.fullscreenElement) MouseGestureView.terminate();
    else browser.runtime.sendMessage({
      subject: "mouseGestureViewTerminate"
    });
  }

  // if the target was removed from dom trace a new element at the starting point
  if (!window.TARGET.isConnected) {
    window.TARGET = document.elementFromPoint(events[0].clientX, events[0].clientY);
  }

  // gather target data and gesture pattern
  const data = getTargetData(window.TARGET);
        data.pattern = patternConstructor.getPattern();
        // transform coordinates to css screen coordinates
        data.mousePosition = {
          x: event.clientX + window.mozInnerScreenX,
          y: event.clientY + window.mozInnerScreenY
        };
  // send data to background script
  browser.runtime.sendMessage({
    subject: "gestureEnd",
    data: data
  });
  // clear pattern
  patternConstructor.clear();
});


// add message listeners to main frame
// these handle the mouse gesture view messages send from embedded frames and the background script

if (!IS_EMBEDDED_FRAME) {
  browser.runtime.onMessage.addListener((message) => {
    switch (message.subject) {
      case "mouseGestureViewInitialize":
        // remap points to client wide css coordinates
        MouseGestureView.initialize(
          message.data.x - window.mozInnerScreenX,
          message.data.y - window.mozInnerScreenY
        );
      break;

      case "mouseGestureViewUpdateGestureTrace":
        // remap points to client wide css coordinates
        message.data.points.forEach(point => {
          point.x -= window.mozInnerScreenX;
          point.y -= window.mozInnerScreenY;
        });
        MouseGestureView.updateGestureTrace(message.data.points);
      break;

      case "mouseGestureViewTerminate":
        MouseGestureView.terminate();
      break;

      case "matchingGesture":
        MouseGestureView.updateGestureCommand(message.data);
      break;
    }
  });
}

// define wheel and rocker gesture controller event listeners
// combine them to one function, since they all do the same except the subject they send to the background script

WheelGestureController.addEventListener("wheelup", event => handleRockerAndWheelEvents("wheelUp", event));
WheelGestureController.addEventListener("wheeldown", event => handleRockerAndWheelEvents("wheelDown", event));
RockerGestureController.addEventListener("rockerleft", event => handleRockerAndWheelEvents("rockerLeft", event));
RockerGestureController.addEventListener("rockerright", event => handleRockerAndWheelEvents("rockerRight", event));

function handleRockerAndWheelEvents (subject, event) {
  // expose target to global variable
  window.TARGET = event.target;

  // cancel mouse gesture and terminate overlay in case it got triggered
  MouseGestureController.cancel();
  // close overlay
  MouseGestureView.terminate();

  // gather specific data
  const data = getTargetData(window.TARGET);
        data.mousePosition = {
          x: event.clientX + window.mozInnerScreenX,
          y: event.clientY + window.mozInnerScreenY
        };
  // send data to background script
  browser.runtime.sendMessage({
    subject: subject,
    data: data
  });
}


/**
 * Main function
 * Applies the user config to the particular controller or interface
 * Enables or disables the appropriate controller
 **/
async function main () {
  await Config.loaded;

  // apply hidden settings
  if (Config.has("Settings.Gesture.patternDifferenceThreshold")) {
    patternConstructor.differenceThreshold = Config.get("Settings.Gesture.patternDifferenceThreshold");
  }
  if (Config.has("Settings.Gesture.patternDistanceThreshold")) {
    patternConstructor.distanceThreshold = Config.get("Settings.Gesture.patternDistanceThreshold");
  }

  // apply all settings
  MouseGestureController.mouseButton = Config.get("Settings.Gesture.mouseButton");
  MouseGestureController.suppressionKey = Config.get("Settings.Gesture.suppressionKey");
  MouseGestureController.distanceThreshold = Config.get("Settings.Gesture.distanceThreshold");
  MouseGestureController.timeoutActive = Config.get("Settings.Gesture.Timeout.active");
  MouseGestureController.timeoutDuration = Config.get("Settings.Gesture.Timeout.duration");

  WheelGestureController.mouseButton = Config.get("Settings.Wheel.mouseButton");
  WheelGestureController.wheelSensitivity = Config.get("Settings.Wheel.wheelSensitivity");

  MouseGestureView.gestureTraceLineColor = Config.get("Settings.Gesture.Trace.Style.strokeStyle");
  MouseGestureView.gestureTraceLineWidth = Config.get("Settings.Gesture.Trace.Style.lineWidth");
  MouseGestureView.gestureTraceLineGrowth = Config.get("Settings.Gesture.Trace.Style.lineGrowth");
  MouseGestureView.gestureCommandFontSize = Config.get("Settings.Gesture.Command.Style.fontSize");
  MouseGestureView.gestureCommandFontColor = Config.get("Settings.Gesture.Command.Style.fontColor");
  MouseGestureView.gestureCommandBackgroundColor = Config.get("Settings.Gesture.Command.Style.backgroundColor");
  MouseGestureView.gestureCommandHorizontalPosition = Config.get("Settings.Gesture.Command.Style.horizontalPosition");
  MouseGestureView.gestureCommandVerticalPosition = Config.get("Settings.Gesture.Command.Style.verticalPosition");

  PopupCommandView.theme = Config.get("Settings.General.theme");

  // check if current url is not listed in the exclusions
  if (!Config.get("Exclusions").some(matchesCurrentURL)) {
    // enable mouse gesture controller
    MouseGestureController.enable();

    // enable/disable rocker gesture
    if (Config.get("Settings.Rocker.active")) {
      RockerGestureController.enable();
    }
    else {
      RockerGestureController.disable();
    }

    // enable/disable wheel gesture
    if (Config.get("Settings.Wheel.active")) {
      WheelGestureController.enable();
    }
    else {
      WheelGestureController.disable();
    }
  }
  // if url is excluded disable everything
  else {
    MouseGestureController.disable();
    RockerGestureController.disable();
    WheelGestureController.disable();
  }
}


/**
 * checks if the given url is a subset of the current url or equal
 * NOTE: window.location.href is returning the frame URL for frames and not the tab URL
 **/
function matchesCurrentURL (urlPattern) {
	// match special regex characters
	const pattern = urlPattern.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, (match) => {
		// replace * with .* -> matches anything 0 or more times, else escape character
		return match === '*' ? '.*' : '\\'+match;
	});
	// ^ matches beginning of input and $ matches ending of input
	return new RegExp('^'+pattern+'$').test(window.location.href);
}
