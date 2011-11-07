/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla Inspector Module.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp (dcamp@mozilla.com) (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

"use strict"

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const HTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const FOCUS_FORWARD = Ci.nsIFocusManager.MOVEFOCUS_FORWARD;
const FOCUS_BACKWARD = Ci.nsIFocusManager.MOVEFOCUS_BACKWARD;

/**
 * These regular expressions are adapted from firebug's css.js, and are
 * used to parse CSSStyleDeclaration's cssText attribute.
 */

// Used to split on css line separators
const CSS_LINE_RE = /(?:[^;\(]*(?:\([^\)]*?\))?[^;\(]*)*;?/g;

// Used to parse a single property line.
const CSS_PROP_RE = /\s*([^:\s]*)\s*:\s*(.*?)\s*(?:! (important))?;?$/;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/CssLogic.jsm");

var EXPORTED_SYMBOLS = ["CssRuleView",
                        "_ElementStyle",
                        "_editableField"];

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
 * @param Element aElement
 *        The element whose style we are viewing.
 * @param object aStore
 *        The ElementStyle can use this object to store metadata
 *        that might outlast the rule view, particularly the current
 *        set of disabled properties.
 *
 * @constructor
 */
function ElementStyle(aElement, aStore)
{
  this.element = aElement;
  this.store = aStore || {};
  if (this.store.disabled) {
    this.store.disabled = aStore.disabled;
  } else {
    this.store.disabled = WeakMap();
  }

  let doc = aElement.ownerDocument;

  // To figure out how shorthand properties are interpreted by the
  // engine, we will set properties on a dummy element and observe
  // how their .style attribute reflects them as computed values.
  this.dummyElement = doc.createElementNS(this.element.namespaceURI,
                                          this.element.tagName);
  this._populate();
}
// We're exporting _ElementStyle for unit tests.
var _ElementStyle = ElementStyle;

ElementStyle.prototype = {

  // The element we're looking at.
  element: null,

  // Empty, unconnected element of the same type as this node, used
  // to figure out how shorthand properties will be parsed.
  dummyElement: null,

  domUtils: Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils),

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
   */
  _populate: function ElementStyle_populate()
  {
    this.rules = [];

    let element = this.element;
    do {
      this._addElementRules(element);
    } while ((element = element.parentNode) &&
             element.nodeType === Ci.nsIDOMNode.ELEMENT_NODE);

    // Mark overridden computed styles.
    this.markOverridden();
  },

  _addElementRules: function ElementStyle_addElementRules(aElement)
  {
    let inherited = aElement !== this.element ? aElement : null;

    // Include the element's style first.
    this._maybeAddRule({
      style: aElement.style,
      selectorText: CssLogic.l10n("rule.sourceElement"),
      inherited: inherited
    });

    // Get the styles that apply to the element.
    var domRules = this.domUtils.getCSSStyleRules(aElement);

    // getCSStyleRules returns ordered from least-specific to
    // most-specific.
    for (let i = domRules.Count() - 1; i >= 0; i--) {
      let domRule = domRules.GetElementAt(i);

      // XXX: Optionally provide access to system sheets.
      let systemSheet = CssLogic.isSystemStyleSheet(domRule.parentStyleSheet);
      if (systemSheet) {
        continue;
      }

      if (domRule.type !== Ci.nsIDOMCSSRule.STYLE_RULE) {
        continue;
      }

      this._maybeAddRule({
        domRule: domRule,
        inherited: inherited
      });
    }
  },

  /**
   * Add a rule if it's one we care about.  Filters out duplicates and
   * inherited styles with no inherited properties.
   *
   * @param {object} aOptions
   *        Options for creating the Rule, see the Rule constructor.
   *
   * @return true if we added the rule.
   */
  _maybeAddRule: function ElementStyle_maybeAddRule(aOptions)
  {
    // If we've already included this domRule (for example, when a
    // common selector is inherited), ignore it.
    if (aOptions.domRule &&
        this.rules.some(function(rule) rule.domRule === aOptions.domRule)) {
      return false;
    }

    let rule = new Rule(this, aOptions);

    // Ignore inherited rules with no properties.
    if (aOptions.inherited && rule.textProps.length == 0) {
      return false;
    }

    this.rules.push(rule);
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
   * @return True if the TextProperty's overridden state (or any of its
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
}

/**
 * A single style rule or declaration.
 *
 * @param {ElementStyle} aElementStyle
 *        The ElementStyle to which this rule belongs.
 * @param {object} aOptions
 *        The information used to construct this rule.  Properties include:
 *          domRule: the nsIDOMCSSStyleRule to view, if any.
 *          style: the nsIDOMCSSStyleDeclaration to view.  If omitted,
 *            the domRule's style will be used.
 *          selectorText: selector text to display.  If omitted, the domRule's
 *            selectorText will be used.
 *          inherited: An element this rule was inherited from.  If omitted,
 *            the rule applies directly to the current element.
 * @constructor
 */
function Rule(aElementStyle, aOptions)
{
  this.elementStyle = aElementStyle;
  this.domRule = aOptions.domRule || null;
  this.style = aOptions.style || this.domRule.style;
  this.selectorText = aOptions.selectorText || this.domRule.selectorText;
  this.inherited = aOptions.inherited || null;
  this._getTextProperties();
}

Rule.prototype = {
  get title()
  {
    if (this._title) {
      return this._title;
    }
    let sheet = this.domRule ? this.domRule.parentStyleSheet : null;
    this._title = CssLogic.shortSource(sheet);
    if (this.domRule) {
      let line = this.elementStyle.domUtils.getRuleLine(this.domRule);
      this._title += ":" + line;
    }

    if (this.inherited) {
      let eltText = this.inherited.tagName.toLowerCase();
      if (this.inherited.id) {
        eltText += "#" + this.inherited.id;
      }
      let args = [eltText, this._title];
      this._title = CssLogic._strings.formatStringFromName("rule.inheritedSource",
                                                           args, args.length);
    }

    return this._title;
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
   */
  applyProperties: function Rule_applyProperties()
  {
    let disabledProps = [];

    for each (let prop in this.textProps) {
      if (!prop.enabled) {
        disabledProps.push({
          name: prop.name,
          value: prop.value,
          priority: prop.priority
        });
        continue;
      }

      this.style.setProperty(prop.name, prop.value, prop.priority);
      // Refresh the property's value from the style, to reflect
      // any changes made during parsing.
      prop.value = this.style.getPropertyValue(prop.name);
      prop.priority = this.style.getPropertyPriority(prop.name);
      prop.updateComputed();
    }
    this.elementStyle._changed();

    // Store disabled properties in the disabled store.
    let disabled = this.elementStyle.store.disabled;
    disabled.set(this.style, disabledProps);

    this.elementStyle.markOverridden();
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
    this.style.removeProperty(aProperty.name);
    aProperty.name = aName;
    this.applyProperties();
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
    this.applyProperties();
  },

  /**
   * Disables or enables given TextProperty.
   */
  setPropertyEnabled: function Rule_enableProperty(aProperty, aValue)
  {
    aProperty.enabled = !!aValue;
    if (!aProperty.enabled) {
      this.style.removeProperty(aProperty.name);
    }
    this.applyProperties();
  },

  /**
   * Remove a given TextProperty from the rule and update the rule
   * accordingly.
   */
  removeProperty: function Rule_removeProperty(aProperty)
  {
    this.textProps = this.textProps.filter(function(prop) prop != aProperty);
    this.style.removeProperty(aProperty);
    // Need to re-apply properties in case removing this TextProperty
    // exposes another one.
    this.applyProperties();
  },

  /**
   * Get the list of TextProperties from the style.  Needs
   * to parse the style's cssText.
   */
  _getTextProperties: function Rule_getTextProperties()
  {
    this.textProps = [];
    let lines = this.style.cssText.match(CSS_LINE_RE);
    for each (let line in lines) {
      let matches = CSS_PROP_RE.exec(line);
      if(!matches || !matches[2])
        continue;

      let name = matches[1];
      if (this.inherited &&
          !this.elementStyle.domUtils.isInheritedProperty(name)) {
        continue;
      }

      let prop = new TextProperty(this, name, matches[2], matches[3] || "");
      this.textProps.push(prop);
    }

    // Include properties from the disabled property store, if any.
    let disabledProps = this.elementStyle.store.disabled.get(this.style);
    if (!disabledProps) {
      return;
    }

    for each (let prop in disabledProps) {
      let textProp = new TextProperty(this, prop.name,
                                      prop.value, prop.priority);
      textProp.enabled = false;
      this.textProps.push(textProp);
    }
  },
}

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
}


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
 * @param Document aDocument
 *        The document that will contain the rule view.
 * @param object aStore
 *        The CSS rule view can use this object to store metadata
 *        that might outlast the rule view, particularly the current
 *        set of disabled properties.
 * @constructor
 */
function CssRuleView(aDoc, aStore)
{
  this.doc = aDoc;
  this.store = aStore;

  this.element = this.doc.createElementNS(HTML_NS, "div");
  this.element.setAttribute("tabindex", "0");
  this.element.classList.add("ruleview");

  // Give a relative position for the inplace editor's measurement
  // span to be placed absolutely against.
  this.element.style.position = "relative";
}

CssRuleView.prototype = {
  // The element that we're inspecting.
  _viewedElement: null,

  /**
   * Update the highlighted element.
   *
   * @param {nsIDOMElement} aElement
   *        The node whose style rules we'll inspect.
   */
  highlight: function CssRuleView_highlight(aElement)
  {
    if (this._viewedElement === aElement) {
      return;
    }

    this.clear();

    this._viewedElement = aElement;
    if (!this._viewedElement) {
      return;
    }

    if (this._elementStyle) {
      delete this._elementStyle.onChanged;
    }

    this._elementStyle = new ElementStyle(aElement, this.store);
    this._elementStyle.onChanged = function() {
      this._changed();
    }.bind(this);

    this._createEditors();
  },

  /**
   * Clear the rule view.
   */
  clear: function CssRuleView_clear()
  {
    while (this.element.hasChildNodes()) {
      this.element.removeChild(this.element.lastChild);
    }
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
    for each (let rule in this._elementStyle.rules) {
      // Don't hold a reference to this editor beyond the one held
      // by the node.
      let editor = new RuleEditor(this.doc, rule);
      this.element.appendChild(editor.element);
    }
  },
};

/**
 * Create a RuleEditor.
 *
 * @param object aDoc
 *        The document holding this rule editor.
 * @param Rule aRule
 *        The Rule object we're editing.
 * @constructor
 */
function RuleEditor(aDoc, aRule)
{
  this.doc = aDoc;
  this.rule = aRule;

  this._onNewProperty = this._onNewProperty.bind(this);

  this._create();
}

RuleEditor.prototype = {
  _create: function RuleEditor_create()
  {
    this.element = this.doc.createElementNS(HTML_NS, "div");
    this.element._ruleEditor = this;

    // Add the source link.
    let source = createChild(this.element, "div", {
      class: "ruleview-rule-source",
      textContent: this.rule.title
    });

    let code = createChild(this.element, "div", {
      class: "ruleview-code"
    });

    let header = createChild(code, "div", {});

    let selectors = createChild(header, "span", {
      class: "ruleview-selector",
      textContent: this.rule.selectorText
    });
    appendText(header, " {");

    this.propertyList = createChild(code, "ul", {
      class: "ruleview-propertylist"
    });

    for each (let prop in this.rule.textProps) {
      let propEditor = new TextPropertyEditor(this, prop);
      this.propertyList.appendChild(propEditor.element);
    }

    this.closeBrace = createChild(code, "div", {
      class: "ruleview-ruleclose",
      tabindex: "0",
      textContent: "}"
    });

    // We made the close brace focusable, tabbing to it
    // or clicking on it should start the new property editor.
    this.closeBrace.addEventListener("focus", function() {
      this.newProperty();
    }.bind(this), true);
  },

  /**
   * Create a text input for a property name.  If a non-empty property
   * name is given, we'll create a real TextProperty and add it to the
   * rule.
   */
  newProperty: function RuleEditor_newProperty()
  {
    // While we're editing a new property, it doesn't make sense to
    // start a second new property editor, so disable focusing the
    // close brace for now.
    this.closeBrace.removeAttribute("tabindex");

    this.newPropItem = createChild(this.propertyList, "li", {
      class: "ruleview-property ruleview-newproperty",
    });

    this.newPropSpan = createChild(this.newPropItem, "span", {
      class: "ruleview-propertyname"
    });

    new InplaceEditor({
      element: this.newPropSpan,
      done: this._onNewProperty,
      advanceChars: ":"
    });
  },

  _onNewProperty: function RuleEditor_onNewProperty(aValue, aCommit)
  {
    // We're done, make the close brace focusable again.
    this.closeBrace.setAttribute("tabindex", "0");

    this.propertyList.removeChild(this.newPropItem);
    delete this.newPropItem;
    delete this.newPropSpan;

    if (!aValue || !aCommit) {
      return;
    }

    // Create an empty-valued property and start editing it.
    let prop = this.rule.createProperty(aValue, "", "");
    let editor = new TextPropertyEditor(this, prop);
    this.propertyList.appendChild(editor.element);
    editor.valueSpan.focus();
  },
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
  this.doc = aRuleEditor.doc;
  this.prop = aProperty;
  this.prop.editor = this;

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
    this.enable = createChild(this.element, "input", {
      class: "ruleview-enableproperty",
      type: "checkbox",
      tabindex: "-1"
    });
    this.enable.addEventListener("click", this._onEnableClicked, true);

    // Click to expand the computed properties of the text property.
    this.expander = createChild(this.element, "span", {
      class: "ruleview-expander"
    });
    this.expander.addEventListener("click", this._onExpandClicked, true);

    // Property name, editable when focused.  Property name
    // is committed when the editor is unfocused.
    this.nameSpan = createChild(this.element, "span", {
      class: "ruleview-propertyname",
      tabindex: "0",
    });
    editableField({
      start: this._onStartEditing,
      element: this.nameSpan,
      done: this._onNameDone,
      advanceChars: ':'
    });

    appendText(this.element, ": ");

    // Property value, editable when focused.  Changes to the
    // property value are applied as they are typed, and reverted
    // if the user presses escape.
    this.valueSpan = createChild(this.element, "span", {
      class: "ruleview-propertyvalue",
      tabindex: "0",
    });

    editableField({
      start: this._onStartEditing,
      element: this.valueSpan,
      done: this._onValueDone,
      advanceChars: ';'
    });

    // Save the initial value as the last committed value,
    // for restoring after pressing escape.
    this.committed = { name: this.prop.name,
                       value: this.prop.value,
                       priority: this.prop.priority };

    appendText(this.element, ";");

    // Holds the viewers for the computed properties.
    // will be populated in |_updateComputed|.
    this.computed = createChild(this.element, "ul", {
      class: "ruleview-computedlist",
    });
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

    this.nameSpan.textContent = this.prop.name;

    // Combine the property's value and priority into one string for
    // the value.
    let val = this.prop.value;
    if (this.prop.priority) {
      val += " !" + this.prop.priority;
    }
    this.valueSpan.textContent = val;

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
        class: "ruleview-propertyname",
        textContent: computed.name
      });
      appendText(li, ": ");
      createChild(li, "span", {
        class: "ruleview-propertyvalue",
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
  _onEnableClicked: function TextPropertyEditor_onEnableClicked()
  {
    this.prop.setEnabled(this.enable.checked);
  },

  /**
   * Handles clicks on the computed property expander.
   */
  _onExpandClicked: function TextPropertyEditor_onExpandClicked()
  {
    this.expander.classList.toggle("styleinspector-open");
    this.computed.classList.toggle("styleinspector-open");
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
   * @return an object with 'value' and 'priority' properties.
   */
  _parseValue: function TextPropertyEditor_parseValue(aValue)
  {
    let pieces = aValue.split("!", 2);
    let value = pieces[0];
    let priority = pieces.length > 1 ? pieces[1] : "";
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
   * @param {boolean} aCommit
   *        True if the change should be applied.
   */
   _onValueDone: function PropertyEditor_onValueDone(aValue, aCommit)
  {
    if (aCommit) {
      let val = this._parseValue(aValue);
      this.prop.setValue(val.value, val.priority);
      this.committed.value = this.prop.value;
      this.committed.priority = this.prop.priority;
    } else {
      this.prop.setValue(this.committed.value, this.committed.priority);
    }
  },
};

/**
 * Mark a span editable.  |editableField| will listen for the span to
 * be focused and create an InlineEditor to handle text input.
 * Changes will be committed when the InlineEditor's input is blurred
 * or dropped when the user presses escape.
 *
 * @param {object} aOptions
 *    Options for the editable field, including:
 *    {Element} element:
 *      (required) The span to be edited on focus.
 *    {function} start:
 *       Will be called when the inplace editor is initialized.
 *    {function} change:
 *       Will be called when the text input changes.  Will be called
 *       with the current value of the text input.
 *    {function} done:
 *       Called when input is committed or blurred.  Called with
 *       current value and a boolean telling the caller whether to
 *       commit the change.
 *    {string} advanceChars:
 *       If any characters in advanceChars are typed, focus will advance
 *       to the next element.
 */
function editableField(aOptions)
{
  aOptions.element.addEventListener("focus", function() {
    new InplaceEditor(aOptions);
  }, false);
}
var _editableField = editableField;

function InplaceEditor(aOptions)
{
  this.elt = aOptions.element;
  this.elt.inplaceEditor = this;

  this.change = aOptions.change;
  this.done = aOptions.done;
  this.initial = aOptions.initial ? aOptions.initial : this.elt.textContent;
  this.doc = this.elt.ownerDocument;

  this._onBlur = this._onBlur.bind(this);
  this._onKeyPress = this._onKeyPress.bind(this);
  this._onInput = this._onInput.bind(this);

  this._createInput();
  this._autosize();

  // Pull out character codes for advanceChars, listing the
  // characters that should trigger a blur.
  this._advanceCharCodes = {};
  let advanceChars = aOptions.advanceChars || '';
  for (let i = 0; i < advanceChars.length; i++) {
    this._advanceCharCodes[advanceChars.charCodeAt(i)] = true;
  }

  // Hide the provided element and add our editor.
  this.originalDisplay = this.elt.style.display;
  this.elt.style.display = "none";
  this.elt.parentNode.insertBefore(this.input, this.elt);

  this.input.select();
  this.input.focus();

  this.input.addEventListener("blur", this._onBlur, false);
  this.input.addEventListener("keypress", this._onKeyPress, false);
  this.input.addEventListener("input", this._onInput, false);

  if (aOptions.start) {
    aOptions.start();
  }
}

InplaceEditor.prototype = {
  _createInput: function InplaceEditor_createEditor()
  {
    this.input = this.doc.createElementNS(HTML_NS, "input");
    this.input.inplaceEditor = this;
    this.input.classList.add("styleinspector-propertyeditor");
    this.input.value = this.initial;

    copyTextStyles(this.elt, this.input);
  },

  /**
   * Get rid of the editor.
   */
  _clear: function InplaceEditor_clear()
  {
    this.input.removeEventListener("blur", this._onBlur, false);
    this.input.removeEventListener("keypress", this._onKeyPress, false);
    this.input.removeEventListener("oninput", this._onInput, false);
    this._stopAutosize();

    this.elt.parentNode.removeChild(this.input);
    this.elt.style.display = this.originalDisplay;
    this.input = null;

    delete this.elt.inplaceEditor;
    delete this.elt;
  },

  /**
   * Keeps the editor close to the size of its input string.  This is pretty
   * crappy, suggestions for improvement welcome.
   */
  _autosize: function InplaceEditor_autosize()
  {
    // Create a hidden, absolutely-positioned span to measure the text
    // in the input.  Boo.

    // We can't just measure the original element because a) we don't
    // change the underlying element's text ourselves (we leave that
    // up to the client), and b) without tweaking the style of the
    // original element, it might wrap differently or something.
    this._measurement = this.doc.createElementNS(HTML_NS, "span");
    this.elt.parentNode.appendChild(this._measurement);
    let style = this._measurement.style;
    style.visibility = "hidden";
    style.position = "absolute";
    style.top = "0";
    style.left = "0";
    copyTextStyles(this.input, this._measurement);
    this._updateSize();
  },

  /**
   * Clean up the mess created by _autosize().
   */
  _stopAutosize: function InplaceEditor_stopAutosize()
  {
    if (!this._measurement) {
      return;
    }
    this._measurement.parentNode.removeChild(this._measurement);
    delete this._measurement;
  },

  /**
   * Size the editor to fit its current contents.
   */
  _updateSize: function InplaceEditor_updateSize()
  {
    // Replace spaces with non-breaking spaces.  Otherwise setting
    // the span's textContent will collapse spaces and the measurement
    // will be wrong.
    this._measurement.textContent = this.input.value.replace(' ', '\u00a0', 'g');

    // We add a bit of padding to the end.  Should be enough to fit
    // any letter that could be typed, otherwise we'll scroll before
    // we get a chance to resize.  Yuck.
    let width = this._measurement.offsetWidth + 10;

    this.input.style.width = width + "px";
  },

  /**
   * Handle loss of focus by calling the client's done handler and
   * clearing out.
   */
  _onBlur: function InplaceEditor_onBlur(aEvent)
  {
    if (this.done) {
      this.done(this.cancelled ? this.initial : this.input.value.trim(),
                !this.cancelled);
    }
    this._clear();
  },

  _onKeyPress: function InplaceEditor_onKeyPress(aEvent)
  {
    let prevent = false;
    if (aEvent.charCode in this._advanceCharCodes
       || aEvent.keyCode === Ci.nsIDOMKeyEvent.DOM_VK_RETURN) {
      // Focus the next element, triggering a blur which
      // will eventually shut us down (making return roughly equal
      // tab).
      prevent = true;
      moveFocus(this.input.ownerDocument.defaultView, FOCUS_FORWARD);
    } else if (aEvent.keyCode === Ci.nsIDOMKeyEvent.DOM_VK_ESCAPE) {
      // Cancel and blur ourselves.  |_onBlur| will call the user's
      // done handler for us.
      prevent = true;
      this.cancelled = true;
      this.input.blur();
    } else if (aEvent.keyCode === Ci.nsIDOMKeyEvent.DOM_VK_SPACE) {
      // No need for leading spaces here.  This is particularly
      // noticable when adding a property: it's very natural to type
      // <name>: (which advances to the next property) then spacebar.
      prevent = !this.input.value;
    }

    if (prevent) {
      aEvent.preventDefault();
    }
  },

  /**
   * Handle changes the input text.
   */
  _onInput: function InplaceEditor_onInput(aEvent)
  {
    // Update size if we're autosizing.
    if (this._measurement) {
      this._updateSize();
    }

    // Call the user's change handler if available.
    if (this.change) {
      this.change(this.input.value.trim());
    }
  }
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

/**
 * Append a text node to an element.
 */
function appendText(aParent, aText)
{
  aParent.appendChild(aParent.ownerDocument.createTextNode(aText));
}

/**
 * Copy text-related styles from one element to another.
 */
function copyTextStyles(aFrom, aTo)
{
  let win = aFrom.ownerDocument.defaultView;
  let style = win.getComputedStyle(aFrom);
  aTo.style.fontFamily = style.getPropertyCSSValue("font-family").cssText;
  aTo.style.fontSize = style.getPropertyCSSValue("font-size").cssText;
  aTo.style.fontWeight = style.getPropertyCSSValue("font-weight").cssText;
  aTo.style.fontStyle = style.getPropertyCSSValue("font-style").cssText;
}

/**
 * Trigger a focus change similar to pressing tab/shift-tab.
 */
function moveFocus(aWin, aDirection)
{
  let fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);
  fm.moveFocus(aWin, null, aDirection, 0);
}
