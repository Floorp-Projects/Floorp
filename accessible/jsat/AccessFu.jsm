/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported AccessFu */

"use strict";

const {utils: Cu, interfaces: Ci} = Components;

this.EXPORTED_SYMBOLS = ["AccessFu"]; // jshint ignore:line

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/accessibility/Utils.jsm");

if (Utils.MozBuildApp === "mobile/android") {
  Cu.import("resource://gre/modules/Messaging.jsm");
}

const ACCESSFU_DISABLE = 0; // jshint ignore:line
const ACCESSFU_ENABLE = 1;
const ACCESSFU_AUTO = 2;

const SCREENREADER_SETTING = "accessibility.screenreader";
const QUICKNAV_MODES_PREF = "accessibility.accessfu.quicknav_modes";
const QUICKNAV_INDEX_PREF = "accessibility.accessfu.quicknav_index";

this.AccessFu = { // jshint ignore:line
  /**
   * Initialize chrome-layer accessibility functionality.
   * If accessibility is enabled on the platform, then a special accessibility
   * mode is started.
   */
  attach: function attach(aWindow) {
    Utils.init(aWindow);

    if (Utils.MozBuildApp === "mobile/android") {
      EventDispatcher.instance.dispatch("Accessibility:Ready");
      EventDispatcher.instance.registerListener(this, "Accessibility:Settings");
    }

    this._activatePref = new PrefCache(
      "accessibility.accessfu.activate", this._enableOrDisable.bind(this));

    this._enableOrDisable();
  },

  /**
   * Shut down chrome-layer accessibility functionality from the outside.
   */
  detach: function detach() {
    // Avoid disabling twice.
    if (this._enabled) {
      this._disable();
    }
    if (Utils.MozBuildApp === "mobile/android") {
      EventDispatcher.instance.unregisterListener(this, "Accessibility:Settings");
    }
    delete this._activatePref;
    Utils.uninit();
  },

  /**
   * A lazy getter for event handler that binds the scope to AccessFu object.
   */
  get handleEvent() {
    delete this.handleEvent;
    this.handleEvent = this._handleEvent.bind(this);
    return this.handleEvent;
  },

  /**
   * Start AccessFu mode, this primarily means controlling the virtual cursor
   * with arrow keys.
   */
  _enable: function _enable() {
    if (this._enabled) {
      return;
    }
    this._enabled = true;

    Cu.import("resource://gre/modules/accessibility/Utils.jsm");
    Cu.import("resource://gre/modules/accessibility/PointerAdapter.jsm");
    Cu.import("resource://gre/modules/accessibility/Presentation.jsm");

    for (let mm of Utils.AllMessageManagers) {
      this._addMessageListeners(mm);
      this._loadFrameScript(mm);
    }

    // Add stylesheet
    let stylesheetURL = "chrome://global/content/accessibility/AccessFu.css";
    let stylesheet = Utils.win.document.createProcessingInstruction(
      "xml-stylesheet", `href="${stylesheetURL}" type="text/css"`);
    Utils.win.document.insertBefore(stylesheet, Utils.win.document.firstChild);
    this.stylesheet = Cu.getWeakReference(stylesheet);


    // Populate quicknav modes
    this._quicknavModesPref =
      new PrefCache(QUICKNAV_MODES_PREF, (aName, aValue, aFirstRun) => {
        this.Input.quickNavMode.updateModes(aValue);
        if (!aFirstRun) {
          // If the modes change, reset the current mode index to 0.
          Services.prefs.setIntPref(QUICKNAV_INDEX_PREF, 0);
        }
      }, true);

    this._quicknavCurrentModePref =
      new PrefCache(QUICKNAV_INDEX_PREF, (aName, aValue) => {
        this.Input.quickNavMode.updateCurrentMode(Number(aValue));
      }, true);

    // Check for output notification
    this._notifyOutputPref =
      new PrefCache("accessibility.accessfu.notify_output");


    this.Input.start();
    Output.start();
    PointerAdapter.start();

    if (Utils.MozBuildApp === "mobile/android") {
      EventDispatcher.instance.registerListener(this, [
        "Accessibility:ActivateObject",
        "Accessibility:Focus",
        "Accessibility:LongPress",
        "Accessibility:MoveByGranularity",
        "Accessibility:NextObject",
        "Accessibility:PreviousObject",
        "Accessibility:ScrollBackward",
        "Accessibility:ScrollForward",
      ]);
    }

    Services.obs.addObserver(this, "remote-browser-shown");
    Services.obs.addObserver(this, "inprocess-browser-shown");
    Utils.win.addEventListener("TabOpen", this);
    Utils.win.addEventListener("TabClose", this);
    Utils.win.addEventListener("TabSelect", this);

    if (this.readyCallback) {
      this.readyCallback();
      delete this.readyCallback;
    }

    Logger.info("AccessFu:Enabled");
  },

  /**
   * Disable AccessFu and return to default interaction mode.
   */
  _disable: function _disable() {
    if (!this._enabled) {
      return;
    }

    this._enabled = false;

    Utils.win.document.removeChild(this.stylesheet.get());

    for (let mm of Utils.AllMessageManagers) {
      mm.sendAsyncMessage("AccessFu:Stop");
      this._removeMessageListeners(mm);
    }

    this.Input.stop();
    Output.stop();
    PointerAdapter.stop();

    Utils.win.removeEventListener("TabOpen", this);
    Utils.win.removeEventListener("TabClose", this);
    Utils.win.removeEventListener("TabSelect", this);

    Services.obs.removeObserver(this, "remote-browser-shown");
    Services.obs.removeObserver(this, "inprocess-browser-shown");

    if (Utils.MozBuildApp === "mobile/android") {
      EventDispatcher.instance.unregisterListener(this, [
        "Accessibility:ActivateObject",
        "Accessibility:Focus",
        "Accessibility:LongPress",
        "Accessibility:MoveByGranularity",
        "Accessibility:NextObject",
        "Accessibility:PreviousObject",
        "Accessibility:ScrollBackward",
        "Accessibility:ScrollForward",
      ]);
    }

    delete this._quicknavModesPref;
    delete this._notifyOutputPref;

    if (this.doneCallback) {
      this.doneCallback();
      delete this.doneCallback;
    }

    Logger.info("AccessFu:Disabled");
  },

  _enableOrDisable: function _enableOrDisable() {
    try {
      if (!this._activatePref) {
        return;
      }
      let activatePref = this._activatePref.value;
      if (activatePref == ACCESSFU_ENABLE ||
          this._systemPref && activatePref == ACCESSFU_AUTO) {
        this._enable();
      } else {
        this._disable();
      }
    } catch (x) {
      dump("Error " + x.message + " " + x.fileName + ":" + x.lineNumber);
    }
  },

  receiveMessage: function receiveMessage(aMessage) {
    Logger.debug(() => {
      return ["Recieved", aMessage.name, JSON.stringify(aMessage.json)];
    });

    switch (aMessage.name) {
      case "AccessFu:Ready":
        let mm = Utils.getMessageManager(aMessage.target);
        if (this._enabled) {
          mm.sendAsyncMessage("AccessFu:Start",
                              {method: "start", buildApp: Utils.MozBuildApp});
        }
        break;
      case "AccessFu:Present":
        this._output(aMessage.json, aMessage.target);
        break;
      case "AccessFu:Input":
        this.Input.setEditState(aMessage.json);
        break;
      case "AccessFu:DoScroll":
        this.Input.doScroll(aMessage.json);
        break;
    }
  },

  _output: function _output(aPresentationData, aBrowser) {
    if (!Utils.isAliveAndVisible(
      Utils.AccService.getAccessibleFor(aBrowser))) {
      return;
    }
    for (let presenter of aPresentationData) {
      if (!presenter) {
        continue;
      }

      try {
        Output[presenter.type](presenter.details, aBrowser);
      } catch (x) {
        Logger.logException(x);
      }
    }

    if (this._notifyOutputPref.value) {
      Services.obs.notifyObservers(null, "accessibility-output",
                                   JSON.stringify(aPresentationData));
    }
  },

  _loadFrameScript: function _loadFrameScript(aMessageManager) {
    if (this._processedMessageManagers.indexOf(aMessageManager) < 0) {
      aMessageManager.loadFrameScript(
        "chrome://global/content/accessibility/content-script.js", true);
      this._processedMessageManagers.push(aMessageManager);
    } else if (this._enabled) {
      // If the content-script is already loaded and AccessFu is enabled,
      // send an AccessFu:Start message.
      aMessageManager.sendAsyncMessage("AccessFu:Start",
        {method: "start", buildApp: Utils.MozBuildApp});
    }
  },

  _addMessageListeners: function _addMessageListeners(aMessageManager) {
    aMessageManager.addMessageListener("AccessFu:Present", this);
    aMessageManager.addMessageListener("AccessFu:Input", this);
    aMessageManager.addMessageListener("AccessFu:Ready", this);
    aMessageManager.addMessageListener("AccessFu:DoScroll", this);
  },

  _removeMessageListeners: function _removeMessageListeners(aMessageManager) {
    aMessageManager.removeMessageListener("AccessFu:Present", this);
    aMessageManager.removeMessageListener("AccessFu:Input", this);
    aMessageManager.removeMessageListener("AccessFu:Ready", this);
    aMessageManager.removeMessageListener("AccessFu:DoScroll", this);
  },

  _handleMessageManager: function _handleMessageManager(aMessageManager) {
    if (this._enabled) {
      this._addMessageListeners(aMessageManager);
    }
    this._loadFrameScript(aMessageManager);
  },

  onEvent(event, data, callback) {
    switch (event) {
      case "Accessibility:Settings":
        this._systemPref = data.enabled;
        this._enableOrDisable();
        break;
      case "Accessibility:NextObject":
      case "Accessibility:PreviousObject": {
        let rule = "Simple";
        if (data && data.rule && data.rule.length) {
          rule = data.rule.substr(0, 1).toUpperCase() +
            data.rule.substr(1).toLowerCase();
        }
        let method = event.replace(/Accessibility:(\w+)Object/, "move$1");
        this.Input.moveCursor(method, rule, "gesture");
        break;
      }
      case "Accessibility:ActivateObject":
        this.Input.activateCurrent(data);
        break;
      case "Accessibility:LongPress":
        this.Input.sendContextMenuMessage();
        break;
      case "Accessibility:ScrollForward":
        this.Input.androidScroll("forward");
        break;
      case "Accessibility:ScrollBackward":
        this.Input.androidScroll("backward");
        break;
      case "Accessibility:Focus":
        this._focused = data.gainFocus;
        if (this._focused) {
          this.autoMove({ forcePresent: true, noOpIfOnScreen: true });
        }
        break;
      case "Accessibility:MoveByGranularity":
        this.Input.moveByGranularity(data);
        break;
    }
  },

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "remote-browser-shown":
      case "inprocess-browser-shown":
      {
        // Ignore notifications that aren't from a Browser
        let frameLoader = aSubject.QueryInterface(Ci.nsIFrameLoader);
        if (!frameLoader.ownerIsMozBrowserFrame) {
          return;
        }
        this._handleMessageManager(frameLoader.messageManager);
        break;
      }
    }
  },

  _handleEvent: function _handleEvent(aEvent) {
    switch (aEvent.type) {
      case "TabOpen":
      {
        let mm = Utils.getMessageManager(aEvent.target);
        this._handleMessageManager(mm);
        break;
      }
      case "TabClose":
      {
        let mm = Utils.getMessageManager(aEvent.target);
        let mmIndex = this._processedMessageManagers.indexOf(mm);
        if (mmIndex > -1) {
          this._removeMessageListeners(mm);
          this._processedMessageManagers.splice(mmIndex, 1);
        }
        break;
      }
      case "TabSelect":
      {
        if (this._focused) {
          // We delay this for half a second so the awesomebar could close,
          // and we could use the current coordinates for the content item.
          // XXX TODO figure out how to avoid magic wait here.
          this.autoMove({
            delay: 500,
            forcePresent: true,
            noOpIfOnScreen: true,
            moveMethod: "moveFirst" });
        }
        break;
      }
      default:
      {
        // A settings change, it does not have an event type
        if (aEvent.settingName == SCREENREADER_SETTING) {
          this._systemPref = aEvent.settingValue;
          this._enableOrDisable();
        }
        break;
      }
    }
  },

  autoMove: function autoMove(aOptions) {
    let mm = Utils.getMessageManager(Utils.CurrentBrowser);
    mm.sendAsyncMessage("AccessFu:AutoMove", aOptions);
  },

  announce: function announce(aAnnouncement) {
    this._output(Presentation.announce(aAnnouncement), Utils.CurrentBrowser);
  },

  // So we don't enable/disable twice
  _enabled: false,

  // Layerview is focused
  _focused: false,

  // Keep track of message managers tha already have a 'content-script.js'
  // injected.
  _processedMessageManagers: [],

  /**
   * Adjusts the given bounds relative to the given browser.
   * @param {Rect} aJsonBounds the bounds to adjust
   * @param {browser} aBrowser the browser we want the bounds relative to
   * @param {bool} aToCSSPixels whether to convert to CSS pixels (as opposed to
   *               device pixels)
   */
  adjustContentBounds(aJsonBounds, aBrowser, aToCSSPixels) {
      let bounds = new Rect(aJsonBounds.left, aJsonBounds.top,
                            aJsonBounds.right - aJsonBounds.left,
                            aJsonBounds.bottom - aJsonBounds.top);
      let win = Utils.win;
      let dpr = win.devicePixelRatio;
      let offset = { left: -win.mozInnerScreenX, top: -win.mozInnerScreenY };

      // Add the offset; the offset is in CSS pixels, so multiply the
      // devicePixelRatio back in before adding to preserve unit consistency.
      bounds = bounds.translate(offset.left * dpr, offset.top * dpr);

      // If we want to get to CSS pixels from device pixels, this needs to be
      // further divided by the devicePixelRatio due to widget scaling.
      if (aToCSSPixels) {
        bounds = bounds.scale(1 / dpr, 1 / dpr);
      }

      return bounds.expandToIntegers();
    }
};

