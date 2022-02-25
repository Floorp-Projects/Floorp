'use strict';

/**
 * get JSON file as object from url
 * returns a promise which is fulfilled with the json object as a parameter
 * otherwise it's rejected
 * request url needs permissions in the addon manifest
 **/


/**
 * check if variable is an object
 * from https://stackoverflow.com/a/37164538/3771196
 **/
function isObject (item) {
  return (item && typeof item === 'object' && !Array.isArray(item));
}


/**
 * clone a serializable javascript object
 **/
function cloneObject (obj) {
  return JSON.parse(JSON.stringify(obj));
}


/**
 * calculates and returns the distance
 * between to points
 **/
function getDistance(x1, y1, x2, y2) {
  return Math.hypot(x2 - x1, y2 - y1);
}


/**
 * converts a pressed button value to its trigger button equivalent
 * returns -1 if the value cannot be converted
 **/
function toSingleButton (pressedButton) {
  if (pressedButton === 1) return 0;
  else if (pressedButton === 2) return 2;
  else if (pressedButton === 4) return 1;
  else return -1;
}


/**
 * returns the selected text, if no text is selected it will return an empty string
 * inspired by https://stackoverflow.com/a/5379408/3771196
 **/
function getTextSelection () {
  // get input/textfield text selection
  if (document.activeElement &&
      typeof document.activeElement.selectionStart === 'number' &&
      typeof document.activeElement.selectionEnd === 'number') {
        return document.activeElement.value.slice(
          document.activeElement.selectionStart,
          document.activeElement.selectionEnd
        );
  }
  // get normal text selection
  return window.getSelection().toString();
}


/**
 * returns all available data of the given target
 * this data is necessary for some commands
 **/
function getTargetData (target) {
	const data = {};

	data.target = {
		src: target.currentSrc || target.src || null,
		title: target.title || null,
		alt: target.alt || null,
		textContent: target.textContent && target.textContent.trim(),
		nodeName: target.nodeName
  };

  // get closest link
  const link = target.closest("a, area");
	if (link) {
		data.link = {
			href: link.href || null,
			title: link.title || null,
			textContent: link.textContent && link.textContent.trim()
		};
	}

	data.textSelection = getTextSelection();

	return data;
}


/**
 * returns the closest html parent element that matches the conditions of the provided test function or null
 **/
function getClosestElement (startNode, testFunction) {
  let node = startNode;
	while (node !== null && !testFunction(node)) {
    node = node.parentElement;
  }
	return node;
}


/**
 * Smooth scrolling to a given y position
 * duration: scroll duration in milliseconds; default is 0 (no transition)
 * element: the html element that should be scrolled; default is the main scrolling element
 **/
function scrollToY (y, duration = 0, element = document.scrollingElement) {
	// clamp y position between 0 and max scroll position
  y = Math.max(0, Math.min(element.scrollHeight - element.clientHeight, y));

  // cancel if already on target position
  if (element.scrollTop === y) return;

  const cosParameter = (element.scrollTop - y) / 2;
  let scrollCount = 0, oldTimestamp = null;

  function step (newTimestamp) {
    if (oldTimestamp !== null) {
      // if duration is 0 scrollCount will be Infinity
      scrollCount += Math.PI * (newTimestamp - oldTimestamp) / duration;
      if (scrollCount >= Math.PI) return element.scrollTop = y;
      element.scrollTop = cosParameter + y + cosParameter * Math.cos(scrollCount);
    }
    oldTimestamp = newTimestamp;
    window.requestAnimationFrame(step);
  }
  window.requestAnimationFrame(step);
}


/**
 * checks if the current window is framed or not
 **/
function isEmbeddedFrame () {
  try {
    return window.self !== window.top;
  }
  catch (e) {
    return true;
  }
}


/**
 * checks if an element has a vertical scrollbar
 **/
function isScrollableY (element) {
  const style = window.getComputedStyle(element);

  if (element.scrollHeight > element.clientHeight &&
      style["overflow"] !== "hidden" && style["overflow-y"] !== "hidden" &&
      style["overflow"] !== "clip" && style["overflow-y"] !== "clip")
  {
    if (element === document.scrollingElement) {
      return true;
    }
    // exception for textarea elements
    else if (element.tagName.toLowerCase() === "textarea") {
      return true;
    }
    else if (style["overflow"] !== "visible" && style["overflow-y"] !== "visible") {
      // special check for body element (https://drafts.csswg.org/cssom-view/#potentially-scrollable)
      if (element === document.body) {
        const parentStyle = window.getComputedStyle(element.parentElement);
        if (parentStyle["overflow"] !== "visible" && parentStyle["overflow-y"] !== "visible" &&
            parentStyle["overflow"] !== "clip" && parentStyle["overflow-y"] !== "clip")
        {
          return true;
        }
      }
      // normal elements with display inline can never be scrolled
      else if (style["display"] !== "inline") return true;
    }
  }

  return false;
}


/**
 * checks if the given element is a writable input element
 **/
function isEditableInput (element) {
  const editableInputTypes = ["text", "textarea", "password", "email", "number", "tel", "url", "search"];
  return (
    (element.tagName === 'INPUT' || element.tagName === 'TEXTAREA')
    && (!element.type || editableInputTypes.includes(element.type))
    && !element.disabled
    && !element.readOnly
  );
}


/**
 * Returns the direction difference of 2 vectors
 * Range: (-1, 0, 1]
 * 0 = same direction
 * 1 = opposite direction
 * + and - indicate if the direction difference is counter clockwise (+) or clockwise (-)
 **/
function vectorDirectionDifference (V1X, V1Y, V2X, V2Y) {
  // calculate the difference of the vectors angle
  let angleDifference = Math.atan2(V1X, V1Y) - Math.atan2(V2X, V2Y);
  // normalize interval to [PI, -PI)
  if (angleDifference > Math.PI) angleDifference -= 2 * Math.PI;
  else if (angleDifference <= -Math.PI) angleDifference += 2 * Math.PI;
  // shift range from [PI, -PI) to [1, -1)
  return angleDifference / Math.PI;
}

/**
 * This class is a wrapper of the native storage API in order to allow synchronous config calls.
 * It also allows loading an optional default configuration which serves as a fallback if the property isn't stored in the user configuration.
 * The config manager should only be used after the config has been loaded.
 * This can be checked via the Promise returned by ConfigManagerInstance.loaded property.
 **/
class ConfigManager {

  /**
   * The constructor of the class requires a storage area as a string.
   * For the first parameter either "local" or "sync" is allowed.
   * An URL to a JSON formatted file can be passed optionally. The containing properties will be treated as the defaults.
   **/
  constructor (storageArea, defaultsURL) {
    if (storageArea !== "local" && storageArea !== "sync") {
      throw "The first argument must be a storage area in form of a string containing either local or sync.";
    }
    if (typeof defaultsURL !== "string" && defaultsURL !== undefined) {
      throw "The second argument must be an URL to a JSON file.";
    }

    this._storageArea = storageArea;
    // empty object as default value so the config doesn't have to be loaded
    this._storage = {};
    this._defaults = {};

    const fetchResources = [ browser.storage[this._storageArea].get() ];
    if (typeof defaultsURL === "string") {
      const defaultsObject = new Promise((resolve, reject) => {
        fetch(defaultsURL, {mode:'same-origin'})
          .then(res => res.json())
          .then(obj => resolve(obj), err => reject(err));
       });
      fetchResources.push( defaultsObject );
    }
    // load resources
    this._loaded = Promise.all(fetchResources);
    // store resources when loaded
    this._loaded.then((values) => {
      if (values[0]) this._storage = values[0];
      if (values[1]) this._defaults = values[1];
    });

    // holds all custom event callbacks
    this._events = {
      'change': new Set()
    };
    // defines if the storage should be automatically loaded und updated on storage changes
    this._autoUpdate = false;
    // setup on storage change handler
    browser.storage.onChanged.addListener((changes, areaName) => {
      if (areaName === this._storageArea) {
        // automatically update config if defined
        if (this._autoUpdate === true) {
          for (let property in changes) {
            this._storage[property] = changes[property].newValue;
          }
        }
        // execute event callbacks
        this._events["change"].forEach((callback) => callback(changes));
      }
    });
  }


