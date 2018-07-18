/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.defineModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "Utils",
  "resource://gre/modules/accessibility/Utils.jsm");
ChromeUtils.defineModuleGetter(this, "Logger",
  "resource://gre/modules/accessibility/Utils.jsm");
ChromeUtils.defineModuleGetter(this, "Presentation",
  "resource://gre/modules/accessibility/Presentation.jsm");
ChromeUtils.defineModuleGetter(this, "Roles",
  "resource://gre/modules/accessibility/Constants.jsm");
ChromeUtils.defineModuleGetter(this, "Events",
  "resource://gre/modules/accessibility/Constants.jsm");
ChromeUtils.defineModuleGetter(this, "States",
  "resource://gre/modules/accessibility/Constants.jsm");
ChromeUtils.defineModuleGetter(this, "clearTimeout",
  "resource://gre/modules/Timer.jsm");
ChromeUtils.defineModuleGetter(this, "setTimeout",
  "resource://gre/modules/Timer.jsm");

var EXPORTED_SYMBOLS = ["EventManager"];

function EventManager(aContentScope, aContentControl) {
  this.contentScope = aContentScope;
  this.contentControl = aContentControl;
  this.addEventListener = this.contentScope.addEventListener.bind(
    this.contentScope);
  this.removeEventListener = this.contentScope.removeEventListener.bind(
    this.contentScope);
  this.sendMsgFunc = this.contentScope.sendAsyncMessage.bind(
    this.contentScope);
  this.webProgress = this.contentScope.docShell.
    QueryInterface(Ci.nsIInterfaceRequestor).
    getInterface(Ci.nsIWebProgress);
}

