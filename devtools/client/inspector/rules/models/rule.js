/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Ci} = require("chrome");
const promise = require("promise");
const CssLogic = require("devtools/shared/inspector/css-logic");
const {ELEMENT_STYLE} = require("devtools/shared/specs/styles");
const {TextProperty} =
      require("devtools/client/inspector/rules/models/text-property");
const {promiseWarn} = require("devtools/client/inspector/shared/utils");
const {parseDeclarations} = require("devtools/shared/css-parsing-utils");
const Services = require("Services");

/**
 * Rule is responsible for the following:
 *   Manages a single style declaration or rule.
 *   Applies changes to the properties in a rule.
 *   Maintains a list of TextProperty objects.
 *
 * @param {ElementStyle} elementStyle
 *        The ElementStyle to which this rule belongs.
 * @param {Object} options
 *        The information used to construct this rule.  Properties include:
 *          rule: A StyleRuleActor
 *          inherited: An element this rule was inherited from.  If omitted,
 *            the rule applies directly to the current element.
 *          isSystem: Is this a user agent style?
 *          isUnmatched: True if the rule does not match the current selected
 *            element, otherwise, false.
 */
function Rule(elementStyle, options) {
  this.elementStyle = elementStyle;
  this.domRule = options.rule || null;
  this.style = options.rule;
  this.matchedSelectors = options.matchedSelectors || [];
  this.pseudoElement = options.pseudoElement || "";

  this.isSystem = options.isSystem;
  this.isUnmatched = options.isUnmatched || false;
  this.inherited = options.inherited || null;
  this.keyframes = options.keyframes || null;
  this._modificationDepth = 0;

  if (this.domRule && this.domRule.mediaText) {
    this.mediaText = this.domRule.mediaText;
  }

  this.cssProperties = this.elementStyle.ruleView.cssProperties;

  // Populate the text properties with the style's current authoredText
  // value, and add in any disabled properties from the store.
  this.textProps = this._getTextProperties();
  this.textProps = this.textProps.concat(this._getDisabledProperties());
}

