/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");
const promise = require("promise");
const {setTimeout, clearTimeout} =
      Cu.import("resource://gre/modules/Timer.jsm", {});
const {CssLogic} = require("devtools/shared/styleinspector/css-logic");
const {InplaceEditor, editableField, editableItem} =
      require("devtools/client/shared/inplace-editor");
const {ELEMENT_STYLE} = require("devtools/server/actors/styles");
const {OutputParser} = require("devtools/client/shared/output-parser");
const {PrefObserver, PREF_ORIG_SOURCES} =
      require("devtools/client/styleeditor/utils");
const {
  createChild,
  appendText,
  advanceValidate,
  blurOnMultipleProperties,
  promiseWarn,
  throttle
} = require("devtools/client/styleinspector/utils");
const {
  escapeCSSComment,
  parseDeclarations,
  parseSingleValue,
  parsePseudoClassesAndAttributes,
  SELECTOR_ATTRIBUTE,
  SELECTOR_ELEMENT,
  SELECTOR_PSEUDO_CLASS
} = require("devtools/client/shared/css-parsing-utils");
loader.lazyRequireGetter(this, "overlays",
  "devtools/client/styleinspector/style-inspector-overlays");
loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/shared/event-emitter");
loader.lazyRequireGetter(this, "StyleInspectorMenu",
  "devtools/client/styleinspector/style-inspector-menu");
loader.lazyImporter(this, "Services", "resource://gre/modules/Services.jsm");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const HTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const PREF_UA_STYLES = "devtools.inspector.showUserAgentStyles";
const PREF_DEFAULT_COLOR_UNIT = "devtools.defaultColorUnit";
const PREF_ENABLE_MDN_DOCS_TOOLTIP =
      "devtools.inspector.mdnDocsTooltip.enabled";
const PROPERTY_NAME_CLASS = "ruleview-propertyname";
const FILTER_CHANGED_TIMEOUT = 150;

