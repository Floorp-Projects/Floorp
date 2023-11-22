/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file contains event collectors that are then used by developer tools in
// order to find information about events affecting an HTML element.

"use strict";

const {
  isAfterPseudoElement,
  isBeforePseudoElement,
  isMarkerPseudoElement,
  isNativeAnonymous,
} = require("resource://devtools/shared/layout/utils.js");
const Debugger = require("Debugger");
const {
  EXCLUDED_LISTENER,
} = require("resource://devtools/server/actors/inspector/constants.js");

// eslint-disable-next-line
const JQUERY_LIVE_REGEX =
  /return typeof \w+.*.event\.triggered[\s\S]*\.event\.(dispatch|handle).*arguments/;

const REACT_EVENT_NAMES = [
  "onAbort",
  "onAnimationEnd",
  "onAnimationIteration",
  "onAnimationStart",
  "onAuxClick",
  "onBeforeInput",
  "onBlur",
  "onCanPlay",
  "onCanPlayThrough",
  "onCancel",
  "onChange",
  "onClick",
  "onClose",
  "onCompositionEnd",
  "onCompositionStart",
  "onCompositionUpdate",
  "onContextMenu",
  "onCopy",
  "onCut",
  "onDoubleClick",
  "onDrag",
  "onDragEnd",
  "onDragEnter",
  "onDragExit",
  "onDragLeave",
  "onDragOver",
  "onDragStart",
  "onDrop",
  "onDurationChange",
  "onEmptied",
  "onEncrypted",
  "onEnded",
  "onError",
  "onFocus",
  "onGotPointerCapture",
  "onInput",
  "onInvalid",
  "onKeyDown",
  "onKeyPress",
  "onKeyUp",
  "onLoad",
  "onLoadStart",
  "onLoadedData",
  "onLoadedMetadata",
  "onLostPointerCapture",
  "onMouseDown",
  "onMouseEnter",
  "onMouseLeave",
  "onMouseMove",
  "onMouseOut",
  "onMouseOver",
  "onMouseUp",
  "onPaste",
  "onPause",
  "onPlay",
  "onPlaying",
  "onPointerCancel",
  "onPointerDown",
  "onPointerEnter",
  "onPointerLeave",
  "onPointerMove",
  "onPointerOut",
  "onPointerOver",
  "onPointerUp",
  "onProgress",
  "onRateChange",
  "onReset",
  "onScroll",
  "onSeeked",
  "onSeeking",
  "onSelect",
  "onStalled",
  "onSubmit",
  "onSuspend",
  "onTimeUpdate",
  "onToggle",
  "onTouchCancel",
  "onTouchEnd",
  "onTouchMove",
  "onTouchStart",
  "onTransitionEnd",
  "onVolumeChange",
  "onWaiting",
  "onWheel",
  "onAbortCapture",
  "onAnimationEndCapture",
  "onAnimationIterationCapture",
  "onAnimationStartCapture",
  "onAuxClickCapture",
  "onBeforeInputCapture",
  "onBlurCapture",
  "onCanPlayCapture",
  "onCanPlayThroughCapture",
  "onCancelCapture",
  "onChangeCapture",
  "onClickCapture",
  "onCloseCapture",
  "onCompositionEndCapture",
  "onCompositionStartCapture",
  "onCompositionUpdateCapture",
  "onContextMenuCapture",
  "onCopyCapture",
  "onCutCapture",
  "onDoubleClickCapture",
  "onDragCapture",
  "onDragEndCapture",
  "onDragEnterCapture",
  "onDragExitCapture",
  "onDragLeaveCapture",
  "onDragOverCapture",
  "onDragStartCapture",
  "onDropCapture",
  "onDurationChangeCapture",
  "onEmptiedCapture",
  "onEncryptedCapture",
  "onEndedCapture",
  "onErrorCapture",
  "onFocusCapture",
  "onGotPointerCaptureCapture",
  "onInputCapture",
  "onInvalidCapture",
  "onKeyDownCapture",
  "onKeyPressCapture",
  "onKeyUpCapture",
  "onLoadCapture",
  "onLoadStartCapture",
  "onLoadedDataCapture",
  "onLoadedMetadataCapture",
  "onLostPointerCaptureCapture",
  "onMouseDownCapture",
  "onMouseEnterCapture",
  "onMouseLeaveCapture",
  "onMouseMoveCapture",
  "onMouseOutCapture",
  "onMouseOverCapture",
  "onMouseUpCapture",
  "onPasteCapture",
  "onPauseCapture",
  "onPlayCapture",
  "onPlayingCapture",
  "onPointerCancelCapture",
  "onPointerDownCapture",
  "onPointerEnterCapture",
  "onPointerLeaveCapture",
  "onPointerMoveCapture",
  "onPointerOutCapture",
  "onPointerOverCapture",
  "onPointerUpCapture",
  "onProgressCapture",
  "onRateChangeCapture",
  "onResetCapture",
  "onScrollCapture",
  "onSeekedCapture",
  "onSeekingCapture",
  "onSelectCapture",
  "onStalledCapture",
  "onSubmitCapture",
  "onSuspendCapture",
  "onTimeUpdateCapture",
  "onToggleCapture",
  "onTouchCancelCapture",
  "onTouchEndCapture",
  "onTouchMoveCapture",
  "onTouchStartCapture",
  "onTransitionEndCapture",
  "onVolumeChangeCapture",
  "onWaitingCapture",
  "onWheelCapture",
];

