/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");
const promise = require("sdk/core/promise");

let {CssLogic} = require("devtools/styleinspector/css-logic");
let {InplaceEditor, editableField, editableItem} = require("devtools/shared/inplace-editor");
let {ELEMENT_STYLE} = require("devtools/server/actors/styles");

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const HTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/**
 * These regular expressions are adapted from firebug's css.js, and are
 * used to parse CSSStyleDeclaration's cssText attribute.
 */

// Used to split on css line separators
const CSS_LINE_RE = /(?:[^;\(]*(?:\([^\)]*?\))?[^;\(]*)*;?/g;

// Used to parse a single property line.
const CSS_PROP_RE = /\s*([^:\s]*)\s*:\s*(.*?)\s*(?:! (important))?;?$/;

// Used to parse an external resource from a property value
const CSS_RESOURCE_RE = /url\([\'\"]?(.*?)[\'\"]?\)/;

const IOService = Cc["@mozilla.org/network/io-service;1"]
                  .getService(Ci.nsIIOService);

function promiseWarn(err) {
  console.error(err);
  return promise.reject(err);
}

/**
 * To figure out how shorthand properties are interpreted by the
 * engine, we will set properties on a dummy element and observe
 * how their .style attribute reflects them as computed values.
 * This function creates the document in which those dummy elements
 * will be created.
 */
var gDummyPromise;
function createDummyDocument() {
  if (gDummyPromise) {
    return gDummyPromise;
  }
  const { getDocShell, create: makeFrame } = require("sdk/frame/utils");

  let frame = makeFrame(Services.appShell.hiddenDOMWindow.document, {
    nodeName: "iframe",
    namespaceURI: "http://www.w3.org/1999/xhtml",
    allowJavascript: false,
    allowPlugins: false,
    allowAuth: false
  });
  let docShell = getDocShell(frame);
  let eventTarget = docShell.chromeEventHandler;
  docShell.createAboutBlankContentViewer(Cc["@mozilla.org/nullprincipal;1"].createInstance(Ci.nsIPrincipal));
  let window = docShell.contentViewer.DOMDocument.defaultView;
  window.location = "data:text/html,<html></html>";
  let deferred = promise.defer()
  eventTarget.addEventListener("DOMContentLoaded", function handler(event) {
    eventTarget.removeEventListener("DOMContentLoaded", handler, false);
    deferred.resolve(window.document);
  }, false);
  gDummyPromise = deferred.promise;
  return gDummyPromise;
}

/**
 * Our model looks like this:
 *
 * ElementStyle:
 *   Responsible for keeping track of which properties are overridden.
 *   Maintains a list of Rule objects that apply to the element.
 * Rule:
 *   Manages a single style declaration or rule.
 *   Responsible for applying changes to the properties in a rule.
 *   Maintains a list of TextProperty objects.
 * TextProperty:
 *   Manages a single property from the cssText attribute of the
 *     relevant declaration.
 *   Maintains a list of computed properties that come from this
 *     property declaration.
 *   Changes to the TextProperty are sent to its related Rule for
 *     application.
 */

/**
 * ElementStyle maintains a list of Rule objects for a given element.
 *
 * @param {Element} aElement
 *        The element whose style we are viewing.
 * @param {object} aStore
 *        The ElementStyle can use this object to store metadata
 *        that might outlast the rule view, particularly the current
 *        set of disabled properties.
 * @param {PageStyleFront} aPageStyle
 *        Front for the page style actor that will be providing
 *        the style information.
 *
 * @constructor
 */
function ElementStyle(aElement, aStore, aPageStyle)
{
  this.element = aElement;
  this.store = aStore || {};
  this.pageStyle = aPageStyle;

  // We don't want to overwrite this.store.userProperties so we only create it
  // if it doesn't already exist.
  if (!("userProperties" in this.store)) {
    this.store.userProperties = new UserProperties();
  }

  if (!("disabled" in this.store)) {
    this.store.disabled = new WeakMap();
  }

  let doc = aElement.ownerDocument;

  // To figure out how shorthand properties are interpreted by the
  // engine, we will set properties on a dummy element and observe
  // how their .style attribute reflects them as computed values.
  this.dummyElementPromise = createDummyDocument().then(document => {
    this.dummyElement = document.createElementNS(this.element.namespaceURI,
                                                 this.element.tagName);
    document.documentElement.appendChild(this.dummyElement);
    return this.dummyElement;
  }).then(null, promiseWarn);
}
// We're exporting _ElementStyle for unit tests.
exports._ElementStyle = ElementStyle;

ElementStyle.prototype = {

  // The element we're looking at.
  element: null,

  // Empty, unconnected element of the same type as this node, used
  // to figure out how shorthand properties will be parsed.
  dummyElement: null,

  destroy: function()
  {
    this.dummyElement = null;
    this.dummyElementPromise.then(dummyElement => {
      if (dummyElement.parentNode) {
        dummyElement.parentNode.removeChild(dummyElement);
      }
      this.dummyElementPromise = null;
    });
  },

  /**
   * Called by the Rule object when it has been changed through the
   * setProperty* methods.
   */
  _changed: function ElementStyle_changed()
  {
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
  populate: function ElementStyle_populate()
  {
    let populated = this.pageStyle.getApplied(this.element, {
      inherited: true,
      matchedSelectors: true
    }).then(entries => {
      // Make sure the dummy element has been created before continuing...
      return this.dummyElementPromise.then(() => {
        if (this.populated != populated) {
          // Don't care anymore.
          return promise.reject("unused");
        }

        // Store the current list of rules (if any) during the population
        // process.  They will be reused if possible.
        this._refreshRules = this.rules;

        this.rules = [];

        for (let entry of entries) {
          this._maybeAddRule(entry);
        }

        // Mark overridden computed styles.
        this.markOverridden();

        // We're done with the previous list of rules.
        delete this._refreshRules;

        return null;
      });
    }).then(null, promiseWarn);
    this.populated = populated;
    return this.populated;
  },

  /**
   * Add a rule if it's one we care about.  Filters out duplicates and
   * inherited styles with no inherited properties.
   *
   * @param {object} aOptions
   *        Options for creating the Rule, see the Rule constructor.
   *
   * @return {bool} true if we added the rule.
   */
  _maybeAddRule: function ElementStyle_maybeAddRule(aOptions)
  {
    // If we've already included this domRule (for example, when a
    // common selector is inherited), ignore it.
    if (aOptions.rule &&
        this.rules.some(function(rule) rule.domRule === aOptions.rule)) {
      return false;
    }

    if (aOptions.system) {
      return false;
    }

    let rule = null;

    // If we're refreshing and the rule previously existed, reuse the
    // Rule object.
    if (this._refreshRules) {
      for (let r of this._refreshRules) {
        if (r.matches(aOptions)) {
          rule = r;
          rule.refresh(aOptions);
          break;
        }
      }
    }

    // If this is a new rule, create its Rule object.
    if (!rule) {
      rule = new Rule(this, aOptions);
    }

    // Ignore inherited rules with no properties.
    if (aOptions.inherited && rule.textProps.length == 0) {
      return false;
    }

    this.rules.push(rule);
    return true;
  },

  /**
   * Mark the properties listed in this.rules with an overridden flag
   * if an earlier property overrides it.
   */
  markOverridden: function ElementStyle_markOverridden()
  {
    // Gather all the text properties applied by these rules, ordered
    // from more- to less-specific.
    let textProps = [];
    for each (let rule in this.rules) {
      textProps = textProps.concat(rule.textProps.slice(0).reverse());
    }

    // Gather all the computed properties applied by those text
    // properties.
    let computedProps = [];
    for each (let textProp in textProps) {
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
    for each (let computedProp in computedProps) {
      let earlier = taken[computedProp.name];
      let overridden;
      if (earlier
          && computedProp.priority === "important"
          && earlier.priority !== "important") {
        // New property is higher priority.  Mark the earlier property
        // overridden (which will reverse its dirty state).
        earlier._overriddenDirty = !earlier._overriddenDirty;
        earlier.overridden = true;
        overridden = false;
      } else {
        overridden = !!earlier;
      }

      computedProp._overriddenDirty = (!!computedProp.overridden != overridden);
      computedProp.overridden = overridden;
      if (!computedProp.overridden && computedProp.textProp.enabled) {
        taken[computedProp.name] = computedProp;
      }
    }

    // For each TextProperty, mark it overridden if all of its
    // computed properties are marked overridden.  Update the text
    // property's associated editor, if any.  This will clear the
    // _overriddenDirty state on all computed properties.
    for each (let textProp in textProps) {
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
   * @param {TextProperty} aProp
   *        The text property to update.
   *
   * @return {bool} true if the TextProperty's overridden state (or any of its
   *         computed properties overridden state) changed.
   */
  _updatePropertyOverridden: function ElementStyle_updatePropertyOverridden(aProp)
  {
    let overridden = true;
    let dirty = false;
    for each (let computedProp in aProp.computed) {
      if (!computedProp.overridden) {
        overridden = false;
      }
      dirty = computedProp._overriddenDirty || dirty;
      delete computedProp._overriddenDirty;
    }

    dirty = (!!aProp.overridden != overridden) || dirty;
    aProp.overridden = overridden;
    return dirty;
  }
};

/**
 * A single style rule or declaration.
 *
 * @param {ElementStyle} aElementStyle
 *        The ElementStyle to which this rule belongs.
 * @param {object} aOptions
 *        The information used to construct this rule.  Properties include:
 *          rule: A StyleRuleActor
 *          inherited: An element this rule was inherited from.  If omitted,
 *            the rule applies directly to the current element.
 * @constructor
 */
function Rule(aElementStyle, aOptions)
{
  this.elementStyle = aElementStyle;
  this.domRule = aOptions.rule || null;
  this.style = aOptions.rule;
  this.matchedSelectors = aOptions.matchedSelectors || [];

  this.inherited = aOptions.inherited || null;
  this._modificationDepth = 0;

  if (this.domRule) {
    let parentRule = this.domRule.parentRule;
    if (parentRule && parentRule.type == Ci.nsIDOMCSSRule.MEDIA_RULE) {
      this.mediaText = parentRule.mediaText;
    }
  }

  // Populate the text properties with the style's current cssText
  // value, and add in any disabled properties from the store.
  this.textProps = this._getTextProperties();
  this.textProps = this.textProps.concat(this._getDisabledProperties());
}

Rule.prototype = {
  mediaText: "",

  get title()
  {
    if (this._title) {
      return this._title;
    }
    this._title = CssLogic.shortSource(this.sheet);
    if (this.domRule.type !== ELEMENT_STYLE) {
      this._title += ":" + this.ruleLine;
    }

    this._title = this._title + (this.mediaText ? " @media " + this.mediaText : "");
    return this._title;
  },

  get inheritedSource()
  {
    if (this._inheritedSource) {
      return this._inheritedSource;
    }
    this._inheritedSource = "";
    if (this.inherited) {
      let eltText = this.inherited.tagName.toLowerCase();
      if (this.inherited.id) {
        eltText += "#" + this.inherited.id;
      }
      this._inheritedSource =
        CssLogic._strings.formatStringFromName("rule.inheritedFrom", [eltText], 1);
    }
    return this._inheritedSource;
  },

  get selectorText()
  {
    return this.domRule.selectors ? this.domRule.selectors.join(", ") : CssLogic.l10n("rule.sourceElement");
  },

  /**
   * The rule's stylesheet.
   */
  get sheet()
  {
    return this.domRule ? this.domRule.parentStyleSheet : null;
  },

  /**
   * The rule's line within a stylesheet
   */
  get ruleLine()
  {
    return this.domRule ? this.domRule.line : null;
  },

  /**
   * Returns true if the rule matches the creation options
   * specified.
   *
   * @param {object} aOptions
   *        Creation options.  See the Rule constructor for documentation.
   */
  matches: function Rule_matches(aOptions)
  {
    return this.style === aOptions.rule;
  },

  /**
   * Create a new TextProperty to include in the rule.
   *
   * @param {string} aName
   *        The text property name (such as "background" or "border-top").
   * @param {string} aValue
   *        The property's value (not including priority).
   * @param {string} aPriority
   *        The property's priority (either "important" or an empty string).
   */
  createProperty: function Rule_createProperty(aName, aValue, aPriority)
  {
    let prop = new TextProperty(this, aName, aValue, aPriority);
    this.textProps.push(prop);
    this.applyProperties();
    return prop;
  },

  /**
   * Reapply all the properties in this rule, and update their
   * computed styles.  Store disabled properties in the element
   * style's store.  Will re-mark overridden properties.
   *
   * @param {string} [aName]
   *        A text property name (such as "background" or "border-top") used
   *        when calling from setPropertyValue & setPropertyName to signify
   *        that the property should be saved in store.userProperties.
   */
  applyProperties: function Rule_applyProperties(aModifications, aName)
  {
    if (!aModifications) {
      aModifications = this.style.startModifyingProperties();
    }
    let disabledProps = [];
    let store = this.elementStyle.store;

    for (let prop of this.textProps) {
      if (!prop.enabled) {
        disabledProps.push({
          name: prop.name,
          value: prop.value,
          priority: prop.priority
        });
        continue;
      }

      aModifications.setProperty(prop.name, prop.value, prop.priority);


      prop.updateComputed();
    }

    // Store disabled properties in the disabled store.
    let disabled = this.elementStyle.store.disabled;
    if (disabledProps.length > 0) {
      disabled.set(this.style, disabledProps);
    } else {
      disabled.delete(this.style);
    }

    let promise = aModifications.apply().then(() => {
      let cssProps = {};
      for (let cssProp of this._parseCSSText(this.style.cssText)) {
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
          }
        }

        if (aName && textProp.name == aName) {
          store.userProperties.setProperty(
            this.style, textProp.name,
            null,
            cssProp.value,
            textProp.value);
        }
        textProp.priority = cssProp.priority;
      }

      this.elementStyle.markOverridden();

      if (promise === this._applyingModifications) {
        this._applyingModifications = null;
      }

      this.elementStyle._changed();
    }).then(null, promiseWarn);
    this._applyingModifications = promise;
    return promise;
  },

  /**
   * Renames a property.
   *
   * @param {TextProperty} aProperty
   *        The property to rename.
   * @param {string} aName
   *        The new property name (such as "background" or "border-top").
   */
  setPropertyName: function Rule_setPropertyName(aProperty, aName)
  {
    if (aName === aProperty.name) {
      return;
    }
    let modifications = this.style.startModifyingProperties();
    modifications.removeProperty(aProperty.name);
    aProperty.name = aName;
    this.applyProperties(modifications, aName);
  },

  /**
   * Sets the value and priority of a property.
   *
   * @param {TextProperty} aProperty
   *        The property to manipulate.
   * @param {string} aValue
   *        The property's value (not including priority).
   * @param {string} aPriority
   *        The property's priority (either "important" or an empty string).
   */
  setPropertyValue: function Rule_setPropertyValue(aProperty, aValue, aPriority)
  {
    if (aValue === aProperty.value && aPriority === aProperty.priority) {
      return;
    }
    aProperty.value = aValue;
    aProperty.priority = aPriority;
    this.applyProperties(null, aProperty.name);
  },

  /**
   * Disables or enables given TextProperty.
   */
  setPropertyEnabled: function Rule_enableProperty(aProperty, aValue)
  {
    aProperty.enabled = !!aValue;
    let modifications = this.style.startModifyingProperties();
    if (!aProperty.enabled) {
      modifications.removeProperty(aProperty.name);
    }
    this.applyProperties(modifications);
  },

  /**
   * Remove a given TextProperty from the rule and update the rule
   * accordingly.
   */
  removeProperty: function Rule_removeProperty(aProperty)
  {
    this.textProps = this.textProps.filter(function(prop) prop != aProperty);
    let modifications = this.style.startModifyingProperties();
    modifications.removeProperty(aProperty.name);
    // Need to re-apply properties in case removing this TextProperty
    // exposes another one.
    this.applyProperties(modifications);
  },

  _parseCSSText: function Rule_parseProperties(aCssText)
  {
    let lines = aCssText.match(CSS_LINE_RE);
    let props = [];

    for (let line of lines) {
      let [, name, value, priority] = CSS_PROP_RE.exec(line) || []
      if (!name || !value) {
        continue;
      }

      props.push({
        name: name,
        value: value,
        priority: priority || ""
      });
    }
    return props;
  },

  /**
   * Get the list of TextProperties from the style.  Needs
   * to parse the style's cssText.
   */
  _getTextProperties: function Rule_getTextProperties()
  {
    let textProps = [];
    let store = this.elementStyle.store;
    let props = this._parseCSSText(this.style.cssText);
    for (let prop of props) {
      let name = prop.name;
      if (this.inherited && !domUtils.isInheritedProperty(name)) {
        continue;
      }
      let value = store.userProperties.getProperty(this.style, name, prop.value);
      let textProp = new TextProperty(this, name, value, prop.priority);
      textProps.push(textProp);
    }

    return textProps;
  },

  /**
   * Return the list of disabled properties from the store for this rule.
   */
  _getDisabledProperties: function Rule_getDisabledProperties()
  {
    let store = this.elementStyle.store;

    // Include properties from the disabled property store, if any.
    let disabledProps = store.disabled.get(this.style);
    if (!disabledProps) {
      return [];
    }

    let textProps = [];

    for each (let prop in disabledProps) {
      let value = store.userProperties.getProperty(this.style, prop.name, prop.value);
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
  refresh: function Rule_refresh(aOptions)
  {
    this.matchedSelectors = aOptions.matchedSelectors || [];
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
   * from the cssText.  Will choose one existing TextProperty to update
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
   * @param {TextProperty} aNewProp
   *        The current version of the property, as parsed from the
   *        cssText in Rule._getTextProperties().
   *
   * @return {bool} true if a property was updated, false if no properties
   *         were updated.
   */
  _updateTextProperty: function Rule__updateTextProperty(aNewProp) {
    let match = { rank: 0, prop: null };

    for each (let prop in this.textProps) {
      if (prop.name != aNewProp.name)
        continue;

      // Mark this property visited.
      prop._visited = true;

      // Start at rank 1 for matching name.
      let rank = 1;

      // Value and Priority matches add 2 to the rank.
      // Being enabled adds 1.  This ranks better matches higher,
      // with priority breaking ties.
      if (prop.value === aNewProp.value) {
        rank += 2;
        if (prop.priority === aNewProp.priority) {
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
      match.prop.set(aNewProp);
      return true;
    }

    return false;
  },
};

/**
 * A single property in a rule's cssText.
 *
 * @param {Rule} aRule
 *        The rule this TextProperty came from.
 * @param {string} aName
 *        The text property name (such as "background" or "border-top").
 * @param {string} aValue
 *        The property's value (not including priority).
 * @param {string} aPriority
 *        The property's priority (either "important" or an empty string).
 *
 */
function TextProperty(aRule, aName, aValue, aPriority)
{
  this.rule = aRule;
  this.name = aName;
  this.value = aValue;
  this.priority = aPriority;
  this.enabled = true;
  this.updateComputed();
}

TextProperty.prototype = {
  /**
   * Update the editor associated with this text property,
   * if any.
   */
  updateEditor: function TextProperty_updateEditor()
  {
    if (this.editor) {
      this.editor.update();
    }
  },

  /**
   * Update the list of computed properties for this text property.
   */
  updateComputed: function TextProperty_updateComputed()
  {
    if (!this.name) {
      return;
    }

    // This is a bit funky.  To get the list of computed properties
    // for this text property, we'll set the property on a dummy element
    // and see what the computed style looks like.
    let dummyElement = this.rule.elementStyle.dummyElement;
    let dummyStyle = dummyElement.style;
    dummyStyle.cssText = "";
    dummyStyle.setProperty(this.name, this.value, this.priority);

    this.computed = [];
    for (let i = 0, n = dummyStyle.length; i < n; i++) {
      let prop = dummyStyle.item(i);
      this.computed.push({
        textProp: this,
        name: prop,
        value: dummyStyle.getPropertyValue(prop),
        priority: dummyStyle.getPropertyPriority(prop),
      });
    }
  },

  /**
   * Set all the values from another TextProperty instance into
   * this TextProperty instance.
   *
   * @param {TextProperty} aOther
   *        The other TextProperty instance.
   */
  set: function TextProperty_set(aOther)
  {
    let changed = false;
    for (let item of ["name", "value", "priority", "enabled"]) {
      if (this[item] != aOther[item]) {
        this[item] = aOther[item];
        changed = true;
      }
    }

    if (changed) {
      this.updateEditor();
    }
  },

  setValue: function TextProperty_setValue(aValue, aPriority)
  {
    this.rule.setPropertyValue(this, aValue, aPriority);
    this.updateEditor();
  },

  setName: function TextProperty_setName(aName)
  {
    this.rule.setPropertyName(this, aName);
    this.updateEditor();
  },

  setEnabled: function TextProperty_setEnabled(aValue)
  {
    this.rule.setPropertyEnabled(this, aValue);
    this.updateEditor();
  },

  remove: function TextProperty_remove()
  {
    this.rule.removeProperty(this);
  }
};


/**
 * View hierarchy mostly follows the model hierarchy.
 *
 * CssRuleView:
 *   Owns an ElementStyle and creates a list of RuleEditors for its
 *    Rules.
 * RuleEditor:
 *   Owns a Rule object and creates a list of TextPropertyEditors
 *     for its TextProperties.
 *   Manages creation of new text properties.
 * TextPropertyEditor:
 *   Owns a TextProperty object.
 *   Manages changes to the TextProperty.
 *   Can be expanded to display computed properties.
 *   Can mark a property disabled or enabled.
 */

/**
 * CssRuleView is a view of the style rules and declarations that
 * apply to a given element.  After construction, the 'element'
 * property will be available with the user interface.
 *
 * @param {Document} aDoc
 *        The document that will contain the rule view.
 * @param {object} aStore
 *        The CSS rule view can use this object to store metadata
 *        that might outlast the rule view, particularly the current
 *        set of disabled properties.
 * @param {PageStyleFront} aPageStyle
 *        The PageStyleFront for communicating with the remote server.
 * @constructor
 */
function CssRuleView(aDoc, aStore, aPageStyle)
{
  this.doc = aDoc;
  this.store = aStore;
  this.pageStyle = aPageStyle;
  this.element = this.doc.createElementNS(HTML_NS, "div");
  this.element.className = "ruleview devtools-monospace";
  this.element.flex = 1;

  this._boundCopy = this._onCopy.bind(this);
  this.element.addEventListener("copy", this._boundCopy);

  let options = {
    fixedWidth: true,
    autoSelect: true,
    theme: "auto"
  };
  this.popup = new AutocompletePopup(aDoc.defaultView.parent.document, options);

  this._showEmpty();
}

exports.CssRuleView = CssRuleView;

CssRuleView.prototype = {
  // The element that we're inspecting.
  _viewedElement: null,

  setPageStyle: function(aPageStyle) {
    this.pageStyle = aPageStyle;
  },

  /**
   * Return {bool} true if the rule view currently has an input editor visible.
   */
  get isEditing() {
    return this.element.querySelectorAll(".styleinspector-propertyeditor").length > 0;
  },

  destroy: function CssRuleView_destroy()
  {
    this.clear();

    this.element.removeEventListener("copy", this._boundCopy);
    delete this._boundCopy;

    if (this.element.parentNode) {
      this.element.parentNode.removeChild(this.element);
    }

    this.elementStyle.destroy();

    this.popup.destroy();
  },

  /**
   * Update the highlighted element.
   *
   * @param {nsIDOMElement} aElement
   *        The node whose style rules we'll inspect.
   */
  highlight: function CssRuleView_highlight(aElement)
  {
    if (this._viewedElement === aElement) {
      return promise.resolve(undefined);
    }

    this.clear();

    if (this._elementStyle) {
      delete this._elementStyle;
    }

    this._viewedElement = aElement;
    if (!this._viewedElement) {
      this._showEmpty();
      return promise.resolve(undefined);
    }

    this._elementStyle = new ElementStyle(aElement, this.store, this.pageStyle);
    return this._populate().then(() => {
      this._elementStyle.onChanged = () => {
        this._changed();
      };
    }).then(null, console.error);
  },

  /**
   * Update the rules for the currently highlighted element.
   */
  nodeChanged: function CssRuleView_nodeChanged()
  {
    // Ignore refreshes during editing or when no element is selected.
    if (this.isEditing || !this._elementStyle) {
      return promise.resolve(null);
    }

    this._clearRules();

    // Repopulate the element style.
    return this._populate();
  },

  _populate: function() {
    let elementStyle = this._elementStyle;
    return this._elementStyle.populate().then(() => {
      if (this._elementStyle != elementStyle) {
        return promise.reject("element changed");
      }
      this._createEditors();

      // Notify anyone that cares that we refreshed.
      var evt = this.doc.createEvent("Events");
      evt.initEvent("CssRuleViewRefreshed", true, false);
      this.element.dispatchEvent(evt);
      return undefined;
    }).then(null, promiseWarn);
  },

  /**
   * Show the user that the rule view has no node selected.
   */
  _showEmpty: function CssRuleView_showEmpty()
  {
    if (this.doc.getElementById("noResults") > 0) {
      return;
    }

    createChild(this.element, "div", {
      id: "noResults",
      textContent: CssLogic.l10n("rule.empty")
    });
  },

  /**
   * Clear the rules.
   */
  _clearRules: function CssRuleView_clearRules()
  {
    while (this.element.hasChildNodes()) {
      this.element.removeChild(this.element.lastChild);
    }
  },

  /**
   * Clear the rule view.
   */
  clear: function CssRuleView_clear()
  {
    this._clearRules();
    this._viewedElement = null;
    this._elementStyle = null;
  },

  /**
   * Called when the user has made changes to the ElementStyle.
   * Emits an event that clients can listen to.
   */
  _changed: function CssRuleView_changed()
  {
    var evt = this.doc.createEvent("Events");
    evt.initEvent("CssRuleViewChanged", true, false);
    this.element.dispatchEvent(evt);
  },

  /**
   * Creates editor UI for each of the rules in _elementStyle.
   */
  _createEditors: function CssRuleView_createEditors()
  {
    // Run through the current list of rules, attaching
    // their editors in order.  Create editors if needed.
    let lastInheritedSource = "";
    for (let rule of this._elementStyle.rules) {
      if (rule.domRule.system) {
        continue;
      }

      let inheritedSource = rule.inheritedSource;
      if (inheritedSource != lastInheritedSource) {
        let h2 = this.doc.createElementNS(HTML_NS, "div");
        h2.className = "ruleview-rule-inheritance theme-gutter";
        h2.textContent = inheritedSource;
        lastInheritedSource = inheritedSource;
        this.element.appendChild(h2);
      }

      if (!rule.editor) {
        new RuleEditor(this, rule);
      }

      this.element.appendChild(rule.editor.element);
    }
  },

  /**
   * Copy selected text from the rule view.
   *
   * @param {Event} aEvent
   *        The event object.
   */
  _onCopy: function CssRuleView_onCopy(aEvent)
  {
    let target = aEvent.target;

    let text;

    if (target.nodeName == "input") {
      let start = Math.min(target.selectionStart, target.selectionEnd);
      let end = Math.max(target.selectionStart, target.selectionEnd);
      let count = end - start;
      text = target.value.substr(start, count);
    } else {
      let win = this.doc.defaultView;
      text = win.getSelection().toString();

      // Remove any double newlines.
      text = text.replace(/(\r?\n)\r?\n/g, "$1");

      // Remove "inline"
      let inline = _strings.GetStringFromName("rule.sourceInline");
      let rx = new RegExp("^" + inline + "\\r?\\n?", "g");
      text = text.replace(rx, "");
    }

    clipboardHelper.copyString(text, this.doc);

    aEvent.preventDefault();
  },

};

/**
 * Create a RuleEditor.
 *
 * @param {CssRuleView} aRuleView
 *        The CssRuleView containg the document holding this rule editor.
 * @param {Rule} aRule
 *        The Rule object we're editing.
 * @constructor
 */
function RuleEditor(aRuleView, aRule)
{
  this.ruleView = aRuleView;
  this.doc = this.ruleView.doc;
  this.rule = aRule;
  this.rule.editor = this;

  this._onNewProperty = this._onNewProperty.bind(this);
  this._newPropertyDestroy = this._newPropertyDestroy.bind(this);

  this._create();
}

RuleEditor.prototype = {
  _create: function RuleEditor_create()
  {
    this.element = this.doc.createElementNS(HTML_NS, "div");
    this.element.className = "ruleview-rule theme-separator";
    this.element._ruleEditor = this;

    // Give a relative position for the inplace editor's measurement
    // span to be placed absolutely against.
    this.element.style.position = "relative";

    // Add the source link.
    let source = createChild(this.element, "div", {
      class: "ruleview-rule-source theme-link"
    });
    source.addEventListener("click", function() {
      let rule = this.rule.domRule;
      let evt = this.doc.createEvent("CustomEvent");
      evt.initCustomEvent("CssRuleViewCSSLinkClicked", true, false, {
        rule: rule,
      });
      this.element.dispatchEvent(evt);
    }.bind(this));
    let sourceLabel = this.doc.createElementNS(XUL_NS, "label");
    sourceLabel.setAttribute("crop", "center");
    sourceLabel.setAttribute("value", this.rule.title);
    sourceLabel.setAttribute("tooltiptext", this.rule.title);
    source.appendChild(sourceLabel);

    let code = createChild(this.element, "div", {
      class: "ruleview-code"
    });

    let header = createChild(code, "div", {});

    this.selectorText = createChild(header, "span", {
      class: "ruleview-selector theme-fg-color3"
    });

    this.openBrace = createChild(header, "span", {
      class: "ruleview-ruleopen",
      textContent: " {"
    });

    code.addEventListener("click", function() {
      let selection = this.doc.defaultView.getSelection();
      if (selection.isCollapsed) {
        this.newProperty();
      }
    }.bind(this), false);

    this.element.addEventListener("mousedown", function() {
      this.doc.defaultView.focus();

      let editorNodes =
        this.doc.querySelectorAll(".styleinspector-propertyeditor");

      if (editorNodes) {
        for (let node of editorNodes) {
          if (node.inplaceEditor) {
            node.inplaceEditor._clear();
          }
        }
      }
    }.bind(this), false);

    this.propertyList = createChild(code, "ul", {
      class: "ruleview-propertylist"
    });

    this.populate();

    this.closeBrace = createChild(code, "div", {
      class: "ruleview-ruleclose",
      tabindex: "0",
      textContent: "}"
    });

    // Create a property editor when the close brace is clicked.
    editableItem({ element: this.closeBrace }, function(aElement) {
      this.newProperty();
    }.bind(this));
  },

  /**
   * Update the rule editor with the contents of the rule.
   */
  populate: function RuleEditor_populate()
  {
    // Clear out existing viewers.
    while (this.selectorText.hasChildNodes()) {
      this.selectorText.removeChild(this.selectorText.lastChild);
    }

    // If selector text comes from a css rule, highlight selectors that
    // actually match.  For custom selector text (such as for the 'element'
    // style, just show the text directly.
    if (this.rule.domRule.type === ELEMENT_STYLE) {
      this.selectorText.textContent = this.rule.selectorText;
    } else {
      this.rule.domRule.selectors.forEach((selector, i) => {
        if (i != 0) {
          createChild(this.selectorText, "span", {
            class: "ruleview-selector-separator",
            textContent: ", "
          });
        }
        let cls;
        if (this.rule.matchedSelectors.indexOf(selector) > -1) {
          cls = "ruleview-selector-matched";
        } else {
          cls = "ruleview-selector-unmatched";
        }
        createChild(this.selectorText, "span", {
          class: cls,
          textContent: selector
        });
      });
    }

    for (let prop of this.rule.textProps) {
      if (!prop.editor) {
        new TextPropertyEditor(this, prop);
        this.propertyList.appendChild(prop.editor.element);
      }
    }
  },

  /**
   * Programatically add a new property to the rule.
   *
   * @param {string} aName
   *        Property name.
   * @param {string} aValue
   *        Property value.
   * @param {string} aPriority
   *        Property priority.
   */
  addProperty: function RuleEditor_addProperty(aName, aValue, aPriority)
  {
    let prop = this.rule.createProperty(aName, aValue, aPriority);
    let editor = new TextPropertyEditor(this, prop);
    this.propertyList.appendChild(editor.element);
  },

  /**
   * Create a text input for a property name.  If a non-empty property
   * name is given, we'll create a real TextProperty and add it to the
   * rule.
   */
  newProperty: function RuleEditor_newProperty()
  {
    // If we're already creating a new property, ignore this.
    if (!this.closeBrace.hasAttribute("tabindex")) {
      return;
    }

    // While we're editing a new property, it doesn't make sense to
    // start a second new property editor, so disable focusing the
    // close brace for now.
    this.closeBrace.removeAttribute("tabindex");

    this.newPropItem = createChild(this.propertyList, "li", {
      class: "ruleview-property ruleview-newproperty",
    });

    this.newPropSpan = createChild(this.newPropItem, "span", {
      class: "ruleview-propertyname",
      tabindex: "0"
    });

    new InplaceEditor({
      element: this.newPropSpan,
      done: this._onNewProperty,
      destroy: this._newPropertyDestroy,
      advanceChars: ":",
      contentType: InplaceEditor.CONTENT_TYPES.CSS_PROPERTY,
      popup: this.ruleView.popup
    });
  },

  /**
   * Called when the new property input has been dismissed.
   * Will create a new TextProperty if necessary.
   *
   * @param {string} aValue
   *        The value in the editor.
   * @param {bool} aCommit
   *        True if the value should be committed.
   */
  _onNewProperty: function RuleEditor__onNewProperty(aValue, aCommit)
  {
    if (!aValue || !aCommit) {
      return;
    }

    // Create an empty-valued property and start editing it.
    let prop = this.rule.createProperty(aValue, "", "");
    let editor = new TextPropertyEditor(this, prop);
    this.propertyList.appendChild(editor.element);
    editor.valueSpan.click();
  },

  /**
   * Called when the new property editor is destroyed.
   */
  _newPropertyDestroy: function RuleEditor__newPropertyDestroy()
  {
    // We're done, make the close brace focusable again.
    this.closeBrace.setAttribute("tabindex", "0");

    this.propertyList.removeChild(this.newPropItem);
    delete this.newPropItem;
    delete this.newPropSpan;
  }
};

/**
 * Create a TextPropertyEditor.
 *
 * @param {RuleEditor} aRuleEditor
 *        The rule editor that owns this TextPropertyEditor.
 * @param {TextProperty} aProperty
 *        The text property to edit.
 * @constructor
 */
function TextPropertyEditor(aRuleEditor, aProperty)
{
  this.ruleEditor = aRuleEditor;
  this.doc = this.ruleEditor.doc;
  this.popup = this.ruleEditor.ruleView.popup;
  this.prop = aProperty;
  this.prop.editor = this;
  this.browserWindow = this.doc.defaultView.top;

  let sheet = this.prop.rule.sheet;
  let href = sheet ? (sheet.href || sheet.nodeHref) : null;
  if (href) {
    this.sheetURI = IOService.newURI(href, null, null);
  }

  this._onEnableClicked = this._onEnableClicked.bind(this);
  this._onExpandClicked = this._onExpandClicked.bind(this);
  this._onStartEditing = this._onStartEditing.bind(this);
  this._onNameDone = this._onNameDone.bind(this);
  this._onValueDone = this._onValueDone.bind(this);

  this._create();
  this.update();
}

TextPropertyEditor.prototype = {
  get editing() {
    return !!(this.nameSpan.inplaceEditor || this.valueSpan.inplaceEditor);
  },

  /**
   * Create the property editor's DOM.
   */
  _create: function TextPropertyEditor_create()
  {
    this.element = this.doc.createElementNS(HTML_NS, "li");
    this.element.classList.add("ruleview-property");

    // The enable checkbox will disable or enable the rule.
    this.enable = createChild(this.element, "div", {
      class: "ruleview-enableproperty theme-checkbox",
      tabindex: "-1"
    });
    this.enable.addEventListener("click", this._onEnableClicked, true);

    // Click to expand the computed properties of the text property.
    this.expander = createChild(this.element, "span", {
      class: "ruleview-expander theme-twisty"
    });
    this.expander.addEventListener("click", this._onExpandClicked, true);

    this.nameContainer = createChild(this.element, "span", {
      class: "ruleview-namecontainer"
    });
    this.nameContainer.addEventListener("click", function(aEvent) {
      // Clicks within the name shouldn't propagate any further.
      aEvent.stopPropagation();
      if (aEvent.target === propertyContainer) {
        this.nameSpan.click();
      }
    }.bind(this), false);

    // Property name, editable when focused.  Property name
    // is committed when the editor is unfocused.
    this.nameSpan = createChild(this.nameContainer, "span", {
      class: "ruleview-propertyname theme-fg-color5",
      tabindex: "0",
    });

    editableField({
      start: this._onStartEditing,
      element: this.nameSpan,
      done: this._onNameDone,
      advanceChars: ':',
      contentType: InplaceEditor.CONTENT_TYPES.CSS_PROPERTY,
      popup: this.popup
    });

    appendText(this.nameContainer, ": ");

    // Create a span that will hold the property and semicolon.
    // Use this span to create a slightly larger click target
    // for the value.
    let propertyContainer = createChild(this.element, "span", {
      class: "ruleview-propertycontainer"
    });
    propertyContainer.addEventListener("click", function(aEvent) {
      // Clicks within the value shouldn't propagate any further.
      aEvent.stopPropagation();
      if (aEvent.target === propertyContainer) {
        this.valueSpan.click();
      }
    }.bind(this), false);

    // Property value, editable when focused.  Changes to the
    // property value are applied as they are typed, and reverted
    // if the user presses escape.
    this.valueSpan = createChild(propertyContainer, "span", {
      class: "ruleview-propertyvalue theme-fg-color1",
      tabindex: "0",
    });

    // Save the initial value as the last committed value,
    // for restoring after pressing escape.
    this.committed = { name: this.prop.name,
                       value: this.prop.value,
                       priority: this.prop.priority };

    appendText(propertyContainer, ";");

    this.warning = createChild(this.element, "div", {
      hidden: "",
      class: "ruleview-warning",
      title: CssLogic.l10n("rule.warning.title"),
    });

    // Holds the viewers for the computed properties.
    // will be populated in |_updateComputed|.
    this.computed = createChild(this.element, "ul", {
      class: "ruleview-computedlist",
    });

    editableField({
      start: this._onStartEditing,
      element: this.valueSpan,
      done: this._onValueDone,
      validate: this._validate.bind(this),
      warning: this.warning,
      advanceChars: ';',
      contentType: InplaceEditor.CONTENT_TYPES.CSS_VALUE,
      property: this.prop,
      popup: this.popup
    });
  },

  /**
   * Resolve a URI based on the rule stylesheet
   * @param {string} relativePath the path to resolve
   * @return {string} the resolved path.
   */
  resolveURI: function(relativePath)
  {
    if (this.sheetURI) {
      relativePath = this.sheetURI.resolve(relativePath);
    }
    return relativePath;
  },

  /**
   * Check the property value to find an external resource (if any).
   * @return {string} the URI in the property value, or null if there is no match.
   */
  getResourceURI: function()
  {
    let val = this.prop.value;
    let uriMatch = CSS_RESOURCE_RE.exec(val);
    let uri = null;

    if (uriMatch && uriMatch[1]) {
      uri = uriMatch[1];
    }

    return uri;
  },

  /**
   * Populate the span based on changes to the TextProperty.
   */
  update: function TextPropertyEditor_update()
  {
    if (this.prop.enabled) {
      this.enable.style.removeProperty("visibility");
      this.enable.setAttribute("checked", "");
    } else {
      this.enable.style.visibility = "visible";
      this.enable.removeAttribute("checked");
    }

    if (this.prop.overridden && !this.editing) {
      this.element.classList.add("ruleview-overridden");
    } else {
      this.element.classList.remove("ruleview-overridden");
    }

    let name = this.prop.name;
    this.nameSpan.textContent = name;

    // Combine the property's value and priority into one string for
    // the value.
    let val = this.prop.value;
    if (this.prop.priority) {
      val += " !" + this.prop.priority;
    }

    // Treat URLs differently than other properties.
    // Allow the user to click a link to the resource and open it.
    let resourceURI = this.getResourceURI();
    if (resourceURI) {
      this.valueSpan.textContent = "";

      appendText(this.valueSpan, val.split(resourceURI)[0]);

      let a = createChild(this.valueSpan, "a",  {
        target: "_blank",
        class: "theme-link",
        textContent: resourceURI,
        href: this.resolveURI(resourceURI)
      });

      a.addEventListener("click", (aEvent) => {

        // Clicks within the link shouldn't trigger editing.
        aEvent.stopPropagation();
        aEvent.preventDefault();

        this.browserWindow.openUILinkIn(aEvent.target.href, "tab");

      }, false);

      appendText(this.valueSpan, val.split(resourceURI)[1]);
    } else {
      this.valueSpan.textContent = val;
    }

    this.warning.hidden = this._validate();

    let store = this.prop.rule.elementStyle.store;
    let propDirty = store.userProperties.contains(this.prop.rule.style, name);
    if (propDirty) {
      this.element.setAttribute("dirty", "");
    } else {
      this.element.removeAttribute("dirty");
    }

    // Populate the computed styles.
    this._updateComputed();
  },

  _onStartEditing: function TextPropertyEditor_onStartEditing()
  {
    this.element.classList.remove("ruleview-overridden");
  },

  /**
   * Populate the list of computed styles.
   */
  _updateComputed: function TextPropertyEditor_updateComputed()
  {
    // Clear out existing viewers.
    while (this.computed.hasChildNodes()) {
      this.computed.removeChild(this.computed.lastChild);
    }

    let showExpander = false;
    for each (let computed in this.prop.computed) {
      // Don't bother to duplicate information already
      // shown in the text property.
      if (computed.name === this.prop.name) {
        continue;
      }

      showExpander = true;

      let li = createChild(this.computed, "li", {
        class: "ruleview-computed"
      });

      if (computed.overridden) {
        li.classList.add("ruleview-overridden");
      }

      createChild(li, "span", {
        class: "ruleview-propertyname theme-fg-color5",
        textContent: computed.name
      });
      appendText(li, ": ");

      createChild(li, "span", {
        class: "ruleview-propertyvalue theme-fg-color1",
        textContent: computed.value
      });
      appendText(li, ";");
    }

    // Show or hide the expander as needed.
    if (showExpander) {
      this.expander.style.visibility = "visible";
    } else {
      this.expander.style.visibility = "hidden";
    }
  },

  /**
   * Handles clicks on the disabled property.
   */
  _onEnableClicked: function TextPropertyEditor_onEnableClicked(aEvent)
  {
    let checked = this.enable.hasAttribute("checked");
    if (checked) {
      this.enable.removeAttribute("checked");
    } else {
      this.enable.setAttribute("checked", "");
    }
    this.prop.setEnabled(!checked);
    aEvent.stopPropagation();
  },

  /**
   * Handles clicks on the computed property expander.
   */
  _onExpandClicked: function TextPropertyEditor_onExpandClicked(aEvent)
  {
    this.computed.classList.toggle("styleinspector-open");
    if (this.computed.classList.contains("styleinspector-open")) {
      this.expander.setAttribute("open", "true");
    } else {
      this.expander.removeAttribute("open");
    }
    aEvent.stopPropagation();
  },

  /**
   * Called when the property name's inplace editor is closed.
   * Ignores the change if the user pressed escape, otherwise
   * commits it.
   *
   * @param {string} aValue
   *        The value contained in the editor.
   * @param {boolean} aCommit
   *        True if the change should be applied.
   */
  _onNameDone: function TextPropertyEditor_onNameDone(aValue, aCommit)
  {
    if (!aCommit) {
      if (this.prop.overridden) {
        this.element.classList.add("ruleview-overridden");
      }

      return;
    }
    if (!aValue) {
      this.prop.remove();
      this.element.parentNode.removeChild(this.element);
      return;
    }
    this.prop.setName(aValue);
  },

  /**
   * Pull priority (!important) out of the value provided by a
   * value editor.
   *
   * @param {string} aValue
   *        The value from the text editor.
   * @return {object} an object with 'value' and 'priority' properties.
   */
  _parseValue: function TextPropertyEditor_parseValue(aValue)
  {
    let pieces = aValue.split("!", 2);
    return {
      value: pieces[0].trim(),
      priority: (pieces.length > 1 ? pieces[1].trim() : "")
    };
  },

  /**
   * Called when a value editor closes.  If the user pressed escape,
   * revert to the value this property had before editing.
   *
   * @param {string} aValue
   *        The value contained in the editor.
   * @param {bool} aCommit
   *        True if the change should be applied.
   */
   _onValueDone: function PropertyEditor_onValueDone(aValue, aCommit)
  {
    if (aCommit) {
      let val = this._parseValue(aValue);
      this.prop.setValue(val.value, val.priority);
      this.committed.value = this.prop.value;
      this.committed.priority = this.prop.priority;
      if (this.prop.overridden) {
        this.element.classList.add("ruleview-overridden");
      }
    } else {
      this.prop.setValue(this.committed.value, this.committed.priority);
    }
  },

  /**
   * Validate this property.
   *
   * @param {string} [aValue]
   *        Override the actual property value used for validation without
   *        applying property values e.g. validate as you type.
   *
   * @return {bool} true if the property value is valid, false otherwise.
   */
  _validate: function TextPropertyEditor_validate(aValue)
  {
    let name = this.prop.name;
    let value = typeof aValue == "undefined" ? this.prop.value : aValue;
    let val = this._parseValue(value);

    let style = this.doc.createElementNS(HTML_NS, "div").style;
    let prefs = Services.prefs;

    // We toggle output of errors whilst the user is typing a property value.
    let prefVal = Services.prefs.getBoolPref("layout.css.report_errors");
    prefs.setBoolPref("layout.css.report_errors", false);

    try {
      style.setProperty(name, val.value, val.priority);
      // Live previewing the change without committing yet just yet, that'll be done in _onValueDone
      this.ruleEditor.rule.setPropertyValue(this.prop, val.value, val.priority);
    } finally {
      prefs.setBoolPref("layout.css.report_errors", prefVal);
    }
    return !!style.getPropertyValue(name);
  },
};

/**
 * Store of CSSStyleDeclarations mapped to properties that have been changed by
 * the user.
 */
function UserProperties()
{
  this.weakMap = new WeakMap();
}

UserProperties.prototype = {
  /**
   * Get a named property for a given CSSStyleDeclaration.
   *
   * @param {CSSStyleDeclaration} aStyle
   *        The CSSStyleDeclaration against which the property is mapped.
   * @param {string} aName
   *        The name of the property to get.
   * @param {string} aComputedValue
   *        The computed value of the property.  The user value will only be
   *        returned if the computed value hasn't changed since, and this will
   *        be returned as the default if no user value is available.
   * @return {string}
   *          The property value if it has previously been set by the user, null
   *          otherwise.
   */
  getProperty: function UP_getProperty(aStyle, aName, aComputedValue) {
    let entry = this.weakMap.get(aStyle, null);

    if (entry && aName in entry) {
      let item = entry[aName];
      if (item.computed != aComputedValue) {
        delete entry[aName];
        return aComputedValue;
      }

      return item.user;
    }
    return aComputedValue;

  },

  /**
   * Set a named property for a given CSSStyleDeclaration.
   *
   * @param {CSSStyleDeclaration} aStyle
   *        The CSSStyleDeclaration against which the property is to be mapped.
   * @param {String} aName
   *        The name of the property to set.
   * @param {String} aComputedValue
   *        The computed property value.  The user value will not be used if the
   *        computed value changes.
   * @param {String} aUserValue
   *        The value of the property to set.
   */
  setProperty: function UP_setProperty(aStyle, aName, aComputedValue, aUserValue) {
    let entry = this.weakMap.get(aStyle, null);
    if (entry) {
      entry[aName] = { computed: aComputedValue, user: aUserValue };
    } else {
      let props = {};
      props[aName] = { computed: aComputedValue, user: aUserValue };
      this.weakMap.set(aStyle, props);
    }
  },

  /**
   * Check whether a named property for a given CSSStyleDeclaration is stored.
   *
   * @param {CSSStyleDeclaration} aStyle
   *        The CSSStyleDeclaration against which the property would be mapped.
   * @param {String} aName
   *        The name of the property to check.
   */
  contains: function UP_contains(aStyle, aName) {
    let entry = this.weakMap.get(aStyle, null);
    return !!entry && aName in entry;
  },
};

/**
 * Helper functions
 */

/**
 * Create a child element with a set of attributes.
 *
 * @param {Element} aParent
 *        The parent node.
 * @param {string} aTag
 *        The tag name.
 * @param {object} aAttributes
 *        A set of attributes to set on the node.
 */
function createChild(aParent, aTag, aAttributes)
{
  let elt = aParent.ownerDocument.createElementNS(HTML_NS, aTag);
  for (let attr in aAttributes) {
    if (aAttributes.hasOwnProperty(attr)) {
      if (attr === "textContent") {
        elt.textContent = aAttributes[attr];
      } else {
        elt.setAttribute(attr, aAttributes[attr]);
      }
    }
  }
  aParent.appendChild(elt);
  return elt;
}

function createMenuItem(aMenu, aAttributes)
{
  let item = aMenu.ownerDocument.createElementNS(XUL_NS, "menuitem");
  item.setAttribute("label", _strings.GetStringFromName(aAttributes.label));
  item.setAttribute("accesskey", _strings.GetStringFromName(aAttributes.accesskey));
  item.addEventListener("command", aAttributes.command);

  aMenu.appendChild(item);

  return item;
}

/**
 * Append a text node to an element.
 */
function appendText(aParent, aText)
{
  aParent.appendChild(aParent.ownerDocument.createTextNode(aText));
}

XPCOMUtils.defineLazyGetter(this, "clipboardHelper", function() {
  return Cc["@mozilla.org/widget/clipboardhelper;1"].
    getService(Ci.nsIClipboardHelper);
});

XPCOMUtils.defineLazyGetter(this, "_strings", function() {
  return Services.strings.createBundle(
    "chrome://browser/locale/devtools/styleinspector.properties");
});

XPCOMUtils.defineLazyGetter(this, "domUtils", function() {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});

loader.lazyGetter(this, "AutocompletePopup", () => require("devtools/shared/autocomplete-popup").AutocompletePopup);
