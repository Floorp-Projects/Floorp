/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const actions = require('../actions/event-listeners');
const { bindActionCreators } = require('devtools/shared/vendor/redux');

/**
 * Functions handling the event listeners UI.
 */
function EventListenersView(store, DebuggerController) {
  dumpn("EventListenersView was instantiated");

  this.actions = bindActionCreators(actions, store.dispatch);
  this.getState = () => store.getState().eventListeners;

  this._onCheck = this._onCheck.bind(this);
  this._onClick = this._onClick.bind(this);
  this._onListeners = this._onListeners.bind(this);

  this.Breakpoints = DebuggerController.Breakpoints;
  this.controller = DebuggerController;
  this.controller.on("@redux:listeners", this._onListeners);
}

EventListenersView.prototype = Heritage.extend(WidgetMethods, {
  /**
   * Initialization function, called when the debugger is started.
   */
  initialize: function() {
    dumpn("Initializing the EventListenersView");

    this.widget = new SideMenuWidget(document.getElementById("event-listeners"), {
      showItemCheckboxes: true,
      showGroupCheckboxes: true
    });

    this.emptyText = L10N.getStr("noEventListenersText");
    this._eventCheckboxTooltip = L10N.getStr("eventCheckboxTooltip");
    this._onSelectorString = " " + L10N.getStr("eventOnSelector") + " ";
    this._inSourceString = " " + L10N.getStr("eventInSource") + " ";
    this._inNativeCodeString = L10N.getStr("eventNative");

    this.widget.addEventListener("check", this._onCheck, false);
    this.widget.addEventListener("click", this._onClick, false);
  },

  /**
   * Destruction function, called when the debugger is closed.
   */
  destroy: function() {
    dumpn("Destroying the EventListenersView");

    this.controller.off("@redux:listeners", this._onListeners);
    this.widget.removeEventListener("check", this._onCheck, false);
    this.widget.removeEventListener("click", this._onClick, false);
    this.controller = this.Breakpoints = null;
  },

  renderListeners: function(listeners) {
    listeners.forEach(listener => {
      this.addListener(listener, { staged: true });
    });

    // Flushes all the prepared events into the event listeners container.
    this.commit();
  },

  /**
   * Adds an event to this event listeners container.
   *
   * @param object aListener
   *        The listener object coming from the active thread.
   * @param object aOptions [optional]
   *        Additional options for adding the source. Supported options:
   *        - staged: true to stage the item to be appended later
   */
  addListener: function(aListener, aOptions = {}) {
    let { node: { selector }, function: { url }, type } = aListener;
    if (!type) return;

    // Some listener objects may be added from plugins, thus getting
    // translated to native code.
    if (!url) {
      url = this._inNativeCodeString;
    }

    // If an event item for this listener's url and type was already added,
    // avoid polluting the view and simply increase the "targets" count.
    let eventItem = this.getItemForPredicate(aItem =>
      aItem.attachment.url == url &&
      aItem.attachment.type == type);

    if (eventItem) {
      let { selectors, view: { targets } } = eventItem.attachment;
      if (selectors.indexOf(selector) == -1) {
        selectors.push(selector);
        targets.setAttribute("value", L10N.getFormatStr("eventNodes", selectors.length));
      }
      return;
    }

    // There's no easy way of grouping event types into higher-level groups,
    // so we need to do this by hand.
    let is = (...args) => args.indexOf(type) != -1;
    let has = str => type.includes(str);
    let starts = str => type.startsWith(str);
    let group;

    if (starts("animation")) {
      group = L10N.getStr("animationEvents");
    } else if (starts("audio")) {
      group = L10N.getStr("audioEvents");
    } else if (is("levelchange")) {
      group = L10N.getStr("batteryEvents");
    } else if (is("cut", "copy", "paste")) {
      group = L10N.getStr("clipboardEvents");
    } else if (starts("composition")) {
      group = L10N.getStr("compositionEvents");
    } else if (starts("device")) {
      group = L10N.getStr("deviceEvents");
    } else if (is("fullscreenchange", "fullscreenerror", "orientationchange",
      "overflow", "resize", "scroll", "underflow", "zoom")) {
      group = L10N.getStr("displayEvents");
    } else if (starts("drag") || starts("drop")) {
      group = L10N.getStr("dragAndDropEvents");
    } else if (starts("gamepad")) {
      group = L10N.getStr("gamepadEvents");
    } else if (is("canplay", "canplaythrough", "durationchange", "emptied",
      "ended", "loadeddata", "loadedmetadata", "pause", "play", "playing",
      "ratechange", "seeked", "seeking", "stalled", "suspend", "timeupdate",
      "volumechange", "waiting")) {
      group = L10N.getStr("mediaEvents");
    } else if (is("blocked", "complete", "success", "upgradeneeded", "versionchange")) {
      group = L10N.getStr("indexedDBEvents");
    } else if (is("blur", "change", "focus", "focusin", "focusout", "invalid",
      "reset", "select", "submit")) {
      group = L10N.getStr("interactionEvents");
    } else if (starts("key") || is("input")) {
      group = L10N.getStr("keyboardEvents");
    } else if (starts("mouse") || has("click") || is("contextmenu", "show", "wheel")) {
      group = L10N.getStr("mouseEvents");
    } else if (starts("DOM")) {
      group = L10N.getStr("mutationEvents");
    } else if (is("abort", "error", "hashchange", "load", "loadend", "loadstart",
      "pagehide", "pageshow", "progress", "timeout", "unload", "uploadprogress",
      "visibilitychange")) {
      group = L10N.getStr("navigationEvents");
    } else if (is("pointerlockchange", "pointerlockerror")) {
      group = L10N.getStr("pointerLockEvents");
    } else if (is("compassneedscalibration", "userproximity")) {
      group = L10N.getStr("sensorEvents");
    } else if (starts("storage")) {
      group = L10N.getStr("storageEvents");
    } else if (is("beginEvent", "endEvent", "repeatEvent")) {
      group = L10N.getStr("timeEvents");
    } else if (starts("touch")) {
      group = L10N.getStr("touchEvents");
    } else {
      group = L10N.getStr("otherEvents");
    }

    // Create the element node for the event listener item.
    const itemView = this._createItemView(type, selector, url);

    // Event breakpoints survive target navigations. Make sure the newly
    // inserted event item is correctly checked.
    const activeEventNames = this.getState().activeEventNames;
    const checkboxState = activeEventNames.indexOf(type) != -1;

    // Append an event listener item to this container.
    this.push([itemView.container], {
      staged: aOptions.staged, /* stage the item to be appended later? */
      attachment: {
        url: url,
        type: type,
        view: itemView,
        selectors: [selector],
        group: group,
        checkboxState: checkboxState,
        checkboxTooltip: this._eventCheckboxTooltip
      }
    });
  },

  /**
   * Gets all the event types known to this container.
   *
   * @return array
   *         List of event types, for example ["load", "click"...]
   */
  getAllEvents: function() {
    return this.attachments.map(e => e.type);
  },

  /**
   * Gets the checked event types in this container.
   *
   * @return array
   *         List of event types, for example ["load", "click"...]
   */
  getCheckedEvents: function() {
    return this.attachments.filter(e => e.checkboxState).map(e => e.type);
  },

  /**
   * Customization function for creating an item's UI.
   *
   * @param string aType
   *        The event type, for example "click".
   * @param string aSelector
   *        The target element's selector.
   * @param string url
   *        The source url in which the event listener is located.
   * @return object
   *         An object containing the event listener view nodes.
   */
  _createItemView: function(aType, aSelector, aUrl) {
    let container = document.createElement("hbox");
    container.className = "dbg-event-listener";

    let eventType = document.createElement("label");
    eventType.className = "plain dbg-event-listener-type";
    eventType.setAttribute("value", aType);
    container.appendChild(eventType);

    let typeSeparator = document.createElement("label");
    typeSeparator.className = "plain dbg-event-listener-separator";
    typeSeparator.setAttribute("value", this._onSelectorString);
    container.appendChild(typeSeparator);

    let eventTargets = document.createElement("label");
    eventTargets.className = "plain dbg-event-listener-targets";
    eventTargets.setAttribute("value", aSelector);
    container.appendChild(eventTargets);

    let selectorSeparator = document.createElement("label");
    selectorSeparator.className = "plain dbg-event-listener-separator";
    selectorSeparator.setAttribute("value", this._inSourceString);
    container.appendChild(selectorSeparator);

    let eventLocation = document.createElement("label");
    eventLocation.className = "plain dbg-event-listener-location";
    eventLocation.setAttribute("value", SourceUtils.getSourceLabel(aUrl));
    eventLocation.setAttribute("flex", "1");
    eventLocation.setAttribute("crop", "center");
    container.appendChild(eventLocation);

    return {
      container: container,
      type: eventType,
      targets: eventTargets,
      location: eventLocation
    };
  },

  /**
   * The check listener for the event listeners container.
   */
  _onCheck: function({ detail: { description, checked }, target }) {
    if (description == "item") {
      this.getItemForElement(target).attachment.checkboxState = checked;

      this.actions.updateEventBreakpoints(this.getCheckedEvents());
      return;
    }

    // Check all the event items in this group.
    this.items
      .filter(e => e.attachment.group == description)
      .forEach(e => this.callMethod("checkItem", e.target, checked));
  },

  /**
   * The select listener for the event listeners container.
   */
  _onClick: function({ target }) {
    // Changing the checkbox state is handled by the _onCheck event. Avoid
    // handling that again in this click event, so pass in "noSiblings"
    // when retrieving the target's item, to ignore the checkbox.
    let eventItem = this.getItemForElement(target, { noSiblings: true });
    if (eventItem) {
      let newState = eventItem.attachment.checkboxState ^= 1;
      this.callMethod("checkItem", eventItem.target, newState);
    }
  },

  /**
   * Called when listeners change.
   */
  _onListeners: function(_, listeners) {
    this.renderListeners(listeners);
  },

  _eventCheckboxTooltip: "",
  _onSelectorString: "",
  _inSourceString: "",
  _inNativeCodeString: ""
});

module.exports = EventListenersView;