/**
 * The base class that all the enent collectors should be based upon.
 */
class MainEventCollector {
  /**
   * We allow displaying chrome events if the page is chrome or if
   * `devtools.chrome.enabled = true`.
   */
  get chromeEnabled() {
    if (typeof this._chromeEnabled === "undefined") {
      this._chromeEnabled = Services.prefs.getBoolPref(
        "devtools.chrome.enabled"
      );
    }

    return this._chromeEnabled;
  }

  /**
   * Check if a node has any event listeners attached. Please do not override
   * this method... your getListeners() implementation needs to have the
   * following signature:
   * `getListeners(node, {checkOnly} = {})`
   *
   * @param  {DOMNode} node
   *         The not for which we want to check for event listeners.
   * @return {Boolean}
   *         true if the node has event listeners, false otherwise.
   */
  hasListeners(node) {
    return this.getListeners(node, {
      checkOnly: true,
    });
  }

  /**
   * Get all listeners for a node. This method must be overridden.
   *
   * @param  {DOMNode} node
   *         The not for which we want to get event listeners.
   * @param  {Object} options
   *         An object for passing in options.
   * @param  {Boolean} [options.checkOnly = false]
   *         Don't get any listeners but return true when the first event is
   *         found.
   * @return {Array}
   *         An array of event handlers.
   */
  getListeners(node, { checkOnly }) {
    throw new Error("You have to implement the method getListeners()!");
  }

  /**
   * Get unfiltered DOM Event listeners for a node.
   * NOTE: These listeners may contain invalid events and events based
   *       on C++ rather than JavaScript.
   *
   * @param  {DOMNode} node
   *         The node for which we want to get unfiltered event listeners.
   * @return {Array}
   *         An array of unfiltered event listeners or an empty array
   */
  getDOMListeners(node) {
    let listeners;
    if (
      typeof node.nodeName !== "undefined" &&
      node.nodeName.toLowerCase() === "html"
    ) {
      const winListeners =
        Services.els.getListenerInfoFor(node.ownerGlobal) || [];
      const docElementListeners = Services.els.getListenerInfoFor(node) || [];
      const docListeners =
        Services.els.getListenerInfoFor(node.parentNode) || [];

      listeners = [...winListeners, ...docElementListeners, ...docListeners];
    } else {
      listeners = Services.els.getListenerInfoFor(node) || [];
    }

    return listeners.filter(listener => {
      const obj = this.unwrap(listener.listenerObject);
      return !obj || !obj[EXCLUDED_LISTENER];
    });
  }

