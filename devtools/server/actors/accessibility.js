/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const Services = require("Services");
const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const defer = require("devtools/shared/defer");
const events = require("devtools/shared/event-emitter");
const {
  accessibleSpec,
  accessibleWalkerSpec,
  accessibilitySpec
} = require("devtools/shared/specs/accessibility");

const nsIAccessibleEvent = Ci.nsIAccessibleEvent;
const nsIAccessibleStateChangeEvent = Ci.nsIAccessibleStateChangeEvent;
const nsIPropertyElement = Ci.nsIPropertyElement;

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

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/**
 * Set of actors that expose accessibility tree information to the
 * devtools protocol clients.
 *
 * The |Accessibility| actor is the main entry point. It is used to request
 * an AccessibleWalker actor that caches the tree of Accessible actors.
 *
 * The |AccessibleWalker| actor is used to cache all seen Accessible actors as
 * well as observe all relevant accesible events.
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
        let defunct = false;

        try {
          let extState = {};
          this.rawAccessible.getState({}, extState);
          // extState.value is a bitmask. We are applying bitwise AND to mask out
          // irrelelvant states.
          defunct = !!(extState.value & Ci.nsIAccessibleStates.EXT_STATE_DEFUNCT);
        } catch (e) {
          defunct = true;
        }

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

  getIndexInParent() {
    if (this.isDefunct) {
      return -1;
    }
    return this.rawAccessible.indexInParent;
  },

  getActions() {
    let actions = [];
    if (this.isDefunct) {
      return actions;
    }

    for (let i = 0; i < this.rawAccessible.actionCount; i++) {
      actions.push(this.rawAccessible.getActionDescription(i));
    }
    return actions;
  },

  getState() {
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

  getAttributes() {
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
      walker: this.walker.form()
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
    this.rootWin = tabActor.window;
    this.rootDoc = tabActor.window.document;
    this.refMap = new Map();
    // Accessibility Walker should only be considered ready, when raw accessible
    // object for root document is fully initialized (e.g. does not have a
    // 'busy' state)
    this.readyDeferred = defer();

    DevToolsUtils.defineLazyGetter(this, "a11yService", () => {
      Services.obs.addObserver(this, "accessible-event");
      return Cc["@mozilla.org/accessibilityService;1"].getService(
        Ci.nsIAccessibilityService);
    });

    this.onLoad = this.onLoad.bind(this);
    this.onUnload = this.onUnload.bind(this);

    events.on(tabActor, "will-navigate", this.onUnload);
    events.on(tabActor, "window-ready", this.onLoad);
  },

  onUnload({ window }) {
    let doc = window.document;
    let actor = this.getRef(doc);

    // If an accessible actor was never created for document, then there's
    // nothing to clean up.
    if (!actor) {
      return;
    }

    // Purge document's subtree from accessible actors cache.
    this.purgeSubtree(this.a11yService.getAccessibleFor(this.doc));
    // If document is a root document, clear it's reference and cache.
    if (this.rootDoc === doc) {
      this.rootDoc = null;
      this.refMap.clear();
      this.readyDeferred = defer();
    }
  },

  onLoad({ window, isTopLevel }) {
    if (isTopLevel) {
      // If root document is dead, unload it and clean up.
      if (this.rootDoc && !Cu.isDeadWrapper(this.rootDoc) &&
          this.rootDoc.defaultView) {
        this.onUnload({ window: this.rootDoc.defaultView });
      }

      this.rootWin = window;
      this.rootDoc = window.document;
    }
  },

  destroy() {
    if (this._destroyed) {
      return;
    }

    this._destroyed = true;

    try {
      Services.obs.removeObserver(this, "accessible-event");
    } catch (e) {
      // Accessible event observer might not have been initialized if a11y
      // service was never used.
    }

    // Clean up accessible actors cache.
    if (this.refMap.size > 0) {
      this.purgeSubtree(this.a11yService.getAccessibleFor(this.rootDoc));
      this.refMap.clear();
    }

    events.off(this.tabActor, "will-navigate", this.onUnload);
    events.off(this.tabActor, "window-ready", this.onLoad);

    this.onLoad = null;
    this.onUnload = null;
    delete this.a11yService;
    this.tabActor = null;
    this.rootDoc = null;
    this.refMap = null;

    Actor.prototype.destroy.call(this);
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
   * @param  {nsIAccessible} rawAccessible
   */
  purgeSubtree(rawAccessible) {
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
    let doc = this.addRef(this.a11yService.getAccessibleFor(this.rootDoc));
    let states = doc.getState();

    if (states.includes("busy")) {
      return this.readyDeferred.promise.then(() => doc);
    }

    this.readyDeferred.resolve();
    return Promise.resolve(doc);
  },

  getAccessibleFor(domNode) {
    // We need to make sure that the document is loaded processed by a11y first.
    return this.getDocument().then(() =>
      this.addRef(this.a11yService.getAccessibleFor(domNode.rawNode)));
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

    switch (event.eventType) {
      case EVENT_STATE_CHANGE:
        let { state, isEnabled } = event.QueryInterface(nsIAccessibleStateChangeEvent);
        let states = [...this.a11yService.getStringStates(state, 0)];

        if (states.includes("busy") && !isEnabled) {
          let { DOMNode } = event;
          // If debugging chrome, wait for top level content document loaded,
          // otherwise wait for root document loaded.
          if (DOMNode == this.rootDoc || (
            this.rootDoc.documentElement.namespaceURI === XUL_NS &&
            this.rootWin.gBrowser.selectedBrowser.contentDocument == DOMNode)) {
            this.readyDeferred.resolve();
          }
        }

        if (accessible) {
          // Only propagate state change events for active accessibles.
          if (states.includes("busy") && isEnabled) {
            return;
          }
          events.emit(accessible, "state-change", accessible.getState());
        }

        break;
      case EVENT_NAME_CHANGE:
        if (accessible) {
          events.emit(accessible, "name-change", rawAccessible.name,
            event.DOMNode == this.rootDoc ?
              undefined : this.getRef(rawAccessible.parent));
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
          events.emit(accessible, "reorder", rawAccessible.childCount);
        }
        break;
      case EVENT_HIDE:
        this.purgeSubtree(rawAccessible);
        break;
      case EVENT_DEFACTION_CHANGE:
      case EVENT_ACTION_CHANGE:
        if (accessible) {
          events.emit(accessible, "actions-change", accessible.getActions());
        }
        break;
      case EVENT_TEXT_CHANGED:
      case EVENT_TEXT_INSERTED:
      case EVENT_TEXT_REMOVED:
        if (accessible) {
          events.emit(accessible, "text-change");
        }
        break;
      case EVENT_DOCUMENT_ATTRIBUTES_CHANGED:
      case EVENT_OBJECT_ATTRIBUTE_CHANGED:
      case EVENT_TEXT_ATTRIBUTE_CHANGED:
        if (accessible) {
          events.emit(accessible, "attributes-change", accessible.getAttributes());
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
    this.tabActor = tabActor;
  },

  getWalker() {
    if (!this.walker) {
      this.walker = new AccessibleWalkerActor(this.conn, this.tabActor);
    }
    return this.walker;
  },

  destroy() {
    Actor.prototype.destroy.call(this);
    this.walker.destroy();
    this.walker = null;
    this.tabActor = null;
  }
});

exports.AccessibleActor = AccessibleActor;
exports.AccessibleWalkerActor = AccessibleWalkerActor;
exports.AccessibilityActor = AccessibilityActor;