this.EventManager.prototype = {
  start: function start() {
    try {
      if (!this._started) {
        Logger.debug("EventManager.start");

        this._started = true;

        AccessibilityEventObserver.addListener(this);

        this.webProgress.addProgressListener(this,
          (Ci.nsIWebProgress.NOTIFY_STATE_ALL |
           Ci.nsIWebProgress.NOTIFY_LOCATION));
        this.addEventListener("wheel", this, true);
        this.addEventListener("scroll", this, true);
        this.addEventListener("resize", this, true);
        this._preDialogPosition = new WeakMap();
      }
      this.present(Presentation.tabStateChanged(null, "newtab"));

    } catch (x) {
      Logger.logException(x, "Failed to start EventManager");
    }
  },

  // XXX: Stop is not called when the tab is closed (|TabClose| event is too
  // late). It is only called when the AccessFu is disabled explicitly.
  stop: function stop() {
    if (!this._started) {
      return;
    }
    Logger.debug("EventManager.stop");
    AccessibilityEventObserver.removeListener(this);
    try {
      this._preDialogPosition = new WeakMap();
      this.webProgress.removeProgressListener(this);
      this.removeEventListener("wheel", this, true);
      this.removeEventListener("scroll", this, true);
      this.removeEventListener("resize", this, true);
    } catch (x) {
      // contentScope is dead.
    } finally {
      this._started = false;
    }
  },

  handleEvent: function handleEvent(aEvent) {
    Logger.debug(() => {
      return ["DOMEvent", aEvent.type];
    });

    try {
      switch (aEvent.type) {
      case "wheel":
      {
        let delta = aEvent.deltaX || aEvent.deltaY;
        this.contentControl.autoMove(
         null,
         { moveMethod: delta > 0 ? "moveNext" : "movePrevious",
           onScreenOnly: true, noOpIfOnScreen: true, delay: 500 });
        break;
      }
      case "scroll":
      case "resize":
      {
        // the target could be an element, document or window
        let window = aEvent.target.ownerGlobal;
        this.present(Presentation.viewportChanged(window));
        break;
      }
      }
    } catch (x) {
      Logger.logException(x, "Error handling DOM event");
    }
  },

  handleAccEvent: function handleAccEvent(aEvent) {
    Logger.debug(() => {
      return ["A11yEvent", Logger.eventToString(aEvent),
              Logger.accessibleToString(aEvent.accessible)];
    });

    // Don't bother with non-content events in firefox.
    if (Utils.MozBuildApp == "browser" &&
        aEvent.eventType != Events.VIRTUALCURSOR_CHANGED &&
        // XXX Bug 442005 results in DocAccessible::getDocType returning
        // NS_ERROR_FAILURE. Checking for aEvent.accessibleDocument.docType ==
        // 'window' does not currently work.
        (aEvent.accessibleDocument.DOMDocument.doctype &&
         aEvent.accessibleDocument.DOMDocument.doctype.name === "window")) {
      return;
    }

    switch (aEvent.eventType) {
      case Events.VIRTUALCURSOR_CHANGED:
      {
        if (!aEvent.isFromUserInput) {
          break;
        }

        const event = aEvent.
          QueryInterface(Ci.nsIAccessibleVirtualCursorChangeEvent);
        const position = event.newAccessible;

        // We pass control to the vc in the embedded frame.
        if (position && position.role == Roles.INTERNAL_FRAME) {
          break;
        }

        // Blur to document if new position is not explicitly focused.
        if (!position || !Utils.getState(position).contains(States.FOCUSED)) {
          aEvent.accessibleDocument.takeFocus();
        }

        this.present(
          Presentation.pivotChanged(position, event.oldAccessible,
                                    event.newStartOffset, event.newEndOffset,
                                    event.reason, event.boundaryType));

        break;
      }
      case Events.STATE_CHANGE:
      {
        const event = aEvent.QueryInterface(Ci.nsIAccessibleStateChangeEvent);
        const state = Utils.getState(event);
        if (state.contains(States.CHECKED)) {
          this.present(Presentation.checked(aEvent.accessible));
        } else if (state.contains(States.SELECTED)) {
          this.present(Presentation.selected(aEvent.accessible,
                                             event.isEnabled ? "select" : "unselect"));
        }
        break;
      }
      case Events.NAME_CHANGE:
      {
        let acc = aEvent.accessible;
        if (acc === this.contentControl.vc.position) {
          this.present(Presentation.nameChanged(acc));
        } else {
          let {liveRegion, isPolite} = this._handleLiveRegion(aEvent,
            ["text", "all"]);
          if (liveRegion) {
            this.present(Presentation.nameChanged(acc, isPolite));
          }
        }
        break;
      }
      case Events.SCROLLING_START:
      {
        this.contentControl.autoMove(aEvent.accessible);
        break;
      }
      case Events.TEXT_CARET_MOVED:
      {
        let acc = aEvent.accessible.QueryInterface(Ci.nsIAccessibleText);
        let caretOffset = aEvent.
          QueryInterface(Ci.nsIAccessibleCaretMoveEvent).caretOffset;

        // We could get a caret move in an accessible that is not focused,
        // it doesn't mean we are not on any editable accessible. just not
        // on this one..
        let state = Utils.getState(acc);
        if (state.contains(States.FOCUSED) && state.contains(States.EDITABLE)) {
          let fromIndex = caretOffset;
          if (acc.selectionCount) {
            const [startSel, endSel] = Utils.getTextSelection(acc);
            fromIndex = startSel == caretOffset ? endSel : startSel;
          }
          this.present(Presentation.textSelectionChanged(
            acc.getText(0, -1), fromIndex, caretOffset, 0, 0,
            aEvent.isFromUserInput));
        }
        break;
      }
      case Events.SHOW:
      {
        this._handleShow(aEvent);
        break;
      }
      case Events.HIDE:
      {
        let evt = aEvent.QueryInterface(Ci.nsIAccessibleHideEvent);
        this._handleHide(evt);
        break;
      }
      case Events.TEXT_INSERTED:
      case Events.TEXT_REMOVED:
      {
        let {liveRegion, isPolite} = this._handleLiveRegion(aEvent,
          ["text", "all"]);
        if (aEvent.isFromUserInput || liveRegion) {
          // Handle all text mutations coming from the user or if they happen
          // on a live region.
          this._handleText(aEvent, liveRegion, isPolite);
        }
        break;
      }
      case Events.FOCUS:
      {
        // Put vc where the focus is at
        let acc = aEvent.accessible;
        if (![Roles.CHROME_WINDOW,
             Roles.DOCUMENT,
             Roles.APPLICATION].includes(acc.role)) {
          this.contentControl.autoMove(acc);
        }

        this.present(Presentation.focused(acc));

       if (this.inTest) {
        this.sendMsgFunc("AccessFu:Focused");
       }
       break;
      }
      case Events.DOCUMENT_LOAD_COMPLETE:
      {
        let position = this.contentControl.vc.position;
        // Check if position is in the subtree of the DOCUMENT_LOAD_COMPLETE
        // event's dialog accessible or accessible document
        let subtreeRoot = aEvent.accessible.role === Roles.DIALOG ?
          aEvent.accessible : aEvent.accessibleDocument;
        if (aEvent.accessible === aEvent.accessibleDocument ||
            (position && Utils.isInSubtree(position, subtreeRoot))) {
          // Do not automove into the document if the virtual cursor is already
          // positioned inside it.
          break;
        }
        this._preDialogPosition.set(aEvent.accessible.DOMNode, position);
        this.contentControl.autoMove(aEvent.accessible, { delay: 500 });
        break;
      }
      case Events.TEXT_VALUE_CHANGE:
        // We handle this events in TEXT_INSERTED/TEXT_REMOVED.
        break;
      case Events.VALUE_CHANGE:
      {
        let position = this.contentControl.vc.position;
        let target = aEvent.accessible;
        if (position === target ||
            Utils.getEmbeddedControl(position) === target) {
          this.present(Presentation.valueChanged(target));
        } else {
          let {liveRegion, isPolite} = this._handleLiveRegion(aEvent,
            ["text", "all"]);
          if (liveRegion) {
            this.present(Presentation.valueChanged(target, isPolite));
          }
        }
      }
    }
  },

  _handleShow: function _handleShow(aEvent) {
    let {liveRegion, isPolite} = this._handleLiveRegion(aEvent,
      ["additions", "all"]);
    // Only handle show if it is a relevant live region.
    if (!liveRegion) {
      return;
    }
    // Show for text is handled by the EVENT_TEXT_INSERTED handler.
    if (aEvent.accessible.role === Roles.TEXT_LEAF) {
      return;
    }
    this._dequeueLiveEvent(Events.HIDE, liveRegion);
    this.present(Presentation.liveRegion(liveRegion, isPolite, false));
  },

  _handleHide: function _handleHide(aEvent) {
    let {liveRegion, isPolite} = this._handleLiveRegion(
      aEvent, ["removals", "all"]);
    let acc = aEvent.accessible;
    if (liveRegion) {
      // Hide for text is handled by the EVENT_TEXT_REMOVED handler.
      if (acc.role === Roles.TEXT_LEAF) {
        return;
      }
      this._queueLiveEvent(Events.HIDE, liveRegion, isPolite);
    } else {
      let vc = Utils.getVirtualCursor(this.contentScope.content.document);
      if (vc.position &&
        (Utils.getState(vc.position).contains(States.DEFUNCT) ||
          Utils.isInSubtree(vc.position, acc))) {
        let position = this._preDialogPosition.get(aEvent.accessible.DOMNode) ||
          aEvent.targetPrevSibling || aEvent.targetParent;
        if (!position) {
          try {
            position = acc.previousSibling;
          } catch (x) {
            // Accessible is unattached from the accessible tree.
            position = acc.parent;
          }
        }
        this.contentControl.autoMove(position,
          { moveToFocused: true, delay: 500 });
      }
    }
  },

  _handleText: function _handleText(aEvent, aLiveRegion, aIsPolite) {
    let event = aEvent.QueryInterface(Ci.nsIAccessibleTextChangeEvent);
    let isInserted = event.isInserted;
    let txtIface = aEvent.accessible.QueryInterface(Ci.nsIAccessibleText);

    let text = "";
    try {
      text = txtIface.getText(0, Ci.nsIAccessibleText.TEXT_OFFSET_END_OF_TEXT);
    } catch (x) {
      // XXX we might have gotten an exception with of a
      // zero-length text. If we did, ignore it (bug #749810).
      if (txtIface.characterCount) {
        throw x;
      }
    }
    // If there are embedded objects in the text, ignore them.
    // Assuming changes to the descendants would already be handled by the
    // show/hide event.
    let modifiedText = event.modifiedText.replace(/\uFFFC/g, "");
    if (modifiedText != event.modifiedText && !modifiedText.trim()) {
      return;
    }

    if (aLiveRegion) {
      if (aEvent.eventType === Events.TEXT_REMOVED) {
        this._queueLiveEvent(Events.TEXT_REMOVED, aLiveRegion, aIsPolite,
          modifiedText);
      } else {
        this._dequeueLiveEvent(Events.TEXT_REMOVED, aLiveRegion);
        this.present(Presentation.liveRegion(aLiveRegion, aIsPolite, false,
          modifiedText));
      }
    } else {
      this.present(Presentation.textChanged(aEvent.accessible, isInserted,
        event.start, event.length, text, modifiedText));
    }
  },

  _handleLiveRegion: function _handleLiveRegion(aEvent, aRelevant) {
    if (aEvent.isFromUserInput) {
      return {};
    }
    let parseLiveAttrs = function parseLiveAttrs(aAccessible) {
      let attrs = Utils.getAttributes(aAccessible);
      if (attrs["container-live"]) {
        return {
          live: attrs["container-live"],
          relevant: attrs["container-relevant"] || "additions text",
          busy: attrs["container-busy"],
          atomic: attrs["container-atomic"],
          memberOf: attrs["member-of"]
        };
      }
      return null;
    };
    // XXX live attributes are not set for hidden accessibles yet. Need to
    // climb up the tree to check for them.
    let getLiveAttributes = function getLiveAttributes(aEvent) {
      let liveAttrs = parseLiveAttrs(aEvent.accessible);
      if (liveAttrs) {
        return liveAttrs;
      }
      let parent = aEvent.targetParent;
      while (parent) {
        liveAttrs = parseLiveAttrs(parent);
        if (liveAttrs) {
          return liveAttrs;
        }
        parent = parent.parent;
      }
      return {};
    };
    let {live, relevant, /* busy, atomic, memberOf */ } = getLiveAttributes(aEvent);
    // If container-live is not present or is set to |off| ignore the event.
    if (!live || live === "off") {
      return {};
    }
    // XXX: support busy and atomic.

    // Determine if the type of the mutation is relevant. Default is additions
    // and text.
    let isRelevant = Utils.matchAttributeValue(relevant, aRelevant);
    if (!isRelevant) {
      return {};
    }
    return {
      liveRegion: aEvent.accessible,
      isPolite: live === "polite"
    };
  },

  _dequeueLiveEvent: function _dequeueLiveEvent(aEventType, aLiveRegion) {
    let domNode = aLiveRegion.DOMNode;
    if (this._liveEventQueue && this._liveEventQueue.has(domNode)) {
      let queue = this._liveEventQueue.get(domNode);
      let nextEvent = queue[0];
      if (nextEvent.eventType === aEventType) {
        clearTimeout(nextEvent.timeoutID);
        queue.shift();
        if (queue.length === 0) {
          this._liveEventQueue.delete(domNode);
        }
      }
    }
  },

  _queueLiveEvent: function _queueLiveEvent(aEventType, aLiveRegion, aIsPolite, aModifiedText) {
    if (!this._liveEventQueue) {
      this._liveEventQueue = new WeakMap();
    }
    let eventHandler = {
      eventType: aEventType,
      timeoutID: setTimeout(this.present.bind(this),
        20, // Wait for a possible EVENT_SHOW or EVENT_TEXT_INSERTED event.
        Presentation.liveRegion(aLiveRegion, aIsPolite, true, aModifiedText))
    };

    let domNode = aLiveRegion.DOMNode;
    if (this._liveEventQueue.has(domNode)) {
      this._liveEventQueue.get(domNode).push(eventHandler);
    } else {
      this._liveEventQueue.set(domNode, [eventHandler]);
    }
  },

  present: function present(aPresentationData) {
    this.sendMsgFunc("AccessFu:Present", aPresentationData);
  },

  onStateChange: function onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    let tabstate = "";

    let loadingState = Ci.nsIWebProgressListener.STATE_TRANSFERRING |
      Ci.nsIWebProgressListener.STATE_IS_DOCUMENT;
    let loadedState = Ci.nsIWebProgressListener.STATE_STOP |
      Ci.nsIWebProgressListener.STATE_IS_NETWORK;

    if ((aStateFlags & loadingState) == loadingState) {
      tabstate = "loading";
    } else if ((aStateFlags & loadedState) == loadedState &&
               !aWebProgress.isLoadingDocument) {
      tabstate = "loaded";
    }

    if (tabstate) {
      let docAcc = Utils.AccService.getAccessibleFor(aWebProgress.DOMWindow.document);
      this.present(Presentation.tabStateChanged(docAcc, tabstate));
    }
  },

  onLocationChange: function onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    let docAcc = Utils.AccService.getAccessibleFor(aWebProgress.DOMWindow.document);
    this.present(Presentation.tabStateChanged(docAcc, "newdoc"));
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIWebProgressListener, Ci.nsISupportsWeakReference, Ci.nsIObserver])
};