  getJQuery(node) {
    if (Cu.isDeadWrapper(node)) {
      return null;
    }

    const global = this.unwrap(node.ownerGlobal);
    if (!global) {
      return null;
    }

    const hasJQuery = global.jQuery?.fn?.jquery;

    if (hasJQuery) {
      return global.jQuery;
    }
    return null;
  }

  unwrap(obj) {
    return Cu.isXrayWrapper(obj) ? obj.wrappedJSObject : obj;
  }

  isChromeHandler(handler) {
    try {
      const handlerPrincipal = Cu.getObjectPrincipal(handler);

      // Chrome codebase may register listeners on the page from a frame script or
      // JSM <video> tags may also report internal listeners, but they won't be
      // coming from the system principal. Instead, they will be using an expanded
      // principal.
      return (
        handlerPrincipal.isSystemPrincipal ||
        handlerPrincipal.isExpandedPrincipal
      );
    } catch (e) {
      // Anything from a dead object to a CSP error can leave us here so let's
      // return false so that we can fail gracefully.
      return false;
    }
  }
}

/**
 * Get or detect DOM events. These may include DOM events created by libraries
 * that enable their custom events to work. At this point we are unable to
 * effectively filter them as they may be proxied or wrapped. Although we know
 * there is an event, we may not know the true contents until it goes
 * through `processHandlerForEvent()`.
 */
class DOMEventCollector extends MainEventCollector {
  getListeners(node, { checkOnly } = {}) {
    const handlers = [];
    const listeners = this.getDOMListeners(node);

    for (const listener of listeners) {
      // Ignore listeners without a type, e.g.
      // node.addEventListener("", function() {})
      if (!listener.type) {
        continue;
      }

      // Get the listener object, either a Function or an Object.
      const obj = listener.listenerObject;

      // Ignore listeners without any listener, e.g.
      // node.addEventListener("mouseover", null);
      if (!obj) {
        continue;
      }

      let handler = null;

      // An object without a valid handleEvent is not a valid listener.
      if (typeof obj === "object") {
        const unwrapped = this.unwrap(obj);
        if (typeof unwrapped.handleEvent === "function") {
          handler = Cu.unwaiveXrays(unwrapped.handleEvent);
        }
      } else if (typeof obj === "function") {
        // Ignore DOM events used to trigger jQuery events as they are only
        // useful to the developers of the jQuery library.
        if (JQUERY_LIVE_REGEX.test(obj.toString())) {
          continue;
        }
        // Otherwise, the other valid listener type is function.
        handler = obj;
      }

      // Ignore listeners that have no handler.
      if (!handler) {
        continue;
      }

      // If we shouldn't be showing chrome events due to context and this is a
      // chrome handler we can ignore it.
      if (!this.chromeEnabled && this.isChromeHandler(handler)) {
        continue;
      }

      // If this is checking if a node has any listeners then we have found one
      // so return now.
      if (checkOnly) {
        return true;
      }

      const eventInfo = {
        nsIEventListenerInfo: listener,
        capturing: listener.capturing,
        type: listener.type,
        handler,
        enabled: listener.enabled,
      };

      handlers.push(eventInfo);
    }

    // If this is checking if a node has any listeners then none were found so
    // return false.
    if (checkOnly) {
      return false;
    }

    return handlers;
  }
}

/**
 * Get or detect jQuery events.
 */
