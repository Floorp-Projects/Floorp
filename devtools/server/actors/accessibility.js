/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci } = require("chrome");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { DebuggerServer } = require("devtools/server/main");
const Services = require("Services");
const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const defer = require("devtools/shared/defer");
const events = require("devtools/shared/event-emitter");
const {
  accessibleSpec,
  accessibleWalkerSpec,
  accessibilitySpec
} = require("devtools/shared/specs/accessibility");

const { isXUL } = require("devtools/server/actors/highlighters/utils/markup");
const { isWindowIncluded } = require("devtools/shared/layout/utils");
const { CustomHighlighterActor, register } =
  require("devtools/server/actors/highlighters");
const PREF_ACCESSIBILITY_FORCE_DISABLED = "accessibility.force_disabled";

const nsIAccessibleEvent = Ci.nsIAccessibleEvent;
const nsIAccessibleStateChangeEvent = Ci.nsIAccessibleStateChangeEvent;
const nsIPropertyElement = Ci.nsIPropertyElement;
const nsIAccessibleRole = Ci.nsIAccessibleRole;

const {
  EVENT_TEXT_CHANGED,
  EVENT_TEXT_INSERTED,
  EVENT_TEXT_REMOVED,
  EVENT_ACCELERATOR_CHANGE,
  EVENT_ACTION_CHANGE,
  EVENT_DEFACTION_CHANGE,
  EVENT_DESCRIPTION_CHANGE,
  EVENT_DOCUMENT_ATTRIBUTES_CHANGED,
  EVENT_HELP_CHANGE,
  EVENT_HIDE,
  EVENT_NAME_CHANGE,
  EVENT_OBJECT_ATTRIBUTE_CHANGED,
  EVENT_REORDER,
  EVENT_STATE_CHANGE,
  EVENT_TEXT_ATTRIBUTE_CHANGED,
  EVENT_VALUE_CHANGE
} = nsIAccessibleEvent;

// TODO: We do not need this once bug 1422913 is fixed. We also would not need
// to fire a name change event for an accessible that has an updated subtree and
// that has its name calculated from the said subtree.
const NAME_FROM_SUBTREE_RULE_ROLES = new Set([
  nsIAccessibleRole.ROLE_BUTTONDROPDOWN,
  nsIAccessibleRole.ROLE_BUTTONDROPDOWNGRID,
  nsIAccessibleRole.ROLE_BUTTONMENU,
  nsIAccessibleRole.ROLE_CELL,
  nsIAccessibleRole.ROLE_CHECKBUTTON,
  nsIAccessibleRole.ROLE_CHECK_MENU_ITEM,
  nsIAccessibleRole.ROLE_CHECK_RICH_OPTION,
  nsIAccessibleRole.ROLE_COLUMN,
  nsIAccessibleRole.ROLE_COLUMNHEADER,
  nsIAccessibleRole.ROLE_COMBOBOX_OPTION,
  nsIAccessibleRole.ROLE_DEFINITION,
  nsIAccessibleRole.ROLE_GRID_CELL,
  nsIAccessibleRole.ROLE_HEADING,
  nsIAccessibleRole.ROLE_HELPBALLOON,
  nsIAccessibleRole.ROLE_HTML_CONTAINER,
  nsIAccessibleRole.ROLE_KEY,
  nsIAccessibleRole.ROLE_LABEL,
  nsIAccessibleRole.ROLE_LINK,
  nsIAccessibleRole.ROLE_LISTITEM,
  nsIAccessibleRole.ROLE_MATHML_IDENTIFIER,
  nsIAccessibleRole.ROLE_MATHML_NUMBER,
  nsIAccessibleRole.ROLE_MATHML_OPERATOR,
  nsIAccessibleRole.ROLE_MATHML_TEXT,
  nsIAccessibleRole.ROLE_MATHML_STRING_LITERAL,
  nsIAccessibleRole.ROLE_MATHML_GLYPH,
  nsIAccessibleRole.ROLE_MENUITEM,
  nsIAccessibleRole.ROLE_OPTION,
  nsIAccessibleRole.ROLE_OUTLINEITEM,
  nsIAccessibleRole.ROLE_PAGETAB,
  nsIAccessibleRole.ROLE_PARENT_MENUITEM,
  nsIAccessibleRole.ROLE_PUSHBUTTON,
  nsIAccessibleRole.ROLE_RADIOBUTTON,
  nsIAccessibleRole.ROLE_RADIO_MENU_ITEM,
  nsIAccessibleRole.ROLE_RICH_OPTION,
  nsIAccessibleRole.ROLE_ROW,
  nsIAccessibleRole.ROLE_ROWHEADER,
  nsIAccessibleRole.ROLE_SUMMARY,
  nsIAccessibleRole.ROLE_SWITCH,
  nsIAccessibleRole.ROLE_TABLE_COLUMN_HEADER,
  nsIAccessibleRole.ROLE_TABLE_ROW_HEADER,
  nsIAccessibleRole.ROLE_TEAR_OFF_MENU_ITEM,
  nsIAccessibleRole.ROLE_TERM,
  nsIAccessibleRole.ROLE_TOGGLE_BUTTON,
  nsIAccessibleRole.ROLE_TOOLTIP
]);