  /**
   * Expose the "is loaded" Promise
   * This enables the programmer to check if the config has been loaded and run code on load
   * get, set, remove calls should generally called after the config has been loaded otherwise they'll have no effect or return undefined
   **/
  get loaded () {
    return this._loaded;
  }


  /**
   * Returns the value of the given storage path
   * A Storage path is constructed of one or more nested JSON keys concatenated with dots or an array of nested JSON keys
   * If the storage path is left the current storage object is returned
   * If the storage path does not exist in the config or the function is called before the config has been loaded it will return undefined
   **/
  get (storagePath = []) {
    if (typeof storagePath === "string") storagePath = storagePath.split('.');
    else if (!Array.isArray(storagePath)) {
      throw "The first argument must be a storage path either in the form of an array or a string concatenated with dots.";
    }

    const pathWalker = (obj, key) => isObject(obj) ? obj[key] : undefined;
    let entry = storagePath.reduce(pathWalker, this._storage);
    // try to get the default value
    if (entry === undefined) entry = storagePath.reduce(pathWalker, this._defaults);
    if (entry !== undefined) return cloneObject(entry);

    return undefined;
  }


  /**
   * Returns true if the the given storage path exists else false
   * If the storage path is left the current storage object will be used
   * If is called before the config has been loaded it will return false
   **/
   has (storagePath = []) {
    return typeof this.get(storagePath) !== "undefined";
  }


  /**
   * Sets the value of a given storage path and creates the JSON keys if not available
   * If only one value of type object is passed the object keys will be stored in the config and existing keys will be overridden
   * Returns the storage set promise which resolves when the storage has been written successfully
   **/
  set (storagePath, value) {
    if (typeof storagePath === "string") storagePath = storagePath.split('.');
    // if only one argument is given and it is an object use this as the new config and override the old one
    else if (arguments.length === 1 && isObject(arguments[0])) {
      this._storage = cloneObject(arguments[0]);
      return browser.storage[this._storageArea].set(this._storage);
    }
    else if (!Array.isArray(storagePath)) {
      throw "The first argument must be a storage path either in the form of an array or a string concatenated with dots.";
    }

    if (storagePath.length > 0) {
      let entry = this._storage;
      const lastIndex = storagePath.length - 1;

      for (let i = 0; i < lastIndex; i++) {
        const key = storagePath[i];
        if (!entry.hasOwnProperty(key) || !isObject(entry[key])) {
          entry[key] = {};
        }
        entry = entry[key];
      }
      entry[ storagePath[lastIndex] ] = cloneObject(value);
      // save to storage
      return browser.storage[this._storageArea].set(this._storage);
    }
  }


  /**
   * Removes the key and value of a given storage path
   * Default values will not be removed, so get() may still return a default value even if removed was called before
   * Returns the storage set promise which resolves when the storage has been written successfully
   **/
  remove (storagePath) {
    if (typeof storagePath === "string") storagePath = storagePath.split('.');
    else if (!Array.isArray(storagePath)) {
      throw "The first argument must be a storage path either in the form of an array or a string concatenated with dots.";
    }

    if (storagePath.length > 0) {
      let entry = this._storage;
      const lastIndex = storagePath.length - 1;

      for (let i = 0; i < lastIndex; i++) {
        const key = storagePath[i];
        if (entry.hasOwnProperty(key) && isObject(entry[key])) {
          entry = entry[key];
        }
        else return;
      }
      delete entry[ storagePath[lastIndex] ];
      // remove single config item
      if (storagePath.length === 1) {
        return browser.storage[this._storageArea].remove(storagePath[0]);
      }
      // overwrite entire config
      return browser.storage[this._storageArea].set(this._storage);
    }
  }


  /**
   * Clears the entire config
   * If a default config is specified this is equal to resetting the config
   * Returns the storage clear promise which resolves when the storage has been written successfully
   **/
  clear () {
    this._storage = {};
    return browser.storage[this._storageArea].clear();
  }


  /**
   * Adds an event listener
   * Requires an event specifier as a string and a callback method
   * Current events are:
   * "change" - Fires when the storage has been changed
   **/
  addEventListener (event, callback) {
    if (!this._events.hasOwnProperty(event)) {
      throw "The first argument is not a valid event.";
    }
    if (typeof callback !== "function") {
      throw "The second argument must be a function.";
    }
    this._events[event].add(callback);
  }


  /**
   * Checks if an event listener exists
   * Requires an event specifier as a string and a callback method
   **/
  hasEventListener (event, callback) {
    if (!this._events.hasOwnProperty(event)) {
      throw "The first argument is not a valid event.";
    }
    if (typeof callback !== "function") {
      throw "The second argument must be a function.";
    }
    this._events[event].has(callback);
  }


  /**
   * Removes an event listener
   * Requires an event specifier as a string and a callback method
   **/
  removeEventListener (event, callback) {
    if (!this._events.hasOwnProperty(event)) {
      throw "The first argument is not a valid event.";
    }
    if (typeof callback !== "function") {
      throw "The second argument must be a function.";
    }
    this._events[event].delete(callback);
  }


  /**
   * Setter for the autoUpdate value
   * If autoUpdate is set to true the cached config will automatically update itself on storage changes
   **/
  set autoUpdate (value) {
    this._autoUpdate = Boolean(value);
  }


  /**
   * Getter for the autoUpdate value
   * If autoUpdate is set to true the cached config will automatically update itself on storage changes
   **/
  get autoUpdate () {
    return this._autoUpdate;
  }
}

// global static variables

const LEFT_MOUSE_BUTTON$2 = 1;
const RIGHT_MOUSE_BUTTON$2 = 2;
const MIDDLE_MOUSE_BUTTON$1 = 4;

const PASSIVE = 0;
const PENDING = 1;
const ACTIVE = 2;
const ABORTED = 3;


/**
 * MouseGestureController "singleton"
 * provides 4 events: on start, update, abort and end
 * events can be added via addEventListener and removed via removeEventListener
 * on default the controller is disabled and must be enabled via enable()
 * cancel() can be called to reset the controller
 **/

// public methods and variables


var MouseGestureController = {
  enable: enable$2,
  disable: disable$2,
  cancel: cancel,
  addEventListener: addEventListener$2,
  hasEventListener: hasEventListener$2,
  removeEventListener: removeEventListener$2,

  get targetElement () {
    return targetElement$2;
  },
  set targetElement (value) {
    targetElement$2 = value;
  },

  get mouseButton () {
    return mouseButton$1;
  },
  set mouseButton (value) {
    mouseButton$1 = Number(value);
  },

  get suppressionKey () {
    return suppressionKey;
  },
  set suppressionKey (value) {
    suppressionKey = value;
  },

  get distanceThreshold () {
    return distanceThreshold;
  },
  set distanceThreshold (value) {
    distanceThreshold = Number(value);
  },

  get timeoutActive () {
    return timeoutActive;
  },
  set timeoutActive (value) {
    timeoutActive = Boolean(value);
  },

  get timeoutDuration () {
    // convert milliseconds back to seconds
    return timeoutDuration / 1000;
  },
  set timeoutDuration (value) {
    // convert seconds to milliseconds
    timeoutDuration = Number(value) * 1000;
  },
};


