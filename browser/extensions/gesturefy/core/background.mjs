import { displayNotification } from "/core/utils/commons.mjs";

import ConfigManager from "/core/helpers/config-manager.mjs";

import Gesture from "/core/models/gesture.mjs";

import Command from "/core/models/command.mjs";

import { getClosestGestureByPattern } from "/core/utils/matching-algorithms.mjs";

import "/core/helpers/message-router.mjs";

// temporary data migration
import "/core/migration.mjs";

const Config = new ConfigManager("local", browser.runtime.getURL("resources/json/defaults.json"));
      Config.autoUpdate = true;
      Config.loaded.then(updateVariablesOnConfigChange);
      Config.addEventListener("change", updateVariablesOnConfigChange);

const MouseGestures = new Set();

let RockerGestureLeft, RockerGestureRight, WheelGestureUp, WheelGestureDown;


/**
 * Updates the gesture objects and command objects on config changes
 **/
function updateVariablesOnConfigChange () {
  MouseGestures.clear();
  for (const gesture of Config.get("Gestures")) {
    MouseGestures.add(new Gesture(gesture));
  }

  RockerGestureLeft = new Command(Config.get("Settings.Rocker.leftMouseClick"));
  RockerGestureRight = new Command(Config.get("Settings.Rocker.rightMouseClick"));
  WheelGestureUp = new Command(Config.get("Settings.Wheel.wheelUp"));
  WheelGestureDown = new Command(Config.get("Settings.Wheel.wheelDown"));
  }


/**
 * Message handler - listens for the content tab script messages
 * mouse gesture:
 * on gesture pattern change, respond gesture name
 * on gesture end, execute command
 * special gesture: execute related command
 **/
browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
  // message subject to handler mapping
  const messageHandler = {
    "gestureChange":          handleMouseGestureCommandResponse,
    "gestureEnd":             handleMouseGestureCommandExecution,

    "rockerLeft":             handleSpecialGestureCommandExecution,
    "rockerRight":            handleSpecialGestureCommandExecution,
    "wheelUp":                handleSpecialGestureCommandExecution,
    "wheelDown":              handleSpecialGestureCommandExecution
  }
  // call subject corresponding message handler if existing
  if (message.subject in messageHandler) messageHandler[message.subject](message, sender, sendResponse);
});


/**
 * Handles messages for gesture changes
 * Sends a response with the label of the best matching gesture
 * If the gesture exceeds the deviation tolerance an empty string will be send
 **/
function handleMouseGestureCommandResponse (message, sender, sendResponse) {
  const bestMatchingGesture = getClosestGestureByPattern(
    message.data,
    MouseGestures,
    Config.get("Settings.Gesture.deviationTolerance"),
    Config.get("Settings.Gesture.matchingAlgorithm")
  );

  // if the mismatch ratio exceeded the deviation tolerance bestMatchingGesture is null
  const gestureName = bestMatchingGesture?.toString();

  // send the matching gesture name if any
  sendResponse(gestureName);

  // if message was sent from a child frame also send a message to the top frame
  if (sender.frameId !== 0) browser.tabs.sendMessage(
    sender.tab.id,
    { subject: "matchingGesture", data: gestureName },
    { frameId: 0 }
  );
}


/**
 * Handles messages for gesture end
 * Executes the command of the best matching gesture if it does not exceed the deviation tolerance
 * Passes the sender and source data to the executed command
 **/
function handleMouseGestureCommandExecution (message, sender, sendResponse) {
  const bestMatchingGesture = getClosestGestureByPattern(
    message.data.pattern,
    MouseGestures,
    Config.get("Settings.Gesture.deviationTolerance"),
    Config.get("Settings.Gesture.matchingAlgorithm")
  );

  if (bestMatchingGesture) {
    const command = bestMatchingGesture.getCommand();
    // run command, apply the current sender object, pass the source data
    command.execute(sender, message.data);
  }
}


/**
 * Handles messages for rocker and wheel gestures
 * Executes the command of the corresponding wheel or rocker gesture
 * Passes the sender and source data to the executed command
 **/
function handleSpecialGestureCommandExecution (message, sender, sendResponse) {
  // run command, pass the sender and source data
  switch (message.subject) {
    case "rockerLeft":
      RockerGestureLeft.execute(sender, message.data); break;
    case "rockerRight":
      RockerGestureRight.execute(sender, message.data); break;
    case "wheelUp":
      WheelGestureUp.execute(sender, message.data); break;
    case "wheelDown":
      WheelGestureDown.execute(sender, message.data); break;
  }
}


/**
 * This is used to simplify background script API calls from content scripts
 * Required for user script API calls
 **/
browser.runtime.onMessage.addListener((message, sender, sendResponse) => {
  if (message.subject === "backgroundScriptAPICall") {
    try {
      // call a background script api function by its given namespace, function name and parameters.
      // return the function promise so the message sender receives its value on resolve
      return browser[message.data.nameSpace][message.data.functionName](...message.data.parameter);
    }
    catch (error) {
      console.warn("Unsupported call to background script API.", error);
    }
  }
});


/**
 * Handle browser action click
 * Open Gesturefy options page
 **/
browser.browserAction.onClicked.addListener(() => {
  browser.runtime.openOptionsPage();
});


/**
 * Listen for addon installation and update
 * Show onboarding page on installation
 * Display notification and show github releases changelog on click
 **/
browser.runtime.onInstalled.addListener((details) => {
  // enable context menu on mouseup
  try {
    browser.browserSettings.contextMenuShowEvent.set({value: "mouseup"});
  }
  catch (error) {
    console.warn("Gesturefy was not able to change the context menu behaviour to mouseup.", error);
  }

  // run this code after the config is loaded
  Config.loaded.then(() => {
  });
});