const IS_OSX = Services.appinfo.OS === "Darwin";

register("AccessibleHighlighter", "accessible");
register("XULWindowAccessibleHighlighter", "xul-accessible");

/**
 * Helper function that determines if nsIAccessible object is in defunct state.
 *
 * @param  {nsIAccessible}  accessible
 *         object to be tested.
 * @return {Boolean}
 *         True if accessible object is defunct, false otherwise.
 */
function isDefunct(accessible) {
  // If accessibility is disabled, safely assume that the accessible object is
  // now dead.
  if (!Services.appinfo.accessibilityEnabled) {
    return true;
  }

  let defunct = false;

  try {
    let extraState = {};
    accessible.getState({}, extraState);
    // extraState.value is a bitmask. We are applying bitwise AND to mask out
    // irrelevant states.
    defunct = !!(extraState.value & Ci.nsIAccessibleStates.EXT_STATE_DEFUNCT);
  } catch (e) {
    defunct = true;
  }

  return defunct;
}

/**
 * Helper function that determines if nsIAccessible object is in stale state. When an
 * object is stale it means its subtree is not up to date.
 *
 * @param  {nsIAccessible}  accessible
 *         object to be tested.
 * @return {Boolean}
 *         True if accessible object is stale, false otherwise.
 */
function isStale(accessible) {
  let extraState = {};
  accessible.getState({}, extraState);
  // extraState.value is a bitmask. We are applying bitwise AND to mask out
  // irrelevant states.
  return !!(extraState.value & Ci.nsIAccessibleStates.EXT_STATE_STALE);
}

/**
 * Set of actors that expose accessibility tree information to the
 * devtools protocol clients.
 *
 * The |Accessibility| actor is the main entry point. It is used to request
 * an AccessibleWalker actor that caches the tree of Accessible actors.
 *
 * The |AccessibleWalker| actor is used to cache all seen Accessible actors as
 * well as observe all relevant accessible events.
 *
 * The |Accessible| actor provides information about a particular accessible
 * object, its properties, , attributes, states, relations, etc.
 */

/**
 * The AccessibleActor provides information about a given accessible object: its
 * role, name, states, etc.
 */