class JQueryEventCollector extends MainEventCollector {
  // eslint-disable-next-line complexity
  getListeners(node, { checkOnly } = {}) {
    const jQuery = this.getJQuery(node);
    const handlers = [];

    // If jQuery is not on the page, if this is an anonymous node or a pseudo
    // element we need to return early.
    if (
      !jQuery ||
      isNativeAnonymous(node) ||
      isMarkerPseudoElement(node) ||
      isBeforePseudoElement(node) ||
      isAfterPseudoElement(node)
    ) {
      if (checkOnly) {
        return false;
      }
      return handlers;
    }

    let eventsObj = null;
    const data = jQuery._data || jQuery.data;

    if (data) {
      // jQuery 1.2+
      try {
        eventsObj = data(node, "events");
      } catch (e) {
        // We have no access to a JS object. This is probably due to a CORS
        // violation. Using try / catch is the only way to avoid this error.
      }
    } else {
      // JQuery 1.0 & 1.1
      let entry;
      try {
        entry = entry = jQuery(node)[0];
      } catch (e) {
        // We have no access to a JS object. This is probably due to a CORS
        // violation. Using try / catch is the only way to avoid this error.
      }

      if (!entry || !entry.events) {
        if (checkOnly) {
          return false;
        }
        return handlers;
      }

      eventsObj = entry.events;
    }

    if (eventsObj) {
      for (const [type, events] of Object.entries(eventsObj)) {
        for (const [, event] of Object.entries(events)) {
          // Skip events that are part of jQueries internals.
          if (node.nodeType == node.DOCUMENT_NODE && event.selector) {
            continue;
          }

          if (typeof event === "function" || typeof event === "object") {
            // If we shouldn't be showing chrome events due to context and this
            // is a chrome handler we can ignore it.
            const handler = event.handler || event;
            if (!this.chromeEnabled && this.isChromeHandler(handler)) {
              continue;
            }

            if (checkOnly) {
              return true;
            }

            const eventInfo = {
              type,
              handler,
              tags: "jQuery",
              hide: {
                capturing: true,
              },
            };

            handlers.push(eventInfo);
          }
        }
      }
    }

    if (checkOnly) {
      return false;
    }
    return handlers;
  }
}

/**
 * Get or detect jQuery live events.
 */
class JQueryLiveEventCollector extends MainEventCollector {
  // eslint-disable-next-line complexity
  getListeners(node, { checkOnly } = {}) {
    const jQuery = this.getJQuery(node);
    const handlers = [];

    if (!jQuery) {
      if (checkOnly) {
        return false;
      }
      return handlers;
    }

    const data = jQuery._data || jQuery.data;

    if (data) {
      // Live events are added to the document and bubble up to all elements.
      // Any element matching the specified selector will trigger the live
      // event.
      const win = this.unwrap(node.ownerGlobal);
      let events = null;

      try {
        events = data(win.document, "events");
      } catch (e) {
        // We have no access to a JS object. This is probably due to a CORS
        // violation. Using try / catch is the only way to avoid this error.
      }

      if (events) {
        for (const [, eventHolder] of Object.entries(events)) {
          for (const [idx, event] of Object.entries(eventHolder)) {
            if (typeof idx !== "string" || isNaN(parseInt(idx, 10))) {
              continue;
            }

            let selector = event.selector;

            if (!selector && event.data) {
              selector = event.data.selector || event.data || event.selector;
            }

            if (!selector || !node.ownerDocument) {
              continue;
            }

            let matches;
            try {
              matches = node.matches && node.matches(selector);
            } catch (e) {
              // Invalid selector, do nothing.
            }

            if (!matches) {
              continue;
            }

            if (typeof event === "function" || typeof event === "object") {
              // If we shouldn't be showing chrome events due to context and this
              // is a chrome handler we can ignore it.
              const handler = event.handler || event;
              if (!this.chromeEnabled && this.isChromeHandler(handler)) {
                continue;
              }

              if (checkOnly) {
                return true;
              }
              const eventInfo = {
                type: event.origType || event.type.substr(selector.length + 1),
                handler,
                tags: "jQuery,Live",
                hide: {
                  capturing: true,
                },
              };

              if (!eventInfo.type && event.data?.live) {
                eventInfo.type = event.data.live;
              }

              handlers.push(eventInfo);
            }
          }
        }
      }
    }

    if (checkOnly) {
      return false;
    }
    return handlers;
  }