/**
 * Add callbacks to the given events
 **/
function addEventListener$2 (event, callback) {
  // if event exists add listener (duplicates won't be added)
  if (event in events$2) events$2[event].add(callback);
}

/**
 * Check if an event listener is registered
 **/
function hasEventListener$2 (event, callback) {
  // if event exists check for listener
  if (event in events$2) events$2[event].has(callback);
}

/**
 * Remove callbacks from the given events
 **/
function removeEventListener$2 (event, callback) {
  // if event exists remove listener
  if (event in events$2) events$2[event].delete(callback);
}

/**
 * Add the event listeners to detect a gesture start
 **/
function enable$2 () {
  targetElement$2.addEventListener('pointerdown', handlePointerdown, true);
}

/**
 * Remove the event listeners and resets the controller
 **/
function disable$2 () {
  targetElement$2.removeEventListener('pointerdown', handlePointerdown, true);

  // reset to initial state
  reset();
}


/**
 * Cancel the gesture controller and reset its state
 **/
function cancel () {
  // reset to initial state
  reset();
}

// private variables and methods


// internal states are PASSIVE, PENDING, ACTIVE, ABORTED
let state = PASSIVE;

// contains the timeout identifier
let timeoutId = null;

// holds all custom module event callbacks
const events$2 = {
  'start': new Set(),
  'update': new Set(),
  'abort': new Set(),
  'end': new Set()
};

// temporary buffer for occurred mouse events where the latest event is always at the end of the array
let mouseEventBuffer = [];

let targetElement$2 = window,
    mouseButton$1 = RIGHT_MOUSE_BUTTON$2,
    suppressionKey = "",
    distanceThreshold = 10,
    timeoutActive = false,
    timeoutDuration = 1000;

/**
 * Initializes the gesture controller to the "pending" state, where it's unclear if the user is starting a gesture or not
 * requires a mouse/pointer event
 **/
function initialize$1 (event) {
  // buffer initial mouse event
  mouseEventBuffer.push(event);

  // change internal state
  state = PENDING;

  // add gesture detection listeners
  targetElement$2.addEventListener('pointermove', handlePointermove, true);
  targetElement$2.addEventListener('dragstart', handleDragstart, true);
  targetElement$2.addEventListener('pointerup', handlePointerup, true);
  targetElement$2.addEventListener('visibilitychange', handleVisibilitychange$2, true);

  // workaround to redirect all events to this frame/element
  // don't redirect them to the root element yet since it's unclear if the user wants to perform a gesture
  event.target.setPointerCapture(event.pointerId);
}


/**
 * Indicates the gesture start and update and should be called every time the cursor position changes
 * start - will be called once after the distance threshold has been exceeded
 * update - will be called afterwards for every pointer move
 * requires a mouse/pointer event
 **/
function update (event) {
  // buffer mouse event
  mouseEventBuffer.push(event);

  // needs to be called to prevent the values of the coalesced events from getting cleared (probably a Firefox bug)
  event.getCoalescedEvents();

  // initiate gesture
  if (state === PENDING) {
    // get the initial and latest event
    const initialEvent = mouseEventBuffer[0];
    const latestEvent = mouseEventBuffer[mouseEventBuffer.length - 1];
    // check if the distance between the initial pointer and the latest pointer is greater than the threshold
    if (getDistance(initialEvent.clientX, initialEvent.clientY, latestEvent.clientX, latestEvent.clientY) > distanceThreshold) {
      // dispatch all bound functions on start and pass the initial event and an array of the buffered mouse events
      events$2['start'].forEach(callback => callback(initialEvent, mouseEventBuffer));

      // change internal state
      state = ACTIVE;

      preparePreventDefault();

      // workaround to redirect all events to this frame/element
      document.documentElement.setPointerCapture(event.pointerId);
    }
  }

  // update gesture
  else if (state === ACTIVE) {
    // dispatch all bound functions on update and pass the latest event and an array of the buffered mouse events
    events$2['update'].forEach(callback => callback(event, mouseEventBuffer));

    // handle timeout
    if (timeoutActive) {
      // clear previous timeout if existing
      if (timeoutId) window.clearTimeout(timeoutId);
      timeoutId = window.setTimeout(abort, timeoutDuration);
    }
  }
}


/**
 * Indicates the gesture abortion and sets the state to aborted
 **/
function abort () {
  // dispatch all bound functions on timeout and pass an array of buffered mouse events
  events$2['abort'].forEach(callback => callback(mouseEventBuffer));
  state = ABORTED;
}


/**
 * Indicates the gesture end and should be called to terminate the gesture
 * requires a mouse/pointer event
 **/
function terminate$1 (event) {
  // buffer mouse event
  mouseEventBuffer.push(event);

  if (state === ACTIVE) {
    // dispatch all bound functions on end and pass the latest event and an array of the buffered mouse events
    events$2['end'].forEach(callback => callback(event, mouseEventBuffer));
  }

  // reset gesture controller
  reset();
}


/**
 * Resets the controller to its initial state
 **/
function reset () {
  // remove gesture detection listeners
  targetElement$2.removeEventListener('pointermove', handlePointermove, true);
  targetElement$2.removeEventListener('pointerup', handlePointerup, true);
  targetElement$2.removeEventListener('dragstart', handleDragstart, true);
  targetElement$2.removeEventListener('visibilitychange', handleVisibilitychange$2, true);

  neglectPreventDefault();

  const firstMouseEvent = mouseEventBuffer[0];
  // release event redirect
  if (firstMouseEvent) {
    firstMouseEvent.target.releasePointerCapture(firstMouseEvent.pointerId);
    document.documentElement.releasePointerCapture(firstMouseEvent.pointerId);
  }

  // reset mouse event buffer and internal state
  mouseEventBuffer = [];
  state = PASSIVE;

  if (timeoutId !== null) {
    window.clearTimeout(timeoutId);
    timeoutId = null;
  }
}


/**
 * Handles pointerdown which will initialize the gesture and switch to the pending state.
 * This will only be called for the first mouse button any subsequent mouse bu
 * This means if the user holds a non-trigger button and then presses the trigger button,
 * no pointerdown event will be dispatched and thus no gesture will be started
 **/
function handlePointerdown (event) {
  // on mouse button and no suppression key
  if (event.isTrusted && event.buttons === mouseButton$1 && (!suppressionKey || !event[suppressionKey])) {
    initialize$1(event);

    // prevent middle click scroll
    if (mouseButton$1 === MIDDLE_MOUSE_BUTTON$1) event.preventDefault();
  }
}


/**
 * Handles pointermove which will either start the gesture or update it.
 * Pointermove event can better be understand as a pointerchange event.
 * This means it will fire even when the mouse does not move but an additional button is pressed.
 **/
function handlePointermove (event) {
  if (event.isTrusted) {
    if (event.buttons === mouseButton$1) {
      update(event);

      // prevent text selection
      if (mouseButton$1 === LEFT_MOUSE_BUTTON$2) window.getSelection().removeAllRanges();
    }
    // button: -1 means that no buttons changed since the last event
    // explicity check if a button changed to prevent https://github.com/Robbendebiene/Gesturefy/issues/622
    else if (event.button !== -1) {
      // a pointermove event triggered by the gesture mouse button means
      // it got released while another mouse button is still pressed.
      // in theory this should never happen because the gesture will be canceled when another mouse button is pressed
      if (event.button === toSingleButton(mouseButton$1)) {
        terminate$1(event);
      }
      // cancel the gesture if another mouse button was pressed
      else {
        abort();
      }
    }
  }
}


