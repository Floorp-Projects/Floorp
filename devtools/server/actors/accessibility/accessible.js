/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cu } = require("chrome");
const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { accessibleSpec } = require("devtools/shared/specs/accessibility");
const {
  accessibility: { AUDIT_TYPE },
} = require("devtools/shared/constants");

loader.lazyRequireGetter(
  this,
  "getContrastRatioFor",
  "devtools/server/actors/accessibility/audit/contrast",
  true
);
loader.lazyRequireGetter(
  this,
  "auditKeyboard",
  "devtools/server/actors/accessibility/audit/keyboard",
  true
);
loader.lazyRequireGetter(
  this,
  "auditTextLabel",
  "devtools/server/actors/accessibility/audit/text-label",
  true
);
loader.lazyRequireGetter(
  this,
  "isDefunct",
  "devtools/server/actors/utils/accessibility",
  true
);
loader.lazyRequireGetter(
  this,
  "findCssSelector",
  "devtools/shared/inspector/css-logic",
  true
);
loader.lazyRequireGetter(this, "events", "devtools/shared/event-emitter");
loader.lazyRequireGetter(
  this,
  "getBounds",
  "devtools/server/actors/highlighters/utils/accessibility",
  true
);

const RELATIONS_TO_IGNORE = new Set([
  Ci.nsIAccessibleRelation.RELATION_CONTAINING_APPLICATION,
  Ci.nsIAccessibleRelation.RELATION_CONTAINING_TAB_PANE,
  Ci.nsIAccessibleRelation.RELATION_CONTAINING_WINDOW,
  Ci.nsIAccessibleRelation.RELATION_PARENT_WINDOW_OF,
  Ci.nsIAccessibleRelation.RELATION_SUBWINDOW_OF,
]);

const nsIAccessibleRole = Ci.nsIAccessibleRole;
const TEXT_ROLES = new Set([
  nsIAccessibleRole.ROLE_TEXT_LEAF,
  nsIAccessibleRole.ROLE_STATICTEXT,
]);

const STATE_DEFUNCT = Ci.nsIAccessibleStates.EXT_STATE_DEFUNCT;
const CSS_TEXT_SELECTOR = "#text";

/**
 * Get node inforamtion such as nodeType and the unique CSS selector for the node.
 * @param  {DOMNode} node
 *         Node for which to get the information.
 * @return {Object}
 *         Information about the type of the node and how to locate it.
 */
function getNodeDescription(node) {
  if (!node || Cu.isDeadWrapper(node)) {
    return { nodeType: undefined, nodeCssSelector: "" };
  }

  const { nodeType } = node;
  return {
    nodeType,
    // If node is a text node, we find a unique CSS selector for its parent and add a
    // CSS_TEXT_SELECTOR postfix to indicate that it's a text node.
    nodeCssSelector:
      nodeType === Node.TEXT_NODE
        ? `${findCssSelector(node.parentNode)}${CSS_TEXT_SELECTOR}`
        : findCssSelector(node),
  };
}

/**
 * Get a snapshot of the nsIAccessible object including its subtree. None of the subtree
 * queried here is cached via accessible walker's refMap.
 * @param  {nsIAccessible} acc
 *         Accessible object to take a snapshot of.
 * @param  {nsIAccessibilityService} a11yService
 *         Accessibility service instance in the current process, used to get localized
 *         string representation of various accessible properties.
 * @return {JSON}
 *         JSON snapshot of the accessibility tree with root at current accessible.
 */