  normalizeListener(handlerDO) {
    function isFunctionInProxy(funcDO) {
      // If the anonymous function is inside the |proxy| function and the
      // function only has guessed atom, the guessed atom should starts with
      // "proxy/".
      const displayName = funcDO.displayName;
      if (displayName && displayName.startsWith("proxy/")) {
        return true;
      }

      // If the anonymous function is inside the |proxy| function and the
      // function gets name at compile time by SetFunctionName, its guessed
      // atom doesn't contain "proxy/".  In that case, check if the caller is
      // "proxy" function, as a fallback.
      const calleeDS = funcDO.environment?.calleeScript;
      if (!calleeDS) {
        return false;
      }
      const calleeName = calleeDS.displayName;
      return calleeName == "proxy";
    }

    function getFirstFunctionVariable(funcDO) {
      // The handler function inside the |proxy| function should point the
      // unwrapped function via environment variable.
      const names = funcDO.environment ? funcDO.environment.names() : [];
      for (const varName of names) {
        const varDO = handlerDO.environment
          ? handlerDO.environment.getVariable(varName)
          : null;
        if (!varDO) {
          continue;
        }
        if (varDO.class == "Function") {
          return varDO;
        }
      }
      return null;
    }

    if (!isFunctionInProxy(handlerDO)) {
      return handlerDO;
    }

    const MAX_NESTED_HANDLER_COUNT = 2;
    for (let i = 0; i < MAX_NESTED_HANDLER_COUNT; i++) {
      const funcDO = getFirstFunctionVariable(handlerDO);
      if (!funcDO) {
        return handlerDO;
      }

      handlerDO = funcDO;
      if (isFunctionInProxy(handlerDO)) {
        continue;
      }
      break;
    }

    return handlerDO;
  }
}

/**
 * Get or detect React events.
 */
class ReactEventCollector extends MainEventCollector {
  getListeners(node, { checkOnly } = {}) {
    const handlers = [];
    const props = this.getProps(node);

    if (props) {
      for (const [name, prop] of Object.entries(props)) {
        if (REACT_EVENT_NAMES.includes(name)) {
          const listener = prop?.__reactBoundMethod || prop;

          if (typeof listener !== "function") {
            continue;
          }

          if (!this.chromeEnabled && this.isChromeHandler(listener)) {
            continue;
          }

          if (checkOnly) {
            return true;
          }

          const handler = {
            type: name,
            handler: listener,
            tags: "React",
            override: {
              capturing: name.endsWith("Capture"),
            },
          };

          handlers.push(handler);
        }
      }
    }

    if (checkOnly) {
      return false;
    }

    return handlers;
  }

  getProps(node) {
    node = this.unwrap(node);

    for (const key of Object.keys(node)) {
      if (key.startsWith("__reactInternalInstance$")) {
        const value = node[key];
        if (value.memoizedProps) {
          return value.memoizedProps; // React 16
        }
        return value?._currentElement?.props; // React 15
      }
    }
    return null;
  }

  normalizeListener(handlerDO, listener) {
    let functionText = "";

    if (handlerDO.boundTargetFunction) {
      handlerDO = handlerDO.boundTargetFunction;
    }

    const script = handlerDO.script;
    // Script might be undefined (eg for methods bound several times, see
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1589658)
    const introScript = script?.source.introductionScript;

    // If this is a Babel transpiled function we have no access to the
    // source location so we need to hide the filename and debugger
    // icon.
    if (introScript && introScript.displayName.endsWith("/transform.run")) {
      listener.hide.debugger = true;
      listener.hide.filename = true;

      if (!handlerDO.isArrowFunction) {
        functionText += "function (";
      } else {
        functionText += "(";
      }

      functionText += handlerDO.parameterNames.join(", ");

      functionText += ") {\n";

      const scriptSource = script.source.text;
      functionText += scriptSource.substr(
        script.sourceStart,
        script.sourceLength
      );

      listener.override.handler = functionText;
    }

    return handlerDO;
  }
}

/**
 * The exposed class responsible for gathering events.
 */
class EventCollector {
  constructor(targetActor) {
    this.targetActor = targetActor;

    // The event collector array. Please preserve the order otherwise there will
    // be multiple failing tests.
    this.eventCollectors = [
      new ReactEventCollector(),
      new JQueryLiveEventCollector(),
      new JQueryEventCollector(),
      new DOMEventCollector(),
    ];
  }