const AccessibleActor = ActorClassWithSpec(accessibleSpec, {
  initialize(walker, rawAccessible) {
    Actor.prototype.initialize.call(this, walker.conn);
    this.walker = walker;
    this.rawAccessible = rawAccessible;

    /**
     * Indicates if the raw accessible is no longer alive.
     *
     * @return Boolean
     */
    Object.defineProperty(this, "isDefunct", {
      get() {
        let defunct = isDefunct(this.rawAccessible);
        if (defunct) {
          delete this.isDefunct;
          this.isDefunct = true;
          return this.isDefunct;
        }

        return defunct;
      },
      configurable: true
    });
  },

  /**
   * Items returned by this actor should belong to the parent walker.
   */
  marshallPool() {
    return this.walker;
  },

  destroy() {
    Actor.prototype.destroy.call(this);
    this.walker = null;
    this.rawAccessible = null;
  },

  get role() {
    if (this.isDefunct) {
      return null;
    }
    return this.walker.a11yService.getStringRole(this.rawAccessible.role);
  },

  get name() {
    if (this.isDefunct) {
      return null;
    }
    return this.rawAccessible.name;
  },

  get value() {
    if (this.isDefunct) {
      return null;
    }
    return this.rawAccessible.value;
  },

  get description() {
    if (this.isDefunct) {
      return null;
    }
    return this.rawAccessible.description;
  },

  get help() {
    if (this.isDefunct) {
      return null;
    }
    return this.rawAccessible.help;
  },

  get keyboardShortcut() {
    if (this.isDefunct) {
      return null;
    }
    return this.rawAccessible.keyboardShortcut;
  },

  get childCount() {
    if (this.isDefunct) {
      return 0;
    }
    return this.rawAccessible.childCount;
  },

  get domNodeType() {
    if (this.isDefunct) {
      return 0;
    }
    return this.rawAccessible.DOMNode ? this.rawAccessible.DOMNode.nodeType : 0;
  },

  get parentAcc() {
    if (this.isDefunct) {
      return null;
    }
    return this.walker.addRef(this.rawAccessible.parent);
  },

  children() {
    let children = [];
    if (this.isDefunct) {
      return children;
    }

    for (let child = this.rawAccessible.firstChild; child; child = child.nextSibling) {
      children.push(this.walker.addRef(child));
    }
    return children;
  },

  get indexInParent() {
    if (this.isDefunct) {
      return -1;
    }

    try {
      return this.rawAccessible.indexInParent;
    } catch (e) {
      // Accessible is dead.
      return -1;
    }
  },

  get actions() {
    let actions = [];
    if (this.isDefunct) {
      return actions;
    }

    for (let i = 0; i < this.rawAccessible.actionCount; i++) {
      actions.push(this.rawAccessible.getActionDescription(i));
    }
    return actions;
  },

  get states() {
    if (this.isDefunct) {
      return [];
    }

    let state = {};
    let extState = {};
    this.rawAccessible.getState(state, extState);
    return [
      ...this.walker.a11yService.getStringStates(state.value, extState.value)
    ];
  },

  get attributes() {
    if (this.isDefunct || !this.rawAccessible.attributes) {
      return {};
    }

    let attributes = {};
    let attrsEnum = this.rawAccessible.attributes.enumerate();
    while (attrsEnum.hasMoreElements()) {
      let { key, value } = attrsEnum.getNext().QueryInterface(
        nsIPropertyElement);
      attributes[key] = value;
    }

    return attributes;
  },

  get bounds() {
    if (this.isDefunct) {
      return null;
    }

    let x = {}, y = {}, w = {}, h = {};
    try {
      this.rawAccessible.getBoundsInCSSPixels(x, y, w, h);
      x = x.value;
      y = y.value;
      w = w.value;
      h = h.value;
    } catch (e) {
      return null;
    }

    return { x, y, w, h };
  },

  form() {
    return {
      actor: this.actorID,
      role: this.role,
      name: this.name,
      value: this.value,
      description: this.description,
      help: this.help,
      keyboardShortcut: this.keyboardShortcut,
      childCount: this.childCount,
      domNodeType: this.domNodeType,
      indexInParent: this.indexInParent,
      states: this.states,
      actions: this.actions,
      attributes: this.attributes
    };
  }
});

/**
 * The AccessibleWalkerActor stores a cache of AccessibleActors that represent
 * accessible objects in a given document.
 *
 * It is also responsible for implicitely initializing and shutting down
 * accessibility engine by storing a reference to the XPCOM accessibility
 * service.
 */