// This is used to parse user input when filtering.
const FILTER_PROP_RE = /\s*([^:\s]*)\s*:\s*(.*?)\s*;?$/;
// This is used to parse the filter search value to see if the filter
// should be strict or not
const FILTER_STRICT_RE = /\s*`(.*?)`\s*$/;
const IOService = Cc["@mozilla.org/network/io-service;1"]
                  .getService(Ci.nsIIOService);

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
  docShell.createAboutBlankContentViewer(Cc["@mozilla.org/nullprincipal;1"]
                                         .createInstance(Ci.nsIPrincipal));
  let window = docShell.contentViewer.DOMDocument.defaultView;
  window.location = "data:text/html,<html></html>";
  let deferred = promise.defer();
  eventTarget.addEventListener("DOMContentLoaded", function handler() {
    eventTarget.removeEventListener("DOMContentLoaded", handler, false);
    deferred.resolve(window.document);
    frame.remove();
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
 *   Manages a single property from the authoredText attribute of the
 *     relevant declaration.
 *   Maintains a list of computed properties that come from this
 *     property declaration.
 *   Changes to the TextProperty are sent to its related Rule for
 *     application.
 */

/**
 * ElementStyle maintains a list of Rule objects for a given element.
 *
 * @param {Element} element
 *        The element whose style we are viewing.
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
function ElementStyle(element, store, pageStyle, showUserAgentStyles) {
  this.element = element;
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

// We're exporting _ElementStyle for unit tests.
exports._ElementStyle = ElementStyle;

ElementStyle.prototype = {
  // The element we're looking at.
  element: null,

  // Empty, unconnected element of the same type as this node, used
  // to figure out how shorthand properties will be parsed.
  dummyElement: null,

  init: function() {
    // To figure out how shorthand properties are interpreted by the
    // engine, we will set properties on a dummy element and observe
    // how their .style attribute reflects them as computed values.
    this.dummyElementPromise = createDummyDocument().then(document => {
      // ::before and ::after do not have a namespaceURI
      let namespaceURI = this.element.namespaceURI ||
          document.documentElement.namespaceURI;
      this.dummyElement = document.createElementNS(namespaceURI,
                                                   this.element.tagName);
      document.documentElement.appendChild(this.dummyElement);
      return this.dummyElement;
    }).then(null, promiseWarn);
    return this.dummyElementPromise;
  },

  destroy: function() {
    if (this.destroyed) {
      return;
    }
    this.destroyed = true;

    for (let rule of this.rules) {
      if (rule.editor) {
        rule.editor.destroy();
      }
    }

    this.dummyElement = null;
    this.dummyElementPromise.then(dummyElement => {
      dummyElement.remove();
      this.dummyElementPromise = null;
    }, console.error);
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
    let populated = this.pageStyle.getApplied(this.element, {
      inherited: true,
      matchedSelectors: true,
      filter: this.showUserAgentStyles ? "ua" : undefined,
    }).then(entries => {
      if (this.destroyed) {
        return promise.resolve(undefined);
      }

      // Make sure the dummy element has been created before continuing...
      return this.dummyElementPromise.then(() => {
        if (this.populated !== populated) {
          // Don't care anymore.
          return;
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
      });
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
  _sortRulesForPseudoElement: function() {
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
  _maybeAddRule: function(options, existingRules) {
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
  markOverriddenAll: function() {
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
  markOverridden: function(pseudo = "") {
    // Gather all the text properties applied by these rules, ordered
    // from more- to less-specific. Text properties from keyframes rule are
    // excluded from being marked as overridden since a number of criteria such
    // as time, and animation overlay are required to be check in order to
    // determine if the property is overridden.
    let textProps = [];
    for (let rule of this.rules) {
      if (rule.pseudoElement === pseudo && !rule.keyframes) {
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
  _updatePropertyOverridden: function(prop) {
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
 * A single style rule or declaration.
 *
 * @param {ElementStyle} elementStyle
 *        The ElementStyle to which this rule belongs.
 * @param {Object} options
 *        The information used to construct this rule.  Properties include:
 *          rule: A StyleRuleActor
 *          inherited: An element this rule was inherited from.  If omitted,
 *            the rule applies directly to the current element.
 *          isSystem: Is this a user agent style?
 */
function Rule(elementStyle, options) {
  this.elementStyle = elementStyle;
  this.domRule = options.rule || null;
  this.style = options.rule;
  this.matchedSelectors = options.matchedSelectors || [];
  this.pseudoElement = options.pseudoElement || "";

  this.isSystem = options.isSystem;
  this.inherited = options.inherited || null;
  this.keyframes = options.keyframes || null;
  this._modificationDepth = 0;

  if (this.domRule && this.domRule.mediaText) {
    this.mediaText = this.domRule.mediaText;
  }

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
      let eltText = this.inherited.tagName.toLowerCase();
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
  getOriginalSourceStrings: function() {
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
  matches: function(options) {
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
   * @param {TextProperty} siblingProp
   *        Optional, property next to which the new property will be added.
   */
  createProperty: function(name, value, priority, siblingProp) {
    let prop = new TextProperty(this, name, value, priority);

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
    });
    return prop;
  },

  /**
   * Helper function for applyProperties that is called when the actor
   * does not support as-authored styles.  Store disabled properties
   * in the element style's store.
   */
  _applyPropertiesNoAuthored: function(modifications) {
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
      for (let cssProp of parseDeclarations(this.style.authoredText)) {
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
  _applyPropertiesAuthored: function(modifications) {
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
  applyProperties: function(modifier) {
    // If there is already a pending modification, we have to wait
    // until it settles before applying the next modification.
    let resultPromise =
        promise.resolve(this._applyingModifications).then(() => {
          let modifications = this.style.startModifyingProperties();
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
  setPropertyName: function(property, name) {
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
  setPropertyValue: function(property, value, priority) {
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
  previewPropertyValue: function(property, value, priority) {
    let modifications = this.style.startModifyingProperties();
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
  setPropertyEnabled: function(property, value) {
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
  removeProperty: function(property) {
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
  _getTextProperties: function() {
    let textProps = [];
    let store = this.elementStyle.store;
    let props = parseDeclarations(this.style.authoredText, true);
    for (let prop of props) {
      let name = prop.name;
      // In an inherited rule, we only show inherited properties.
      // However, we must keep all properties in order for rule
      // rewriting to work properly.  So, compute the "invisible"
      // property here.
      let invisible = this.inherited && !domUtils.isInheritedProperty(name);
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
  _getDisabledProperties: function() {
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
  refresh: function(options) {
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
  _updateTextProperty: function(newProp) {
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
  editClosestTextProperty: function(textProperty, direction) {
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
  stringifyRule: function() {
    let selectorText = this.selectorText;
    let cssText = "";
    let terminator = osString === "WINNT" ? "\r\n" : "\n";

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
  hasAnyVisibleProperties: function() {
    for (let prop of this.textProps) {
      if (!prop.invisible) {
        return true;
      }
    }
    return false;
  }
};

/**
 * A single property in a rule's authoredText.
 *
 * @param {Rule} rule
 *        The rule this TextProperty came from.
 * @param {String} name
 *        The text property name (such as "background" or "border-top").
 * @param {String} value
 *        The property's value (not including priority).
 * @param {String} priority
 *        The property's priority (either "important" or an empty string).
 * @param {Boolean} enabled
 *        Whether the property is enabled.
 * @param {Boolean} invisible
 *        Whether the property is invisible.  An invisible property
 *        does not show up in the UI; these are needed so that the
 *        index of a property in Rule.textProps is the same as the index
 *        coming from parseDeclarations.
 */
function TextProperty(rule, name, value, priority, enabled = true,
                      invisible = false) {
  this.rule = rule;
  this.name = name;
  this.value = value;
  this.priority = priority;
  this.enabled = !!enabled;
  this.invisible = invisible;
  this.updateComputed();
}

TextProperty.prototype = {
  /**
   * Update the editor associated with this text property,
   * if any.
   */
  updateEditor: function() {
    if (this.editor) {
      this.editor.update();
    }
  },

  /**
   * Update the list of computed properties for this text property.
   */
  updateComputed: function() {
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

    try {
      // Manually get all the properties that are set when setting a value on
      // this.name and check the computed style on dummyElement for each one.
      // If we just read dummyStyle, it would skip properties when value === "".
      let subProps = domUtils.getSubpropertiesForCSSProperty(this.name);

      for (let prop of subProps) {
        this.computed.push({
          textProp: this,
          name: prop,
          value: dummyStyle.getPropertyValue(prop),
          priority: dummyStyle.getPropertyPriority(prop),
        });
      }
    } catch (e) {
      // This is a partial property name, probably from cutting and pasting
      // text. At this point don't check for computed properties.
    }
  },

  /**
   * Set all the values from another TextProperty instance into
   * this TextProperty instance.
   *
   * @param {TextProperty} prop
   *        The other TextProperty instance.
   */
  set: function(prop) {
    let changed = false;
    for (let item of ["name", "value", "priority", "enabled"]) {
      if (this[item] !== prop[item]) {
        this[item] = prop[item];
        changed = true;
      }
    }

    if (changed) {
      this.updateEditor();
    }
  },

  setValue: function(value, priority, force = false) {
    let store = this.rule.elementStyle.store;

    if (this.editor && value !== this.editor.committed.value || force) {
      store.userProperties.setProperty(this.rule.style, this.name, value);
    }

    this.rule.setPropertyValue(this, value, priority);
    this.updateEditor();
  },

  /**
   * Called when the property's value has been updated externally, and
   * the property and editor should update.
   */
  noticeNewValue: function(value) {
    if (value !== this.value) {
      this.value = value;
      this.updateEditor();
    }
  },

  setName: function(name) {
    let store = this.rule.elementStyle.store;

    if (name !== this.name) {
      store.userProperties.setProperty(this.rule.style, name,
                                       this.editor.committed.value);
    }

    this.rule.setPropertyName(this, name);
    this.updateEditor();
  },

  setEnabled: function(value) {
    this.rule.setPropertyEnabled(this, value);
    this.updateEditor();
  },

  remove: function() {
    this.rule.removeProperty(this);
  },

  /**
   * Return a string representation of the rule property.
   */
  stringifyProperty: function() {
    // Get the displayed property value
    let declaration = this.name + ": " + this.editor.valueSpan.textContent +
      ";";

    // Comment out property declarations that are not enabled
    if (!this.enabled) {
      declaration = "/* " + escapeCSSComment(declaration) + " */";
    }

    return declaration;
  },

  /**
   * See whether this property's name is known.
   *
   * @return {Boolean} true if the property name is known, false otherwise.
   */
  isKnownProperty: function() {
    try {
      // If the property name is invalid, the cssPropertyIsShorthand
      // will throw an exception.  But if it is valid, no exception will
      // be thrown; so we just ignore the return value.
      domUtils.cssPropertyIsShorthand(this.name);
      return true;
    } catch (e) {
      return false;
    }
  },

  /**
   * Validate this property. Does it make sense for this value to be assigned
   * to this property name? This does not apply the property value
   *
   * @return {Boolean} true if the property value is valid, false otherwise.
   */
  isValid: function() {
    return domUtils.cssPropertyIsValid(this.name, this.value);
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
 * @param {Inspector} inspector
 *        Inspector toolbox panel
 * @param {Document} document
 *        The document that will contain the rule view.
 * @param {Object} store
 *        The CSS rule view can use this object to store metadata
 *        that might outlast the rule view, particularly the current
 *        set of disabled properties.
 * @param {PageStyleFront} pageStyle
 *        The PageStyleFront for communicating with the remote server.
 */
function CssRuleView(inspector, document, store, pageStyle) {
  this.inspector = inspector;
  this.styleDocument = document;
  this.styleWindow = this.styleDocument.defaultView;
  this.store = store || {};
  this.pageStyle = pageStyle;

  this._outputParser = new OutputParser(document);

  this._onKeydown = this._onKeydown.bind(this);
  this._onKeypress = this._onKeypress.bind(this);
  this._onAddRule = this._onAddRule.bind(this);
  this._onContextMenu = this._onContextMenu.bind(this);
  this._onCopy = this._onCopy.bind(this);
  this._onFilterStyles = this._onFilterStyles.bind(this);
  this._onFilterKeyPress = this._onFilterKeyPress.bind(this);
  this._onClearSearch = this._onClearSearch.bind(this);
  this._onFilterTextboxContextMenu =
    this._onFilterTextboxContextMenu.bind(this);
  this._onTogglePseudoClassPanel = this._onTogglePseudoClassPanel.bind(this);
  this._onTogglePseudoClass = this._onTogglePseudoClass.bind(this);

  let doc = this.styleDocument;
  this.element = doc.getElementById("ruleview-container");
  this.addRuleButton = doc.getElementById("ruleview-add-rule-button");
  this.searchField = doc.getElementById("ruleview-searchbox");
  this.searchClearButton = doc.getElementById("ruleview-searchinput-clear");
  this.pseudoClassPanel = doc.getElementById("pseudo-class-panel");
  this.pseudoClassToggle = doc.getElementById("pseudo-class-panel-toggle");
  this.hoverCheckbox = doc.getElementById("pseudo-hover-toggle");
  this.activeCheckbox = doc.getElementById("pseudo-active-toggle");
  this.focusCheckbox = doc.getElementById("pseudo-focus-toggle");

  this.searchClearButton.hidden = true;

  this.styleDocument.addEventListener("keydown", this._onKeydown);
  this.styleDocument.addEventListener("keypress", this._onKeypress);
  this.element.addEventListener("copy", this._onCopy);
  this.element.addEventListener("contextmenu", this._onContextMenu);
  this.addRuleButton.addEventListener("click", this._onAddRule);
  this.searchField.addEventListener("input", this._onFilterStyles);
  this.searchField.addEventListener("keypress", this._onFilterKeyPress);
  this.searchField.addEventListener("contextmenu",
                                    this._onFilterTextboxContextMenu);
  this.searchClearButton.addEventListener("click", this._onClearSearch);
  this.pseudoClassToggle.addEventListener("click",
                                          this._onTogglePseudoClassPanel);
  this.hoverCheckbox.addEventListener("click", this._onTogglePseudoClass);
  this.activeCheckbox.addEventListener("click", this._onTogglePseudoClass);
  this.focusCheckbox.addEventListener("click", this._onTogglePseudoClass);

  this._handlePrefChange = this._handlePrefChange.bind(this);
  this._onSourcePrefChanged = this._onSourcePrefChanged.bind(this);

  this._prefObserver = new PrefObserver("devtools.");
  this._prefObserver.on(PREF_ORIG_SOURCES, this._onSourcePrefChanged);
  this._prefObserver.on(PREF_UA_STYLES, this._handlePrefChange);
  this._prefObserver.on(PREF_DEFAULT_COLOR_UNIT, this._handlePrefChange);
  this._prefObserver.on(PREF_ENABLE_MDN_DOCS_TOOLTIP, this._handlePrefChange);

  this.showUserAgentStyles = Services.prefs.getBoolPref(PREF_UA_STYLES);
  this.enableMdnDocsTooltip =
    Services.prefs.getBoolPref(PREF_ENABLE_MDN_DOCS_TOOLTIP);

  let options = {
    autoSelect: true,
    theme: "auto"
  };
  this.popup = new AutocompletePopup(this.styleWindow.parent.document, options);

  this._showEmpty();

  this._contextmenu = new StyleInspectorMenu(this, { isRuleView: true });

  // Add the tooltips and highlighters to the view
  this.tooltips = new overlays.TooltipsOverlay(this);
  this.tooltips.addToView();
  this.highlighters = new overlays.HighlightersOverlay(this);
  this.highlighters.addToView();

  EventEmitter.decorate(this);
}

exports.CssRuleView = CssRuleView;

CssRuleView.prototype = {
  // The element that we're inspecting.
  _viewedElement: null,

  // Used for cancelling timeouts in the style filter.
  _filterChangedTimeout: null,

  // Get the filter search value.
  get searchValue() {
    return this.searchField.value.toLowerCase();
  },

  /**
   * Get an instance of SelectorHighlighter (used to highlight nodes that match
   * selectors in the rule-view). A new instance is only created the first time
   * this function is called. The same instance will then be returned.
   *
   * @return {Promise} Resolves to the instance of the highlighter.
   */
  getSelectorHighlighter: Task.async(function*() {
    let utils = this.inspector.toolbox.highlighterUtils;
    if (!utils.supportsCustomHighlighters()) {
      return null;
    }

    if (this.selectorHighlighter) {
      return this.selectorHighlighter;
    }

    try {
      let h = yield utils.getHighlighterByType("SelectorHighlighter");
      this.selectorHighlighter = h;
      return h;
    } catch (e) {
      // The SelectorHighlighter type could not be created in the
      // current target.  It could be an older server, or a XUL page.
      return null;
    }
  }),

  /**
   * Highlight/unhighlight all the nodes that match a given set of selectors
   * inside the document of the current selected node.
   * Only one selector can be highlighted at a time, so calling the method a
   * second time with a different selector will first unhighlight the previously
   * highlighted nodes.
   * Calling the method a second time with the same selector will just
   * unhighlight the highlighted nodes.
   *
   * @param {DOMNode} selectorIcon
   *        The icon that was clicked to toggle the selector. The
   *        class 'highlighted' will be added when the selector is
   *        highlighted.
   * @param {String} selector
   *        The selector used to find nodes in the page.
   */
  toggleSelectorHighlighter: function(selectorIcon, selector) {
    if (this.lastSelectorIcon) {
      this.lastSelectorIcon.classList.remove("highlighted");
    }
    selectorIcon.classList.remove("highlighted");

    this.unhighlightSelector().then(() => {
      if (selector !== this.highlightedSelector) {
        this.highlightedSelector = selector;
        selectorIcon.classList.add("highlighted");
        this.lastSelectorIcon = selectorIcon;
        this.highlightSelector(selector).then(() => {
          this.emit("ruleview-selectorhighlighter-toggled", true);
        }, Cu.reportError);
      } else {
        this.highlightedSelector = null;
        this.emit("ruleview-selectorhighlighter-toggled", false);
      }
    }, Cu.reportError);
  },

  highlightSelector: Task.async(function*(selector) {
    let node = this.inspector.selection.nodeFront;

    let highlighter = yield this.getSelectorHighlighter();
    if (!highlighter) {
      return;
    }

    yield highlighter.show(node, {
      hideInfoBar: true,
      hideGuides: true,
      selector
    });
  }),

  unhighlightSelector: Task.async(function*() {
    let highlighter = yield this.getSelectorHighlighter();
    if (!highlighter) {
      return;
    }

    yield highlighter.hide();
  }),

  /**
   * Get the type of a given node in the rule-view
   *
   * @param {DOMNode} node
   *        The node which we want information about
   * @return {Object} The type information object contains the following props:
   * - type {String} One of the VIEW_NODE_XXX_TYPE const in
   *   style-inspector-overlays
   * - value {Object} Depends on the type of the node
   * returns null of the node isn't anything we care about
   */
  getNodeInfo: function(node) {
    if (!node) {
      return null;
    }

    let type, value;
    let classes = node.classList;
    let prop = getParentTextProperty(node);

    if (classes.contains(PROPERTY_NAME_CLASS) && prop) {
      type = overlays.VIEW_NODE_PROPERTY_TYPE;
      value = {
        property: node.textContent,
        value: getPropertyNameAndValue(node).value,
        enabled: prop.enabled,
        overridden: prop.overridden,
        pseudoElement: prop.rule.pseudoElement,
        sheetHref: prop.rule.domRule.href,
        textProperty: prop
      };
    } else if (classes.contains("ruleview-propertyvalue") && prop) {
      type = overlays.VIEW_NODE_VALUE_TYPE;
      value = {
        property: getPropertyNameAndValue(node).name,
        value: node.textContent,
        enabled: prop.enabled,
        overridden: prop.overridden,
        pseudoElement: prop.rule.pseudoElement,
        sheetHref: prop.rule.domRule.href,
        textProperty: prop
      };
    } else if (classes.contains("theme-link") &&
               !classes.contains("ruleview-rule-source") && prop) {
      type = overlays.VIEW_NODE_IMAGE_URL_TYPE;
      value = {
        property: getPropertyNameAndValue(node).name,
        value: node.parentNode.textContent,
        url: node.href,
        enabled: prop.enabled,
        overridden: prop.overridden,
        pseudoElement: prop.rule.pseudoElement,
        sheetHref: prop.rule.domRule.href,
        textProperty: prop
      };
    } else if (classes.contains("ruleview-selector-unmatched") ||
               classes.contains("ruleview-selector-matched") ||
               classes.contains("ruleview-selectorcontainer") ||
               classes.contains("ruleview-selector") ||
               classes.contains("ruleview-selector-attribute") ||
               classes.contains("ruleview-selector-pseudo-class") ||
               classes.contains("ruleview-selector-pseudo-class-lock")) {
      type = overlays.VIEW_NODE_SELECTOR_TYPE;
      value = this._getRuleEditorForNode(node).selectorText.textContent;
    } else if (classes.contains("ruleview-rule-source") ||
               classes.contains("ruleview-rule-source-label")) {
      type = overlays.VIEW_NODE_LOCATION_TYPE;
      let rule = this._getRuleEditorForNode(node).rule;
      value = (rule.sheet && rule.sheet.href) ? rule.sheet.href : rule.title;
    } else {
      return null;
    }

    return {type, value};
  },

  /**
   * Retrieve the RuleEditor instance that should be stored on
   * the offset parent of the node
   */
  _getRuleEditorForNode: function(node) {
    if (!node.offsetParent) {
      // some nodes don't have an offsetParent, but their parentNode does
      node = node.parentNode;
    }
    return node.offsetParent._ruleEditor;
  },

  /**
   * Context menu handler.
   */
  _onContextMenu: function(event) {
    this._contextmenu.show(event);
  },

  /**
   * Callback for copy event. Copy the selected text.
   *
   * @param {Event} event
   *        copy event object.
   */
  _onCopy: function(event) {
    if (event) {
      this.copySelection(event.target);
      event.preventDefault();
    }
  },

  /**
   * Copy the current selection. The current target is necessary
   * if the selection is inside an input or a textarea
   *
   * @param {DOMNode} target
   *        DOMNode target of the copy action
   */
  copySelection: function(target) {
    try {
      let text = "";

      if (target && target.nodeName === "input") {
        let start = Math.min(target.selectionStart, target.selectionEnd);
        let end = Math.max(target.selectionStart, target.selectionEnd);
        let count = end - start;
        text = target.value.substr(start, count);
      } else {
        text = this.styleWindow.getSelection().toString();

        // Remove any double newlines.
        text = text.replace(/(\r?\n)\r?\n/g, "$1");

        // Remove "inline"
        let inline = _strings.GetStringFromName("rule.sourceInline");
        let rx = new RegExp("^" + inline + "\\r?\\n?", "g");
        text = text.replace(rx, "");
      }

      clipboardHelper.copyString(text);
    } catch (e) {
      console.error(e);
    }
  },

  /**
   * A helper for _onAddRule that handles the case where the actor
   * does not support as-authored styles.
   */
  _onAddNewRuleNonAuthored: function() {
    let elementStyle = this._elementStyle;
    let element = elementStyle.element;
    let rules = elementStyle.rules;
    let pseudoClasses = element.pseudoClassLocks;

    this.pageStyle.addNewRule(element, pseudoClasses).then(options => {
      let newRule = new Rule(elementStyle, options);
      rules.push(newRule);
      let editor = new RuleEditor(this, newRule);
      newRule.editor = editor;

      // Insert the new rule editor after the inline element rule
      if (rules.length <= 1) {
        this.element.appendChild(editor.element);
      } else {
        for (let rule of rules) {
          if (rule.domRule.type === ELEMENT_STYLE) {
            let referenceElement = rule.editor.element.nextSibling;
            this.element.insertBefore(editor.element, referenceElement);
            break;
          }
        }
      }

      // Focus and make the new rule's selector editable
      editor.selectorText.click();
      elementStyle._changed();
    });
  },

  /**
   * Add a new rule to the current element.
   */
  _onAddRule: function() {
    let elementStyle = this._elementStyle;
    let element = elementStyle.element;
    let client = this.inspector.toolbox._target.client;
    let pseudoClasses = element.pseudoClassLocks;

    if (!client.traits.addNewRule) {
      return;
    }

    if (!this.pageStyle.supportsAuthoredStyles) {
      // We're talking to an old server.
      this._onAddNewRuleNonAuthored();
      return;
    }

    // Adding a new rule with authored styles will cause the actor to
    // emit an event, which will in turn cause the rule view to be
    // updated.  So, we wait for this update and for the rule creation
    // request to complete, and then focus the new rule's selector.
    let eventPromise = this.once("ruleview-refreshed");
    let newRulePromise = this.pageStyle.addNewRule(element, pseudoClasses);
    promise.all([eventPromise, newRulePromise]).then((values) => {
      let options = values[1];
      // Be sure the reference the correct |rules| here.
      for (let rule of this._elementStyle.rules) {
        if (options.rule === rule.domRule) {
          rule.editor.selectorText.click();
          elementStyle._changed();
          break;
        }
      }
    });
  },

  /**
   * Disables add rule button when needed
   */
  refreshAddRuleButtonState: function() {
    let shouldBeDisabled = !this._viewedElement ||
                           !this.inspector.selection.isElementNode() ||
                           this.inspector.selection.isAnonymousNode();
    this.addRuleButton.disabled = shouldBeDisabled;
  },

  setPageStyle: function(pageStyle) {
    this.pageStyle = pageStyle;
  },

  /**
   * Return {Boolean} true if the rule view currently has an input
   * editor visible.
   */
  get isEditing() {
    return this.tooltips.isEditing ||
      this.element.querySelectorAll(".styleinspector-propertyeditor")
        .length > 0;
  },

  _handlePrefChange: function(pref) {
    if (pref === PREF_UA_STYLES) {
      this.showUserAgentStyles = Services.prefs.getBoolPref(pref);
    }

    // Reselect the currently selected element
    let refreshOnPrefs = [PREF_UA_STYLES, PREF_DEFAULT_COLOR_UNIT];
    if (refreshOnPrefs.indexOf(pref) > -1) {
      this.selectElement(this._viewedElement, true);
    }
  },

  /**
   * Update source links when pref for showing original sources changes
   */
  _onSourcePrefChanged: function() {
    if (this._elementStyle && this._elementStyle.rules) {
      for (let rule of this._elementStyle.rules) {
        if (rule.editor) {
          rule.editor.updateSourceLink();
        }
      }
      this.inspector.emit("rule-view-sourcelinks-updated");
    }
  },

  /**
   * Set the filter style search value.
   * @param {String} value
   *        The search value.
   */
  setFilterStyles: function(value = "") {
    this.searchField.value = value;
    this.searchField.focus();
    this._onFilterStyles();
  },

  /**
   * Called when the user enters a search term in the filter style search box.
   */
  _onFilterStyles: function() {
    if (this._filterChangedTimeout) {
      clearTimeout(this._filterChangedTimeout);
    }

    let filterTimeout = (this.searchValue.length > 0) ?
                        FILTER_CHANGED_TIMEOUT : 0;
    this.searchClearButton.hidden = this.searchValue.length === 0;

    this._filterChangedTimeout = setTimeout(() => {
      if (this.searchField.value.length > 0) {
        this.searchField.setAttribute("filled", true);
      } else {
        this.searchField.removeAttribute("filled");
      }

      this.searchData = {
        searchPropertyMatch: FILTER_PROP_RE.exec(this.searchValue),
        searchPropertyName: this.searchValue,
        searchPropertyValue: this.searchValue,
        strictSearchValue: "",
        strictSearchPropertyName: false,
        strictSearchPropertyValue: false,
        strictSearchAllValues: false
      };

      if (this.searchData.searchPropertyMatch) {
        // Parse search value as a single property line and extract the
        // property name and value. If the parsed property name or value is
        // contained in backquotes (`), extract the value within the backquotes
        // and set the corresponding strict search for the property to true.
        if (FILTER_STRICT_RE.test(this.searchData.searchPropertyMatch[1])) {
          this.searchData.strictSearchPropertyName = true;
          this.searchData.searchPropertyName =
            FILTER_STRICT_RE.exec(this.searchData.searchPropertyMatch[1])[1];
        } else {
          this.searchData.searchPropertyName =
            this.searchData.searchPropertyMatch[1];
        }

        if (FILTER_STRICT_RE.test(this.searchData.searchPropertyMatch[2])) {
          this.searchData.strictSearchPropertyValue = true;
          this.searchData.searchPropertyValue =
            FILTER_STRICT_RE.exec(this.searchData.searchPropertyMatch[2])[1];
        } else {
          this.searchData.searchPropertyValue =
            this.searchData.searchPropertyMatch[2];
        }

        // Strict search for stylesheets will match the property line regex.
        // Extract the search value within the backquotes to be used
        // in the strict search for stylesheets in _highlightStyleSheet.
        if (FILTER_STRICT_RE.test(this.searchValue)) {
          this.searchData.strictSearchValue =
            FILTER_STRICT_RE.exec(this.searchValue)[1];
        }
      } else if (FILTER_STRICT_RE.test(this.searchValue)) {
        // If the search value does not correspond to a property line and
        // is contained in backquotes, extract the search value within the
        // backquotes and set the flag to perform a strict search for all
        // the values (selector, stylesheet, property and computed values).
        let searchValue = FILTER_STRICT_RE.exec(this.searchValue)[1];
        this.searchData.strictSearchAllValues = true;
        this.searchData.searchPropertyName = searchValue;
        this.searchData.searchPropertyValue = searchValue;
        this.searchData.strictSearchValue = searchValue;
      }

      this._clearHighlight(this.element);
      this._clearRules();
      this._createEditors();

      this.inspector.emit("ruleview-filtered");

      this._filterChangeTimeout = null;
    }, filterTimeout);
  },

  /**
   * Handle the search box's keypress event. If the escape key is pressed,
   * clear the search box field.
   */
  _onFilterKeyPress: function(event) {
    if (event.keyCode === Ci.nsIDOMKeyEvent.DOM_VK_ESCAPE &&
        this._onClearSearch()) {
      event.preventDefault();
      event.stopPropagation();
    }
  },

  /**
   * Context menu handler for filter style search box.
   */
  _onFilterTextboxContextMenu: function(event) {
    try {
      this.styleWindow.focus();
      let contextmenu = this.inspector.toolbox.textboxContextMenuPopup;
      contextmenu.openPopupAtScreen(event.screenX, event.screenY, true);
    } catch (e) {
      console.error(e);
    }
  },

  /**
   * Called when the user clicks on the clear button in the filter style search
   * box. Returns true if the search box is cleared and false otherwise.
   */
  _onClearSearch: function() {
    if (this.searchField.value) {
      this.setFilterStyles("");
      return true;
    }

    return false;
  },

  destroy: function() {
    this.isDestroyed = true;
    this.clear();

    gDummyPromise = null;

    this._prefObserver.off(PREF_ORIG_SOURCES, this._onSourcePrefChanged);
    this._prefObserver.off(PREF_UA_STYLES, this._handlePrefChange);
    this._prefObserver.off(PREF_DEFAULT_COLOR_UNIT, this._handlePrefChange);
    this._prefObserver.destroy();

    this._outputParser = null;

    // Remove context menu
    if (this._contextmenu) {
      this._contextmenu.destroy();
      this._contextmenu = null;
    }

    this.tooltips.destroy();
    this.highlighters.destroy();

    // Remove bound listeners
    this.styleDocument.removeEventListener("keydown", this._onKeydown);
    this.styleDocument.removeEventListener("keypress", this._onKeypress);
    this.element.removeEventListener("copy", this._onCopy);
    this.element.removeEventListener("contextmenu", this._onContextMenu);
    this.addRuleButton.removeEventListener("click", this._onAddRule);
    this.searchField.removeEventListener("input", this._onFilterStyles);
    this.searchField.removeEventListener("keypress", this._onFilterKeyPress);
    this.searchField.removeEventListener("contextmenu",
      this._onFilterTextboxContextMenu);
    this.searchClearButton.removeEventListener("click", this._onClearSearch);
    this.pseudoClassToggle.removeEventListener("click",
      this._onTogglePseudoClassPanel);
    this.hoverCheckbox.removeEventListener("click", this._onTogglePseudoClass);
    this.activeCheckbox.removeEventListener("click", this._onTogglePseudoClass);
    this.focusCheckbox.removeEventListener("click", this._onTogglePseudoClass);

    this.searchField = null;
    this.searchClearButton = null;
    this.pseudoClassPanel = null;
    this.pseudoClassToggle = null;
    this.hoverCheckbox = null;
    this.activeCheckbox = null;
    this.focusCheckbox = null;

    this.inspector = null;
    this.styleDocument = null;
    this.styleWindow = null;

    if (this.element.parentNode) {
      this.element.parentNode.removeChild(this.element);
    }

    if (this._elementStyle) {
      this._elementStyle.destroy();
    }

    this.popup.destroy();
  },

  /**
   * Mark the view as selecting an element, disabling all interaction, and
   * visually clearing the view after a few milliseconds to avoid confusion
   * about which element's styles the rule view shows.
   */
  _startSelectingElement: function() {
    this.element.classList.add("non-interactive");
  },

  /**
   * Mark the view as no longer selecting an element, re-enabling interaction.
   */
  _stopSelectingElement: function() {
    this.element.classList.remove("non-interactive");
  },

  /**
   * Update the view with a new selected element.
   *
   * @param {NodeActor} element
   *        The node whose style rules we'll inspect.
   * @param {Boolean} allowRefresh
   *        Update the view even if the element is the same as last time.
   */
  selectElement: function(element, allowRefresh = false) {
    let refresh = (this._viewedElement === element);
    if (refresh && !allowRefresh) {
      return promise.resolve(undefined);
    }

    if (this.popup.isOpen) {
      this.popup.hidePopup();
    }

    this.clear(false);
    this._viewedElement = element;

    this.clearPseudoClassPanel();
    this.refreshAddRuleButtonState();

    if (!this._viewedElement) {
      this._stopSelectingElement();
      this._clearRules();
      this._showEmpty();
      this.refreshPseudoClassPanel();
      return promise.resolve(undefined);
    }

    let elementStyle = new ElementStyle(element, this.store,
      this.pageStyle, this.showUserAgentStyles);
    this._elementStyle = elementStyle;

    this._startSelectingElement();

    return this._elementStyle.init().then(() => {
      if (this._elementStyle === elementStyle) {
        return this._populate();
      }
    }).then(() => {
      if (this._elementStyle === elementStyle) {
        if (!refresh) {
          this.element.scrollTop = 0;
        }
        this._stopSelectingElement();
        this._elementStyle.onChanged = () => {
          this._changed();
        };
      }
    }).then(null, e => {
      if (this._elementStyle === elementStyle) {
        this._stopSelectingElement();
        this._clearRules();
      }
      console.error(e);
    });
  },

  /**
   * Update the rules for the currently highlighted element.
   */
  refreshPanel: function() {
    // Ignore refreshes during editing or when no element is selected.
    if (this.isEditing || !this._elementStyle) {
      return promise.resolve(undefined);
    }

    // Repopulate the element style once the current modifications are done.
    let promises = [];
    for (let rule of this._elementStyle.rules) {
      if (rule._applyingModifications) {
        promises.push(rule._applyingModifications);
      }
    }

    return promise.all(promises).then(() => {
      return this._populate();
    });
  },

  /**
   * Clear the pseudo class options panel by removing the checked and disabled
   * attributes for each checkbox.
   */
  clearPseudoClassPanel: function() {
    this.hoverCheckbox.checked = this.hoverCheckbox.disabled = false;
    this.activeCheckbox.checked = this.activeCheckbox.disabled = false;
    this.focusCheckbox.checked = this.focusCheckbox.disabled = false;
  },

  /**
   * Update the pseudo class options for the currently highlighted element.
   */
  refreshPseudoClassPanel: function() {
    if (!this._elementStyle || !this.inspector.selection.isElementNode()) {
      this.hoverCheckbox.disabled = true;
      this.activeCheckbox.disabled = true;
      this.focusCheckbox.disabled = true;
      return;
    }

    for (let pseudoClassLock of this._elementStyle.element.pseudoClassLocks) {
      switch (pseudoClassLock) {
        case ":hover": {
          this.hoverCheckbox.checked = true;
          break;
        }
        case ":active": {
          this.activeCheckbox.checked = true;
          break;
        }
        case ":focus": {
          this.focusCheckbox.checked = true;
          break;
        }
      }
    }
  },

  _populate: function() {
    let elementStyle = this._elementStyle;
    return this._elementStyle.populate().then(() => {
      if (this._elementStyle !== elementStyle || this.isDestroyed) {
        return;
      }

      this._clearRules();
      this._createEditors();

      this.refreshPseudoClassPanel();

      // Notify anyone that cares that we refreshed.
      this.emit("ruleview-refreshed");
    }).then(null, promiseWarn);
  },

  /**
   * Show the user that the rule view has no node selected.
   */
  _showEmpty: function() {
    if (this.styleDocument.getElementById("noResults") > 0) {
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
  _clearRules: function() {
    this.element.innerHTML = "";
  },

  /**
   * Clear the rule view.
   */
  clear: function(clearDom = true) {
    this.lastSelectorIcon = null;

    if (clearDom) {
      this._clearRules();
    }
    this._viewedElement = null;

    if (this._elementStyle) {
      this._elementStyle.destroy();
      this._elementStyle = null;
    }
  },

  /**
   * Called when the user has made changes to the ElementStyle.
   * Emits an event that clients can listen to.
   */
  _changed: function() {
    this.emit("ruleview-changed");
  },

  /**
   * Text for header that shows above rules for this element
   */
  get selectedElementLabel() {
    if (this._selectedElementLabel) {
      return this._selectedElementLabel;
    }
    this._selectedElementLabel = CssLogic.l10n("rule.selectedElement");
    return this._selectedElementLabel;
  },

  /**
   * Text for header that shows above rules for pseudo elements
   */
  get pseudoElementLabel() {
    if (this._pseudoElementLabel) {
      return this._pseudoElementLabel;
    }
    this._pseudoElementLabel = CssLogic.l10n("rule.pseudoElement");
    return this._pseudoElementLabel;
  },

  get showPseudoElements() {
    if (this._showPseudoElements === undefined) {
      this._showPseudoElements =
        Services.prefs.getBoolPref("devtools.inspector.show_pseudo_elements");
    }
    return this._showPseudoElements;
  },

  /**
   * Creates an expandable container in the rule view
   *
   * @param  {String} label
   *         The label for the container header
   * @param  {Boolean} isPseudo
   *         Whether or not the container will hold pseudo element rules
   * @return {DOMNode} The container element
   */
  createExpandableContainer: function(label, isPseudo = false) {
    let header = this.styleDocument.createElementNS(HTML_NS, "div");
    header.className = this._getRuleViewHeaderClassName(true);
    header.classList.add("show-expandable-container");
    header.textContent = label;

    let twisty = this.styleDocument.createElementNS(HTML_NS, "span");
    twisty.className = "ruleview-expander theme-twisty";
    twisty.setAttribute("open", "true");

    header.insertBefore(twisty, header.firstChild);
    this.element.appendChild(header);

    let container = this.styleDocument.createElementNS(HTML_NS, "div");
    container.classList.add("ruleview-expandable-container");
    this.element.appendChild(container);

    header.addEventListener("dblclick", () => {
      this._toggleContainerVisibility(twisty, header, isPseudo,
        !this.showPseudoElements);
    }, false);

    twisty.addEventListener("click", () => {
      this._toggleContainerVisibility(twisty, header, isPseudo,
        !this.showPseudoElements);
    }, false);

    if (isPseudo) {
      this._toggleContainerVisibility(twisty, header, isPseudo,
        this.showPseudoElements);
    }

    return container;
  },

  /**
   * Toggle the visibility of an expandable container
   *
   * @param  {DOMNode}  twisty
   *         clickable toggle DOM Node
   * @param  {DOMNode}  header
   *         expandable container header DOM Node
   * @param  {Boolean}  isPseudo
   *         whether or not the container will hold pseudo element rules
   * @param  {Boolean}  showPseudo
   *         whether or not pseudo element rules should be displayed
   */
  _toggleContainerVisibility: function(twisty, header, isPseudo, showPseudo) {
    let isOpen = twisty.getAttribute("open");

    if (isPseudo) {
      this._showPseudoElements = !!showPseudo;

      Services.prefs.setBoolPref("devtools.inspector.show_pseudo_elements",
        this.showPseudoElements);

      header.classList.toggle("show-expandable-container",
        this.showPseudoElements);

      isOpen = !this.showPseudoElements;
    } else {
      header.classList.toggle("show-expandable-container");
    }

    if (isOpen) {
      twisty.removeAttribute("open");
    } else {
      twisty.setAttribute("open", "true");
    }
  },

  _getRuleViewHeaderClassName: function(isPseudo) {
    let baseClassName = "theme-gutter ruleview-header";
    return isPseudo ? baseClassName + " ruleview-expandable-header" :
      baseClassName;
  },

  /**
   * Creates editor UI for each of the rules in _elementStyle.
   */
  _createEditors: function() {
    // Run through the current list of rules, attaching
    // their editors in order.  Create editors if needed.
    let lastInheritedSource = "";
    let lastKeyframes = null;
    let seenPseudoElement = false;
    let seenNormalElement = false;
    let seenSearchTerm = false;
    let container = null;

    if (!this._elementStyle.rules) {
      return;
    }

    for (let rule of this._elementStyle.rules) {
      if (rule.domRule.system) {
        continue;
      }

      // Initialize rule editor
      if (!rule.editor) {
        rule.editor = new RuleEditor(this, rule);
      }

      // Filter the rules and highlight any matches if there is a search input
      if (this.searchValue && this.searchData) {
        if (this.highlightRule(rule)) {
          seenSearchTerm = true;
        } else if (rule.domRule.type !== ELEMENT_STYLE) {
          continue;
        }
      }

      // Only print header for this element if there are pseudo elements
      if (seenPseudoElement && !seenNormalElement && !rule.pseudoElement) {
        seenNormalElement = true;
        let div = this.styleDocument.createElementNS(HTML_NS, "div");
        div.className = this._getRuleViewHeaderClassName();
        div.textContent = this.selectedElementLabel;
        this.element.appendChild(div);
      }

      let inheritedSource = rule.inheritedSource;
      if (inheritedSource && inheritedSource !== lastInheritedSource) {
        let div = this.styleDocument.createElementNS(HTML_NS, "div");
        div.className = this._getRuleViewHeaderClassName();
        div.textContent = inheritedSource;
        lastInheritedSource = inheritedSource;
        this.element.appendChild(div);
      }

      if (!seenPseudoElement && rule.pseudoElement) {
        seenPseudoElement = true;
        container = this.createExpandableContainer(this.pseudoElementLabel,
                                                   true);
      }

      let keyframes = rule.keyframes;
      if (keyframes && keyframes !== lastKeyframes) {
        lastKeyframes = keyframes;
        container = this.createExpandableContainer(rule.keyframesName);
      }

      if (container && (rule.pseudoElement || keyframes)) {
        container.appendChild(rule.editor.element);
      } else {
        this.element.appendChild(rule.editor.element);
      }
    }

    if (this.searchValue && !seenSearchTerm) {
      this.searchField.classList.add("devtools-style-searchbox-no-match");
    } else {
      this.searchField.classList.remove("devtools-style-searchbox-no-match");
    }
  },

  /**
   * Highlight rules that matches the filter search value and returns a
   * boolean indicating whether or not rules were highlighted.
   *
   * @param  {Rule} rule
   *         The rule object we're highlighting if its rule selectors or
   *         property values match the search value.
   * @return {Boolean} true if the rule was highlighted, false otherwise.
   */
  highlightRule: function(rule) {
    let isRuleSelectorHighlighted = this._highlightRuleSelector(rule);
    let isStyleSheetHighlighted = this._highlightStyleSheet(rule);
    let isHighlighted = isRuleSelectorHighlighted || isStyleSheetHighlighted;

    // Highlight search matches in the rule properties
    for (let textProp of rule.textProps) {
      if (!textProp.invisible && this._highlightProperty(textProp.editor)) {
        isHighlighted = true;
      }
    }

    return isHighlighted;
  },

  /**
   * Highlights the rule selector that matches the filter search value and
   * returns a boolean indicating whether or not the selector was highlighted.
   *
   * @param  {Rule} rule
   *         The Rule object.
   * @return {Boolean} true if the rule selector was highlighted,
   *         false otherwise.
   */
  _highlightRuleSelector: function(rule) {
    let isSelectorHighlighted = false;

    let selectorNodes = [...rule.editor.selectorText.childNodes];
    if (rule.domRule.type === Ci.nsIDOMCSSRule.KEYFRAME_RULE) {
      selectorNodes = [rule.editor.selectorText];
    } else if (rule.domRule.type === ELEMENT_STYLE) {
      selectorNodes = [];
    }

    // Highlight search matches in the rule selectors
    for (let selectorNode of selectorNodes) {
      let selector = selectorNode.textContent.toLowerCase();
      if ((this.searchData.strictSearchAllValues &&
           selector === this.searchData.strictSearchValue) ||
          (!this.searchData.strictSearchAllValues &&
           selector.includes(this.searchValue))) {
        selectorNode.classList.add("ruleview-highlight");
        isSelectorHighlighted = true;
      }
    }

    return isSelectorHighlighted;
  },

  /**
   * Highlights the stylesheet source that matches the filter search value and
   * returns a boolean indicating whether or not the stylesheet source was
   * highlighted.
   *
   * @return {Boolean} true if the stylesheet source was highlighted, false
   *         otherwise.
   */
  _highlightStyleSheet: function(rule) {
    let styleSheetSource = rule.title.toLowerCase();
    let isStyleSheetHighlighted = this.searchData.strictSearchValue ?
      styleSheetSource === this.searchData.strictSearchValue :
      styleSheetSource.includes(this.searchValue);

    if (isStyleSheetHighlighted) {
      rule.editor.source.classList.add("ruleview-highlight");
    }

    return isStyleSheetHighlighted;
  },

  /**
   * Highlights the rule properties and computed properties that match the
   * filter search value and returns a boolean indicating whether or not the
   * property or computed property was highlighted.
   *
   * @param  {TextPropertyEditor} editor
   *         The rule property TextPropertyEditor object.
   * @return {Boolean} true if the property or computed property was
   *         highlighted, false otherwise.
   */
  _highlightProperty: function(editor) {
    let isPropertyHighlighted = this._highlightRuleProperty(editor);
    let isComputedHighlighted = this._highlightComputedProperty(editor);

    // Expand the computed list if a computed property is highlighted and the
    // property rule is not highlighted
    if (!isPropertyHighlighted && isComputedHighlighted &&
        !editor.computed.hasAttribute("user-open")) {
      editor.expandForFilter();
    }

    return isPropertyHighlighted || isComputedHighlighted;
  },

  /**
   * Called when TextPropertyEditor is updated and updates the rule property
   * highlight.
   *
   * @param  {TextPropertyEditor} editor
   *         The rule property TextPropertyEditor object.
   */
  _updatePropertyHighlight: function(editor) {
    if (!this.searchValue || !this.searchData) {
      return;
    }

    this._clearHighlight(editor.element);

    if (this._highlightProperty(editor)) {
      this.searchField.classList.remove("devtools-style-searchbox-no-match");
    }
  },

  /**
   * Highlights the rule property that matches the filter search value
   * and returns a boolean indicating whether or not the property was
   * highlighted.
   *
   * @param  {TextPropertyEditor} editor
   *         The rule property TextPropertyEditor object.
   * @return {Boolean} true if the rule property was highlighted,
   *         false otherwise.
   */
  _highlightRuleProperty: function(editor) {
    // Get the actual property value displayed in the rule view
    let propertyName = editor.prop.name.toLowerCase();
    let propertyValue = editor.valueSpan.textContent.toLowerCase();

    return this._highlightMatches(editor.container, propertyName,
                                  propertyValue);
  },

  /**
   * Highlights the computed property that matches the filter search value and
   * returns a boolean indicating whether or not the computed property was
   * highlighted.
   *
   * @param  {TextPropertyEditor} editor
   *         The rule property TextPropertyEditor object.
   * @return {Boolean} true if the computed property was highlighted, false
   *         otherwise.
   */
  _highlightComputedProperty: function(editor) {
    let isComputedHighlighted = false;

    // Highlight search matches in the computed list of properties
    editor._populateComputed();
    for (let computed of editor.prop.computed) {
      if (computed.element) {
        // Get the actual property value displayed in the computed list
        let computedName = computed.name.toLowerCase();
        let computedValue = computed.parsedValue.toLowerCase();

        isComputedHighlighted = this._highlightMatches(computed.element,
          computedName, computedValue) ? true : isComputedHighlighted;
      }
    }

    return isComputedHighlighted;
  },

  /**
   * Helper function for highlightRules that carries out highlighting the given
   * element if the search terms match the property, and returns a boolean
   * indicating whether or not the search terms match.
   *
   * @param  {DOMNode} element
   *         The node to highlight if search terms match
   * @param  {String} propertyName
   *         The property name of a rule
   * @param  {String} propertyValue
   *         The property value of a rule
   * @return {Boolean} true if the given search terms match the property, false
   *         otherwise.
   */
  _highlightMatches: function(element, propertyName, propertyValue) {
    let {
      searchPropertyName,
      searchPropertyValue,
      searchPropertyMatch,
      strictSearchPropertyName,
      strictSearchPropertyValue,
      strictSearchAllValues,
    } = this.searchData;
    let matches = false;

    // If the inputted search value matches a property line like
    // `font-family: arial`, then check to make sure the name and value match.
    // Otherwise, just compare the inputted search string directly against the
    // name and value of the rule property.
    let hasNameAndValue = searchPropertyMatch &&
                          searchPropertyName &&
                          searchPropertyValue;
    let isMatch = (value, query, isStrict) => {
      return isStrict ? value === query : query && value.includes(query);
    };

    if (hasNameAndValue) {
      matches =
        isMatch(propertyName, searchPropertyName, strictSearchPropertyName) &&
        isMatch(propertyValue, searchPropertyValue, strictSearchPropertyValue);
    } else {
      matches =
        isMatch(propertyName, searchPropertyName,
                strictSearchPropertyName || strictSearchAllValues) ||
        isMatch(propertyValue, searchPropertyValue,
                strictSearchPropertyValue || strictSearchAllValues);
    }

    if (matches) {
      element.classList.add("ruleview-highlight");
    }

    return matches;
  },

  /**
   * Clear all search filter highlights in the panel, and close the computed
   * list if toggled opened
   */
  _clearHighlight: function(element) {
    for (let el of element.querySelectorAll(".ruleview-highlight")) {
      el.classList.remove("ruleview-highlight");
    }

    for (let computed of element.querySelectorAll(
          ".ruleview-computedlist[filter-open]")) {
      computed.parentNode._textPropertyEditor.collapseForFilter();
    }
  },

  /**
   * Called when the pseudo class panel button is clicked and toggles
   * the display of the pseudo class panel.
   */
  _onTogglePseudoClassPanel: function() {
    if (this.pseudoClassPanel.hidden) {
      this.pseudoClassToggle.setAttribute("checked", "true");
      this.hoverCheckbox.setAttribute("tabindex", "0");
      this.activeCheckbox.setAttribute("tabindex", "0");
      this.focusCheckbox.setAttribute("tabindex", "0");
    } else {
      this.pseudoClassToggle.removeAttribute("checked");
      this.hoverCheckbox.setAttribute("tabindex", "-1");
      this.activeCheckbox.setAttribute("tabindex", "-1");
      this.focusCheckbox.setAttribute("tabindex", "-1");
    }

    this.pseudoClassPanel.hidden = !this.pseudoClassPanel.hidden;
  },

  /**
   * Called when a pseudo class checkbox is clicked and toggles
   * the pseudo class for the current selected element.
   */
  _onTogglePseudoClass: function(event) {
    let target = event.currentTarget;
    this.inspector.togglePseudoClass(target.value);
  },

  /**
   * Handle the keydown event in the rule view.
   */
  _onKeydown: function(event) {
    if (this.element.classList.contains("non-interactive") &&
        (event.code === "Enter" || event.code === " ")) {
      event.preventDefault();
    }
  },

  /**
   * Handle the keypress event in the rule view.
   */
  _onKeypress: function(event) {
    let isOSX = Services.appinfo.OS === "Darwin";

    if (((isOSX && event.metaKey && !event.ctrlKey && !event.altKey) ||
        (!isOSX && event.ctrlKey && !event.metaKey && !event.altKey)) &&
        event.code === "KeyF") {
      this.searchField.focus();
      event.preventDefault();
    }
  }
};

/**
 * Create a RuleEditor.
 *
 * @param {CssRuleView} ruleView
 *        The CssRuleView containg the document holding this rule editor.
 * @param {Rule} rule
 *        The Rule object we're editing.
 */
function RuleEditor(ruleView, rule) {
  this.ruleView = ruleView;
  this.doc = this.ruleView.styleDocument;
  this.rule = rule;

  this.isEditable = !rule.isSystem;
  // Flag that blocks updates of the selector and properties when it is
  // being edited
  this.isEditing = false;

  this._onNewProperty = this._onNewProperty.bind(this);
  this._newPropertyDestroy = this._newPropertyDestroy.bind(this);
  this._onSelectorDone = this._onSelectorDone.bind(this);
  this._locationChanged = this._locationChanged.bind(this);

  this.rule.domRule.on("location-changed", this._locationChanged);

  this._create();
}

RuleEditor.prototype = {
  destroy: function() {
    this.rule.domRule.off("location-changed");
  },

  get isSelectorEditable() {
    let toolbox = this.ruleView.inspector.toolbox;
    let trait = this.isEditable &&
      toolbox.target.client.traits.selectorEditable &&
      this.rule.domRule.type !== ELEMENT_STYLE &&
      this.rule.domRule.type !== Ci.nsIDOMCSSRule.KEYFRAME_RULE;

    // Do not allow editing anonymousselectors until we can
    // detect mutations on  pseudo elements in Bug 1034110.
    return trait && !this.rule.elementStyle.element.isAnonymous;
  },

  _create: function() {
    this.element = this.doc.createElementNS(HTML_NS, "div");
    this.element.className = "ruleview-rule theme-separator";
    this.element.setAttribute("uneditable", !this.isEditable);
    this.element._ruleEditor = this;

    // Give a relative position for the inplace editor's measurement
    // span to be placed absolutely against.
    this.element.style.position = "relative";

    // Add the source link.
    this.source = createChild(this.element, "div", {
      class: "ruleview-rule-source theme-link"
    });
    this.source.addEventListener("click", function() {
      if (this.source.hasAttribute("unselectable")) {
        return;
      }
      let rule = this.rule.domRule;
      this.ruleView.emit("ruleview-linked-clicked", rule);
    }.bind(this));
    let sourceLabel = this.doc.createElementNS(XUL_NS, "label");
    sourceLabel.setAttribute("crop", "center");
    sourceLabel.classList.add("ruleview-rule-source-label");
    this.source.appendChild(sourceLabel);

    this.updateSourceLink();

    let code = createChild(this.element, "div", {
      class: "ruleview-code"
    });

    let header = createChild(code, "div", {});

    this.selectorText = createChild(header, "span", {
      class: "ruleview-selectorcontainer theme-fg-color3",
      tabindex: this.isSelectorEditable ? "0" : "-1",
    });

    if (this.isSelectorEditable) {
      this.selectorText.addEventListener("click", event => {
        // Clicks within the selector shouldn't propagate any further.
        event.stopPropagation();
      }, false);

      editableField({
        element: this.selectorText,
        done: this._onSelectorDone,
      });
    }

    if (this.rule.domRule.type !== Ci.nsIDOMCSSRule.KEYFRAME_RULE &&
        this.rule.domRule.selectors) {
      let selector = this.rule.domRule.selectors.join(", ");

      let selectorHighlighter = createChild(header, "span", {
        class: "ruleview-selectorhighlighter" +
               (this.ruleView.highlightedSelector === selector ?
                " highlighted" : ""),
        title: CssLogic.l10n("rule.selectorHighlighter.tooltip")
      });
      selectorHighlighter.addEventListener("click", () => {
        this.ruleView.toggleSelectorHighlighter(selectorHighlighter, selector);
      });
    }

    this.openBrace = createChild(header, "span", {
      class: "ruleview-ruleopen",
      textContent: " {"
    });

    this.propertyList = createChild(code, "ul", {
      class: "ruleview-propertylist"
    });

    this.populate();

    this.closeBrace = createChild(code, "div", {
      class: "ruleview-ruleclose",
      tabindex: this.isEditable ? "0" : "-1",
      textContent: "}"
    });

    if (this.isEditable) {
      code.addEventListener("click", () => {
        let selection = this.doc.defaultView.getSelection();
        if (selection.isCollapsed) {
          this.newProperty();
        }
      }, false);

      this.element.addEventListener("mousedown", () => {
        this.doc.defaultView.focus();
      }, false);

      // Create a property editor when the close brace is clicked.
      editableItem({ element: this.closeBrace }, () => {
        this.newProperty();
      });
    }
  },

  /**
   * Event handler called when a property changes on the
   * StyleRuleActor.
   */
  _locationChanged: function() {
    this.updateSourceLink();
  },

  updateSourceLink: function() {
    let sourceLabel = this.element.querySelector(".ruleview-rule-source-label");
    let title = this.rule.title;
    let sourceHref = (this.rule.sheet && this.rule.sheet.href) ?
      this.rule.sheet.href : title;
    let sourceLine = this.rule.ruleLine > 0 ? ":" + this.rule.ruleLine : "";

    sourceLabel.setAttribute("tooltiptext", sourceHref + sourceLine);

    if (this.rule.isSystem) {
      let uaLabel = _strings.GetStringFromName("rule.userAgentStyles");
      sourceLabel.setAttribute("value", uaLabel + " " + title);

      // Special case about:PreferenceStyleSheet, as it is generated on the
      // fly and the URI is not registered with the about: handler.
      // https://bugzilla.mozilla.org/show_bug.cgi?id=935803#c37
      if (sourceHref === "about:PreferenceStyleSheet") {
        sourceLabel.parentNode.setAttribute("unselectable", "true");
        sourceLabel.setAttribute("value", uaLabel);
        sourceLabel.removeAttribute("tooltiptext");
      }
    } else {
      sourceLabel.setAttribute("value", title);
      if (this.rule.ruleLine === -1 && this.rule.domRule.parentStyleSheet) {
        sourceLabel.parentNode.setAttribute("unselectable", "true");
      }
    }

    let showOrig = Services.prefs.getBoolPref(PREF_ORIG_SOURCES);
    if (showOrig && !this.rule.isSystem &&
        this.rule.domRule.type !== ELEMENT_STYLE) {
      this.rule.getOriginalSourceStrings().then((strings) => {
        sourceLabel.setAttribute("value", strings.short);
        sourceLabel.setAttribute("tooltiptext", strings.full);
      }, console.error);
    }
  },

  /**
   * Update the rule editor with the contents of the rule.
   */
  populate: function() {
    // Clear out existing viewers.
    while (this.selectorText.hasChildNodes()) {
      this.selectorText.removeChild(this.selectorText.lastChild);
    }

    // If selector text comes from a css rule, highlight selectors that
    // actually match.  For custom selector text (such as for the 'element'
    // style, just show the text directly.
    if (this.rule.domRule.type === ELEMENT_STYLE) {
      this.selectorText.textContent = this.rule.selectorText;
    } else if (this.rule.domRule.type === Ci.nsIDOMCSSRule.KEYFRAME_RULE) {
      this.selectorText.textContent = this.rule.domRule.keyText;
    } else {
      this.rule.domRule.selectors.forEach((selector, i) => {
        if (i !== 0) {
          createChild(this.selectorText, "span", {
            class: "ruleview-selector-separator",
            textContent: ", "
          });
        }

        let containerClass =
          (this.rule.matchedSelectors.indexOf(selector) > -1) ?
          "ruleview-selector-matched" : "ruleview-selector-unmatched";
        let selectorContainer = createChild(this.selectorText, "span", {
          class: containerClass
        });

        let parsedSelector = parsePseudoClassesAndAttributes(selector);

        for (let selectorText of parsedSelector) {
          let selectorClass = "";

          switch (selectorText.type) {
            case SELECTOR_ATTRIBUTE:
              selectorClass = "ruleview-selector-attribute";
              break;
            case SELECTOR_ELEMENT:
              selectorClass = "ruleview-selector";
              break;
            case SELECTOR_PSEUDO_CLASS:
              selectorClass = [":active", ":focus", ":hover"].some(
                  pseudo => selectorText.value === pseudo) ?
                "ruleview-selector-pseudo-class-lock" :
                "ruleview-selector-pseudo-class";
              break;
            default:
              break;
          }

          createChild(selectorContainer, "span", {
            textContent: selectorText.value,
            class: selectorClass
          });
        }
      });
    }

    for (let prop of this.rule.textProps) {
      if (!prop.editor && !prop.invisible) {
        let editor = new TextPropertyEditor(this, prop);
        this.propertyList.appendChild(editor.element);
      }
    }
  },

  /**
   * Programatically add a new property to the rule.
   *
   * @param {String} name
   *        Property name.
   * @param {String} value
   *        Property value.
   * @param {String} priority
   *        Property priority.
   * @param {TextProperty} siblingProp
   *        Optional, property next to which the new property will be added.
   * @return {TextProperty}
   *        The new property
   */
  addProperty: function(name, value, priority, siblingProp) {
    let prop = this.rule.createProperty(name, value, priority, siblingProp);
    let index = this.rule.textProps.indexOf(prop);
    let editor = new TextPropertyEditor(this, prop);

    // Insert this node before the DOM node that is currently at its new index
    // in the property list.  There is currently one less node in the DOM than
    // in the property list, so this causes it to appear after siblingProp.
    // If there is no node at its index, as is the case where this is the last
    // node being inserted, then this behaves as appendChild.
    this.propertyList.insertBefore(editor.element,
      this.propertyList.children[index]);

    return prop;
  },

  /**
   * Programatically add a list of new properties to the rule.  Focus the UI
   * to the proper location after adding (either focus the value on the
   * last property if it is empty, or create a new property and focus it).
   *
   * @param {Array} properties
   *        Array of properties, which are objects with this signature:
   *        {
   *          name: {string},
   *          value: {string},
   *          priority: {string}
   *        }
   * @param {TextProperty} siblingProp
   *        Optional, the property next to which all new props should be added.
   */
  addProperties: function(properties, siblingProp) {
    if (!properties || !properties.length) {
      return;
    }

    let lastProp = siblingProp;
    for (let p of properties) {
      lastProp = this.addProperty(p.name, p.value, p.priority, lastProp);
    }

    // Either focus on the last value if incomplete, or start a new one.
    if (lastProp && lastProp.value.trim() === "") {
      lastProp.editor.valueSpan.click();
    } else {
      this.newProperty();
    }
  },

  /**
   * Create a text input for a property name.  If a non-empty property
   * name is given, we'll create a real TextProperty and add it to the
   * rule.
   */
  newProperty: function() {
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
      class: PROPERTY_NAME_CLASS,
      tabindex: "0"
    });

    this.multipleAddedProperties = null;

    this.editor = new InplaceEditor({
      element: this.newPropSpan,
      done: this._onNewProperty,
      destroy: this._newPropertyDestroy,
      advanceChars: ":",
      contentType: InplaceEditor.CONTENT_TYPES.CSS_PROPERTY,
      popup: this.ruleView.popup
    });

    // Auto-close the input if multiple rules get pasted into new property.
    this.editor.input.addEventListener("paste",
      blurOnMultipleProperties, false);
  },

  /**
   * Called when the new property input has been dismissed.
   *
   * @param {String} value
   *        The value in the editor.
   * @param {Boolean} commit
   *        True if the value should be committed.
   */
  _onNewProperty: function(value, commit) {
    if (!value || !commit) {
      return;
    }

    // parseDeclarations allows for name-less declarations, but in the present
    // case, we're creating a new declaration, it doesn't make sense to accept
    // these entries
    this.multipleAddedProperties =
      parseDeclarations(value).filter(d => d.name);

    // Blur the editor field now and deal with adding declarations later when
    // the field gets destroyed (see _newPropertyDestroy)
    this.editor.input.blur();
  },

  /**
   * Called when the new property editor is destroyed.
   * This is where the properties (type TextProperty) are actually being
   * added, since we want to wait until after the inplace editor `destroy`
   * event has been fired to keep consistent UI state.
   */
  _newPropertyDestroy: function() {
    // We're done, make the close brace focusable again.
    this.closeBrace.setAttribute("tabindex", "0");

    this.propertyList.removeChild(this.newPropItem);
    delete this.newPropItem;
    delete this.newPropSpan;

    // If properties were added, we want to focus the proper element.
    // If the last new property has no value, focus the value on it.
    // Otherwise, start a new property and focus that field.
    if (this.multipleAddedProperties && this.multipleAddedProperties.length) {
      this.addProperties(this.multipleAddedProperties);
    }
  },

  /**
   * Called when the selector's inplace editor is closed.
   * Ignores the change if the user pressed escape, otherwise
   * commits it.
   *
   * @param {String} value
   *        The value contained in the editor.
   * @param {Boolean} commit
   *        True if the change should be applied.
   * @param {Number} direction
   *        The move focus direction number.
   */
  _onSelectorDone: function(value, commit, direction) {
    if (!commit || this.isEditing || value === "" ||
        value === this.rule.selectorText) {
      return;
    }

    let ruleView = this.ruleView;
    let elementStyle = ruleView._elementStyle;
    let element = elementStyle.element;
    let supportsUnmatchedRules =
      this.rule.domRule.supportsModifySelectorUnmatched;

    this.isEditing = true;

    this.rule.domRule.modifySelector(element, value).then(response => {
      this.isEditing = false;

      if (!supportsUnmatchedRules) {
        if (response) {
          this.ruleView.refreshPanel();
        }
        return;
      }

      let {ruleProps, isMatching} = response;
      if (!ruleProps) {
        // Notify for changes, even when nothing changes,
        // just to allow tests being able to track end of this request.
        ruleView.emit("ruleview-invalid-selector");
        return;
      }

      let newRule = new Rule(elementStyle, ruleProps);
      let editor = new RuleEditor(ruleView, newRule);
      let rules = elementStyle.rules;

      rules.splice(rules.indexOf(this.rule), 1);
      rules.push(newRule);
      elementStyle._changed();
      elementStyle.markOverriddenAll();

      editor.element.setAttribute("unmatched", !isMatching);
      this.element.parentNode.replaceChild(editor.element, this.element);

      // Remove highlight for modified selector
      if (ruleView.highlightedSelector) {
        ruleView.toggleSelectorHighlighter(ruleView.lastSelectorIcon,
          ruleView.highlightedSelector);
      }

      editor._moveSelectorFocus(direction);
    }).then(null, err => {
      this.isEditing = false;
      promiseWarn(err);
    });
  },

  /**
   * Handle moving the focus change after a tab or return keypress in the
   * selector inplace editor.
   *
   * @param {Number} direction
   *        The move focus direction number.
   */
  _moveSelectorFocus: function(direction) {
    if (!direction || direction === Ci.nsIFocusManager.MOVEFOCUS_BACKWARD) {
      return;
    }

    if (this.rule.textProps.length > 0) {
      this.rule.textProps[0].editor.nameSpan.click();
    } else {
      this.propertyList.click();
    }
  }
};

/**
 * Create a TextPropertyEditor.
 *
 * @param {RuleEditor} ruleEditor
 *        The rule editor that owns this TextPropertyEditor.
 * @param {TextProperty} property
 *        The text property to edit.
 */
function TextPropertyEditor(ruleEditor, property) {
  this.ruleEditor = ruleEditor;
  this.ruleView = this.ruleEditor.ruleView;
  this.doc = this.ruleEditor.doc;
  this.popup = this.ruleView.popup;
  this.prop = property;
  this.prop.editor = this;
  this.browserWindow = this.doc.defaultView.top;
  this._populatedComputed = false;

  this._onEnableClicked = this._onEnableClicked.bind(this);
  this._onExpandClicked = this._onExpandClicked.bind(this);
  this._onStartEditing = this._onStartEditing.bind(this);
  this._onNameDone = this._onNameDone.bind(this);
  this._onValueDone = this._onValueDone.bind(this);
  this._onSwatchCommit = this._onSwatchCommit.bind(this);
  this._onSwatchPreview = this._onSwatchPreview.bind(this);
  this._onSwatchRevert = this._onSwatchRevert.bind(this);
  this._onValidate = throttle(this._previewValue, 10, this);
  this.update = this.update.bind(this);

  this._create();
  this.update();
}

TextPropertyEditor.prototype = {
  /**
   * Boolean indicating if the name or value is being currently edited.
   */
  get editing() {
    return !!(this.nameSpan.inplaceEditor || this.valueSpan.inplaceEditor ||
      this.ruleView.tooltips.isEditing) || this.popup.isOpen;
  },

  /**
   * Get the rule to the current text property
   */
  get rule() {
    return this.prop.rule;
  },

  /**
   * Create the property editor's DOM.
   */
  _create: function() {
    this.element = this.doc.createElementNS(HTML_NS, "li");
    this.element.classList.add("ruleview-property");
    this.element._textPropertyEditor = this;

    this.container = createChild(this.element, "div", {
      class: "ruleview-propertycontainer"
    });

    // The enable checkbox will disable or enable the rule.
    this.enable = createChild(this.container, "div", {
      class: "ruleview-enableproperty theme-checkbox",
      tabindex: "-1"
    });

    // Click to expand the computed properties of the text property.
    this.expander = createChild(this.container, "span", {
      class: "ruleview-expander theme-twisty"
    });
    this.expander.addEventListener("click", this._onExpandClicked, true);

    this.nameContainer = createChild(this.container, "span", {
      class: "ruleview-namecontainer"
    });

    // Property name, editable when focused.  Property name
    // is committed when the editor is unfocused.
    this.nameSpan = createChild(this.nameContainer, "span", {
      class: "ruleview-propertyname theme-fg-color5",
      tabindex: this.ruleEditor.isEditable ? "0" : "-1",
    });

    appendText(this.nameContainer, ": ");

    // Create a span that will hold the property and semicolon.
    // Use this span to create a slightly larger click target
    // for the value.
    let propertyContainer = createChild(this.container, "span", {
      class: "ruleview-propertyvaluecontainer"
    });

    // Property value, editable when focused.  Changes to the
    // property value are applied as they are typed, and reverted
    // if the user presses escape.
    this.valueSpan = createChild(propertyContainer, "span", {
      class: "ruleview-propertyvalue theme-fg-color1",
      tabindex: this.ruleEditor.isEditable ? "0" : "-1",
    });

    // Storing the TextProperty on the elements for easy access
    // (for instance by the tooltip)
    this.valueSpan.textProperty = this.prop;
    this.nameSpan.textProperty = this.prop;

    // If the value is a color property we need to put it through the parser
    // so that colors can be coerced into the default color type. This prevents
    // us from thinking that when colors are coerced they have been changed by
    // the user.
    let outputParser = this.ruleView._outputParser;
    let frag = outputParser.parseCssProperty(this.prop.name, this.prop.value);
    let parsedValue = frag.textContent;

    // Save the initial value as the last committed value,
    // for restoring after pressing escape.
    this.committed = { name: this.prop.name,
                       value: parsedValue,
                       priority: this.prop.priority };

    appendText(propertyContainer, ";");

    this.warning = createChild(this.container, "div", {
      class: "ruleview-warning",
      hidden: "",
      title: CssLogic.l10n("rule.warning.title"),
    });

    // Filter button that filters for the current property name and is
    // displayed when the property is overridden by another rule.
    this.filterProperty = createChild(this.container, "div", {
      class: "ruleview-overridden-rule-filter",
      hidden: "",
      title: CssLogic.l10n("rule.filterProperty.title"),
    });

    this.filterProperty.addEventListener("click", event => {
      this.ruleEditor.ruleView.setFilterStyles("`" + this.prop.name + "`");
      event.stopPropagation();
    }, false);

    // Holds the viewers for the computed properties.
    // will be populated in |_updateComputed|.
    this.computed = createChild(this.element, "ul", {
      class: "ruleview-computedlist",
    });

    // Only bind event handlers if the rule is editable.
    if (this.ruleEditor.isEditable) {
      this.enable.addEventListener("click", this._onEnableClicked, true);

      this.nameContainer.addEventListener("click", (event) => {
        // Clicks within the name shouldn't propagate any further.
        event.stopPropagation();
        if (event.target === propertyContainer) {
          this.nameSpan.click();
        }
      }, false);

      editableField({
        start: this._onStartEditing,
        element: this.nameSpan,
        done: this._onNameDone,
        destroy: this.update,
        advanceChars: ":",
        contentType: InplaceEditor.CONTENT_TYPES.CSS_PROPERTY,
        popup: this.popup
      });

      // Auto blur name field on multiple CSS rules get pasted in.
      this.nameContainer.addEventListener("paste",
        blurOnMultipleProperties, false);

      propertyContainer.addEventListener("click", (event) => {
        // Clicks within the value shouldn't propagate any further.
        event.stopPropagation();

        if (event.target === propertyContainer) {
          this.valueSpan.click();
        }
      }, false);

      this.valueSpan.addEventListener("click", (event) => {
        let target = event.target;

        if (target.nodeName === "a") {
          event.stopPropagation();
          event.preventDefault();
          this.browserWindow.openUILinkIn(target.href, "tab");
        }
      }, false);

      editableField({
        start: this._onStartEditing,
        element: this.valueSpan,
        done: this._onValueDone,
        destroy: this.update,
        validate: this._onValidate,
        advanceChars: advanceValidate,
        contentType: InplaceEditor.CONTENT_TYPES.CSS_VALUE,
        property: this.prop,
        popup: this.popup
      });
    }
  },

  /**
   * Get the path from which to resolve requests for this
   * rule's stylesheet.
   *
   * @return {String} the stylesheet's href.
   */
  get sheetHref() {
    let domRule = this.rule.domRule;
    if (domRule) {
      return domRule.href || domRule.nodeHref;
    }
  },

  /**
   * Get the URI from which to resolve relative requests for
   * this rule's stylesheet.
   *
   * @return {nsIURI} A URI based on the the stylesheet's href.
   */
  get sheetURI() {
    if (this._sheetURI === undefined) {
      if (this.sheetHref) {
        this._sheetURI = IOService.newURI(this.sheetHref, null, null);
      } else {
        this._sheetURI = null;
      }
    }

    return this._sheetURI;
  },

  /**
   * Resolve a URI based on the rule stylesheet
   *
   * @param {String} relativePath
   *        the path to resolve
   * @return {String} the resolved path.
   */
  resolveURI: function(relativePath) {
    if (this.sheetURI) {
      relativePath = this.sheetURI.resolve(relativePath);
    }
    return relativePath;
  },

  /**
   * Populate the span based on changes to the TextProperty.
   */
  update: function() {
    if (this.ruleView.isDestroyed) {
      return;
    }

    if (this.prop.enabled) {
      this.enable.style.removeProperty("visibility");
      this.enable.setAttribute("checked", "");
    } else {
      this.enable.style.visibility = "visible";
      this.enable.removeAttribute("checked");
    }

    this.warning.hidden = this.editing || this.isValid();
    this.filterProperty.hidden = this.editing ||
                                 !this.isValid() ||
                                 !this.prop.overridden;

    if (!this.editing &&
        (this.prop.overridden || !this.prop.enabled ||
         !this.prop.isKnownProperty())) {
      this.element.classList.add("ruleview-overridden");
    } else {
      this.element.classList.remove("ruleview-overridden");
    }

    let name = this.prop.name;
    this.nameSpan.textContent = name;

    // Combine the property's value and priority into one string for
    // the value.
    let store = this.rule.elementStyle.store;
    let val = store.userProperties.getProperty(this.rule.style, name,
                                               this.prop.value);
    if (this.prop.priority) {
      val += " !" + this.prop.priority;
    }

    let propDirty = store.userProperties.contains(this.rule.style, name);

    if (propDirty) {
      this.element.setAttribute("dirty", "");
    } else {
      this.element.removeAttribute("dirty");
    }

    const sharedSwatchClass = "ruleview-swatch ";
    const colorSwatchClass = "ruleview-colorswatch";
    const bezierSwatchClass = "ruleview-bezierswatch";
    const filterSwatchClass = "ruleview-filterswatch";

    let outputParser = this.ruleView._outputParser;
    let parserOptions = {
      colorSwatchClass: sharedSwatchClass + colorSwatchClass,
      colorClass: "ruleview-color",
      bezierSwatchClass: sharedSwatchClass + bezierSwatchClass,
      bezierClass: "ruleview-bezier",
      filterSwatchClass: sharedSwatchClass + filterSwatchClass,
      filterClass: "ruleview-filter",
      defaultColorType: !propDirty,
      urlClass: "theme-link",
      baseURI: this.sheetURI
    };
    let frag = outputParser.parseCssProperty(name, val, parserOptions);
    this.valueSpan.innerHTML = "";
    this.valueSpan.appendChild(frag);

    // Attach the color picker tooltip to the color swatches
    this._colorSwatchSpans =
      this.valueSpan.querySelectorAll("." + colorSwatchClass);
    if (this.ruleEditor.isEditable) {
      for (let span of this._colorSwatchSpans) {
        // Adding this swatch to the list of swatches our colorpicker
        // knows about
        this.ruleView.tooltips.colorPicker.addSwatch(span, {
          onShow: this._onStartEditing,
          onPreview: this._onSwatchPreview,
          onCommit: this._onSwatchCommit,
          onRevert: this._onSwatchRevert
        });
      }
    }

    // Attach the cubic-bezier tooltip to the bezier swatches
    this._bezierSwatchSpans =
      this.valueSpan.querySelectorAll("." + bezierSwatchClass);
    if (this.ruleEditor.isEditable) {
      for (let span of this._bezierSwatchSpans) {
        // Adding this swatch to the list of swatches our colorpicker
        // knows about
        this.ruleView.tooltips.cubicBezier.addSwatch(span, {
          onShow: this._onStartEditing,
          onPreview: this._onSwatchPreview,
          onCommit: this._onSwatchCommit,
          onRevert: this._onSwatchRevert
        });
      }
    }

    // Attach the filter editor tooltip to the filter swatch
    let span = this.valueSpan.querySelector("." + filterSwatchClass);
    if (this.ruleEditor.isEditable) {
      if (span) {
        parserOptions.filterSwatch = true;

        this.ruleView.tooltips.filterEditor.addSwatch(span, {
          onShow: this._onStartEditing,
          onPreview: this._onSwatchPreview,
          onCommit: this._onSwatchCommit,
          onRevert: this._onSwatchRevert
        }, outputParser, parserOptions);
      }
    }

    // Populate the computed styles.
    this._updateComputed();

    // Update the rule property highlight.
    this.ruleView._updatePropertyHighlight(this);
  },

  _onStartEditing: function() {
    this.element.classList.remove("ruleview-overridden");
    this.enable.style.visibility = "hidden";
  },

  /**
   * Update the indicator for computed styles. The computed styles themselves
   * are populated on demand, when they become visible.
   */
  _updateComputed: function() {
    this.computed.innerHTML = "";

    let showExpander = this.prop.computed.some(c => c.name !== this.prop.name);
    this.expander.style.visibility = showExpander ? "visible" : "hidden";

    this._populatedComputed = false;
    if (this.expander.hasAttribute("open")) {
      this._populateComputed();
    }
  },

  /**
   * Populate the list of computed styles.
   */
  _populateComputed: function() {
    if (this._populatedComputed) {
      return;
    }
    this._populatedComputed = true;

    for (let computed of this.prop.computed) {
      // Don't bother to duplicate information already
      // shown in the text property.
      if (computed.name === this.prop.name) {
        continue;
      }

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

      let outputParser = this.ruleView._outputParser;
      let frag = outputParser.parseCssProperty(
        computed.name, computed.value, {
          colorSwatchClass: "ruleview-swatch ruleview-colorswatch",
          urlClass: "theme-link",
          baseURI: this.sheetURI
        }
      );

      // Store the computed property value that was parsed for output
      computed.parsedValue = frag.textContent;

      createChild(li, "span", {
        class: "ruleview-propertyvalue theme-fg-color1",
        child: frag
      });

      appendText(li, ";");

      // Store the computed style element for easy access when highlighting
      // styles
      computed.element = li;
    }
  },

  /**
   * Handles clicks on the disabled property.
   */
  _onEnableClicked: function(event) {
    let checked = this.enable.hasAttribute("checked");
    if (checked) {
      this.enable.removeAttribute("checked");
    } else {
      this.enable.setAttribute("checked", "");
    }
    this.prop.setEnabled(!checked);
    event.stopPropagation();
  },

  /**
   * Handles clicks on the computed property expander. If the computed list is
   * open due to user expanding or style filtering, collapse the computed list
   * and close the expander. Otherwise, add user-open attribute which is used to
   * expand the computed list and tracks whether or not the computed list is
   * expanded by manually by the user.
   */
  _onExpandClicked: function(event) {
    if (this.computed.hasAttribute("filter-open") ||
        this.computed.hasAttribute("user-open")) {
      this.expander.removeAttribute("open");
      this.computed.removeAttribute("filter-open");
      this.computed.removeAttribute("user-open");
    } else {
      this.expander.setAttribute("open", "true");
      this.computed.setAttribute("user-open", "");
      this._populateComputed();
    }

    event.stopPropagation();
  },

  /**
   * Expands the computed list when a computed property is matched by the style
   * filtering. The filter-open attribute is used to track whether or not the
   * computed list was toggled opened by the filter.
   */
  expandForFilter: function() {
    if (!this.computed.hasAttribute("user-open")) {
      this.expander.setAttribute("open", "true");
      this.computed.setAttribute("filter-open", "");
      this._populateComputed();
    }
  },

  /**
   * Collapses the computed list that was expanded by style filtering.
   */
  collapseForFilter: function() {
    this.computed.removeAttribute("filter-open");

    if (!this.computed.hasAttribute("user-open")) {
      this.expander.removeAttribute("open");
    }
  },

  /**
   * Called when the property name's inplace editor is closed.
   * Ignores the change if the user pressed escape, otherwise
   * commits it.
   *
   * @param {String} value
   *        The value contained in the editor.
   * @param {Boolean} commit
   *        True if the change should be applied.
   * @param {Number} direction
   *        The move focus direction number.
   */
  _onNameDone: function(value, commit, direction) {
    let isNameUnchanged = (!commit && !this.ruleEditor.isEditing) ||
                          this.committed.name === value;
    if (this.prop.value && isNameUnchanged) {
      return;
    }

    // Remove a property if the name is empty
    if (!value.trim()) {
      this.remove(direction);
      return;
    }

    // Remove a property if the property value is empty and the property
    // value is not about to be focused
    if (!this.prop.value &&
        direction !== Ci.nsIFocusManager.MOVEFOCUS_FORWARD) {
      this.remove(direction);
      return;
    }

    // Adding multiple rules inside of name field overwrites the current
    // property with the first, then adds any more onto the property list.
    let properties = parseDeclarations(value);

    if (properties.length) {
      this.prop.setName(properties[0].name);
      this.committed.name = this.prop.name;

      if (!this.prop.enabled) {
        this.prop.setEnabled(true);
      }

      if (properties.length > 1) {
        this.prop.setValue(properties[0].value, properties[0].priority);
        this.ruleEditor.addProperties(properties.slice(1), this.prop);
      }
    }
  },

  /**
   * Remove property from style and the editors from DOM.
   * Begin editing next or previous available property given the focus
   * direction.
   *
   * @param {Number} direction
   *        The move focus direction number.
   */
  remove: function(direction) {
    if (this._colorSwatchSpans && this._colorSwatchSpans.length) {
      for (let span of this._colorSwatchSpans) {
        this.ruleView.tooltips.colorPicker.removeSwatch(span);
      }
    }

    this.element.parentNode.removeChild(this.element);
    this.ruleEditor.rule.editClosestTextProperty(this.prop, direction);
    this.nameSpan.textProperty = null;
    this.valueSpan.textProperty = null;
    this.prop.remove();
  },

  /**
   * Called when a value editor closes.  If the user pressed escape,
   * revert to the value this property had before editing.
   *
   * @param {String} value
   *        The value contained in the editor.
   * @param {Boolean} commit
   *        True if the change should be applied.
   * @param {Number} direction
   *        The move focus direction number.
   */
  _onValueDone: function(value = "", commit, direction) {
    let parsedProperties = this._getValueAndExtraProperties(value);
    let val = parseSingleValue(parsedProperties.firstValue);
    let isValueUnchanged = (!commit && !this.ruleEditor.isEditing) ||
                           !parsedProperties.propertiesToAdd.length &&
                           this.committed.value === val.value &&
                           this.committed.priority === val.priority;
    // If the value is not empty and unchanged, revert the property back to
    // its original value and enabled or disabled state
    if (value.trim() && isValueUnchanged) {
      this.ruleEditor.rule.previewPropertyValue(this.prop, val.value,
                                                val.priority);
      this.rule.setPropertyEnabled(this.prop, this.prop.enabled);
      return;
    }

    // First, set this property value (common case, only modified a property)
    this.prop.setValue(val.value, val.priority);

    if (!this.prop.enabled) {
      this.prop.setEnabled(true);
    }

    this.committed.value = this.prop.value;
    this.committed.priority = this.prop.priority;

    // If needed, add any new properties after this.prop.
    this.ruleEditor.addProperties(parsedProperties.propertiesToAdd, this.prop);

    // If the input value is empty and the focus is moving forward to the next
    // editable field, then remove the whole property.
    // A timeout is used here to accurately check the state, since the inplace
    // editor `done` and `destroy` events fire before the next editor
    // is focused.
    if (!value.trim() && direction !== Ci.nsIFocusManager.MOVEFOCUS_BACKWARD) {
      setTimeout(() => {
        if (!this.editing) {
          this.remove(direction);
        }
      }, 0);
    }
  },

  /**
   * Called when the swatch editor wants to commit a value change.
   */
  _onSwatchCommit: function() {
    this._onValueDone(this.valueSpan.textContent, true);
    this.update();
  },

  /**
   * Called when the swatch editor wants to preview a value change.
   */
  _onSwatchPreview: function() {
    this._previewValue(this.valueSpan.textContent);
  },

  /**
   * Called when the swatch editor closes from an ESC. Revert to the original
   * value of this property before editing.
   */
  _onSwatchRevert: function() {
    this._previewValue(this.prop.value, true);
    this.update();
  },

  /**
   * Parse a value string and break it into pieces, starting with the
   * first value, and into an array of additional properties (if any).
   *
   * Example: Calling with "red; width: 100px" would return
   * { firstValue: "red", propertiesToAdd: [{ name: "width", value: "100px" }] }
   *
   * @param {String} value
   *        The string to parse
   * @return {Object} An object with the following properties:
   *        firstValue: A string containing a simple value, like
   *                    "red" or "100px!important"
   *        propertiesToAdd: An array with additional properties, following the
   *                         parseDeclarations format of {name,value,priority}
   */
  _getValueAndExtraProperties: function(value) {
    // The inplace editor will prevent manual typing of multiple properties,
    // but we need to deal with the case during a paste event.
    // Adding multiple properties inside of value editor sets value with the
    // first, then adds any more onto the property list (below this property).
    let firstValue = value;
    let propertiesToAdd = [];

    let properties = parseDeclarations(value);

    // Check to see if the input string can be parsed as multiple properties
    if (properties.length) {
      // Get the first property value (if any), and any remaining
      // properties (if any)
      if (!properties[0].name && properties[0].value) {
        firstValue = properties[0].value;
        propertiesToAdd = properties.slice(1);
      } else if (properties[0].name && properties[0].value) {
        // In some cases, the value could be a property:value pair
        // itself.  Join them as one value string and append
        // potentially following properties
        firstValue = properties[0].name + ": " + properties[0].value;
        propertiesToAdd = properties.slice(1);
      }
    }

    return {
      propertiesToAdd: propertiesToAdd,
      firstValue: firstValue
    };
  },

  /**
   * Live preview this property, without committing changes.
   *
   * @param {String} value
   *        The value to set the current property to.
   * @param {Boolean} reverting
   *        True if we're reverting the previously previewed value
   */
  _previewValue: function(value, reverting = false) {
    // Since function call is throttled, we need to make sure we are still
    // editing, and any selector modifications have been completed
    if (!reverting && (!this.editing || this.ruleEditor.isEditing)) {
      return;
    }

    let val = parseSingleValue(value);
    this.ruleEditor.rule.previewPropertyValue(this.prop, val.value,
                                              val.priority);
  },

  /**
   * Validate this property. Does it make sense for this value to be assigned
   * to this property name? This does not apply the property value
   *
   * @return {Boolean} true if the property value is valid, false otherwise.
   */
  isValid: function() {
    return this.prop.isValid();
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
  getProperty: function(style, name, value) {
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
  setProperty: function(style, bame, userValue) {
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
  contains: function(style, name) {
    let key = this.getKey(style, name);
    let entry = this.map.get(key, null);
    return !!entry && name in entry;
  },

  getKey: function(style, name) {
    return style.actorID + ":" + name;
  },

  clear: function() {
    this.map.clear();
  }
};

/**
 * Helper functions
 */

/**
 * Walk up the DOM from a given node until a parent property holder is found.
 * For elements inside the computed property list, the non-computed parent
 * property holder will be returned
 *
 * @param {DOMNode} node
 *        The node to start from
 * @return {DOMNode} The parent property holder node, or null if not found
 */
function getParentTextPropertyHolder(node) {
  while (true) {
    if (!node || !node.classList) {
      return null;
    }
    if (node.classList.contains("ruleview-property")) {
      return node;
    }
    node = node.parentNode;
  }
}

/**
 * For any given node, find the TextProperty it is in if any
 * @param {DOMNode} node
 *        The node to start from
 * @return {TextProperty}
 */
function getParentTextProperty(node) {
  let parent = getParentTextPropertyHolder(node);
  if (!parent) {
    return null;
  }

  let propValue = parent.querySelector(".ruleview-propertyvalue");
  if (!propValue) {
    return null;
  }

  return propValue.textProperty;
}

/**
 * Walker up the DOM from a given node until a parent property holder is found,
 * and return the textContent for the name and value nodes.
 * Stops at the first property found, so if node is inside the computed property
 * list, the computed property will be returned
 *
 * @param {DOMNode} node
 *        The node to start from
 * @return {Object} {name, value}
 */
function getPropertyNameAndValue(node) {
  while (true) {
    if (!node || !node.classList) {
      return null;
    }
    // Check first for ruleview-computed since it's the deepest
    if (node.classList.contains("ruleview-computed") ||
        node.classList.contains("ruleview-property")) {
      return {
        name: node.querySelector(".ruleview-propertyname").textContent,
        value: node.querySelector(".ruleview-propertyvalue").textContent
      };
    }
    node = node.parentNode;
  }
}

XPCOMUtils.defineLazyGetter(this, "clipboardHelper", function() {
  return Cc["@mozilla.org/widget/clipboardhelper;1"]
    .getService(Ci.nsIClipboardHelper);
});

XPCOMUtils.defineLazyGetter(this, "osString", function() {
  return Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
});

XPCOMUtils.defineLazyGetter(this, "_strings", function() {
  return Services.strings.createBundle(
    "chrome://devtools-shared/locale/styleinspector.properties");
});

XPCOMUtils.defineLazyGetter(this, "domUtils", function() {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});

loader.lazyGetter(this, "AutocompletePopup", function() {
  return require("devtools/client/shared/autocomplete-popup").AutocompletePopup;
});

loader.lazyGetter(this, "PSEUDO_ELEMENTS", () => {
  return domUtils.getCSSPseudoElementNames();
});
