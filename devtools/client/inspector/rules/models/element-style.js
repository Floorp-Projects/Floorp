/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const promise = require("promise");
const Rule = require("devtools/client/inspector/rules/models/rule");
const UserProperties = require("devtools/client/inspector/rules/models/user-properties");
const { promiseWarn } = require("devtools/client/inspector/shared/utils");
const { getCssProperties, isCssVariable } = require("devtools/shared/fronts/css-properties");
const { ELEMENT_STYLE } = require("devtools/shared/specs/styles");

/**
 * ElementStyle is responsible for the following:
 *   Keeps track of which properties are overridden.
 *   Maintains a list of Rule objects for a given element.
 *
 * @param  {Element} element
 *         The element whose style we are viewing.
 * @param  {CssRuleView} ruleView
 *         The instance of the rule-view panel.
 * @param  {Object} store
 *         The ElementStyle can use this object to store metadata
 *         that might outlast the rule view, particularly the current
 *         set of disabled properties.
 * @param  {PageStyleFront} pageStyle
 *         Front for the page style actor that will be providing
 *         the style information.
 * @param  {Boolean} showUserAgentStyles
 *         Should user agent styles be inspected?
 */
function ElementStyle(element, ruleView, store, pageStyle, showUserAgentStyles) {
  this.element = element;
  this.ruleView = ruleView;
  this.store = store || {};
  this.pageStyle = pageStyle;
  this.showUserAgentStyles = showUserAgentStyles;
  this.rules = [];
  this.cssProperties = getCssProperties(this.ruleView.inspector.toolbox);
  this.variables = new Map();

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
  destroy: function() {
    if (this.destroyed) {
      return;
    }

    this.destroyed = true;

    for (const rule of this.rules) {
      if (rule.editor) {
        rule.editor.destroy();
      }
    }
  },

  /**
   * Called by the Rule object when it has been changed through the
   * setProperty* methods.
   */
  _changed: function() {
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
  populate: function() {
    const populated = this.pageStyle.getApplied(this.element, {
      inherited: true,
      matchedSelectors: true,
      filter: this.showUserAgentStyles ? "ua" : undefined,
    }).then(entries => {
      if (this.destroyed || this.populated !== populated) {
        return promise.resolve(undefined);
      }

      // Store the current list of rules (if any) during the population
      // process. They will be reused if possible.
      const existingRules = this.rules;

      this.rules = [];

      for (const entry of entries) {
        this._maybeAddRule(entry, existingRules);
      }

      // Mark overridden computed styles.
      this.markOverriddenAll();

      this._sortRulesForPseudoElement();

      // We're done with the previous list of rules.
      for (const r of existingRules) {
        if (r && r.editor) {
          r.editor.destroy();
        }
      }

      return undefined;
    }).catch(e => {
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
   * Get the font families in use by the element.
   *
   * Returns a promise that will be resolved to a list of CSS family
   * names. The list might have duplicates.
   */
  getUsedFontFamilies: function() {
    return new Promise((resolve, reject) => {
      this.ruleView.styleWindow.requestIdleCallback(async () => {
        try {
          const fonts = await this.pageStyle.getUsedFontFaces(
            this.element, { includePreviews: false });
          resolve(fonts.map(font => font.CSSFamilyName));
        } catch (e) {
          reject(e);
        }
      });
    });
  },

  /**
   * Put pseudo elements in front of others.
   */
  _sortRulesForPseudoElement: function() {
    this.rules = this.rules.sort((a, b) => {
      return (a.pseudoElement || "z") > (b.pseudoElement || "z");
    });
  },

  /**
   * Add a rule if it's one we care about. Filters out duplicates and
   * inherited styles with no inherited properties.
   *
   * @param  {Object} options
   *         Options for creating the Rule, see the Rule constructor.
   * @param  {Array} existingRules
   *         Rules to reuse if possible. If a rule is reused, then it
   *         it will be deleted from this array.
   * @return {Boolean} true if we added the rule.
   */
  _maybeAddRule: function(options, existingRules) {
    // If we've already included this domRule (for example, when a
    // common selector is inherited), ignore it.
    if (options.system ||
        (options.rule && this.rules.some(rule => rule.domRule === options.rule))) {
      return false;
    }

    let rule = null;

    // If we're refreshing and the rule previously existed, reuse the
    // Rule object.
    if (existingRules) {
      const ruleIndex = existingRules.findIndex((r) => r.matches(options));
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
  markOverriddenAll: function() {
    this.variables.clear();
    this.markOverridden();

    for (const pseudo of this.cssProperties.pseudoElements) {
      this.markOverridden(pseudo);
    }
  },

  /**
   * Mark the properties listed in this.rules for a given pseudo element
   * with an overridden flag if an earlier property overrides it.
   *
   * @param  {String} pseudo
   *         Which pseudo element to flag as overridden.
   *         Empty string or undefined will default to no pseudo element.
   */
  markOverridden: function(pseudo = "") {
    // Gather all the text properties applied by these rules, ordered
    // from more- to less-specific. Text properties from keyframes rule are
    // excluded from being marked as overridden since a number of criteria such
    // as time, and animation overlay are required to be check in order to
    // determine if the property is overridden.
    const textProps = [];
    for (const rule of this.rules) {
      if ((rule.matchedSelectors.length > 0 ||
           rule.domRule.type === ELEMENT_STYLE) &&
          rule.pseudoElement === pseudo && !rule.keyframes) {
        for (const textProp of rule.textProps.slice(0).reverse()) {
          if (textProp.enabled) {
            textProps.push(textProp);
          }
        }
      }
    }

    // Gather all the computed properties applied by those text
    // properties.
    let computedProps = [];
    for (const textProp of textProps) {
      computedProps = computedProps.concat(textProp.computed);
    }

    // Walk over the computed properties. As we see a property name
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
    const taken = {};
    for (const computedProp of computedProps) {
      const earlier = taken[computedProp.name];

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
        // New property is higher priority. Mark the earlier property
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

        if (isCssVariable(computedProp.name)) {
          this.variables.set(computedProp.name, computedProp.value);
        }
      }
    }

    // For each TextProperty, mark it overridden if all of its
    // computed properties are marked overridden. Update the text
    // property's associated editor, if any. This will clear the
    // _overriddenDirty state on all computed properties.
    for (const textProp of textProps) {
      // _updatePropertyOverridden will return true if the
      // overridden state has changed for the text property.
      if (this._updatePropertyOverridden(textProp)) {
        textProp.updateEditor();
      }
    }
  },

  /**
   * Mark a given TextProperty as overridden or not depending on the
   * state of its computed properties. Clears the _overriddenDirty state
   * on all computed properties.
   *
   * @param  {TextProperty} prop
   *         The text property to update.
   * @return {Boolean} true if the TextProperty's overridden state (or any of
   *         its computed properties overridden state) changed.
   */
  _updatePropertyOverridden: function(prop) {
    let overridden = true;
    let dirty = false;

    for (const computedProp of prop.computed) {
      if (!computedProp.overridden) {
        overridden = false;
      }

      dirty = computedProp._overriddenDirty || dirty;
      delete computedProp._overriddenDirty;
    }

    dirty = (!!prop.overridden !== overridden) || dirty;
    prop.overridden = overridden;
    return dirty;
  },

 /**
  * Returns the current value of a CSS variable; or null if the
  * variable is not defined.
  *
  * @param  {String} name
  *         The name of the variable.
  * @return {String} the variable's value or null if the variable is
  *         not defined.
  */
  getVariable: function(name) {
    return this.variables.get(name);
  },
};

module.exports = ElementStyle;