var Output = {
  brailleState: {
    startOffset: 0,
    endOffset: 0,
    text: "",
    selectionStart: 0,
    selectionEnd: 0,

    init: function init(aOutput) {
      if (aOutput && "output" in aOutput) {
        this.startOffset = aOutput.startOffset;
        this.endOffset = aOutput.endOffset;
        // We need to append a space at the end so that the routing key
        // corresponding to the end of the output (i.e. the space) can be hit to
        // move the caret there.
        this.text = aOutput.output + " ";
        this.selectionStart = typeof aOutput.selectionStart === "number" ?
                              aOutput.selectionStart : this.selectionStart;
        this.selectionEnd = typeof aOutput.selectionEnd === "number" ?
                            aOutput.selectionEnd : this.selectionEnd;

        return { text: this.text,
                 selectionStart: this.selectionStart,
                 selectionEnd: this.selectionEnd };
      }

      return null;
    },

    adjustText: function adjustText(aText) {
      let newBraille = [];
      let braille = {};

      let prefix = this.text.substring(0, this.startOffset).trim();
      if (prefix) {
        prefix += " ";
        newBraille.push(prefix);
      }

      newBraille.push(aText);

      let suffix = this.text.substring(this.endOffset).trim();
      if (suffix) {
        suffix = " " + suffix;
        newBraille.push(suffix);
      }

      this.startOffset = braille.startOffset = prefix.length;
      this.text = braille.text = newBraille.join("") + " ";
      this.endOffset = braille.endOffset = braille.text.length - suffix.length;
      braille.selectionStart = this.selectionStart;
      braille.selectionEnd = this.selectionEnd;

      return braille;
    },

    adjustSelection: function adjustSelection(aSelection) {
      let braille = {};

      braille.startOffset = this.startOffset;
      braille.endOffset = this.endOffset;
      braille.text = this.text;
      this.selectionStart = braille.selectionStart =
        aSelection.selectionStart + this.startOffset;
      this.selectionEnd = braille.selectionEnd =
        aSelection.selectionEnd + this.startOffset;

      return braille;
    }
  },

  start: function start() {
    Cu.import("resource://gre/modules/Geometry.jsm");
  },

  stop: function stop() {
    if (this.highlightBox) {
      let highlightBox = this.highlightBox.get();
      if (highlightBox) {
        highlightBox.remove();
      }
      delete this.highlightBox;
    }
  },

  B2G: function B2G(aDetails) {
    Utils.dispatchChromeEvent("accessibility-output", aDetails);
  },

  Visual: function Visual(aDetail, aBrowser) {
    switch (aDetail.eventType) {
      case "viewport-change":
      case "vc-change":
      {
        let highlightBox = null;
        if (!this.highlightBox) {
          let doc = Utils.win.document;
          // Add highlight box
          highlightBox = Utils.win.document.
            createElementNS("http://www.w3.org/1999/xhtml", "div");
          let parent = doc.body || doc.documentElement;
          parent.appendChild(highlightBox);
          highlightBox.id = "virtual-cursor-box";

          // Add highlight inset for inner shadow
          highlightBox.appendChild(
            doc.createElementNS("http://www.w3.org/1999/xhtml", "div"));

          this.highlightBox = Cu.getWeakReference(highlightBox);
        } else {
          highlightBox = this.highlightBox.get();
        }

        let padding = aDetail.padding;
        let r = AccessFu.adjustContentBounds(aDetail.bounds, aBrowser, true);

        // First hide it to avoid flickering when changing the style.
        highlightBox.classList.remove("show");
        highlightBox.style.top = (r.top - padding) + "px";
        highlightBox.style.left = (r.left - padding) + "px";
        highlightBox.style.width = (r.width + padding * 2) + "px";
        highlightBox.style.height = (r.height + padding * 2) + "px";
        highlightBox.classList.add("show");

        break;
      }
      case "tabstate-change":
      {
        let highlightBox = this.highlightBox ? this.highlightBox.get() : null;
        if (highlightBox) {
          highlightBox.classList.remove("show");
        }
        break;
      }
    }
  },

  Android: function Android(aDetails, aBrowser) {
    const ANDROID_VIEW_TEXT_CHANGED = 0x10;
    const ANDROID_VIEW_TEXT_SELECTION_CHANGED = 0x2000;

    for (let androidEvent of aDetails) {
      androidEvent.type = "Accessibility:Event";
      if (androidEvent.bounds) {
        androidEvent.bounds = AccessFu.adjustContentBounds(
          androidEvent.bounds, aBrowser);
      }

      switch (androidEvent.eventType) {
        case ANDROID_VIEW_TEXT_CHANGED:
          androidEvent.brailleOutput = this.brailleState.adjustText(
            androidEvent.text);
          break;
        case ANDROID_VIEW_TEXT_SELECTION_CHANGED:
          androidEvent.brailleOutput = this.brailleState.adjustSelection(
            androidEvent.brailleOutput);
          break;
        default:
          androidEvent.brailleOutput = this.brailleState.init(
            androidEvent.brailleOutput);
          break;
      }

      Utils.win.WindowEventDispatcher.sendRequest(androidEvent);
    }
  },

  Braille: function Braille(aDetails) {
    Logger.debug("Braille output: " + aDetails.output);
  }
};