const AccessibilityEventObserver = {

  /**
   * A WeakMap containing [content, EventManager] pairs.
   */
  eventManagers: new WeakMap(),

  /**
   * A total number of registered eventManagers.
   */
  listenerCount: 0,

  /**
   * An indicator of an active 'accessible-event' observer.
   */
  started: false,

  /**
   * Start an AccessibilityEventObserver.
   */
  start: function start() {
    if (this.started || this.listenerCount === 0) {
      return;
    }
    Services.obs.addObserver(this, "accessible-event");
    this.started = true;
  },

  /**
   * Stop an AccessibilityEventObserver.
   */
  stop: function stop() {
    if (!this.started) {
      return;
    }
    Services.obs.removeObserver(this, "accessible-event");
    // Clean up all registered event managers.
    this.eventManagers = new WeakMap();
    this.listenerCount = 0;
    this.started = false;
  },

  /**
   * Register an EventManager and start listening to the
   * 'accessible-event' messages.
   *
   * @param aEventManager EventManager
   *        An EventManager object that was loaded into the specific content.
   */
  addListener: function addListener(aEventManager) {
    let content = aEventManager.contentScope.content;
    if (!this.eventManagers.has(content)) {
      this.listenerCount++;
    }
    this.eventManagers.set(content, aEventManager);
    // Since at least one EventManager was registered, start listening.
    Logger.debug("AccessibilityEventObserver.addListener. Total:",
      this.listenerCount);
    this.start();
  },

  /**
   * Unregister an EventManager and, optionally, stop listening to the
   * 'accessible-event' messages.
   *
   * @param aEventManager EventManager
   *        An EventManager object that was stopped in the specific content.
   */
  removeListener: function removeListener(aEventManager) {
    let content = aEventManager.contentScope.content;
    if (!this.eventManagers.delete(content)) {
      return;
    }
    this.listenerCount--;
    Logger.debug("AccessibilityEventObserver.removeListener. Total:",
      this.listenerCount);
    if (this.listenerCount === 0) {
      // If there are no EventManagers registered at the moment, stop listening
      // to the 'accessible-event' messages.
      this.stop();
    }
  },

  /**
   * Lookup an EventManager for a specific content. If the EventManager is not
   * found, walk up the hierarchy of parent windows.
   * @param content Window
   *        A content Window used to lookup the corresponding EventManager.
   */
  getListener: function getListener(content) {
    let eventManager = this.eventManagers.get(content);
    if (eventManager) {
      return eventManager;
    }
    let parent = content.parent;
    if (parent === content) {
      // There is no parent or the parent is of a different type.
      return null;
    }
    return this.getListener(parent);
  },

  /**
   * Handle the 'accessible-event' message.
   */
  observe: function observe(aSubject, aTopic, aData) {
    if (aTopic !== "accessible-event") {
      return;
    }
    let event = aSubject.QueryInterface(Ci.nsIAccessibleEvent);
    if (!event.accessibleDocument) {
      Logger.warning(
        "AccessibilityEventObserver.observe: no accessible document:",
        Logger.eventToString(event), "accessible:",
        Logger.accessibleToString(event.accessible));
      return;
    }
    let content;
    try {
      content = event.accessibleDocument.window;
    } catch (e) {
      Logger.warning(
        "AccessibilityEventObserver.observe: no window for accessible document:",
        Logger.eventToString(event), "accessible:",
        Logger.accessibleToString(event.accessible));
      return;
    }
    // Match the content window to its EventManager.
    let eventManager = this.getListener(content);
    if (!eventManager || !eventManager._started) {
      if (Utils.MozBuildApp === "browser" && !content.isChromeWindow) {
        Logger.warning(
          "AccessibilityEventObserver.observe: ignored event:",
          Logger.eventToString(event), "accessible:",
          Logger.accessibleToString(event.accessible), "document:",
          Logger.accessibleToString(event.accessibleDocument));
      }
      return;
    }
    try {
      eventManager.handleAccEvent(event);
    } catch (x) {
      Logger.logException(x, "Error handing accessible event");
    } finally {
      return;
    }
  }
};