/**
 * Handles pointerup and terminates the gesture.
 * Pointerup is only fired if all pressed buttons of the mouse are released.
 **/
function handlePointerup (event) {
  if (event.isTrusted && event.button === toSingleButton(mouseButton$1)) {
    terminate$1(event);
  }
}


/**
 * Handles dragstart and prevents it if needed
 **/
function handleDragstart (event) {
  // prevent drag if mouse button and no suppression key is pressed
  if (event.isTrusted && event.buttons === mouseButton$1 && (!suppressionKey || !event[suppressionKey])) {
    event.preventDefault();
  }
}


/**
 * This is only needed for tab changing actions
 **/
function handleVisibilitychange$2() {
  // call abort to trigger attached events
  abort();
  // reset to initial state
  reset();
}




//////// WORKAROUND TO PROPERLY SUPPRESS CONTEXTMENU AND CLICK \\\\\\\\


const TIME_TO_WAIT_FOR_PREVENTION = 200;

let pendingPreventionTimeout = null;

let isTargetFrame = false;

browser.runtime.onMessage.addListener((message, sender) => {
  // filter messages if the mouse gesture controller runs in the options page (which is a background page)
  if (!sender.tab) {
    switch (message.subject) {
      case "mouseGestureControllerPreparePreventDefault":
        if (!isTargetFrame) enablePreventDefault();
      break;

      case "mouseGestureControllerNeglectPreventDefault":
        if (!isTargetFrame) {
          const elapsedTime = Date.now() - message.data.timestamp;
          // take elapsed time into account to ensure that the prevention is removed at the same time across frames
          pendingPreventionTimeout = window.setTimeout(disablePreventDefault, Math.max(TIME_TO_WAIT_FOR_PREVENTION - elapsedTime, 0));
        }
      break;
    }
  }
});


/**
 * Enables the prevention functions in every frame
 **/
function preparePreventDefault () {
  isTargetFrame = true;

  browser.runtime.sendMessage({
    subject: "mouseGestureControllerPreparePreventDefault"
  });

  enablePreventDefault();
}


/**
 * Disables the prevention functions in every frame after a short amount of time to give them time to prevent something if needed
 **/
function neglectPreventDefault () {
  isTargetFrame = true;

  browser.runtime.sendMessage({
    subject: "mouseGestureControllerNeglectPreventDefault",
    data: {
      timestamp: Date.now()
    }
  });

  // need to wait a specific time before we can be sure that nothing needs to be prevented
  pendingPreventionTimeout = window.setTimeout(disablePreventDefault, TIME_TO_WAIT_FOR_PREVENTION);
}


/**
 * Adds all event listeners that handle the prevention
 * Clears any existing prevention timeout, in case a new gesture was started but the previous prevention timeout is still running
 **/
function enablePreventDefault () {
  if (pendingPreventionTimeout !== null) {
    window.clearTimeout(pendingPreventionTimeout);
    pendingPreventionTimeout = null;
  }

  // prevent default behaviors and
  // stop propagation because a custom context menus might trigger on the contextmenu event for example

  // prevent the context menu for right mouse button
  targetElement$2.addEventListener('contextmenu', preventDefault$2, true);
  // prevent the left click from opening links or pressing buttons
  targetElement$2.addEventListener('click', preventDefault$2, true);
  // prevent the middle click from opening links, or clipboard pasting on linux
  targetElement$2.addEventListener('auxclick', preventDefault$2, true);
  // prevent clicking links
  targetElement$2.addEventListener('mouseup', preventDefault$2, true);
  // prevent focus of e.g. input fields
  targetElement$2.addEventListener('mousedown', preventDefault$2, true);
}


/**
 * Removed all event listeners that handle the prevention and resets necessary variables
 **/
function disablePreventDefault () {
  pendingPreventionTimeout = null;

  isTargetFrame = false;

  targetElement$2.removeEventListener('contextmenu', preventDefault$2, true);
  targetElement$2.removeEventListener('click', preventDefault$2, true);
  targetElement$2.removeEventListener('auxclick', preventDefault$2, true);
  targetElement$2.removeEventListener('mouseup', preventDefault$2, true);
  targetElement$2.removeEventListener('mousedown', preventDefault$2, true);
}


/**
 * Prevent the context menu for right mouse button
 **/
function preventDefault$2 (event) {
  if (event.isTrusted) {
    event.preventDefault();
    event.stopPropagation();
  }
}

// global static variables

const LEFT_MOUSE_BUTTON$1 = 1;
const RIGHT_MOUSE_BUTTON$1 = 2;

/**
 * RockerGestureController "singleton"
 * provides 2 events: on rockerleft and rockerright
 * events can be added via addEventListener and removed via removeEventListener
 * on default the controller is disabled and must be enabled via enable()
 **/


// public methods and variables


var RockerGestureController = {
  enable: enable$1,
  disable: disable$1,
  addEventListener: addEventListener$1,
  hasEventListener: hasEventListener$1,
  removeEventListener: removeEventListener$1,

  get targetElement () {
    return targetElement$1;
  },
  set targetElement (value) {
    targetElement$1 = value;
  }
};


/**
 * Add callbacks to the given events
 **/
function addEventListener$1 (event, callback) {
  // if event exists add listener (duplicates won't be added)
  if (event in events$1) events$1[event].add(callback);
}

/**
 * Check if an event listener is registered
 **/
function hasEventListener$1 (event, callback) {
  // if event exists check for listener
  if (event in events$1) events$1[event].has(callback);
}

/**
 * Remove callbacks from the given events
 **/
function removeEventListener$1 (event, callback) {
  // if event exists remove listener
  if (event in events$1) events$1[event].delete(callback);
}

/**
 * Add the event listeners to detect a gesture start
 **/
function enable$1 () {
  targetElement$1.addEventListener('mousedown', handleMousedown$1, true);
  targetElement$1.addEventListener('mouseup', handleMouseup$1, true);
  targetElement$1.addEventListener('click', handleClick$1, true);
  targetElement$1.addEventListener('contextmenu', handleContextmenu$1, true);
  targetElement$1.addEventListener('visibilitychange', handleVisibilitychange$1, true);
}

/**
 * Remove the event listeners and resets the controller
 **/
function disable$1 () {
  preventDefault$1 = true;
  targetElement$1.removeEventListener('mousedown', handleMousedown$1, true);
  targetElement$1.removeEventListener('mouseup', handleMouseup$1, true);
  targetElement$1.removeEventListener('click', handleClick$1, true);
  targetElement$1.removeEventListener('contextmenu', handleContextmenu$1, true);
  targetElement$1.removeEventListener('visibilitychange', handleVisibilitychange$1, true);
}

// private variables and methods

// holds all custom module event callbacks
const events$1 = {
  'rockerleft': new Set(),
  'rockerright': new Set()
};

let targetElement$1 = window;

// defines whether or not the click/contextmenu should be prevented
// keep preventDefault true for the special case that the contextmenu or click is fired without a previous mousedown
let preventDefault$1 = true;

// timestamp of the last mouseup
// this is needed to distinguish between true mouse click events and other click events fired by pressing enter or by clicking labels
let lastMouseup$1 = 0;


/**
 * Handles mousedown which will detect the target and handle prevention
 **/
