/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci} = require("chrome");
const promise = require("promise");
const {Rule} = require("devtools/client/inspector/rules/models/rule");
const {promiseWarn} = require("devtools/client/inspector/shared/utils");
const {ELEMENT_STYLE} = require("devtools/server/actors/styles");
const {XPCOMUtils} = require("resource://gre/modules/XPCOMUtils.jsm");

loader.lazyGetter(this, "PSEUDO_ELEMENTS", () => {
  return domUtils.getCSSPseudoElementNames();
});

XPCOMUtils.defineLazyGetter(this, "domUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});

/**
 * ElementStyle is responsible for the following:
 *   Keeps track of which properties are overridden.
 *   Maintains a list of Rule objects for a given element.
 *
 * @param {Element} element
 *        The element whose style we are viewing.
 * @param {CssRuleView} ruleView
 *        The instance of the rule-view panel.
 * @param {Object} store
 *        The ElementStyle can use this object to store metadata
 *        that might outlast the rule view, particularly the current
 *        set of disabled properties.
 * @param {PageStyleFront} pageStyle
 *        Front for the page style actor that will be providing
 *        the style information.
 * @param {Boolean} showUserAgentStyles
 *        Should user agent styles be inspected?
 */
function ElementStyle(element, ruleView, store, pageStyle,
    showUserAgentStyles) {
  this.element = element;
  this.ruleView = ruleView;
  this.store = store || {};
  this.pageStyle = pageStyle;
  this.showUserAgentStyles = showUserAgentStyles;
  this.rules = [];

  // We don't want to overwrite this.store.userProperties so we only create it
  // if it doesn't already exist.
  if (!("userProperties" in this.store)) {
    this.store.userProperties = new UserProperties();
  }

  if (!("disabled" in this.store)) {
    this.store.disabled = new WeakMap();
  }
}

ElementStyle.prototype = {
  // The element we're looking at.
  element: null,

  destroy: function () {
    if (this.destroyed) {
      return;
    }
    this.destroyed = true;

    for (let rule of this.rules) {
      if (rule.editor) {
        rule.editor.destroy();
      }
    }
  },

  /**
   * Called by the Rule object when it has been changed through the
   * setProperty* methods.
   */
  _changed: function () {
    if (this.onChanged) {
      this.onChanged();
    }
  },

  /**
   * Refresh the list of rules to be displayed for the active element.
   * Upon completion, this.rules[] will hold a list of Rule objects.
   *
   * Returns a promise that will be resolved when the elementStyle is
   * ready.
   */
  populate: function () {
    let populated = this.pageStyle.getApplied(this.element, {
      inherited: true,
      matchedSelectors: true,
      filter: this.showUserAgentStyles ? "ua" : undefined,
    }).then(entries => {
      if (this.destroyed) {
        return promise.resolve(undefined);
      }

      if (this.populated !== populated) {
        // Don't care anymore.
        return promise.resolve(undefined);
      }

      // Store the current list of rules (if any) during the population
      // process.  They will be reused if possible.
      let existingRules = this.rules;

      this.rules = [];

      for (let entry of entries) {
        this._maybeAddRule(entry, existingRules);
      }

      // Mark overridden computed styles.
      this.markOverriddenAll();

      this._sortRulesForPseudoElement();

      // We're done with the previous list of rules.
      for (let r of existingRules) {
        if (r && r.editor) {
          r.editor.destroy();
        }
      }

      return undefined;
    }).then(null, e => {
      // populate is often called after a setTimeout,
      // the connection may already be closed.
      if (this.destroyed) {
        return promise.resolve(undefined);
      }
      return promiseWarn(e);
    });
    this.populated = populated;
    return this.populated;
  },

  /**
   * Put pseudo elements in front of others.
   */
  _sortRulesForPseudoElement: function () {
    this.rules = this.rules.sort((a, b) => {
      return (a.pseudoElement || "z") > (b.pseudoElement || "z");
    });
  },

  /**
   * Add a rule if it's one we care about.  Filters out duplicates and
   * inherited styles with no inherited properties.
   *
   * @param {Object} options
   *        Options for creating the Rule, see the Rule constructor.
   * @param {Array} existingRules
   *        Rules to reuse if possible.  If a rule is reused, then it
   *        it will be deleted from this array.
   * @return {Boolean} true if we added the rule.
   */
  _maybeAddRule: function (options, existingRules) {
    // If we've already included this domRule (for example, when a
    // common selector is inherited), ignore it.
    if (options.rule &&
        this.rules.some(rule => rule.domRule === options.rule)) {
      return false;
    }

    if (options.system) {
      return false;
    }

    let rule = null;

    // If we're refreshing and the rule previously existed, reuse the
    // Rule object.
    if (existingRules) {
      let ruleIndex = existingRules.findIndex((r) => r.matches(options));
      if (ruleIndex >= 0) {
        rule = existingRules[ruleIndex];
        rule.refresh(options);
        existingRules.splice(ruleIndex, 1);
      }
    }

    // If this is a new rule, create its Rule object.
    if (!rule) {
      rule = new Rule(this, options);
    }

    // Ignore inherited rules with no visible properties.
    if (options.inherited && !rule.hasAnyVisibleProperties()) {
      return false;
    }

    this.rules.push(rule);
    return true;
  },

  /**
   * Calls markOverridden with all supported pseudo elements
   */
  markOverriddenAll: function () {
    this.markOverridden();
    for (let pseudo of PSEUDO_ELEMENTS) {
      this.markOverridden(pseudo);
    }
  },

  /**
   * Mark the properties listed in this.rules for a given pseudo element
   * with an overridden flag if an earlier property overrides it.
   *
   * @param {String} pseudo
   *        Which pseudo element to flag as overridden.
   *        Empty string or undefined will default to no pseudo element.
   */
  markOverridden: function (pseudo = "") {
    // Gather all the text properties applied by these rules, ordered
    // from more- to less-specific. Text properties from keyframes rule are
    // excluded from being marked as overridden since a number of criteria such
    // as time, and animation overlay are required to be check in order to
    // determine if the property is overridden.
    let textProps = [];
    for (let rule of this.rules) {
      if ((rule.matchedSelectors.length > 0 ||
           rule.domRule.type === ELEMENT_STYLE) &&
          rule.pseudoElement === pseudo && !rule.keyframes) {
        for (let textProp of rule.textProps.slice(0).reverse()) {
          if (textProp.enabled) {
            textProps.push(textProp);
          }
        }
      }
    }

    // Gather all the computed properties applied by those text
    // properties.
    let computedProps = [];
    for (let textProp of textProps) {
      computedProps = computedProps.concat(textProp.computed);
    }

    // Walk over the computed properties.  As we see a property name
    // for the first time, mark that property's name as taken by this
    // property.
    //
    // If we come across a property whose name is already taken, check
    // its priority against the property that was found first:
    //
    //   If the new property is a higher priority, mark the old
    //   property overridden and mark the property name as taken by
    //   the new property.
    //
    //   If the new property is a lower or equal priority, mark it as
    //   overridden.
    //
    // _overriddenDirty will be set on each prop, indicating whether its
    // dirty status changed during this pass.
    let taken = {};
    for (let computedProp of computedProps) {
      let earlier = taken[computedProp.name];

      // Prevent -webkit-gradient from being selected after unchecking
      // linear-gradient in this case:
      //  -moz-linear-gradient: ...;
      //  -webkit-linear-gradient: ...;
      //  linear-gradient: ...;
      if (!computedProp.textProp.isValid()) {
        computedProp.overridden = true;
        continue;
      }
      let overridden;
      if (earlier &&
          computedProp.priority === "important" &&
          earlier.priority !== "important" &&
          (earlier.textProp.rule.inherited ||
           !computedProp.textProp.rule.inherited)) {
        // New property is higher priority.  Mark the earlier property
        // overridden (which will reverse its dirty state).
        earlier._overriddenDirty = !earlier._overriddenDirty;
        earlier.overridden = true;
        overridden = false;
      } else {
        overridden = !!earlier;
      }

      computedProp._overriddenDirty =
        (!!computedProp.overridden !== overridden);
      computedProp.overridden = overridden;
      if (!computedProp.overridden && computedProp.textProp.enabled) {
        taken[computedProp.name] = computedProp;
      }
    }

    // For each TextProperty, mark it overridden if all of its
    // computed properties are marked overridden.  Update the text
    // property's associated editor, if any.  This will clear the
    // _overriddenDirty state on all computed properties.
    for (let textProp of textProps) {
      // _updatePropertyOverridden will return true if the
      // overridden state has changed for the text property.
      if (this._updatePropertyOverridden(textProp)) {
        textProp.updateEditor();
      }
    }
  },

  /**
   * Mark a given TextProperty as overridden or not depending on the
   * state of its computed properties.  Clears the _overriddenDirty state
   * on all computed properties.
   *
   * @param {TextProperty} prop
   *        The text property to update.
   * @return {Boolean} true if the TextProperty's overridden state (or any of
   *         its computed properties overridden state) changed.
   */
  _updatePropertyOverridden: function (prop) {
    let overridden = true;
    let dirty = false;
    for (let computedProp of prop.computed) {
      if (!computedProp.overridden) {
        overridden = false;
      }
      dirty = computedProp._overriddenDirty || dirty;
      delete computedProp._overriddenDirty;
    }

    dirty = (!!prop.overridden !== overridden) || dirty;
    prop.overridden = overridden;
    return dirty;
  }
};

/**
 * Store of CSSStyleDeclarations mapped to properties that have been changed by
 * the user.
 */
function UserProperties() {
  this.map = new Map();
}

UserProperties.prototype = {
  /**
   * Get a named property for a given CSSStyleDeclaration.
   *
   * @param {CSSStyleDeclaration} style
   *        The CSSStyleDeclaration against which the property is mapped.
   * @param {String} name
   *        The name of the property to get.
   * @param {String} value
   *        Default value.
   * @return {String}
   *        The property value if it has previously been set by the user, null
   *        otherwise.
   */
  getProperty: function (style, name, value) {
    let key = this.getKey(style);
    let entry = this.map.get(key, null);

    if (entry && name in entry) {
      return entry[name];
    }
    return value;
  },

  /**
   * Set a named property for a given CSSStyleDeclaration.
   *
   * @param {CSSStyleDeclaration} style
   *        The CSSStyleDeclaration against which the property is to be mapped.
   * @param {String} bame
   *        The name of the property to set.
   * @param {String} userValue
   *        The value of the property to set.
   */
  setProperty: function (style, bame, userValue) {
    let key = this.getKey(style, bame);
    let entry = this.map.get(key, null);

    if (entry) {
      entry[bame] = userValue;
    } else {
      let props = {};
      props[bame] = userValue;
      this.map.set(key, props);
    }
  },

  /**
   * Check whether a named property for a given CSSStyleDeclaration is stored.
   *
   * @param {CSSStyleDeclaration} style
   *        The CSSStyleDeclaration against which the property would be mapped.
   * @param {String} name
   *        The name of the property to check.
   */
  contains: function (style, name) {
    let key = this.getKey(style, name);
    let entry = this.map.get(key, null);
    return !!entry && name in entry;
  },

  getKey: function (style, name) {
    return style.actorID + ":" + name;
  },

  clear: function () {
    this.map.clear();
  }
};

exports.ElementStyle = ElementStyle;