const AccessibleWalkerActor = ActorClassWithSpec(accessibleWalkerSpec, {
  initialize(conn, tabActor) {
    Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;
    this.refMap = new Map();
    this.setA11yServiceGetter();
    this.onPick = this.onPick.bind(this);
    this.onHovered = this.onHovered.bind(this);
    this.onKey = this.onKey.bind(this);

    this.highlighter = CustomHighlighterActor(this, isXUL(this.rootWin) ?
      "XULWindowAccessibleHighlighter" : "AccessibleHighlighter");
  },

  setA11yServiceGetter() {
    DevToolsUtils.defineLazyGetter(this, "a11yService", () => {
      Services.obs.addObserver(this, "accessible-event");
      return Cc["@mozilla.org/accessibilityService;1"].getService(
        Ci.nsIAccessibilityService);
    });
  },

  get rootWin() {
    return this.tabActor && this.tabActor.window;
  },

  get rootDoc() {
    return this.tabActor && this.tabActor.window.document;
  },

  reset() {
    try {
      Services.obs.removeObserver(this, "accessible-event");
    } catch (e) {
      // Accessible event observer might not have been initialized if a11y
      // service was never used.
    }

    this.cancelPick();

    // Clean up accessible actors cache.
    if (this.refMap.size > 0) {
      try {
        if (this.rootDoc) {
          this.purgeSubtree(this.getRawAccessibleFor(this.rootDoc),
                            this.rootDoc);
        }
      } catch (e) {
        // Accessibility service might be already destroyed.
      } finally {
        this.refMap.clear();
      }
    }

    delete this.a11yService;
    this.setA11yServiceGetter();
  },

  destroy() {
    Actor.prototype.destroy.call(this);

    this.reset();

    this.highlighter.destroy();
    this.highlighter = null;

    this.tabActor = null;
    this.refMap = null;
  },

  getRef(rawAccessible) {
    return this.refMap.get(rawAccessible);
  },

  addRef(rawAccessible) {
    let actor = this.refMap.get(rawAccessible);
    if (actor) {
      return actor;
    }

    actor = new AccessibleActor(this, rawAccessible);
    this.manage(actor);
    this.refMap.set(rawAccessible, actor);

    return actor;
  },

  /**
   * Clean up accessible actors cache for a given accessible's subtree.
   *
   * @param  {null|nsIAccessible} rawAccessible
   * @param  {null|Object}   rawNode
   */
  purgeSubtree(rawAccessible, rawNode) {
    if (!rawAccessible) {
      return;
    }

    let actor = this.getRef(rawAccessible);
    if (actor && rawAccessible && !actor.isDefunct) {
      for (let child = rawAccessible.firstChild; child; child = child.nextSibling) {
        this.purgeSubtree(child);
      }
    }

    this.refMap.delete(rawAccessible);

    if (actor) {
      events.emit(this, "accessible-destroy", actor);
      actor.destroy();
    }

    // If corresponding DOMNode is a top level document, clear entire cache.
    if (rawNode && rawNode === this.rootDoc) {
      this.refMap.clear();
    }
  },

  /**
   * A helper method. Accessibility walker is assumed to have only 1 child which
   * is the top level document.
   */
  children() {
    return Promise.all([this.getDocument()]);
  },

  /**
   * A promise for a root document accessible actor that only resolves when its
   * corresponding document accessible object is fully loaded.
   *
   * @return {Promise}
   */
  getDocument() {
    if (!this.rootDoc || !this.rootDoc.documentElement) {
      return this.once("document-ready").then(docAcc => this.addRef(docAcc));
    }

    if (isXUL(this.rootWin)) {
      let doc = this.addRef(this.getRawAccessibleFor(this.rootDoc));
      return Promise.resolve(doc);
    }

    let doc = this.getRawAccessibleFor(this.rootDoc);
    if (isStale(doc)) {
      return this.once("document-ready").then(docAcc => this.addRef(docAcc));
    }

    return Promise.resolve(this.addRef(doc));
  },

  /**
   * Get an accessible actor for a domnode actor.
   * @param  {Object} domNode
   *         domnode actor for which accessible actor is being created.
   * @return {Promse}
   *         A promise that resolves when accessible actor is created for a
   *         domnode actor.
   */
  getAccessibleFor(domNode) {
    // We need to make sure that the document is loaded processed by a11y first.
    return this.getDocument().then(() =>
      this.addRef(this.getRawAccessibleFor(domNode.rawNode)));
  },

  /**
   * Get a raw accessible object for a raw node.
   * @param  {DOMNode} rawNode
   *         Raw node for which accessible object is being retrieved.
   * @return {nsIAccessible}
   *         Accessible object for a given DOMNode.
   */
  getRawAccessibleFor(rawNode) {
    // Accessible can only be retrieved iff accessibility service is enabled.
    if (!Services.appinfo.accessibilityEnabled) {
      return null;
    }

    return this.a11yService.getAccessibleFor(rawNode);
  },

  async getAncestry(accessible) {
    if (accessible.indexInParent === -1) {
      return [];
    }
    const doc = await this.getDocument();
    let ancestry = [];
    try {
      let parent = accessible;
      while (parent && (parent = parent.parentAcc) && parent != doc) {
        ancestry.push(parent);
      }
      ancestry.push(doc);
    } catch (error) {
      throw new Error(`Failed to get ancestor for ${accessible}: ${error}`);
    }

    return ancestry.map(parent => (
      { accessible: parent, children: parent.children() }));
  },

  /**
   * Accessible event observer function.
   *
   * @param {nsIAccessibleEvent} subject
   *                                      accessible event object.
   */
  observe(subject) {
    let event = subject.QueryInterface(nsIAccessibleEvent);
    let rawAccessible = event.accessible;
    let accessible = this.getRef(rawAccessible);

    if ((rawAccessible instanceof Ci.nsIAccessibleDocument) && !accessible) {
      let rootDocAcc = this.getRawAccessibleFor(this.rootDoc);
      if (rawAccessible === rootDocAcc && !isStale(rawAccessible)) {
        this.purgeSubtree(rawAccessible, event.DOMNode);
        // If it's a top level document notify listeners about the document
        // being ready.
        events.emit(this, "document-ready", rawAccessible);
      }
    }

    switch (event.eventType) {
      case EVENT_STATE_CHANGE:
        let { state, isEnabled } = event.QueryInterface(nsIAccessibleStateChangeEvent);
        let isBusy = state & Ci.nsIAccessibleStates.STATE_BUSY;
        // Accessible document is recreated.
        if (isBusy && !isEnabled && rawAccessible instanceof Ci.nsIAccessibleDocument) {
          // Remove its existing cache from tree.
          this.purgeSubtree(rawAccessible, event.DOMNode);
        }

        if (accessible) {
          // Only propagate state change events for active accessibles.
          if (isBusy && isEnabled) {
            if (rawAccessible instanceof Ci.nsIAccessibleDocument) {
              // Remove its existing cache from tree.
              this.purgeSubtree(rawAccessible, event.DOMNode);
            }
            return;
          }
          events.emit(accessible, "states-change", accessible.states);
        }

        break;
      case EVENT_NAME_CHANGE:
        if (accessible) {
          events.emit(accessible, "name-change", rawAccessible.name,
            event.DOMNode == this.rootDoc ?
              undefined : this.getRef(rawAccessible.parent), this);
        }
        break;
      case EVENT_VALUE_CHANGE:
        if (accessible) {
          events.emit(accessible, "value-change", rawAccessible.value);
        }
        break;
      case EVENT_DESCRIPTION_CHANGE:
        if (accessible) {
          events.emit(accessible, "description-change", rawAccessible.description);
        }
        break;
      case EVENT_HELP_CHANGE:
        if (accessible) {
          events.emit(accessible, "help-change", rawAccessible.help);
        }
        break;
      case EVENT_REORDER:
        if (accessible) {
          accessible.children().forEach(child =>
            events.emit(child, "index-in-parent-change", child.indexInParent));
          events.emit(accessible, "reorder", rawAccessible.childCount, this);
        }
        break;
      case EVENT_HIDE:
        this.purgeSubtree(rawAccessible);
        break;
      case EVENT_DEFACTION_CHANGE:
      case EVENT_ACTION_CHANGE:
        if (accessible) {
          events.emit(accessible, "actions-change", accessible.actions);
        }
        break;
      case EVENT_TEXT_CHANGED:
      case EVENT_TEXT_INSERTED:
      case EVENT_TEXT_REMOVED:
        if (accessible) {
          events.emit(accessible, "text-change", this);
          if (NAME_FROM_SUBTREE_RULE_ROLES.has(rawAccessible.role)) {
            events.emit(accessible, "name-change", rawAccessible.name,
              event.DOMNode == this.rootDoc ?
                undefined : this.getRef(rawAccessible.parent), this);
          }
        }
        break;
      case EVENT_DOCUMENT_ATTRIBUTES_CHANGED:
      case EVENT_OBJECT_ATTRIBUTE_CHANGED:
      case EVENT_TEXT_ATTRIBUTE_CHANGED:
        if (accessible) {
          events.emit(accessible, "attributes-change", accessible.attributes);
        }
        break;
      case EVENT_ACCELERATOR_CHANGE:
        if (accessible) {
          events.emit(accessible, "shortcut-change", rawAccessible.keyboardShortcut);
        }
        break;
      default:
        break;
    }
  },

  /**
   * Public method used to show an accessible object highlighter on the client
   * side.
   *
   * @param  {Object} accessible
   *         AccessibleActor to be highlighted.
   * @param  {Object} options
   *         Object used for passing options. Available options:
   *         - duration {Number}
   *                    Duration of time that the highlighter should be shown.
   * @return {Boolean}
   *         True if highlighter shows the accessible object.
   */
  highlightAccessible(accessible, options = {}) {
    let bounds = accessible.bounds;
    if (!bounds) {
      return false;
    }

    return this.highlighter.show({ rawNode: accessible.rawAccessible.DOMNode },
                                 { ...options, ...bounds });
  },

  /**
   * Public method used to hide an accessible object highlighter on the client
   * side.
   */
  unhighlight() {
    this.highlighter.hide();
  },

  /**
   * Picking state that indicates if picking is currently enabled and, if so,
   * what the current and hovered accessible objects are.
   */
  _isPicking: false,
  _currentAccessible: null,

  /**
   * Check is event handling is allowed.
   */
  _isEventAllowed: function({ view }) {
    return this.rootWin instanceof Ci.nsIDOMChromeWindow ||
           isWindowIncluded(this.rootWin, view);
  },

  _preventContentEvent(event) {
    event.stopPropagation();
    event.preventDefault();
  },

  /**
   * Click event handler for when picking is enabled.
   *
   * @param  {Object} event
   *         Current click event.
   */
  async onPick(event) {
    if (!this._isPicking) {
      return;
    }

    this._preventContentEvent(event);
    if (!this._isEventAllowed(event)) {
      return;
    }

    // If shift is pressed, this is only a preview click, send the event to
    // the client, but don't stop picking.
    if (event.shiftKey) {
      if (!this._currentAccessible) {
        this._currentAccessible = await this._findAndAttachAccessible(event);
      }
      events.emit(this, "picker-accessible-previewed", this._currentAccessible);
      return;
    }

    this._stopPickerListeners();
    this._isPicking = false;
    if (!this._currentAccessible) {
      this._currentAccessible = await this._findAndAttachAccessible(event);
    }
    events.emit(this, "picker-accessible-picked", this._currentAccessible);
  },

  /**
   * Hover event handler for when picking is enabled.
   *
   * @param  {Object} event
   *         Current hover event.
   */
  async onHovered(event) {
    if (!this._isPicking) {
      return;
    }

    this._preventContentEvent(event);
    if (!this._isEventAllowed(event)) {
      return;
    }

    let accessible = await this._findAndAttachAccessible(event);
    if (!accessible) {
      return;
    }

    if (this._currentAccessible !== accessible) {
      let { bounds } = accessible;
      if (bounds) {
        this.highlighter.show({ rawNode: event.originalTarget || event.target }, bounds);
      }

      events.emit(this, "picker-accessible-hovered", accessible);
      this._currentAccessible = accessible;
    }
  },

  /**
   * Keyboard event handler for when picking is enabled.
   *
   * @param  {Object} event
   *         Current keyboard event.
   */
  onKey(event) {
    if (!this._currentAccessible || !this._isPicking) {
      return;
    }

    this._preventContentEvent(event);
    if (!this._isEventAllowed(event)) {
      return;
    }

    /**
     * KEY: Action/scope
     * ENTER/CARRIAGE_RETURN: Picks current accessible
     * ESC/CTRL+SHIFT+C: Cancels picker
     */
    switch (event.keyCode) {
      // Select the element.
      case event.DOM_VK_RETURN:
        this._onPick(event);
        break;
      // Cancel pick mode.
      case event.DOM_VK_ESCAPE:
        this.cancelPick();
        events.emit(this, "picker-accessible-canceled");
        break;
      case event.DOM_VK_C:
        if ((IS_OSX && event.metaKey && event.altKey) ||
          (!IS_OSX && event.ctrlKey && event.shiftKey)) {
          this.cancelPick();
          events.emit(this, "picker-accessible-canceled");
        }
        break;
      default:
        break;
    }
  },

  /**
   * Picker method that starts picker content listeners.
   */
  pick: function() {
    if (!this._isPicking) {
      this._isPicking = true;
      this._startPickerListeners();
    }
  },

  /**
   * This pick method also focuses the highlighter's target window.
   */
  pickAndFocus: function() {
    this.pick();
    this.rootWin.focus();
  },

  /**
   * Find accessible object that corresponds to a DOMNode and attach (lookup its
   * ancestry to the root doc) to the AccessibilityWalker tree.
   *
   * @param  {Object} event
   *         Correspoinding content event.
   * @return {null|Object}
   *         Accessible object, if available, that corresponds to a DOM node.
   */
  async _findAndAttachAccessible(event) {
    let target = event.originalTarget || event.target;
    let rawAccessible;
    // Find a first accessible object in the target's ancestry, including
    // target. Note: not all DOM nodes have corresponding accessible objects
    // (for example, a <DIV> element that is used as a container for other
    // things) thus we need to find one that does.
    while (!rawAccessible && target) {
      rawAccessible = this.getRawAccessibleFor(target);
      target = target.parentNode;
    }
    // If raw accessible object is defunct or detached, no need to cache it and
    // its ancestry.
    if (!rawAccessible || isDefunct(rawAccessible) || rawAccessible.indexInParent < 0) {
      return null;
    }

    const doc = await this.getDocument();
    let accessible = this.addRef(rawAccessible);
    // There is a chance that ancestry lookup can fail if the accessible is in
    // the detached subtree. At that point the root accessible object would be
    // defunct and accessing it via parent property will throw.
    try {
      let parent = accessible;
      while (parent && parent != doc) {
        parent = parent.parentAcc;
      }
    } catch (error) {
      throw new Error(`Failed to get ancestor for ${accessible}: ${error}`);
    }

    return accessible;
  },

  /**
   * Start picker content listeners.
   */
  _startPickerListeners: function() {
    let target = this.tabActor.chromeEventHandler;
    target.addEventListener("mousemove", this.onHovered, true);
    target.addEventListener("click", this.onPick, true);
    target.addEventListener("mousedown", this._preventContentEvent, true);
    target.addEventListener("mouseup", this._preventContentEvent, true);
    target.addEventListener("dblclick", this._preventContentEvent, true);
    target.addEventListener("keydown", this.onKey, true);
    target.addEventListener("keyup", this._preventContentEvent, true);
  },

  /**
   * If content is still alive, stop picker content listeners.
   */
  _stopPickerListeners: function() {
    let target = this.tabActor.chromeEventHandler;

    if (!target) {
      return;
    }

    target.removeEventListener("mousemove", this.onHovered, true);
    target.removeEventListener("click", this.onPick, true);
    target.removeEventListener("mousedown", this._preventContentEvent, true);
    target.removeEventListener("mouseup", this._preventContentEvent, true);
    target.removeEventListener("dblclick", this._preventContentEvent, true);
    target.removeEventListener("keydown", this.onKey, true);
    target.removeEventListener("keyup", this._preventContentEvent, true);
  },

  /**
   * Cacncel picker pick. Remvoe all content listeners and hide the highlighter.
   */
  cancelPick: function() {
    this.highlighter.hide();

    if (this._isPicking) {
      this._stopPickerListeners();
      this._isPicking = false;
      this._currentAccessible = null;
    }
  }
});