  /**
   * Destructor (must be called manually).
   */
  destroy() {
    this.eventCollectors = null;
  }

  /**
   * Iterate through all event collectors returning on the first found event.
   *
   * @param  {DOMNode} node
   *         The node to be checked for events.
   * @return {Boolean}
   *         True if the node has event listeners, false otherwise.
   */
  hasEventListeners(node) {
    for (const collector of this.eventCollectors) {
      if (collector.hasListeners(node)) {
        return true;
      }
    }

    return false;
  }

  /**
   * We allow displaying chrome events if the page is chrome or if
   * `devtools.chrome.enabled = true`.
   */
  get chromeEnabled() {
    if (typeof this._chromeEnabled === "undefined") {
      this._chromeEnabled = Services.prefs.getBoolPref(
        "devtools.chrome.enabled"
      );
    }

    return this._chromeEnabled;
  }

  /**
   *
   * @param  {DOMNode} node
   *         The node for which events are to be gathered.
   * @return {Array<Object>}
   *         An array containing objects in the following format:
   *         {
   *           {String} type: The event type, e.g. "click"
   *           {Function} handler: The function called when event is triggered.
   *           {Boolean} enabled: Whether the listener is enabled or not (event listeners can
   *                     be disabled via the inspector)
   *           {String} tags: Comma separated list of tags displayed inside event bubble (e.g. "JQuery")
   *           {Object} hide: Flags for hiding certain properties.
   *             {Boolean} capturing
   *           }
   *           {Boolean} native
   *           {String|undefined} sourceActor: The sourceActor id of the event listener
   *           {nsIEventListenerInfo|undefined} nsIEventListenerInfo
   *         }
   */
  getEventListeners(node) {
    const listenerArray = [];
    let dbg;
    if (!this.chromeEnabled) {
      dbg = new Debugger();
    } else {
      // When the chrome pref is turned on, we may try to debug system compartments.
      // But since bug 1517210, the server is also loaded using the system principal
      // and so here, we have to ensure using a special Debugger instance, loaded
      // in a compartment flagged with invisibleToDebugger=true. This helps the Debugger
      // know about the precise boundary between debuggee and debugger code.
      const ChromeDebugger = require("ChromeDebugger");
      dbg = new ChromeDebugger();
    }

    for (const collector of this.eventCollectors) {
      const listeners = collector.getListeners(node);

      if (!listeners) {
        continue;
      }

      for (const listener of listeners) {
        const eventObj = this.processHandlerForEvent(
          listener,
          dbg,
          collector.normalizeListener
        );
        if (eventObj) {
          listenerArray.push(eventObj);
        }
      }
    }

    listenerArray.sort((a, b) => {
      return a.type.localeCompare(b.type);
    });

    return listenerArray;
  }