function handleMousedown$1 (event) {
  if (event.isTrusted) {
    // always disable prevention on mousedown
    preventDefault$1 = false;

    if (event.buttons === LEFT_MOUSE_BUTTON$1 + RIGHT_MOUSE_BUTTON$1 && (event.button === toSingleButton(LEFT_MOUSE_BUTTON$1) || event.button === toSingleButton(RIGHT_MOUSE_BUTTON$1))) {
      // dispatch all bound functions on rocker and pass the appropriate event
      if (event.button === toSingleButton(LEFT_MOUSE_BUTTON$1)) events$1['rockerleft'].forEach((callback) => callback(event));
      else if (event.button === toSingleButton(RIGHT_MOUSE_BUTTON$1)) events$1['rockerright'].forEach((callback) => callback(event));

      event.stopPropagation();
      event.preventDefault();
      // enable prevention
      preventDefault$1 = true;
    }
  }
}


/**
 * This is only needed to distinguish between true mouse click events and other click events fired by pressing enter or by clicking labels
 * Other property values like screen position or target could be used in the same manner
 **/
function handleMouseup$1(event) {
  lastMouseup$1 = event.timeStamp;
}


/**
 * This is only needed for tab changing actions
 * Because the rocker gesture is executed in a different tab as where click/contextmenu needs to be prevented
 **/
function handleVisibilitychange$1() {
  // keep preventDefault true for the special case that the contextmenu or click is fired without a previous mousedown
  preventDefault$1 = true;
}


/**
 * Handles and prevents context menu if needed (right click)
 **/
function handleContextmenu$1 (event) {
  if (event.isTrusted && preventDefault$1 && event.button === toSingleButton(RIGHT_MOUSE_BUTTON$1)) {
    // prevent contextmenu
    event.stopPropagation();
    event.preventDefault();
  }
}


/**
 * Handles and prevents click event if needed (left click)
 **/
function handleClick$1 (event) {
  // event.detail because a click event can be fired without clicking (https://stackoverflow.com/questions/4763638/enter-triggers-button-click)
  // timeStamp check ensures that the click is fired by mouseup
  if (event.isTrusted && preventDefault$1 && event.button === toSingleButton(LEFT_MOUSE_BUTTON$1) && event.detail && event.timeStamp === lastMouseup$1) {
    // prevent click
    event.stopPropagation();
    event.preventDefault();
  }
}

// global static variables

const LEFT_MOUSE_BUTTON = 1;
const RIGHT_MOUSE_BUTTON = 2;
const MIDDLE_MOUSE_BUTTON = 4;

/**
 * WheelGestureController "singleton"
 * provides 2 events: on wheelup and wheeldown
 * events can be added via addEventListener and removed via removeEventListener
 * on default the controller is disabled and must be enabled via enable()
 **/


// public methods and variables


var WheelGestureController = {
  enable: enable,
  disable: disable,
  addEventListener: addEventListener,
  hasEventListener: hasEventListener,
  removeEventListener: removeEventListener,

  get targetElement () {
    return targetElement;
  },
  set targetElement (value) {
    targetElement = value;
  },

  get mouseButton () {
    return mouseButton;
  },
  set mouseButton (value) {
    mouseButton = Number(value);
  },

  get wheelSensitivity () {
    return wheelSensitivity;
  },
  set wheelSensitivity (value) {
    wheelSensitivity = Number(value);
  }
};


/**
 * Add callbacks to the given events
 **/
function addEventListener (event, callback) {
  // if event exists add listener (duplicates won't be added)
  if (event in events) events[event].add(callback);
}

/**
 * Check if an event listener is registered
 **/
function hasEventListener (event, callback) {
  // if event exists check for listener
  if (event in events) events[event].has(callback);
}

/**
 * Remove callbacks from the given events
 **/
function removeEventListener (event, callback) {
  // if event exists remove listener
  if (event in events) events[event].delete(callback);
}

/**
 * Add the document event listener
 **/
function enable () {
  targetElement.addEventListener('wheel', handleWheel, {capture: true, passive: false});
  targetElement.addEventListener('mousedown', handleMousedown, true);
  targetElement.addEventListener('mouseup', handleMouseup, true);
  targetElement.addEventListener('click', handleClick, true);
  targetElement.addEventListener('contextmenu', handleContextmenu, true);
  targetElement.addEventListener('visibilitychange', handleVisibilitychange, true);
}

/**
 * Remove the event listeners and resets the handler
 **/
function disable () {
  preventDefault = true;
  targetElement.removeEventListener('wheel', handleWheel, {capture: true, passive: false});
  targetElement.removeEventListener('mousedown', handleMousedown, true);
  targetElement.removeEventListener('mouseup', handleMouseup, true);
  targetElement.removeEventListener('click', handleClick, true);
  targetElement.removeEventListener('contextmenu', handleContextmenu, true);
  targetElement.removeEventListener('visibilitychange', handleVisibilitychange, true);
}

// private variables and methods

// holds all custom module event callbacks
const events = {
  'wheelup': new Set(),
  'wheeldown': new Set()
};

let targetElement = window,
    mouseButton = LEFT_MOUSE_BUTTON,
    wheelSensitivity = 2;

// keep preventDefault true for the special case that the contextmenu or click is fired without a previous mousedown
let preventDefault = true;

let lastMouseup = 0;

let accumulatedDeltaY = 0;


/**
 * Handles mousedown which will detect the target and handle prevention
 **/
function handleMousedown (event) {
  if (event.isTrusted) {
    // always disable prevention on mousedown
    preventDefault = false;

    // always reset the accumulated detlaY
    accumulatedDeltaY = 0;

    // prevent middle click scroll
    if (mouseButton === MIDDLE_MOUSE_BUTTON && event.buttons === MIDDLE_MOUSE_BUTTON) event.preventDefault();
  }
}


/**
 * Handles mousewheel up and down and prevents scrolling if needed
 **/
function handleWheel (event) {
  if (event.isTrusted && event.buttons === mouseButton && event.deltaY !== 0) {

    // check if the sign is different and reset the accumulated value
    if ((accumulatedDeltaY < 0) !== (event.deltaY < 0)) accumulatedDeltaY = 0;

    accumulatedDeltaY += event.deltaY;

    if (Math.abs(accumulatedDeltaY) >= wheelSensitivity) {
      // dispatch all bound functions on wheel up/down and pass the appropriate event
      if (accumulatedDeltaY < 0) {
        events['wheelup'].forEach((callback) => callback(event));
      }
      else if (accumulatedDeltaY > 0) {
        events['wheeldown'].forEach((callback) => callback(event));
      }

      // reset accumulated deltaY if it reaches the sensitivity value
      accumulatedDeltaY = 0;
    }

    event.stopPropagation();
    event.preventDefault();
    // enable prevention
    preventDefault = true;
  }
}


/**
 * This is only needed to distinguish between true mouse click events and other click events fired by pressing enter or by clicking labels
 * Other property values like screen position or target could be used in the same manner
 **/
function handleMouseup(event) {
  lastMouseup = event.timeStamp;
}


/**
 * This is only needed for tab changing actions
 * Because the wheel gesture is executed in a different tab as where click/contextmenu needs to be prevented
 **/
function handleVisibilitychange() {
  // keep preventDefault true for the special case that the contextmenu or click is fired without a previous mousedown
  preventDefault = true;
  // always reset the accumulated detlaY
  accumulatedDeltaY = 0;
}


/**
 * Handles and prevents context menu if needed
 **/
function handleContextmenu (event) {
  if (event.isTrusted && preventDefault && event.button === toSingleButton(mouseButton) && mouseButton === RIGHT_MOUSE_BUTTON) {
    // prevent contextmenu
    event.stopPropagation();
    event.preventDefault();
  }
}


/**
 * Handles and prevents click event if needed
 **/
