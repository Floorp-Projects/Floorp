/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { accessibleSpec } = require("devtools/shared/specs/accessibility");

loader.lazyRequireGetter(this, "getContrastRatioFor", "devtools/server/actors/utils/accessibility", true);
loader.lazyRequireGetter(this, "isDefunct", "devtools/server/actors/utils/accessibility", true);

const nsIAccessibleRelation = Ci.nsIAccessibleRelation;
const RELATIONS_TO_IGNORE = new Set([
  nsIAccessibleRelation.RELATION_CONTAINING_APPLICATION,
  nsIAccessibleRelation.RELATION_CONTAINING_TAB_PANE,
  nsIAccessibleRelation.RELATION_CONTAINING_WINDOW,
  nsIAccessibleRelation.RELATION_PARENT_WINDOW_OF,
  nsIAccessibleRelation.RELATION_SUBWINDOW_OF,
]);

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
        const defunct = isDefunct(this.rawAccessible);
        if (defunct) {
          delete this.isDefunct;
          this.isDefunct = true;
          return this.isDefunct;
        }

        return defunct;
      },
      configurable: true,
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

  get keyboardShortcut() {
    if (this.isDefunct) {
      return null;
    }
    // Gecko accessibility exposes two key bindings: Accessible::AccessKey and
    // Accessible::KeyboardShortcut. The former is used for accesskey, where the latter
    // is used for global shortcuts defined by XUL menu items, etc. Here - do what the
    // Windows implementation does: try AccessKey first, and if that's empty, use
    // KeyboardShortcut.
    return this.rawAccessible.accessKey || this.rawAccessible.keyboardShortcut;
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
    const children = [];
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
    const actions = [];
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

    const state = {};
    const extState = {};
    this.rawAccessible.getState(state, extState);
    return [
      ...this.walker.a11yService.getStringStates(state.value, extState.value),
    ];
  },

  get attributes() {
    if (this.isDefunct || !this.rawAccessible.attributes) {
      return {};
    }

    const attributes = {};
    for (const { key, value } of this.rawAccessible.attributes.enumerate()) {
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

    // Check if accessible bounds are invalid.
    const left = x, right = x + w, top = y, bottom = y + h;
    if (left === right || top === bottom) {
      return null;
    }

    return { x, y, w, h };
  },

  async getRelations() {
    const relationObjects = [];
    if (this.isDefunct) {
      return relationObjects;
    }

    const relations =
      [...this.rawAccessible.getRelations().enumerate(nsIAccessibleRelation)];
    if (relations.length === 0) {
      return relationObjects;
    }

    const doc = await this.walker.getDocument();
    relations.forEach(relation => {
      if (RELATIONS_TO_IGNORE.has(relation.relationType)) {
        return;
      }

      const type = this.walker.a11yService.getStringRelationType(relation.relationType);
      const targets = [...relation.getTargets().enumerate(Ci.nsIAccessible)];
      let relationObject;
      for (const target of targets) {
        // Target of the relation is not part of the current root document.
        if (target.rootDocument !== doc.rawAccessible) {
          continue;
        }

        let targetAcc;
        try {
          targetAcc = this.walker.attachAccessible(target, doc.rawAccessible);
        } catch (e) {
          // Target is not available.
        }

        if (targetAcc) {
          if (!relationObject) {
            relationObject = { type, targets: [] };
          }

          relationObject.targets.push(targetAcc);
        }
      }

      if (relationObject) {
        relationObjects.push(relationObject);
      }
    });

    return relationObjects;
  },

  form() {
    return {
      actor: this.actorID,
      role: this.role,
      name: this.name,
      value: this.value,
      description: this.description,
      keyboardShortcut: this.keyboardShortcut,
      childCount: this.childCount,
      domNodeType: this.domNodeType,
      indexInParent: this.indexInParent,
      states: this.states,
      actions: this.actions,
      attributes: this.attributes,
    };
  },

  _isValidTextLeaf(rawAccessible) {
    return !isDefunct(rawAccessible) &&
           rawAccessible.role === Ci.nsIAccessibleRole.ROLE_TEXT_LEAF &&
           rawAccessible.name && rawAccessible.name.trim().length > 0;
  },

  get _nonEmptyTextLeafs() {
    return this.children().filter(child => this._isValidTextLeaf(child.rawAccessible));
  },

  /**
   * Calculate the contrast ratio of the given accessible.
   */
  _getContrastRatio() {
    if (!this._isValidTextLeaf(this.rawAccessible)) {
      return null;
    }

    return getContrastRatioFor(this.rawAccessible.DOMNode.parentNode, {
      bounds: this.bounds,
      contexts: this.walker.contexts,
      win: this.walker.rootWin,
    });
  },

  /**
   * Audit the state of the accessible object.
   *
   * @return {Object|null}
   *         Audit results for the accessible object.
  */
  get audit() {
    return this.isDefunct ? null : {
      contrastRatio: this._getContrastRatio(),
    };
  },
});

exports.AccessibleActor = AccessibleActor;
