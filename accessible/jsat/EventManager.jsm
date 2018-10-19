/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "Utils",
  "resource://gre/modules/accessibility/Utils.jsm");
ChromeUtils.defineModuleGetter(this, "Logger",
  "resource://gre/modules/accessibility/Utils.jsm");
ChromeUtils.defineModuleGetter(this, "Roles",
  "resource://gre/modules/accessibility/Constants.jsm");
ChromeUtils.defineModuleGetter(this, "Events",
  "resource://gre/modules/accessibility/Constants.jsm");
ChromeUtils.defineModuleGetter(this, "States",
  "resource://gre/modules/accessibility/Constants.jsm");

var EXPORTED_SYMBOLS = ["EventManager"];

function EventManager(aContentScope) {
  this.contentScope = aContentScope;
  this.addEventListener = this.contentScope.addEventListener.bind(
    this.contentScope);
  this.removeEventListener = this.contentScope.removeEventListener.bind(
    this.contentScope);
  this.sendMsgFunc = this.contentScope.sendAsyncMessage.bind(
    this.contentScope);
}

this.EventManager.prototype = {
  start: function start() {
    try {
      if (!this._started) {
        Logger.debug("EventManager.start");

        this._started = true;

        AccessibilityEventObserver.addListener(this);

        this._preDialogPosition = new WeakMap();
      }
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
    } catch (x) {
      // contentScope is dead.
    } finally {
      this._started = false;
    }
  },

  get contentControl() {
    return this.contentScope._jsat_contentControl;
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
        break;
      }
      case Events.NAME_CHANGE:
      {
        // XXX: Port to Android
        break;
      }
      case Events.SCROLLING_START:
      {
        this.contentControl.autoMove(aEvent.accessible);
        break;
      }
      case Events.SHOW:
      {
        // XXX: Port to Android
        break;
      }
      case Events.HIDE:
      {
        // XXX: Port to Android
        break;
      }
      case Events.VALUE_CHANGE:
      {
        // XXX: Port to Android
        break;
      }
    }
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsISupportsWeakReference, Ci.nsIObserver]),
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
  },
};