function handleClick (event) {
  // event.detail because a click event can be fired without clicking (https://stackoverflow.com/questions/4763638/enter-triggers-button-click)
  // timeStamp check ensures that the click is fired by mouseup
  if (event.isTrusted && preventDefault && event.button === toSingleButton(mouseButton) && (mouseButton === LEFT_MOUSE_BUTTON || mouseButton === MIDDLE_MOUSE_BUTTON) && event.detail && event.timeStamp === lastMouseup) {
    // prevent left and middle click
    event.stopPropagation();
    event.preventDefault();
  }
}

/**
 * Helper class to create a pattern alongside mouse movement from points
 * A Pattern is a combination/array of 2D Vectors while each Vector is an array
 **/
class PatternConstructor {

  constructor (differenceThreshold = 0, distanceThreshold = 0) {
    this.differenceThreshold = differenceThreshold;
    this.distanceThreshold = distanceThreshold;

    this._lastExtractedPointX = null;
    this._lastExtractedPointY = null;
    this._previousPointX = null;
    this._previousPointY = null;
    this._lastPointX = null;
    this._lastPointY = null;
    this._previousVectorX = null;
    this._previousVectorY = null;

    this._extractedVectors = [];
  }

  /**
   * Resets the internal class variables and clears the constructed pattern
   **/
  clear () {
    // clear extracted vectors
    this._extractedVectors.length = 0;
    // reset variables
    this._lastExtractedPointX = null;
    this._lastExtractedPointY = null;
    this._previousPointX = null;
    this._previousPointY = null;
    this._lastPointX = null;
    this._lastPointY = null;
    this._previousVectorX = null;
    this._previousVectorY = null;
  }


  /**
   * Add a point to the constructor
   * Returns an integer value:
   * 1 if the added point passed the distance threshold [PASSED_DISTANCE_THRESHOLD]
   * 2 if the added point also passed the difference threshold [PASSED_DIFFERENCE_THRESHOLD]
   * else 0 [PASSED_NO_THRESHOLD]
   **/
  addPoint (x, y) {
    // return variable
    let changeIndicator = 0;
    // on first point / if no previous point exists
    if (this._previousPointX === null || this._previousPointY === null) {
      // set first extracted point
      this._lastExtractedPointX = x;
      this._lastExtractedPointY = y;
      // set previous point to first point
      this._previousPointX = x;
      this._previousPointY = y;
    }

    else {
      const newVX = x - this._previousPointX;
      const newVY = y - this._previousPointY;

      const vectorDistance = Math.hypot(newVX, newVY);
      if (vectorDistance > this.distanceThreshold) {
        // on second point / if no previous vector exists
        if (this._previousVectorX === null || this._previousVectorY === null) {
          // store previous vector
          this._previousVectorX = newVX;
          this._previousVectorY = newVY;
        }
        else {
          // calculate vector difference
          const vectorDifference = vectorDirectionDifference(this._previousVectorX, this._previousVectorY, newVX, newVY);
          if (Math.abs(vectorDifference) > this.differenceThreshold) {
            // store new extracted vector
            this._extractedVectors.push([
              this._previousPointX - this._lastExtractedPointX,
              this._previousPointY - this._lastExtractedPointY
            ]);
            // update previous vector
            this._previousVectorX = newVX;
            this._previousVectorY = newVY;
            // update last extracted point
            this._lastExtractedPointX = this._previousPointX;
            this._lastExtractedPointY = this._previousPointY;
            // update change variable
            changeIndicator++;
          }
        }
        // update previous point
        this._previousPointX = x;
        this._previousPointY = y;
        // update change variable
        changeIndicator++;
      }
    }
    // always store the last point
    this._lastPointX = x;
    this._lastPointY = y;

    return changeIndicator;
  }


  /**
   * Returns the current constructed pattern
   * Adds the last added point as the current end point
   **/
  getPattern () {
    // check if variables contain point values
    if (this._lastPointX === null || this._lastPointY === null || this._lastExtractedPointX === null || this._lastExtractedPointY === null) {
      return [];
    }
    // calculate vector from last extracted point to ending point
    const lastVector = [
      this._lastPointX - this._lastExtractedPointX,
      this._lastPointY - this._lastExtractedPointY
    ];
    return [...this._extractedVectors, lastVector];
  }
}

// TODO: move these inside the class using the "static" keyword once eslint finally supports it
PatternConstructor.PASSED_NO_THRESHOLD = 0;

PatternConstructor.PASSED_DISTANCE_THRESHOLD = 1;

PatternConstructor.PASSED_DIFFERENCE_THRESHOLD = 2;

/**
 * MouseGestureView "singleton"
 * provides multiple functions to manipulate the overlay
 **/


// public methods and variables


var MouseGestureView = {
  initialize: initialize,
  updateGestureTrace: updateGestureTrace,
  updateGestureCommand: updateGestureCommand,
  terminate: terminate,

  // gesture Trace styles

  get gestureTraceLineColor () {
    const rgbHex = Context.fillStyle;
    const alpha = parseFloat(Canvas.style.getPropertyValue("opacity")) || 1;
    let aHex = Math.round(alpha * 255).toString(16);
    // add leading zero if string length is 1
    if (aHex.length === 1) aHex = "0" + aHex;
    return rgbHex + aHex;
  },
  set gestureTraceLineColor (value) {
    const rgbHex = value.substring(0, 7);
    const aHex = value.slice(7);
    const alpha = parseInt(aHex, 16)/255;
    Context.fillStyle = rgbHex;
    Canvas.style.setProperty("opacity", alpha, "important");
  },

  get gestureTraceLineWidth () {
    return gestureTraceLineWidth;
  },
  set gestureTraceLineWidth (value) {
    gestureTraceLineWidth = value;
  },

  get gestureTraceLineGrowth () {
    return gestureTraceLineGrowth;
  },
  set gestureTraceLineGrowth (value) {
    gestureTraceLineGrowth = Boolean(value);
  },

  // gesture command styles

  get gestureCommandFontSize () {
    return Command.style.getPropertyValue('font-size');
  },
  set gestureCommandFontSize (value) {
    Command.style.setProperty('font-size', value, 'important');
  },

  get gestureCommandFontColor () {
    return Command.style.getPropertyValue('color');
  },
  set gestureCommandFontColor (value) {
    Command.style.setProperty('color', value, 'important');
  },

  get gestureCommandBackgroundColor () {
    return Command.style.getPropertyValue('background-color');
  },
  set gestureCommandBackgroundColor (value) {
    Command.style.setProperty('background-color', value);
  },

  get gestureCommandHorizontalPosition () {
    return parseFloat(Command.style.getPropertyValue("--horizontalPosition"));
  },
  set gestureCommandHorizontalPosition (value) {
    Command.style.setProperty("--horizontalPosition", value);
  },

  get gestureCommandVerticalPosition () {
    return parseFloat(Command.style.getPropertyValue("--verticalPosition"));
  },
  set gestureCommandVerticalPosition (value) {
    Command.style.setProperty("--verticalPosition", value);
  }
};


/**
 * append overlay and start drawing the gesture
 **/
function initialize (x, y) {
  // overlay is not working in a pure svg or other xml pages thus do not append the overlay
  if (!document.body && document.documentElement.namespaceURI !== "http://www.w3.org/1999/xhtml") {
    return;
  }
  // if an element is in fullscreen mode and this element is not the document root (html element)
  // append the overlay to this element (issue #148)
  if (document.fullscreenElement && document.fullscreenElement !== document.documentElement) {
    document.fullscreenElement.appendChild(Overlay);
  }
  else if (document.body.tagName.toUpperCase() === "FRAMESET") {
    document.documentElement.appendChild(Overlay);
  }
  else {
    document.body.appendChild(Overlay);
  }
  // store starting point
  lastPoint.x = x;
  lastPoint.y = y;
}


/**
 * draw line for gesture
 */