var Input = {
  editState: {},

  start: function start() {
    // XXX: This is too disruptive on desktop for now.
    // Might need to add special modifiers.
    if (Utils.MozBuildApp != "browser") {
      Utils.win.document.addEventListener("keypress", this, true);
    }
    Utils.win.addEventListener("mozAccessFuGesture", this, true);
  },

  stop: function stop() {
    if (Utils.MozBuildApp != "browser") {
      Utils.win.document.removeEventListener("keypress", this, true);
    }
    Utils.win.removeEventListener("mozAccessFuGesture", this, true);
  },

  handleEvent: function Input_handleEvent(aEvent) {
    try {
      switch (aEvent.type) {
      case "keypress":
        this._handleKeypress(aEvent);
        break;
      case "mozAccessFuGesture":
        this._handleGesture(aEvent.detail);
        break;
      }
    } catch (x) {
      Logger.logException(x);
    }
  },

  _handleGesture: function _handleGesture(aGesture) {
    let gestureName = aGesture.type + aGesture.touches.length;
    Logger.debug("Gesture", aGesture.type,
                 "(fingers: " + aGesture.touches.length + ")");

    switch (gestureName) {
      case "dwell1":
      case "explore1":
        this.moveToPoint("Simple", aGesture.touches[0].x,
          aGesture.touches[0].y);
        break;
      case "doubletap1":
        this.activateCurrent();
        break;
      case "doubletaphold1":
        Utils.dispatchChromeEvent("accessibility-control", "quicknav-menu");
        break;
      case "swiperight1":
        this.moveCursor("moveNext", "Simple", "gestures");
        break;
      case "swipeleft1":
        this.moveCursor("movePrevious", "Simple", "gesture");
        break;
      case "swipeup1":
        this.moveCursor(
          "movePrevious", this.quickNavMode.current, "gesture", true);
        break;
      case "swipedown1":
        this.moveCursor("moveNext", this.quickNavMode.current, "gesture", true);
        break;
      case "exploreend1":
      case "dwellend1":
        this.activateCurrent(null, true);
        break;
      case "swiperight2":
        if (aGesture.edge) {
          Utils.dispatchChromeEvent("accessibility-control",
            "edge-swipe-right");
          break;
        }
        this.sendScrollMessage(-1, true);
        break;
      case "swipedown2":
        if (aGesture.edge) {
          Utils.dispatchChromeEvent("accessibility-control", "edge-swipe-down");
          break;
        }
        this.sendScrollMessage(-1);
        break;
      case "swipeleft2":
        if (aGesture.edge) {
          Utils.dispatchChromeEvent("accessibility-control", "edge-swipe-left");
          break;
        }
        this.sendScrollMessage(1, true);
        break;
      case "swipeup2":
        if (aGesture.edge) {
          Utils.dispatchChromeEvent("accessibility-control", "edge-swipe-up");
          break;
        }
        this.sendScrollMessage(1);
        break;
      case "explore2":
        Utils.CurrentBrowser.contentWindow.scrollBy(
          -aGesture.deltaX, -aGesture.deltaY);
        break;
      case "swiperight3":
        this.moveCursor("moveNext", this.quickNavMode.current, "gesture");
        break;
      case "swipeleft3":
        this.moveCursor("movePrevious", this.quickNavMode.current, "gesture");
        break;
      case "swipedown3":
        this.quickNavMode.next();
        AccessFu.announce("quicknav_" + this.quickNavMode.current);
        break;
      case "swipeup3":
        this.quickNavMode.previous();
        AccessFu.announce("quicknav_" + this.quickNavMode.current);
        break;
      case "tripletap3":
        Utils.dispatchChromeEvent("accessibility-control", "toggle-shade");
        break;
      case "tap2":
        Utils.dispatchChromeEvent("accessibility-control", "toggle-pause");
        break;
    }
  },

  _handleKeypress: function _handleKeypress(aEvent) {
    let target = aEvent.target;

    // Ignore keys with modifiers so the content could take advantage of them.
    if (aEvent.ctrlKey || aEvent.altKey || aEvent.metaKey) {
      return;
    }

    switch (aEvent.keyCode) {
      case 0:
        // an alphanumeric key was pressed, handle it separately.
        // If it was pressed with either alt or ctrl, just pass through.
        // If it was pressed with meta, pass the key on without the meta.
        if (this.editState.editing) {
          return;
        }

        let key = String.fromCharCode(aEvent.charCode);
        try {
          let [methodName, rule] = this.keyMap[key];
          this.moveCursor(methodName, rule, "keyboard");
        } catch (x) {
          return;
        }
        break;
      case aEvent.DOM_VK_RIGHT:
        if (this.editState.editing) {
          if (!this.editState.atEnd) {
            // Don't move forward if caret is not at end of entry.
            // XXX: Fix for rtl
            return;
          }
          target.blur();

        }
        this.moveCursor(aEvent.shiftKey ?
          "moveLast" : "moveNext", "Simple", "keyboard");
        break;
      case aEvent.DOM_VK_LEFT:
        if (this.editState.editing) {
          if (!this.editState.atStart) {
            // Don't move backward if caret is not at start of entry.
            // XXX: Fix for rtl
            return;
          }
          target.blur();

        }
        this.moveCursor(aEvent.shiftKey ?
          "moveFirst" : "movePrevious", "Simple", "keyboard");
        break;
      case aEvent.DOM_VK_UP:
        if (this.editState.multiline) {
          if (!this.editState.atStart) {
            // Don't blur content if caret is not at start of text area.
            return;
          }
          target.blur();

        }

        if (Utils.MozBuildApp == "mobile/android") {
          // Return focus to native Android browser chrome.
          Utils.win.WindowEventDispatcher.dispatch("ToggleChrome:Focus");
        }
        break;
      case aEvent.DOM_VK_RETURN:
        if (this.editState.editing) {
          return;
        }
        this.activateCurrent();
        break;
    default:
      return;
    }

    aEvent.preventDefault();
    aEvent.stopPropagation();
  },

  moveToPoint: function moveToPoint(aRule, aX, aY) {
    // XXX: Bug 1013408 - There is no alignment between the chrome window's
    // viewport size and the content viewport size in Android. This makes
    // sending mouse events beyond its bounds impossible.
    if (Utils.MozBuildApp === "mobile/android") {
      let mm = Utils.getMessageManager(Utils.CurrentBrowser);
      mm.sendAsyncMessage("AccessFu:MoveToPoint",
        {rule: aRule, x: aX, y: aY, origin: "top"});
    } else {
      let win = Utils.win;
      Utils.winUtils.sendMouseEvent("mousemove",
        aX - win.mozInnerScreenX, aY - win.mozInnerScreenY, 0, 0, 0);
    }
  },

  moveCursor: function moveCursor(aAction, aRule, aInputType, aAdjustRange) {
    let mm = Utils.getMessageManager(Utils.CurrentBrowser);
    mm.sendAsyncMessage("AccessFu:MoveCursor",
                        { action: aAction, rule: aRule,
                          origin: "top", inputType: aInputType,
                          adjustRange: aAdjustRange });
  },

  androidScroll: function androidScroll(aDirection) {
    let mm = Utils.getMessageManager(Utils.CurrentBrowser);
    mm.sendAsyncMessage("AccessFu:AndroidScroll",
                        { direction: aDirection, origin: "top" });
  },

  moveByGranularity: function moveByGranularity(aDetails) {
    const GRANULARITY_PARAGRAPH = 8;
    const GRANULARITY_LINE = 4;

    if (!this.editState.editing) {
      if (aDetails.granularity & (GRANULARITY_PARAGRAPH | GRANULARITY_LINE)) {
        this.moveCursor("move" + aDetails.direction, "Simple", "gesture");
        return;
      }
    } else {
      aDetails.atStart = this.editState.atStart;
      aDetails.atEnd = this.editState.atEnd;
    }

    let mm = Utils.getMessageManager(Utils.CurrentBrowser);
    let type = this.editState.editing ? "AccessFu:MoveCaret" :
                                        "AccessFu:MoveByGranularity";
    mm.sendAsyncMessage(type, aDetails);
  },

  activateCurrent: function activateCurrent(aData, aActivateIfKey = false) {
    let mm = Utils.getMessageManager(Utils.CurrentBrowser);
    let offset = aData && typeof aData.keyIndex === "number" ?
                 aData.keyIndex - Output.brailleState.startOffset : -1;

    mm.sendAsyncMessage("AccessFu:Activate",
                        {offset, activateIfKey: aActivateIfKey});
  },

  sendContextMenuMessage: function sendContextMenuMessage() {
    let mm = Utils.getMessageManager(Utils.CurrentBrowser);
    mm.sendAsyncMessage("AccessFu:ContextMenu", {});
  },

  setEditState: function setEditState(aEditState) {
    Logger.debug(() => { return ["setEditState", JSON.stringify(aEditState)]; });
    this.editState = aEditState;
  },

  // XXX: This is here for backwards compatability with screen reader simulator
  // it should be removed when the extension is updated on amo.
  scroll: function scroll(aPage, aHorizontal) {
    this.sendScrollMessage(aPage, aHorizontal);
  },

  sendScrollMessage: function sendScrollMessage(aPage, aHorizontal) {
    let mm = Utils.getMessageManager(Utils.CurrentBrowser);
    mm.sendAsyncMessage("AccessFu:Scroll",
      {page: aPage, horizontal: aHorizontal, origin: "top"});
  },

  doScroll: function doScroll(aDetails) {
    let horizontal = aDetails.horizontal;
    let page = aDetails.page;
    let p = AccessFu.adjustContentBounds(
      aDetails.bounds, Utils.CurrentBrowser, true).center();
    Utils.winUtils.sendWheelEvent(p.x, p.y,
      horizontal ? page : 0, horizontal ? 0 : page, 0,
      Utils.win.WheelEvent.DOM_DELTA_PAGE, 0, 0, 0, 0);
  },

  get keyMap() {
    delete this.keyMap;
    this.keyMap = {
      a: ["moveNext", "Anchor"],
      A: ["movePrevious", "Anchor"],
      b: ["moveNext", "Button"],
      B: ["movePrevious", "Button"],
      c: ["moveNext", "Combobox"],
      C: ["movePrevious", "Combobox"],
      d: ["moveNext", "Landmark"],
      D: ["movePrevious", "Landmark"],
      e: ["moveNext", "Entry"],
      E: ["movePrevious", "Entry"],
      f: ["moveNext", "FormElement"],
      F: ["movePrevious", "FormElement"],
      g: ["moveNext", "Graphic"],
      G: ["movePrevious", "Graphic"],
      h: ["moveNext", "Heading"],
      H: ["movePrevious", "Heading"],
      i: ["moveNext", "ListItem"],
      I: ["movePrevious", "ListItem"],
      k: ["moveNext", "Link"],
      K: ["movePrevious", "Link"],
      l: ["moveNext", "List"],
      L: ["movePrevious", "List"],
      p: ["moveNext", "PageTab"],
      P: ["movePrevious", "PageTab"],
      r: ["moveNext", "RadioButton"],
      R: ["movePrevious", "RadioButton"],
      s: ["moveNext", "Separator"],
      S: ["movePrevious", "Separator"],
      t: ["moveNext", "Table"],
      T: ["movePrevious", "Table"],
      x: ["moveNext", "Checkbox"],
      X: ["movePrevious", "Checkbox"]
    };

    return this.keyMap;
  },

  quickNavMode: {
    get current() {
      return this.modes[this._currentIndex];
    },

    previous: function quickNavMode_previous() {
      Services.prefs.setIntPref(QUICKNAV_INDEX_PREF,
        this._currentIndex > 0 ?
          this._currentIndex - 1 : this.modes.length - 1);
    },

    next: function quickNavMode_next() {
      Services.prefs.setIntPref(QUICKNAV_INDEX_PREF,
        this._currentIndex + 1 >= this.modes.length ?
          0 : this._currentIndex + 1);
    },

    updateModes: function updateModes(aModes) {
      if (aModes) {
        this.modes = aModes.split(",");
      } else {
        this.modes = [];
      }
    },

    updateCurrentMode: function updateCurrentMode(aModeIndex) {
      Logger.debug("Quicknav mode:", this.modes[aModeIndex]);
      this._currentIndex = aModeIndex;
    }
  }
};
AccessFu.Input = Input;