function getSnapshot(acc, a11yService) {
  if (isDefunct(acc)) {
    return {
      states: [a11yService.getStringStates(0, STATE_DEFUNCT)],
    };
  }

  const actions = [];
  for (let i = 0; i < acc.actionCount; i++) {
    actions.push(acc.getActionDescription(i));
  }

  const attributes = {};
  if (acc.attributes) {
    for (const { key, value } of acc.attributes.enumerate()) {
      attributes[key] = value;
    }
  }

  const state = {};
  const extState = {};
  acc.getState(state, extState);
  const states = [...a11yService.getStringStates(state.value, extState.value)];

  const children = [];
  for (let child = acc.firstChild; child; child = child.nextSibling) {
    children.push(getSnapshot(child, a11yService));
  }

  const { nodeType, nodeCssSelector } = getNodeDescription(acc.DOMNode);
  return {
    name: acc.name,
    role: a11yService.getStringRole(acc.role),
    actions,
    value: acc.value,
    nodeCssSelector,
    nodeType,
    description: acc.description,
    keyboardShortcut: acc.accessKey || acc.keyboardShortcut,
    childCount: acc.childCount,
    indexInParent: acc.indexInParent,
    states,
    children,
    attributes,
  };
}

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

  get isDestroyed() {
    return this.actorID == null;
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

    for (
      let child = this.rawAccessible.firstChild;
      child;
      child = child.nextSibling
    ) {
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

    let x = {},
      y = {},
      w = {},
      h = {};
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
    const left = x,
      right = x + w,
      top = y,
      bottom = y + h;
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

    const relations = [
      ...this.rawAccessible.getRelations().enumerate(Ci.nsIAccessibleRelation),
    ];
    if (relations.length === 0) {
      return relationObjects;
    }

    const doc = await this.walker.getDocument();
    relations.forEach(relation => {
      if (RELATIONS_TO_IGNORE.has(relation.relationType)) {
        return;
      }

      const type = this.walker.a11yService.getStringRelationType(
        relation.relationType
      );
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
      childCount: this.childCount,
      checks: this._lastAudit,
    };
  },

  /**
   * Provide additional (full) information about the accessible object that is
   * otherwise missing from the form.
   *
   * @return {Object}
   *         Object that contains accessible object information such as states,
   *         actions, attributes, etc.
   */
  hydrate() {
    return {
      value: this.value,
      description: this.description,
      keyboardShortcut: this.keyboardShortcut,
      domNodeType: this.domNodeType,
      indexInParent: this.indexInParent,
      states: this.states,
      actions: this.actions,
      attributes: this.attributes,
    };
  },

  _isValidTextLeaf(rawAccessible) {
    return (
      !isDefunct(rawAccessible) &&
      TEXT_ROLES.has(rawAccessible.role) &&
      rawAccessible.name &&
      rawAccessible.name.trim().length > 0
    );
  },

  /**
   * Calculate the contrast ratio of the given accessible.
   */
  async _getContrastRatio() {
    if (!this._isValidTextLeaf(this.rawAccessible)) {
      return null;
    }

    const { bounds } = this;
    if (!bounds) {
      return null;
    }

    const { DOMNode: rawNode } = this.rawAccessible;
    const win = rawNode.ownerGlobal;

    // Keep the reference to the walker actor in case the actor gets destroyed
    // during the colour contrast ratio calculation.
    const { walker } = this;
    walker.clearStyles(win);
    const contrastRatio = await getContrastRatioFor(rawNode.parentNode, {
      bounds: getBounds(win, bounds),
      win,
    });

    walker.restoreStyles(win);

    return contrastRatio;
  },

  /**
   * Run an accessibility audit for a given audit type.
   * @param {String} type
   *        Type of an audit (Check AUDIT_TYPE in devtools/shared/constants
   *        to see available audit types).
   *
   * @return {null|Object}
   *         Object that contains accessible audit data for a given type or null
   *         if there's nothing to report for this accessible.
   */
  _getAuditByType(type) {
    switch (type) {
      case AUDIT_TYPE.CONTRAST:
        return this._getContrastRatio();
      case AUDIT_TYPE.KEYBOARD:
        // Determine if keyboard accessibility is lacking where it is necessary.
        return auditKeyboard(this.rawAccessible);
      case AUDIT_TYPE.TEXT_LABEL:
        // Determine if text alternative is missing for an accessible where it
        // is necessary.
        return auditTextLabel(this.rawAccessible);
      default:
        return null;
    }
  },

  /**
   * Audit the state of the accessible object.
   *
   * @param  {Object} options
   *         Options for running audit, may include:
   *         - types: Array of audit types to be performed during audit.
   *
   * @return {Object|null}
   *         Audit results for the accessible object.
   */
  audit(options = {}) {
    if (this._auditing) {
      return this._auditing;
    }

    const { types } = options;
    let auditTypes = Object.values(AUDIT_TYPE);
    if (types && types.length > 0) {
      auditTypes = auditTypes.filter(auditType => types.includes(auditType));
    }

    // More audit steps will be added here in the near future. In addition to
    // colour contrast ratio we will add autits for to the missing names,
    // invalid states, etc. (For example see bug 1518808).
    this._auditing = Promise.all(
      auditTypes.map(auditType => this._getAuditByType(auditType))
    )
      .then(results => {
        if (this.isDefunct || this.isDestroyed) {
          return null;
        }

        const audit = results.reduce((auditResults, result, index) => {
          auditResults[auditTypes[index]] = result;
          return auditResults;
        }, {});
        this._lastAudit = this._lastAudit || {};
        Object.assign(this._lastAudit, audit);
        events.emit(this, "audited", audit);

        return audit;
      })
      .catch(error => {
        if (!this.isDefunct && !this.isDestroyed) {
          throw error;
        }
        return null;
      })
      .finally(() => {
        this._auditing = null;
      });

    return this._auditing;
  },

  snapshot() {
    return getSnapshot(this.rawAccessible, this.walker.a11yService);
  },
});

exports.AccessibleActor = AccessibleActor;