function updateGestureTrace (points) {
  if (!Overlay.contains(Canvas)) Overlay.appendChild(Canvas);

  // temporary path in order draw all segments in one call
  const path = new Path2D();

  for (let point of points) {
    if (gestureTraceLineGrowth && lastTraceWidth < gestureTraceLineWidth) {
      // the length in pixels after which the line should be grown to its final width
      // in this case the length depends on the final width defined by the user
      const growthDistance = gestureTraceLineWidth * 50;
      // the distance from the last point to the current
      const distance = getDistance(lastPoint.x, lastPoint.y, point.x, point.y);
      // cap the line width by its final width value
      const currentTraceWidth = Math.min(
        lastTraceWidth + distance / growthDistance * gestureTraceLineWidth,
        gestureTraceLineWidth
      );
      const pathSegment = createGrowingLine(lastPoint.x, lastPoint.y, point.x, point.y, lastTraceWidth, currentTraceWidth);
      path.addPath(pathSegment);

      lastTraceWidth = currentTraceWidth;
    }
    else {
      const pathSegment = createGrowingLine(lastPoint.x, lastPoint.y, point.x, point.y, gestureTraceLineWidth, gestureTraceLineWidth);
      path.addPath(pathSegment);
    }

    lastPoint.x = point.x;
    lastPoint.y = point.y;
  }
  // draw accumulated path segments
  Context.fill(path);
}


/**
 * update command on match
 **/
function updateGestureCommand (command) {
  if (command && Overlay.isConnected) {
    Command.textContent = command;
    if (!Overlay.contains(Command)) Overlay.appendChild(Command);
  }
  else Command.remove();
}


/**
 * remove and reset overlay
 **/
function terminate () {
  Overlay.remove();
  Canvas.remove();
  Command.remove();
  // clear canvas
  Context.clearRect(0, 0, Canvas.width, Canvas.height);
  // reset trace line width
  lastTraceWidth = 0;
  Command.textContent = "";
}


// private variables and methods

// use HTML namespace so proper HTML elements will be created even in foreign doctypes/namespaces (issue #565)

const Overlay = document.createElementNS("http://www.w3.org/1999/xhtml", "div");
      Overlay.style = `
        all: initial !important;
        position: fixed !important;
        top: 0 !important;
        bottom: 0 !important;
        left: 0 !important;
        right: 0 !important;
        z-index: 2147483647 !important;

        pointer-events: none !important;
      `;

const Canvas = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
      Canvas.style = `
        all: initial !important;

        pointer-events: none !important;
      `;

const Context = Canvas.getContext('2d');

const Command = document.createElementNS("http://www.w3.org/1999/xhtml", "div");
      Command.style = `
        --horizontalPosition: 0;
        --verticalPosition: 0;
        all: initial !important;
        position: absolute !important;
        top: calc(var(--verticalPosition) * 1%) !important;
        left: calc(var(--horizontalPosition) * 1%) !important;
        transform: translate(calc(var(--horizontalPosition) * -1%), calc(var(--verticalPosition) * -1%)) !important;
        font-family: "NunitoSans Regular", "Arial", sans-serif !important;
        line-height: normal !important;
        text-shadow: 0.01em 0.01em 0.01em rgba(0,0,0, 0.5) !important;
        text-align: center !important;
        padding: 0.4em 0.4em 0.3em !important;
        font-weight: bold !important;
        background-color: rgba(0,0,0,0) !important;
        width: max-content !important;
        max-width: 50vw !important;

        pointer-events: none !important;
      `;


let gestureTraceLineWidth = 10,
    gestureTraceLineGrowth = true;


let lastTraceWidth = 0,
    lastPoint = { x: 0, y: 0 };


// resize canvas on window resize
window.addEventListener('resize', maximizeCanvas, true);
maximizeCanvas();


/**
 * Adjust the canvas size to the size of the window
 **/
function maximizeCanvas () {
  // save context properties, because they get cleared on canvas resize
  const tmpContext = {
    lineCap: Context.lineCap,
    lineJoin: Context.lineJoin,
    fillStyle: Context.fillStyle,
    strokeStyle: Context.strokeStyle,
    lineWidth: Context.lineWidth
  };

  Canvas.width = window.innerWidth;
  Canvas.height = window.innerHeight;

  // restore previous context properties
  Object.assign(Context, tmpContext);
}


/**
 * creates a growing line from a starting point and strike width to an end point and stroke width
 * returns a path 2d object
 **/
function createGrowingLine (x1, y1, x2, y2, startWidth, endWidth) {
  // calculate direction vector of point 1 and 2
  const directionVectorX = x2 - x1,
        directionVectorY = y2 - y1;
  // calculate angle of perpendicular vector
  const perpendicularVectorAngle = Math.atan2(directionVectorY, directionVectorX) + Math.PI/2;
  // construct shape
  const path = new Path2D();
        path.arc(x1, y1, startWidth/2, perpendicularVectorAngle, perpendicularVectorAngle + Math.PI);
        path.arc(x2, y2, endWidth/2, perpendicularVectorAngle + Math.PI, perpendicularVectorAngle);
        path.closePath();
  return path;
}

/**
 * PopupCommandView "singleton"
 * provides simple function to style the popup
 **/


// public methods and variables


var PopupCommandView = {
  get theme () {
    return PopupURL.searchParams.get("theme");
  },
  set theme (value) {
    PopupURL.searchParams.set("theme", value);
  }
};


// private variables and methods

// use HTML namespace so proper HTML elements will be created even in foreign doctypes/namespaces (issue #565)
const Popup = document.createElementNS("http://www.w3.org/1999/xhtml", "iframe");

const PopupURL = new URL(browser.runtime.getURL("/core/views/popup-command-view/popup-command-view.html"));

let mousePositionX = 0,
    mousePositionY = 0;

// setup background/command message event listener for top frame
if (!isEmbeddedFrame()) browser.runtime.onMessage.addListener(handleMessage);


/**
 * Handles background messages
 * Procedure:
 * 1. wait for message to create popup hiddenly
 * 2. wait for message from popup with list dimension and update popup size + show popup / remove opacity
 * 3. wait for popup close message
 **/
function handleMessage (message) {
  switch (message.subject) {
    case "popupRequest": return loadPopup(message.data);

    case "popupInitiation": return initiatePopup(message.data);

    case "popupTermination": return terminatePopup();
  }
}


/**
 * Creates and appends the Popup iframe hiddenly
 * Promise resolves to true if the Popup was created and loaded successfully else false
 **/
async function loadPopup (data) {
  // if popup is still appended to DOM
  if (Popup.isConnected) {
    // trigger the terminate event in the iframe/popup via blur
    // wait for the popup termination message and its removal by the terminatePopup function
    // otherwise the termination message / terminatePopup function may remove the newly created popup
    await new Promise((resolve) => {
      browser.runtime.onMessage.addListener((message) => {
        if (message.subject === "popupTermination") {
          resolve();
        }
      });
      Popup.blur();
    });
  }

  // popup is not working in a pure svg or other xml pages thus cancel the popup creation
  if (!document.body && document.documentElement.namespaceURI !== "http://www.w3.org/1999/xhtml") {
    return false;
  }

  Popup.style = `
      all: initial !important;
      position: fixed !important;
      top: 0 !important;
      left: 0 !important;
      border: 0 !important;
      z-index: 2147483647 !important;
      box-shadow: 1px 1px 4px rgba(0,0,0,0.3) !important;
      opacity: 0 !important;
      transition: opacity .3s !important;
      visibility: hidden !important;
    `;
  const popupLoaded = new Promise((resolve, reject) => {
    Popup.onload = resolve;
  });

  // calc and store correct mouse position
  mousePositionX = data.mousePositionX - window.mozInnerScreenX;
  mousePositionY = data.mousePositionY - window.mozInnerScreenY;

  // appending the element to the DOM will start loading the iframe content

  // if an element is in fullscreen mode and this element is not the document root (html element)
  // append the popup to this element (issue #148)
  if (document.fullscreenElement && document.fullscreenElement !== document.documentElement) {
    document.fullscreenElement.appendChild(Popup);
  }
  else if (document.body.tagName.toUpperCase() === "FRAMESET") {
    document.documentElement.appendChild(Popup);
  }
  else {
    document.body.appendChild(Popup);
  }

  // navigate iframe (from about:blank to extension page) via window.location instead of setting the src
  // this prevents UUID leakage and therefore reduces potential fingerprinting
  // see https://bugzilla.mozilla.org/show_bug.cgi?id=1372288#c25
  Popup.contentWindow.location = PopupURL.href;

  // return true when popup is loaded
  await popupLoaded;

  return true;
}