/**
 * The AccessibilityActor is a top level container actor that initializes
 * accessible walker and is the top-most point of interaction for accessibility
 * tools UI.
 */
const AccessibilityActor = ActorClassWithSpec(accessibilitySpec, {
  initialize(conn, tabActor) {
    Actor.prototype.initialize.call(this, conn);

    this.initializedDeferred = defer();

    if (DebuggerServer.isInChildProcess) {
      this._msgName = `debug:${this.conn.prefix}accessibility`;
      this.conn.setupInParent({
        module: "devtools/server/actors/accessibility-parent",
        setupParent: "setupParentProcess"
      });

      this.onMessage = this.onMessage.bind(this);
      this.messageManager.addMessageListener(`${this._msgName}:event`, this.onMessage);
    } else {
      this.userPref = Services.prefs.getIntPref(PREF_ACCESSIBILITY_FORCE_DISABLED);
      Services.obs.addObserver(this, "a11y-consumers-changed");
      Services.prefs.addObserver(PREF_ACCESSIBILITY_FORCE_DISABLED, this);
      this.initializedDeferred.resolve();
    }

    Services.obs.addObserver(this, "a11y-init-or-shutdown");
    this.tabActor = tabActor;
  },

  bootstrap() {
    return this.initializedDeferred.promise.then(() => ({
      enabled: this.enabled,
      canBeEnabled: this.canBeEnabled,
      canBeDisabled: this.canBeDisabled
    }));
  },

  get enabled() {
    return Services.appinfo.accessibilityEnabled;
  },

  get canBeEnabled() {
    if (DebuggerServer.isInChildProcess) {
      return this._canBeEnabled;
    }

    return Services.prefs.getIntPref(PREF_ACCESSIBILITY_FORCE_DISABLED) < 1;
  },

  get canBeDisabled() {
    if (DebuggerServer.isInChildProcess) {
      return this._canBeDisabled;
    } else if (!this.enabled) {
      return true;
    }

    let { PlatformAPI } = JSON.parse(this.walker.a11yService.getConsumers());
    return !PlatformAPI;
  },

  /**
   * Getter for a message manager that corresponds to a current tab. It is onyl
   * used if the AccessibilityActor runs in the child process.
   *
   * @return {Object}
   *         Message manager that corresponds to the current content tab.
   */
  get messageManager() {
    if (!DebuggerServer.isInChildProcess) {
      throw new Error(
        "Message manager should only be used when actor is in child process.");
    }

    return this.conn.parentMessageManager;
  },

  onMessage(msg) {
    let { topic, data } = msg.data;

    switch (topic) {
      case "initialized":
        this._canBeEnabled = data.canBeEnabled;
        this._canBeDisabled = data.canBeDisabled;

        // Sometimes when the tool is reopened content process accessibility service is
        // not shut down yet because GC did not run in that process (though it did in
        // parent process and the service was shut down there). We need to sync the two
        // services if possible.
        if (!data.enabled && this.enabled && data.canBeEnabled) {
          this.messageManager.sendAsyncMessage(this._msgName, { action: "enable" });
        }

        this.initializedDeferred.resolve();
        break;
      case "can-be-disabled-change":
        this._canBeDisabled = data;
        events.emit(this, "can-be-disabled-change", this.canBeDisabled);
        break;

      case "can-be-enabled-change":
        this._canBeEnabled = data;
        events.emit(this, "can-be-enabled-change", this.canBeEnabled);
        break;

      default:
        break;
    }
  },

  /**
   * Enable acessibility service in the given process.
   */
  async enable() {
    if (this.enabled || !this.canBeEnabled) {
      return;
    }

    let initPromise = this.once("init");

    if (DebuggerServer.isInChildProcess) {
      this.messageManager.sendAsyncMessage(this._msgName, { action: "enable" });
    } else {
      // This executes accessibility service lazy getter and adds accessible
      // events observer.
      this.walker.a11yService;
    }

    await initPromise;
  },

  /**
   * Disable acessibility service in the given process.
   */
  async disable() {
    if (!this.enabled || !this.canBeDisabled) {
      return;
    }

    this.disabling = true;
    let shutdownPromise = this.once("shutdown");
    if (DebuggerServer.isInChildProcess) {
      this.messageManager.sendAsyncMessage(this._msgName, { action: "disable" });
    } else {
      // Set PREF_ACCESSIBILITY_FORCE_DISABLED to 1 to force disable
      // accessibility service. This is the only way to guarantee an immediate
      // accessibility service shutdown in all processes. This also prevents
      // accessibility service from starting up in the future.
      //
      // TODO: Introduce a shutdown method that is exposed via XPCOM on
      // accessibility service.
      Services.prefs.setIntPref(PREF_ACCESSIBILITY_FORCE_DISABLED, 1);
      // Set PREF_ACCESSIBILITY_FORCE_DISABLED back to previous default or user
      // set value. This will not start accessibility service until the user
      // activates it again. It simply ensures that accessibility service can
      // start again (when value is below 1).
      Services.prefs.setIntPref(PREF_ACCESSIBILITY_FORCE_DISABLED, this.userPref);
    }

    await shutdownPromise;
    delete this.disabling;
  },

  /**
   * Observe Accessibility service init and shutdown events. It relays these
   * events to AccessibilityFront iff the event is fired for the a11y service
   * that lives in the same process.
   *
   * @param  {null} subject
   *         Not used.
   * @param  {String} topic
   *         Name of the a11y service event: "a11y-init-or-shutdown".
   * @param  {String} data
   *         "0" corresponds to shutdown and "1" to init.
   */
  observe(subject, topic, data) {
    if (topic === "a11y-init-or-shutdown") {
      // This event is fired when accessibility service is initialized or shut
      // down. "init" and "shutdown" events are only relayed when the enabled
      // state matches the event (e.g. the event came from the same process as
      // the actor).
      const enabled = data === "1";
      if (enabled && this.enabled) {
        events.emit(this, "init");
      } else if (!enabled && !this.enabled) {
        if (this.walker) {
          this.walker.reset();
        }

        events.emit(this, "shutdown");
      }
    } else if (topic === "a11y-consumers-changed") {
      // This event is fired when accessibility service consumers change. There
      // are 3 possible consumers of a11y service: XPCOM, PlatformAPI (e.g.
      // screen readers) and MainProcess. PlatformAPI consumer can only be set
      // in parent process, and MainProcess consumer can only be set in child
      // process. We only care about PlatformAPI consumer changes because when
      // set, we can no longer disable accessibility service.
      let { PlatformAPI } = JSON.parse(data);
      events.emit(this, "can-be-disabled-change", !PlatformAPI);
    } else if (!this.disabling && topic === "nsPref:changed" &&
               data === PREF_ACCESSIBILITY_FORCE_DISABLED) {
      // PREF_ACCESSIBILITY_FORCE_DISABLED preference change event. When set to
      // >=1, it means that the user wants to disable accessibility service and
      // prevent it from starting in the future. Note: we also check
      // this.disabling state when handling this pref change because this is how
      // we disable the accessibility inspector itself.
      events.emit(this, "can-be-enabled-change", this.canBeEnabled);
    }
  },

  /**
   * Get or create AccessibilityWalker actor, similar to WalkerActor.
   *
   * @return {Object}
   *         AccessibleWalkerActor for the current tab.
   */
  getWalker() {
    if (!this.walker) {
      this.walker = new AccessibleWalkerActor(this.conn, this.tabActor);
    }
    return this.walker;
  },

  /**
   * Destroy accessibility service actor. This method also shutsdown
   * accessibility service if possible.
   */
  async destroy() {
    if (this.destroyed) {
      await this.destroyed;
      return;
    }

    let resolver;
    this.destroyed = new Promise(resolve => {
      resolver = resolve;
    });

    if (this.walker) {
      this.walker.reset();
    }

    Services.obs.removeObserver(this, "a11y-init-or-shutdown");
    if (DebuggerServer.isInChildProcess) {
      this.messageManager.removeMessageListener(`${this._msgName}:event`,
                                                this.onMessage);
    } else {
      Services.obs.removeObserver(this, "a11y-consumers-changed");
      Services.prefs.removeObserver(PREF_ACCESSIBILITY_FORCE_DISABLED, this);
    }

    Actor.prototype.destroy.call(this);
    this.walker = null;
    this.tabActor = null;
    resolver();
  }
});

exports.AccessibleActor = AccessibleActor;
exports.AccessibleWalkerActor = AccessibleWalkerActor;
exports.AccessibilityActor = AccessibilityActor;