  /**
   * Process an event listener.
   *
   * @param  {EventListener} listener
   *         The event listener to process.
   * @param  {Debugger} dbg
   *         Debugger instance.
   * @param  {Function|null} normalizeListener
   *         An optional function that will be called to retrieve data about the listener.
   *         It should be a *Collector method.
   *
   * @return {Array}
   *         An array of objects where a typical object looks like this:
   *           {
   *             type: "click",
   *             handler: function() { doSomething() },
   *             origin: "http://www.mozilla.com",
   *             tags: tags,
   *             capturing: true,
   *             hide: {
   *               capturing: true
   *             },
   *             native: false,
   *             enabled: true
   *             sourceActor: "sourceActor.1234",
   *             nsIEventListenerInfo: nsIEventListenerInfo {…},
   *           }
   */
  // eslint-disable-next-line complexity
  processHandlerForEvent(listener, dbg, normalizeListener) {
    let globalDO;
    let eventObj;

    try {
      const { capturing, handler } = listener;

      const global = Cu.getGlobalForObject(handler);

      // It is important that we recreate the globalDO for each handler because
      // their global object can vary e.g. resource:// URLs on a video control. If
      // we don't do this then all chrome listeners simply display "native code."
      globalDO = dbg.addDebuggee(global);
      let listenerDO = globalDO.makeDebuggeeValue(handler);

      if (normalizeListener) {
        listenerDO = normalizeListener(listenerDO, listener);
      }

      const hide = listener.hide || {};
      const override = listener.override || {};
      const tags = listener.tags || "";
      const type = listener.type || "";
      const enabled = !!listener.enabled;
      let functionSource = handler.toString();
      let line = 0;
      let column = null;
      let native = false;
      let url = "";
      let sourceActor = "";

      // If the listener is an object with a 'handleEvent' method, use that.
      if (
        listenerDO.class === "Object" ||
        /^XUL\w*Element$/.test(listenerDO.class)
      ) {
        let desc;

        while (!desc && listenerDO) {
          desc = listenerDO.getOwnPropertyDescriptor("handleEvent");
          listenerDO = listenerDO.proto;
        }

        if (desc?.value) {
          listenerDO = desc.value;
        }
      }

      // If the listener is bound to a different context then we need to switch
      // to the bound function.
      if (listenerDO.isBoundFunction) {
        listenerDO = listenerDO.boundTargetFunction;
      }

      const { isArrowFunction, name, script, parameterNames } = listenerDO;

      if (script) {
        const scriptSource = script.source.text;

        line = script.startLine;
        column = script.startColumn;
        url = script.url;
        const actor = this.targetActor.sourcesManager.getOrCreateSourceActor(
          script.source
        );
        sourceActor = actor ? actor.actorID : null;

        // Checking for the string "[native code]" is the only way at this point
        // to check for native code. Even if this provides a false positive then
        // grabbing the source code a second time is harmless.
        if (
          functionSource === "[object Object]" ||
          functionSource === "[object XULElement]" ||
          functionSource.includes("[native code]")
        ) {
          functionSource = scriptSource.substr(
            script.sourceStart,
            script.sourceLength
          );

          // At this point the script looks like this:
          // () { ... }
          // We prefix this with "function" if it is not a fat arrow function.
          if (!isArrowFunction) {
            functionSource = "function " + functionSource;
          }
        }
      } else {
        // If the listener is a native one (provided by C++ code) then we have no
        // access to the script. We use the native flag to prevent showing the
        // debugger button because the script is not available.
        native = true;
      }

      // Arrow function text always contains the parameters. Function
      // parameters are often missing e.g. if Array.sort is used as a handler.
      // If they are missing we provide the parameters ourselves.
      if (parameterNames && parameterNames.length) {
        const prefix = "function " + name + "()";
        const paramString = parameterNames.join(", ");

        if (functionSource.startsWith(prefix)) {
          functionSource = functionSource.substr(prefix.length);

          functionSource = `function ${name} (${paramString})${functionSource}`;
        }
      }

      // If the listener is native code we display the filename "[native code]."
      // This is the official string and should *not* be translated.
      let origin;
      if (native) {
        origin = "[native code]";
      } else {
        origin =
          url +
          (line ? ":" + line + (column === null ? "" : ":" + column) : "");
      }

      eventObj = {
        type: override.type || type,
        handler: override.handler || functionSource.trim(),
        origin: override.origin || origin,
        tags: override.tags || tags,
        capturing:
          typeof override.capturing !== "undefined"
            ? override.capturing
            : capturing,
        hide: typeof override.hide !== "undefined" ? override.hide : hide,
        native,
        sourceActor,
        nsIEventListenerInfo: listener.nsIEventListenerInfo,
        enabled,
      };

      // Hide the debugger icon for DOM0 and native listeners. DOM0 listeners are
      // generated dynamically from e.g. an onclick="" attribute so the script
      // doesn't actually exist.
      if (!sourceActor) {
        eventObj.hide.debugger = true;
      }
    } finally {
      // Ensure that we always remove the debuggee.
      if (globalDO) {
        dbg.removeDebuggee(globalDO);
      }
    }

    return eventObj;
  }
}

exports.EventCollector = EventCollector;