/**
 * Sizes, positions and fades in the popup
 * Returns a promise with the calculated available width and height
 **/
async function initiatePopup (data) {
  const relativeScreenWidth = document.documentElement.clientWidth || document.body.clientWidth || window.innerWidth;
  const relativeScreenHeight = document.documentElement.clientHeight || document.body.clientHeight || window.innerHeight;

  // get the absolute maximum available height from the current position either from the top or bottom
  const maxAvailableHeight = Math.max(relativeScreenHeight - mousePositionY, mousePositionY);

  // get absolute list dimensions
  const width = data.width;
  const height = Math.min(data.height, maxAvailableHeight);

  // calculate absolute available space to the right and bottom
  const availableSpaceRight = (relativeScreenWidth - mousePositionX);
  const availableSpaceBottom = (relativeScreenHeight - mousePositionY);

  // get the ideal relative position based on the given available space and dimensions
  const x = availableSpaceRight >= width ? mousePositionX : mousePositionX - width;
  const y = availableSpaceBottom >= height ? mousePositionY : mousePositionY - height;

  // apply scale, position, dimensions to Popup / iframe and make it visible
  Popup.style.setProperty('width', Math.round(width) + 'px', 'important');
  Popup.style.setProperty('height', Math.round(height) + 'px', 'important');
  Popup.style.setProperty('transform-origin', `0 0`, 'important');
  Popup.style.setProperty('transform', `translate(${Math.round(x)}px, ${Math.round(y)}px)`, 'important');
  Popup.style.setProperty('opacity', '1', 'important');
  Popup.style.setProperty('visibility','visible', 'important');

  return {
    width: width,
    height: height
  }
}


/**
 * Removes the popup from the DOM and resets all variables
 * Returns an empty promise
 **/
async function terminatePopup () {
  Popup.remove();
  // reset variables
  mousePositionX = 0;
  mousePositionY = 0;
}

/**
 * An observer that detects event listener removal triggered by the document.open function.
 * https://stackoverflow.com/questions/37176536/document-open-removes-all-window-listeners/68896987
 */
var ListenerObserver = {
  onDetach: {
    addListener: c => void callbacks.add(c),
    hasListener: c =>  callbacks.has(c),
    removeListener: c => callbacks.delete(c)
  }
};

const callbacks = new Set();
const eventName = "listenerStillAttached";

window.addEventListener(eventName, _handleListenerStillAttached);


/**
 * The method document.open removes/replaces the current document.
 * Therefore a MutationObserver is used to get notified when this happens.
 * After that we trigger both the custom event and the timeout.
 * Depending on who wins the "onDetach" callbacks will be called or not.
 */
new MutationObserver((entries) => {
  // check if a new document got attached
  const documentReplaced = entries.some(entry =>
    Array.from(entry.addedNodes).includes(document.documentElement)
  );
  if (documentReplaced) {
    const timeoutId = setTimeout(_handleListenerDetached);
    // test if previously registered event listener will still fire
    window.dispatchEvent(new CustomEvent(eventName, {detail: timeoutId}));
  }
}).observe(document, { childList: true });


/**
 * This function will only be called by the setTimeout method.
 * This happens after the event loop handled all queued event listeners.
 * If the timeout wasn't prevented/canceled from the event listener, the event listener must have been removed.
 * Therefore we call the "onDetach" callbacks and re-register the event listener.
 */
function _handleListenerDetached() {
  // reattach event listener
  window.addEventListener(eventName, _handleListenerStillAttached);
  // run registered callbacks
  callbacks.forEach((callback) => callback());
}


/**
 * This function will only be called by the event listener of the custom event.
 * If this is called the event listener is still attached which means apparently no event listener got removed.
 * Therefore we need to prevent the timeout from calling the "onDetach" callbacks.
 */
function _handleListenerStillAttached(event) {
  clearTimeout(event.detail);
}

/**
 * User Script Controller
 * helper to safely execute custom user scripts in the page context
 * will hopefully one day be replaced with https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/userScripts
 **/

//  This script is using content script specific functions like cloneInto so it should only be run as a content script (thus not in the installation page)
if (typeof cloneInto !== "function") console.warn("User scripts are disabled on this page.");

else {

// add the message event listener
browser.runtime.onMessage.addListener(handleMessage);


/**
 * Handles user script execution messages from the user script command
 **/

// Do not use an async method here because this would block every other message listener.
// Instead promises are returned when appropriate.
// See: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/Runtime/onMessage
function handleMessage (message, sender, sendResponse) {
  if (message.subject === "executeUserScript") {
    // create function in page script scope (not content script scope)
    // so it is not executed as privileged extension code and thus has no access to webextension apis
    // this also prevents interference with the extension code
    try {
      const executeUserScript = new window.wrappedJSObject.Function("TARGET", "API", message.data);
      const returnValue = executeUserScript(window.TARGET, API);
      // interpret every value even undefined or 0 as true (success) and only an explicit false as failure
      return Promise.resolve(returnValue != false);
    }
    // catch CSP errors that prevent the user script from running
    // this allows to declare a fallback command via the multi purpose command
    catch(error) {
      return Promise.resolve(false);
    }
  }
}


/**
 * Build API function object
 **/
const API = cloneInto({
  tabs: {
    query: apiFunctionCallHandler.bind(null, "tabs", "query"),
    create: apiFunctionCallHandler.bind(null, "tabs", "create"),
    remove: apiFunctionCallHandler.bind(null, "tabs", "remove"),
    update: apiFunctionCallHandler.bind(null, "tabs", "update"),
    move: apiFunctionCallHandler.bind(null, "tabs", "move")
  },
  windows: {
    get: apiFunctionCallHandler.bind(null, "windows", "get"),
    getCurrent: apiFunctionCallHandler.bind(null, "windows", "getCurrent"),
    create: apiFunctionCallHandler.bind(null, "windows", "create"),
    remove: apiFunctionCallHandler.bind(null, "windows", "remove"),
    update: apiFunctionCallHandler.bind(null, "windows", "update"),
  }
}, window.wrappedJSObject, { cloneFunctions: true });


/**
 * Forwards function calls to the background script and returns their values
 **/
function apiFunctionCallHandler (nameSpace, functionName, ...args) {
  return new window.Promise((resolve, reject) => {
    const value = browser.runtime.sendMessage({
      subject: "backgroundScriptAPICall",
      data: {
        "nameSpace": nameSpace,
        "functionName": functionName,
        "parameter": args
      }
    });
    value.then((value) => {
      resolve(cloneInto(value, window.wrappedJSObject));
    });
    value.catch((reason) => {
      reject(reason);
    });
  });
}

}

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