Rule.prototype = {
  mediaText: "",

  get title() {
    let title = CssLogic.shortSource(this.sheet);
    if (this.domRule.type !== ELEMENT_STYLE && this.ruleLine > 0) {
      title += ":" + this.ruleLine;
    }

    return title + (this.mediaText ? " @media " + this.mediaText : "");
  },

  get inheritedSource() {
    if (this._inheritedSource) {
      return this._inheritedSource;
    }
    this._inheritedSource = "";
    if (this.inherited) {
      let eltText = this.inherited.displayName;
      if (this.inherited.id) {
        eltText += "#" + this.inherited.id;
      }
      this._inheritedSource =
        CssLogic._strings.formatStringFromName("rule.inheritedFrom",
                                               [eltText], 1);
    }
    return this._inheritedSource;
  },

  get keyframesName() {
    if (this._keyframesName) {
      return this._keyframesName;
    }
    this._keyframesName = "";
    if (this.keyframes) {
      this._keyframesName =
        CssLogic._strings.formatStringFromName("rule.keyframe",
                                               [this.keyframes.name], 1);
    }
    return this._keyframesName;
  },

  get selectorText() {
    return this.domRule.selectors ? this.domRule.selectors.join(", ") :
      CssLogic.l10n("rule.sourceElement");
  },

  /**
   * The rule's stylesheet.
   */
  get sheet() {
    return this.domRule ? this.domRule.parentStyleSheet : null;
  },

  /**
   * The rule's line within a stylesheet
   */
  get ruleLine() {
    return this.domRule ? this.domRule.line : "";
  },

  /**
   * The rule's column within a stylesheet
   */
  get ruleColumn() {
    return this.domRule ? this.domRule.column : null;
  },

  /**
   * Get display name for this rule based on the original source
   * for this rule's style sheet.
   *
   * @return {Promise}
   *         Promise which resolves with location as an object containing
   *         both the full and short version of the source string.
   */
  getOriginalSourceStrings: function () {
    return this.domRule.getOriginalLocation().then(({href,
                                                     line, mediaText}) => {
      let mediaString = mediaText ? " @" + mediaText : "";
      let linePart = line > 0 ? (":" + line) : "";

      let sourceStrings = {
        full: (href || CssLogic.l10n("rule.sourceInline")) + linePart +
          mediaString,
        short: CssLogic.shortSource({href: href}) + linePart + mediaString
      };

      return sourceStrings;
    });
  },

  /**
   * Returns true if the rule matches the creation options
   * specified.
   *
   * @param {Object} options
   *        Creation options. See the Rule constructor for documentation.
   */
  matches: function (options) {
    return this.style === options.rule;
  },

  /**
   * Create a new TextProperty to include in the rule.
   *
   * @param {String} name
   *        The text property name (such as "background" or "border-top").
   * @param {String} value
   *        The property's value (not including priority).
   * @param {String} priority
   *        The property's priority (either "important" or an empty string).
   * @param {Boolean} enabled
   *        True if the property should be enabled.
   * @param {TextProperty} siblingProp
   *        Optional, property next to which the new property will be added.
   */
  createProperty: function (name, value, priority, enabled, siblingProp) {
    let prop = new TextProperty(this, name, value, priority, enabled);

    let ind;
    if (siblingProp) {
      ind = this.textProps.indexOf(siblingProp) + 1;
      this.textProps.splice(ind, 0, prop);
    } else {
      ind = this.textProps.length;
      this.textProps.push(prop);
    }

    this.applyProperties((modifications) => {
      modifications.createProperty(ind, name, value, priority);
      // Now that the rule has been updated, the server might have given us data
      // that changes the state of the property. Update it now.
      prop.updateEditor();
    });

    return prop;
  },

  /**
   * Helper function for applyProperties that is called when the actor
   * does not support as-authored styles.  Store disabled properties
   * in the element style's store.
   */
  _applyPropertiesNoAuthored: function (modifications) {
    this.elementStyle.markOverriddenAll();

    let disabledProps = [];

    for (let prop of this.textProps) {
      if (prop.invisible) {
        continue;
      }
      if (!prop.enabled) {
        disabledProps.push({
          name: prop.name,
          value: prop.value,
          priority: prop.priority
        });
        continue;
      }
      if (prop.value.trim() === "") {
        continue;
      }

      modifications.setProperty(-1, prop.name, prop.value, prop.priority);

      prop.updateComputed();
    }

    // Store disabled properties in the disabled store.
    let disabled = this.elementStyle.store.disabled;
    if (disabledProps.length > 0) {
      disabled.set(this.style, disabledProps);
    } else {
      disabled.delete(this.style);
    }

    return modifications.apply().then(() => {
      let cssProps = {};
      // Note that even though StyleRuleActors normally provide parsed
      // declarations already, _applyPropertiesNoAuthored is only used when
      // connected to older backend that do not provide them. So parse here.
      for (let cssProp of parseDeclarations(this.cssProperties.isKnown,
                                            this.style.authoredText)) {
        cssProps[cssProp.name] = cssProp;
      }

      for (let textProp of this.textProps) {
        if (!textProp.enabled) {
          continue;
        }
        let cssProp = cssProps[textProp.name];

        if (!cssProp) {
          cssProp = {
            name: textProp.name,
            value: "",
            priority: ""
          };
        }

        textProp.priority = cssProp.priority;
      }
    });
  },

  /**
   * A helper for applyProperties that applies properties in the "as
   * authored" case; that is, when the StyleRuleActor supports
   * setRuleText.
   */
  _applyPropertiesAuthored: function (modifications) {
    return modifications.apply().then(() => {
      // The rewriting may have required some other property values to
      // change, e.g., to insert some needed terminators.  Update the
      // relevant properties here.
      for (let index in modifications.changedDeclarations) {
        let newValue = modifications.changedDeclarations[index];
        this.textProps[index].noticeNewValue(newValue);
      }
      // Recompute and redisplay the computed properties.
      for (let prop of this.textProps) {
        if (!prop.invisible && prop.enabled) {
          prop.updateComputed();
          prop.updateEditor();
        }
      }
    });
  },

  /**
   * Reapply all the properties in this rule, and update their
   * computed styles.  Will re-mark overridden properties.  Sets the
   * |_applyingModifications| property to a promise which will resolve
   * when the edit has completed.
   *
   * @param {Function} modifier a function that takes a RuleModificationList
   *        (or RuleRewriter) as an argument and that modifies it
   *        to apply the desired edit
   * @return {Promise} a promise which will resolve when the edit
   *        is complete
   */
  applyProperties: function (modifier) {
    // If there is already a pending modification, we have to wait
    // until it settles before applying the next modification.
    let resultPromise =
        promise.resolve(this._applyingModifications).then(() => {
          let modifications = this.style.startModifyingProperties(
            this.cssProperties);
          modifier(modifications);
          if (this.style.canSetRuleText) {
            return this._applyPropertiesAuthored(modifications);
          }
          return this._applyPropertiesNoAuthored(modifications);
        }).then(() => {
          this.elementStyle.markOverriddenAll();

          if (resultPromise === this._applyingModifications) {
            this._applyingModifications = null;
            this.elementStyle._changed();
          }
        }).catch(promiseWarn);

    this._applyingModifications = resultPromise;
    return resultPromise;
  },

  /**
   * Renames a property.
   *
   * @param {TextProperty} property
   *        The property to rename.
   * @param {String} name
   *        The new property name (such as "background" or "border-top").
   */
  setPropertyName: function (property, name) {
    if (name === property.name) {
      return;
    }

    let oldName = property.name;
    property.name = name;
    let index = this.textProps.indexOf(property);
    this.applyProperties((modifications) => {
      modifications.renameProperty(index, oldName, name);
    });
  },

  /**
   * Sets the value and priority of a property, then reapply all properties.
   *
   * @param {TextProperty} property
   *        The property to manipulate.
   * @param {String} value
   *        The property's value (not including priority).
   * @param {String} priority
   *        The property's priority (either "important" or an empty string).
   */
  setPropertyValue: function (property, value, priority) {
    if (value === property.value && priority === property.priority) {
      return;
    }

    property.value = value;
    property.priority = priority;

    let index = this.textProps.indexOf(property);
    this.applyProperties((modifications) => {
      modifications.setProperty(index, property.name, value, priority);
    });
  },

  /**
   * Just sets the value and priority of a property, in order to preview its
   * effect on the content document.
   *
   * @param {TextProperty} property
   *        The property which value will be previewed
   * @param {String} value
   *        The value to be used for the preview
   * @param {String} priority
   *        The property's priority (either "important" or an empty string).
   */
  previewPropertyValue: function (property, value, priority) {
    let modifications = this.style.startModifyingProperties(this.cssProperties);
    modifications.setProperty(this.textProps.indexOf(property),
                              property.name, value, priority);
    modifications.apply().then(() => {
      // Ensure dispatching a ruleview-changed event
      // also for previews
      this.elementStyle._changed();
    });
  },

  /**
   * Disables or enables given TextProperty.
   *
   * @param {TextProperty} property
   *        The property to enable/disable
   * @param {Boolean} value
   */
  setPropertyEnabled: function (property, value) {
    if (property.enabled === !!value) {
      return;
    }
    property.enabled = !!value;
    let index = this.textProps.indexOf(property);
    this.applyProperties((modifications) => {
      modifications.setPropertyEnabled(index, property.name, property.enabled);
    });
  },

  /**
   * Remove a given TextProperty from the rule and update the rule
   * accordingly.
   *
   * @param {TextProperty} property
   *        The property to be removed
   */
  removeProperty: function (property) {
    let index = this.textProps.indexOf(property);
    this.textProps.splice(index, 1);
    // Need to re-apply properties in case removing this TextProperty
    // exposes another one.
    this.applyProperties((modifications) => {
      modifications.removeProperty(index, property.name);
    });
  },

  /**
   * Get the list of TextProperties from the style. Needs
   * to parse the style's authoredText.
   */
  _getTextProperties: function () {
    let textProps = [];
    let store = this.elementStyle.store;

    // Starting with FF49, StyleRuleActors provide parsed declarations.
    let props = this.style.declarations;
    if (!props.length) {
      props = parseDeclarations(this.cssProperties.isKnown,
                                this.style.authoredText, true);
    }

    for (let prop of props) {
      let name = prop.name;
      // If the authored text has an invalid property, it will show up
      // as nameless.  Skip these as we don't currently have a good
      // way to display them.
      if (!name) {
        continue;
      }
      // In an inherited rule, we only show inherited properties.
      // However, we must keep all properties in order for rule
      // rewriting to work properly.  So, compute the "invisible"
      // property here.
      let invisible = this.inherited && !this.cssProperties.isInherited(name);
      let value = store.userProperties.getProperty(this.style, name,
                                                   prop.value);
      let textProp = new TextProperty(this, name, value, prop.priority,
                                      !("commentOffsets" in prop),
                                      invisible);
      textProps.push(textProp);
    }

    return textProps;
  },

  /**
   * Return the list of disabled properties from the store for this rule.
   */
  _getDisabledProperties: function () {
    let store = this.elementStyle.store;

    // Include properties from the disabled property store, if any.
    let disabledProps = store.disabled.get(this.style);
    if (!disabledProps) {
      return [];
    }

    let textProps = [];

    for (let prop of disabledProps) {
      let value = store.userProperties.getProperty(this.style, prop.name,
                                                   prop.value);
      let textProp = new TextProperty(this, prop.name, value, prop.priority);
      textProp.enabled = false;
      textProps.push(textProp);
    }

    return textProps;
  },

  /**
   * Reread the current state of the rules and rebuild text
   * properties as needed.
   */
  refresh: function (options) {
    this.matchedSelectors = options.matchedSelectors || [];
    let newTextProps = this._getTextProperties();

    // Update current properties for each property present on the style.
    // This will mark any touched properties with _visited so we
    // can detect properties that weren't touched (because they were
    // removed from the style).
    // Also keep track of properties that didn't exist in the current set
    // of properties.
    let brandNewProps = [];
    for (let newProp of newTextProps) {
      if (!this._updateTextProperty(newProp)) {
        brandNewProps.push(newProp);
      }
    }

    // Refresh editors and disabled state for all the properties that
    // were updated.
    for (let prop of this.textProps) {
      // Properties that weren't touched during the update
      // process must no longer exist on the node.  Mark them disabled.
      if (!prop._visited) {
        prop.enabled = false;
        prop.updateEditor();
      } else {
        delete prop._visited;
      }
    }

    // Add brand new properties.
    this.textProps = this.textProps.concat(brandNewProps);

    // Refresh the editor if one already exists.
    if (this.editor) {
      this.editor.populate();
    }
  },

  /**
   * Update the current TextProperties that match a given property
   * from the authoredText.  Will choose one existing TextProperty to update
   * with the new property's value, and will disable all others.
   *
   * When choosing the best match to reuse, properties will be chosen
   * by assigning a rank and choosing the highest-ranked property:
   *   Name, value, and priority match, enabled. (6)
   *   Name, value, and priority match, disabled. (5)
   *   Name and value match, enabled. (4)
   *   Name and value match, disabled. (3)
   *   Name matches, enabled. (2)
   *   Name matches, disabled. (1)
   *
   * If no existing properties match the property, nothing happens.
   *
   * @param {TextProperty} newProp
   *        The current version of the property, as parsed from the
   *        authoredText in Rule._getTextProperties().
   * @return {Boolean} true if a property was updated, false if no properties
   *         were updated.
   */
  _updateTextProperty: function (newProp) {
    let match = { rank: 0, prop: null };

    for (let prop of this.textProps) {
      if (prop.name !== newProp.name) {
        continue;
      }

      // Mark this property visited.
      prop._visited = true;

      // Start at rank 1 for matching name.
      let rank = 1;

      // Value and Priority matches add 2 to the rank.
      // Being enabled adds 1.  This ranks better matches higher,
      // with priority breaking ties.
      if (prop.value === newProp.value) {
        rank += 2;
        if (prop.priority === newProp.priority) {
          rank += 2;
        }
      }

      if (prop.enabled) {
        rank += 1;
      }

      if (rank > match.rank) {
        if (match.prop) {
          // We outrank a previous match, disable it.
          match.prop.enabled = false;
          match.prop.updateEditor();
        }
        match.rank = rank;
        match.prop = prop;
      } else if (rank) {
        // A previous match outranks us, disable ourself.
        prop.enabled = false;
        prop.updateEditor();
      }
    }

    // If we found a match, update its value with the new text property
    // value.
    if (match.prop) {
      match.prop.set(newProp);
      return true;
    }

    return false;
  },

  /**
   * Jump between editable properties in the UI. If the focus direction is
   * forward, begin editing the next property name if available or focus the
   * new property editor otherwise. If the focus direction is backward,
   * begin editing the previous property value or focus the selector editor if
   * this is the first element in the property list.
   *
   * @param {TextProperty} textProperty
   *        The text property that will be left to focus on a sibling.
   * @param {Number} direction
   *        The move focus direction number.
   */
  editClosestTextProperty: function (textProperty, direction) {
    let index = this.textProps.indexOf(textProperty);

    if (direction === Ci.nsIFocusManager.MOVEFOCUS_FORWARD) {
      for (++index; index < this.textProps.length; ++index) {
        if (!this.textProps[index].invisible) {
          break;
        }
      }
      if (index === this.textProps.length) {
        textProperty.rule.editor.closeBrace.click();
      } else {
        this.textProps[index].editor.nameSpan.click();
      }
    } else if (direction === Ci.nsIFocusManager.MOVEFOCUS_BACKWARD) {
      for (--index; index >= 0; --index) {
        if (!this.textProps[index].invisible) {
          break;
        }
      }
      if (index < 0) {
        textProperty.editor.ruleEditor.selectorText.click();
      } else {
        this.textProps[index].editor.valueSpan.click();
      }
    }
  },

  /**
   * Return a string representation of the rule.
   */
  stringifyRule: function () {
    let selectorText = this.selectorText;
    let cssText = "";
    let terminator = Services.appinfo.OS === "WINNT" ? "\r\n" : "\n";

    for (let textProp of this.textProps) {
      if (!textProp.invisible) {
        cssText += "\t" + textProp.stringifyProperty() + terminator;
      }
    }

    return selectorText + " {" + terminator + cssText + "}";
  },

  /**
   * See whether this rule has any non-invisible properties.
   * @return {Boolean} true if there is any visible property, or false
   *         if all properties are invisible
   */
  hasAnyVisibleProperties: function () {
    for (let prop of this.textProps) {
      if (!prop.invisible) {
        return true;
      }
    }
    return false;
  }
};

exports.Rule = Rule;
