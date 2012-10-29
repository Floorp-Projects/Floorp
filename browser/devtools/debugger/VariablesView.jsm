/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const LAZY_EMPTY_DELAY = 150; // ms

Components.utils.import('resource://gre/modules/Services.jsm');

let EXPORTED_SYMBOLS = ["VariablesView", "create"];

/**
 * A tree view for inspecting scopes, objects and properties.
 * Iterable via "for (let [id, scope] in instance) { }".
 * Requires the devtools common.css and debugger.css skin stylesheets.
 *
 * To allow replacing variable or property values in this view, provide an
 * "eval" function property.
 *
 * @param nsIDOMNode aParentNode
 *        The parent node to hold this view.
 */
function VariablesView(aParentNode) {
  this._store = new Map();
  this._prevHierarchy = new Map();
  this._currHierarchy = new Map();
  this._parent = aParentNode;
  this._appendEmptyNotice();

  // Create an internal list container.
  this._list = this.document.createElement("vbox");
  this._parent.appendChild(this._list);
}

VariablesView.prototype = {
  /**
   * Adds a scope to contain any inspected variables.
   *
   * @param string aName
   *        The scope's name (e.g. "Local", "Global" etc.).
   * @return Scope
   *         The newly created Scope instance.
   */
  addScope: function VV_addScope(aName) {
    this._removeEmptyNotice();

    let scope = new Scope(this, aName);
    this._store.set(scope.id, scope);
    this._currHierarchy.set(aName, scope);
    return scope;
  },

  /**
   * Removes all items from this container.
   *
   * @param number aTimeout [optional]
   *        The number of milliseconds to delay the operation if
   *        lazy emptying of this container is enabled.
   */
  empty: function VV_empty(aTimeout = LAZY_EMPTY_DELAY) {
    // If there are no items in this container, emptying is useless.
    if (!this._store.size()) {
      return;
    }
    // Check if this empty operation may be executed lazily.
    if (this.lazyEmpty && aTimeout > 0) {
      this._emptySoon(aTimeout);
      return;
    }

    let list = this._list;
    let firstChild;

    while (firstChild = list.firstChild) {
      list.removeChild(firstChild);
    }

    this._store = new Map();
    this._appendEmptyNotice();
  },

  /**
   * Emptying this container and rebuilding it immediately afterwards would
   * result in a brief redraw flicker, because the previously expanded nodes
   * may get asynchronously re-expanded, after fetching the prototype and
   * properties from a server.
   *
   * To avoid such behaviour, a normal container list is rebuild, but not
   * immediately attached to the parent container. The old container list
   * is kept around for a short period of time, hopefully accounting for the
   * data fetching delay. In the meantime, any operations can be executed
   * normally.
   *
   * @see VariablesView.empty
   * @see VariablesView.commitHierarchy
   */
  _emptySoon: function VV__emptySoon(aTimeout) {
    let window = this.window;
    let document = this.document;

    let prevList = this._list;
    let currList = this._list = this.document.createElement("vbox");
    this._store = new Map();

    this._emptyTimeout = window.setTimeout(function() {
      this._emptyTimeout = null;

      this._parent.removeChild(prevList);
      this._parent.appendChild(currList);

      if (!this._store.size()) {
        this._appendEmptyNotice();
      }
    }.bind(this), aTimeout);
  },

  /**
   * Specifies if enumerable properties and variables should be displayed.
   * @param boolean aFlag
   */
  set enumVisible(aFlag) {
    this._enumVisible = aFlag;

    for (let [_, scope] in this) {
      scope._nonEnumVisible = aFlag;
    }
  },

  /**
   * Specifies if non-enumerable properties and variables should be displayed.
   * @param boolean aFlag
   */
  set nonEnumVisible(aFlag) {
    this._nonEnumVisible = aFlag;

    for (let [_, scope] in this) {
      scope._nonEnumVisible = aFlag;
    }
  },

  /**
   * Sets the text displayed in this container when there are no available items.
   * @param string aValue
   */
  set emptyText(aValue) {
    if (this._emptyTextNode) {
      this._emptyTextNode.setAttribute("value", aValue);
    }
    this._emptyTextValue = aValue;
  },

  /**
   * Creates and appends a label signaling that this container is empty.
   */
  _appendEmptyNotice: function VV__appendEmptyNotice() {
    if (this._emptyTextNode) {
      return;
    }

    let label = this.document.createElement("label");
    label.className = "empty list-item";
    label.setAttribute("value", this._emptyTextValue);

    this._parent.appendChild(label);
    this._emptyTextNode = label;
  },

  /**
   * Removes the label signaling that this container is empty.
   */
  _removeEmptyNotice: function VV__removeEmptyNotice() {
    if (!this._emptyTextNode) {
      return;
    }

    this._parent.removeChild(this._emptyTextNode);
    this._emptyTextNode = null;
  },

  /**
   * Gets the parent node holding this view.
   * @return nsIDOMNode
   */
  get parentNode() this._parent,

  /**
   * Gets the owner document holding this view.
   * @return nsIHTMLDocument
   */
  get document() this._parent.ownerDocument,

  /**
   * Gets the default window holding this view.
   * @return nsIDOMWindow
   */
  get window() this.document.defaultView,

  eval: null,
  lazyEmpty: false,
  _store: null,
  _prevHierarchy: null,
  _currHierarchy: null,
  _emptyTimeout: null,
  _enumVisible: true,
  _nonEnumVisible: true,
  _list: null,
  _parent: null,
  _emptyTextNode: null,
  _emptyTextValue: ""
};

/**
 * A Scope is an object holding Variable instances.
 * Iterable via "for (let [name, variable] in instance) { }".
 *
 * @param VariablesView aView
 *        The view to contain this scope.
 * @param string aName
 *        The scope's name.
 * @param object aFlags [optional]
 *        Additional options or flags for this scope.
 */
function Scope(aView, aName, aFlags = {}) {
  this.show = this.show.bind(this);
  this.hide = this.hide.bind(this);
  this.expand = this.expand.bind(this);
  this.collapse = this.collapse.bind(this);
  this.toggle = this.toggle.bind(this);

  this.ownerView = aView;
  this.eval = aView.eval;

  this._store = new Map();
  this._init(aName.trim(), aFlags);
}

Scope.prototype = {
  /**
   * Adds a variable to contain any inspected properties.
   *
   * @param string aName
   *        The variable's name.
   * @param object aDescriptor
   *        Specifies the value and/or type & class of the variable,
   *        or 'get' & 'set' accessor properties. If the type is implicit,
   *        it will be inferred from the value.
   *        e.g. - { value: 42 }
   *             - { value: true }
   *             - { value: "nasu" }
   *             - { value: { type: "undefined" } }
   *             - { value: { type: "null" } }
   *             - { value: { type: "object", class: "Object" } }
   *             - { get: { type: "object", class: "Function" },
   *                 set: { type: "undefined" } }
   * @return Variable
   *         The newly created Variable instance, null if it already exists.
   */
  addVar: function S_addVar(aName, aDescriptor = {}) {
    if (this._store.has(aName)) {
      return null;
    }

    let variable = new Variable(this, aName, aDescriptor);
    this._store.set(aName, variable);
    this._variablesView._currHierarchy.set(variable._absoluteName, variable);
    return variable;
  },

  /**
   * Gets the variable in this container having the specified name.
   *
   * @return Variable
   *         The matched variable, or null if nothing is found.
   */
  get: function S_get(aName) {
    return this._store.get(aName);
  },

  /**
   * Shows the scope.
   */
  show: function S_show() {
    this._target.hidden = false;
    this._isShown = true;

    if (this.onshow) {
      this.onshow(this);
    }
  },

  /**
   * Hides the scope.
   */
  hide: function S_hide() {
    this._target.hidden = true;
    this._isShown = false;

    if (this.onhide) {
      this.onhide(this);
    }
  },

  /**
   * Expands the scope, showing all the added details.
   *
   * @param boolean aSkipAnimationFlag
   *        Pass true to not show an opening animation.
   */
  expand: function S_expand(aSkipAnimationFlag) {
    if (this._locked) {
      return;
    }
    if (this._variablesView._enumVisible) {
      this._arrow.setAttribute("open", "");
      this._enum.setAttribute("open", "");
    }
    if (this._variablesView._nonEnumVisible) {
      this._nonenum.setAttribute("open", "");
    }
    if (!aSkipAnimationFlag) {
      this._enum.setAttribute("animated", "");
      this._nonenum.setAttribute("animated", "");
    }
    this._isExpanded = true;

    if (this.onexpand) {
      this.onexpand(this);
    }
  },

  /**
   * Collapses the scope, hiding all the added details.
   */
  collapse: function S_collapse() {
    if (this._locked) {
      return;
    }
    this._arrow.removeAttribute("open");
    this._enum.removeAttribute("open");
    this._nonenum.removeAttribute("open");
    this._enum.removeAttribute("animated");
    this._nonenum.removeAttribute("animated");
    this._isExpanded = false;

    if (this.oncollapse) {
      this.oncollapse(this);
    }
  },

  /**
   * Toggles between the scope's collapsed and expanded state.
   */
  toggle: function S_toggle() {
    this.expanded ^= 1;

    if (this.ontoggle) {
      this.ontoggle(this);
    }
  },

  /**
   * Shows the scope's expand/collapse arrow.
   */
  showArrow: function S_showArrow() {
    this._arrow.removeAttribute("invisible");
    this._isArrowVisible = true;
  },

  /**
   * Hides the scope's expand/collapse arrow.
   */
  hideArrow: function S_hideArrow() {
    this._arrow.setAttribute("invisible", "");
    this._isArrowVisible = false;
  },

  /**
   * Gets the visibility state.
   * @return boolean
   */
  get visible() this._isShown,

  /**
   * Gets the expanded state.
   * @return boolean
   */
  get expanded() this._isExpanded,

  /**
   * Gets the twisty visibility state.
   * @return boolean
   */
  get twisty() this._isArrowVisible,

  /**
   * Sets the visibility state.
   * @param boolean aFlag
   */
  set visible(aFlag) aFlag ? this.show() : this.hide(),

  /**
   * Sets the expanded state.
   * @param boolean aFlag
   */
  set expanded(aFlag) aFlag ? this.expand() : this.collapse(),

  /**
   * Sets the twisty visibility state.
   * @param boolean aFlag
   */
  set twisty(aFlag) aFlag ? this.showArrow() : this.hideArrow(),

  /**
   * Gets the id associated with this item.
   * @return string
   */
  get id() this._idString,

  /**
   * Gets the name associated with this item.
   * @return string
   */
  get name() this._nameString,

  /**
   * Gets the element associated with this item.
   * @return nsIDOMNode
   */
  get target() this._target,

  /**
   * Initializes this scope's id, view and binds event listeners.
   *
   * @param string aName
   *        The scope's name.
   * @param object aFlags [optional]
   *        Additional options or flags for this scope.
   * @param string aClassName [optional]
   *        A custom class name for this scope.
   */
  _init: function S__init(aName, aFlags = {}, aClassName = "scope") {
    this._idString = generateId(this._nameString = aName);
    this._createScope(aName, aClassName);
    this._addEventListeners();
    this.parentNode.appendChild(this._target);
  },

  /**
   * Creates the necessary nodes for this scope.
   *
   * @param string aName
   *        The scope's name.
   * @param string aClassName
   *        A custom class name for this scope.
   */
  _createScope: function S__createScope(aName, aClassName) {
    let document = this.document;

    let element = this._target = document.createElement("vbox");
    element.id = this._id;
    element.className = aClassName;

    let arrow = this._arrow = document.createElement("hbox");
    arrow.className = "arrow";

    let name = this._name = document.createElement("label");
    name.className = "name plain";
    name.setAttribute("value", aName);

    let title = this._title = document.createElement("hbox");
    title.className = "title" + (aClassName == "scope" ? " devtools-toolbar" : "");
    title.setAttribute("align", "center");

    let enumerable = this._enum = document.createElement("vbox");
    let nonenum = this._nonenum = document.createElement("vbox");
    enumerable.className = "details";
    nonenum.className = "details nonenum";

    title.appendChild(arrow);
    title.appendChild(name);

    element.appendChild(title);
    element.appendChild(enumerable);
    element.appendChild(nonenum);
  },

  /**
   * Adds the necessary event listeners for this scope.
   */
  _addEventListeners: function S__addEventListeners() {
    this._title.addEventListener("mousedown", this.toggle, false);
  },

  /**
   * Specifies if enumerable properties and variables should be displayed.
   * @param boolean aFlag
   */
  set _enumVisible(aFlag) {
    for (let [_, variable] in this) {
      variable._enumVisible = aFlag;

      if (!this.expanded) {
        continue;
      }
      if (aFlag) {
        this._enum.setAttribute("open", "");
      } else {
        this._enum.removeAttribute("open");
      }
    }
  },

  /**
   * Specifies if non-enumerable properties and variables should be displayed.
   * @param boolean aFlag
   */
  set _nonEnumVisible(aFlag) {
    for (let [_, variable] in this) {
      variable._nonEnumVisible = aFlag;

      if (!this.expanded) {
        continue;
      }
      if (aFlag) {
        this._nonenum.setAttribute("open", "");
      } else {
        this._nonenum.removeAttribute("open");
      }
    }
  },

  /**
   * Gets top level variables view instance.
   * @return VariablesView
   */
  get _variablesView() {
    let parentView = this.ownerView;
    let topView;

    while (topView = parentView.ownerView) {
      parentView = topView;
    }
    return parentView;
  },

  /**
   * Gets the parent node holding this scope.
   * @return nsIDOMNode
   */
  get parentNode() this.ownerView._list,

  /**
   * Gets the owner document holding this scope.
   * @return nsIHTMLDocument
   */
  get document() this.ownerView.document,

  /**
   * Gets the default window holding this scope.
   * @return nsIDOMWindow
   */
  get window() this.ownerView.window,

  ownerView: null,
  eval: null,
  _committed: false,
  _locked: false,
  _isShown: true,
  _isExpanded: false,
  _isArrowVisible: true,
  _store: null,
  _idString: null,
  _nameString: null,
  _target: null,
  _arrow: null,
  _name: null,
  _title: null,
  _enum: null,
  _nonenum: null
};

/**
 * A Variable is a Scope holding Property instances.
 * Iterable via "for (let [name, property] in instance) { }".
 *
 * @param Scope aScope
 *        The scope to contain this varialbe.
 * @param string aName
 *        The variable's name.
 * @param object aDescriptor
 *        The variable's descriptor.
 */
function Variable(aScope, aName, aDescriptor) {
  this._activateInput = this._activateInput.bind(this);
  this._deactivateInput = this._deactivateInput.bind(this);
  this._saveInput = this._saveInput.bind(this);
  this._onInputKeyPress = this._onInputKeyPress.bind(this);

  Scope.call(this, aScope, aName, aDescriptor);
  this._setGrip(aDescriptor.value);
  this._symbolicName = aName;
  this._absoluteName = aScope.name + "." + aName;
  this._initialDescriptor = aDescriptor;
}

create({ constructor: Variable, proto: Scope.prototype }, {
  /**
   * Adds a property for this variable.
   *
   * @param string aName
   *        The property's name.
   * @param object aDescriptor
   *        Specifies the value and/or type & class of the property,
   *        or 'get' & 'set' accessor properties. If the type is implicit,
   *        it will be inferred from the value.
   *        e.g. - { value: 42 }
   *             - { value: true }
   *             - { value: "nasu" }
   *             - { value: { type: "undefined" } }
   *             - { value: { type: "null" } }
   *             - { value: { type: "object", class: "Object" } }
   *             - { get: { type: "object", class: "Function" },
   *                 set: { type: "undefined" } }
   * @return Property
   *         The newly created Property instance, null if it already exists.
   */
  addProperty: function V_addProperty(aName, aDescriptor = {}) {
    if (this._store.has(aName)) {
      return null;
    }

    let property = new Property(this, aName, aDescriptor);
    this._store.set(aName, property);
    this._variablesView._currHierarchy.set(property._absoluteName, property);
    return property;
  },

  /**
   * Adds properties for this variable.
   *
   * @param object aProperties
   *        An object containing some { name: descriptor } data properties,
   *        specifying the value and/or type & class of the variable,
   *        or 'get' & 'set' accessor properties. If the type is implicit,
   *        it will be inferred from the value.
   *        e.g. - { someProp0: { value: 42 },
   *                 someProp1: { value: true },
   *                 someProp2: { value: "nasu" },
   *                 someProp3: { value: { type: "undefined" } },
   *                 someProp4: { value: { type: "null" } },
   *                 someProp5: { valu§e: { type: "object", class: "Object" } },
   *                 someProp6: { get: { type: "object", class: "Function" },
   *                              set: { type: "undefined" } }
   */
  addProperties: function V_addProperties(aProperties) {
    // Sort all of the properties before adding them.
    let sortedPropertyNames = Object.keys(aProperties).sort();

    for (let name of sortedPropertyNames) {
      this.addProperty(name, aProperties[name]);
    }
  },

  /**
   * Sets the specific grip for this variable.
   * The grip should contain the value or the type & class, as defined in the
   * remote debugger protocol. For convenience, undefined and null are
   * both considered types.
   *
   * @param any aGrip
   *        Specifies the value and/or type & class of the variable.
   *        e.g. - 42
   *             - true
   *             - "nasu"
   *             - { type: "undefined" }
   *             - { type: "null" }
   *             - { type: "object", class: "Object" }
   */
  _setGrip: function V__setGrip(aGrip) {
    if (aGrip === undefined) {
      aGrip = { type: "undefined" };
    }
    if (aGrip === null) {
      aGrip = { type: "null" };
    }
    this._applyGrip(aGrip);
  },

  /**
   * Applies the necessary text content and class name to a value node based
   * on a grip.
   *
   * @param any aGrip
   *        @see Variable._setGrip
   */
  _applyGrip: function V__applyGrip(aGrip) {
    let prevGrip = this._valueGrip;
    if (prevGrip) {
      this._valueLabel.classList.remove(VariablesView.getClass(prevGrip));
    }
    this._valueGrip = aGrip;
    this._valueString = VariablesView.getString(aGrip);
    this._valueClassName = VariablesView.getClass(aGrip);

    this._valueLabel.classList.add(this._valueClassName);
    this._valueLabel.setAttribute("value", this._valueString);
  },

  /**
   * Initializes this variable's id, view and binds event listeners.
   *
   * @param string aName
   *        The variable's name.
   * @param object aDescriptor
   *        The variable's descriptor.
   */
  _init: function V__init(aName, aDescriptor) {
    this._idString = generateId(this._nameString = aName);
    this._createScope(aName, "variable");
    this._displayVariable(aDescriptor);
    this._displayTooltip();
    this._setAttributes(aName, aDescriptor);
    this._addEventListeners();

    if (aDescriptor.enumerable || aName == "this" || aName == "<exception>") {
      this.ownerView._enum.appendChild(this._target);
    } else {
      this.ownerView._nonenum.appendChild(this._target);
    }
  },

  /**
   * Creates the necessary nodes for this variable.
   *
   * @param object aDescriptor
   *        The property's descriptor.
   */
  _displayVariable: function V__displayVariable(aDescriptor) {
    let document = this.document;

    let separatorLabel = this._separatorLabel = document.createElement("label");
    separatorLabel.className = "plain";
    separatorLabel.setAttribute("value", ":");

    let valueLabel = this._valueLabel = document.createElement("label");
    valueLabel.className = "value plain";

    this._title.appendChild(separatorLabel);
    this._title.appendChild(valueLabel);

    if (VariablesView.isPrimitive(aDescriptor)) {
      this.hideArrow();
    }
    if (aDescriptor.get || aDescriptor.set) {
      this.addProperty("get ", { value: aDescriptor.get });
      this.addProperty("set ", { value: aDescriptor.set });
      this.expand(true);
      separatorLabel.hidden = true;
      valueLabel.hidden = true;
    }
  },

  /**
   * Creates a tooltip for this variable.
   */
  _displayTooltip: function V__displayTooltip() {
    let document = this.document;

    let tooltip = document.createElement("tooltip");
    tooltip.id = "tooltip-" + this.id;

    let configurableLabel = document.createElement("label");
    configurableLabel.setAttribute("value", "configurable");

    let enumerableLabel = document.createElement("label");
    enumerableLabel.setAttribute("value", "enumerable");

    let writableLabel = document.createElement("label");
    writableLabel.setAttribute("value", "writable");

    tooltip.setAttribute("orient", "horizontal")
    tooltip.appendChild(configurableLabel);
    tooltip.appendChild(enumerableLabel);
    tooltip.appendChild(writableLabel);

    this._target.appendChild(tooltip);
    this._target.setAttribute("tooltip", tooltip.id);
  },

  /**
   * Sets a variable's configurable, enumerable and writable attributes,
   * and specifies if it's a 'this', '<exception>' or '__proto__' reference.
   *
   * @param object aName
   *        The varialbe name.
   * @param object aDescriptor
   *        The variable's descriptor.
   */
  _setAttributes: function V__setAttributes(aName, aDescriptor) {
    if (aDescriptor) {
      if (!aDescriptor.configurable) {
        this._target.setAttribute("non-configurable", "");
      }
      if (!aDescriptor.enumerable) {
        this._target.setAttribute("non-enumerable", "");
      }
      if (!aDescriptor.writable) {
        this._target.setAttribute("non-writable", "");
      }
    }
    if (aName == "this") {
      this._target.setAttribute("self", "");
    }
    if (aName == "<exception>") {
      this._target.setAttribute("exception", "");
    }
    if (aName == "__proto__") {
      this._target.setAttribute("proto", "");
    }
  },

  /**
   * Adds the necessary event listeners for this variable.
   */
  _addEventListeners: function V__addEventListeners() {
    this._arrow.addEventListener("mousedown", this.toggle, false);
    this._name.addEventListener("mousedown", this.toggle, false);
    this._valueLabel.addEventListener("click", this._activateInput, false);
  },

  /**
   * Makes this variable's value editable.
   */
  _activateInput: function V__activateInput(e) {
    if (!this.eval) {
      return;
    }

    let title = this._title;
    let valueLabel = this._valueLabel;
    let initialString = this._valueLabel.getAttribute("value");

    // Create a texbox input element which will be shown in the current
    // element's value location.
    let input = this.document.createElement("textbox");
    input.setAttribute("value", initialString);
    input.className = "element-input";
    input.width = valueLabel.clientWidth + 1;

    title.removeChild(valueLabel);
    title.appendChild(input);
    input.select();

    // When the value is a string (displayed as "value"), then we probably want
    // to change it to another string in the textbox, so to avoid typing the ""
    // again, tackle with the selection bounds just a bit.
    if (valueLabel.getAttribute("value").match(/^"[^"]*"$/)) {
      input.selectionEnd--;
      input.selectionStart++;
    }

    input.addEventListener("keypress", this._onInputKeyPress, false);
    input.addEventListener("blur", this._deactivateInput, false);

    this._prevExpandable = this.twisty;
    this._prevExpanded = this.expanded;
    this.collapse();
    this.hideArrow();
    this._locked = true;
  },

  /**
   * Deactivates this variable's editable mode.
   */
  _deactivateInput: function V__deactivateInput(e) {
    let input = e.target;
    let title = this._title;
    let valueLabel = this._valueLabel;

    title.removeChild(input);
    title.appendChild(valueLabel);

    input.removeEventListener("keypress", this._onInputKeyPress, false);
    input.removeEventListener("blur", this._deactivateInput, false);

    this._locked = false;
    this.twisty = this._prevExpandable;
    this.expanded = this._prevExpanded;
  },

  /**
   * Deactivates this variable's editable mode and evaluates a new value.
   */
  _saveInput: function V__saveInput(e) {
    let input = e.target;
    let valueLabel = this._valueLabel;
    let initialString = this._valueLabel.getAttribute("value");
    let currentString = input.value;

    this._deactivateInput(e);

    if (initialString != currentString) {
      this._separatorLabel.hidden = true;
      this._valueLabel.hidden = true;
      this.collapse();
      this.eval("(" + this._symbolicName + "=" + currentString + ")");
    }
  },

  /**
   * The key press listener for this variable's editable mode textbox.
   */
  _onInputKeyPress: function V__onInputKeyPress(e) {
    switch(e.keyCode) {
      case e.DOM_VK_RETURN:
      case e.DOM_VK_ENTER:
        this._saveInput(e);
        return;
      case e.DOM_VK_ESCAPE:
        this._deactivateInput(e);
        return;
    }
  },

  _symbolicName: "",
  _absoluteName: "",
  _initialDescriptor: null,
  _separatorLabel: null,
  _valueLabel: null,
  _tooltip: null,
  _valueGrip: null,
  _valueString: "",
  _valueClassName: "",
  _prevExpandable: false,
  _prevExpanded: false
});

/**
 * A Property is a Variable holding additional child Property instances.
 * Iterable via "for (let [name, property] in instance) { }".
 *
 * @param Variable aVar
 *        The variable to contain this property.
 * @param string aName
 *        The property's name.
 * @param object aDescriptor
 *        The property's descriptor.
 */
function Property(aVar, aName, aDescriptor) {
  Variable.call(this, aVar, aName, aDescriptor);
  this._symbolicName = aVar._symbolicName + "[\"" + aName + "\"]";
  this._absoluteName = aVar._absoluteName + "." + aName;
  this._initialDescriptor = aDescriptor;
}

create({ constructor: Property, proto: Variable.prototype }, {
  /**
   * Initializes this property's id, view and binds event listeners.
   *
   * @param string aName
   *        The property's name.
   * @param object aDescriptor
   *        The property's descriptor.
   */
  _init: function P__init(aName, aDescriptor) {
    this._idString = generateId(this._nameString = aName);
    this._createScope(aName, "property");
    this._displayVariable(aDescriptor);
    this._displayTooltip();
    this._setAttributes(aName, aDescriptor);
    this._addEventListeners();

    if (aDescriptor.enumerable) {
      this.ownerView._enum.appendChild(this._target);
    } else {
      this.ownerView._nonenum.appendChild(this._target);
    }
  }
});

/**
 * A generator-iterator over the VariablesView, Scopes, Variables and Properties.
 */
VariablesView.prototype.__iterator__ =
Scope.prototype.__iterator__ =
Variable.prototype.__iterator__ =
Property.prototype.__iterator__ = function VV_iterator() {
  for (let item of this._store) {
    yield item;
  }
};

/**
 * Start recording a hierarchy of any added scopes, variables or properties.
 * @see VariablesView.commitHierarchy
 */
VariablesView.prototype.createHierarchy = function VV_createHierarchy() {
  this._prevHierarchy = this._currHierarchy;
  this._currHierarchy = new Map();
};

/**
 * Briefly flash the variables that changed between the previous and current
 * scope/variable/property hierarchies and reopen previously expanded nodes.
 */
VariablesView.prototype.commitHierarchy = function VV_commitHierarchy() {
  let prevHierarchy = this._prevHierarchy;
  let currHierarchy = this._currHierarchy;

  for (let [absoluteName, currVariable] of currHierarchy) {
    // Ignore variables which were already commmitted.
    if (currVariable._committed) {
      continue;
    }

    // Try to get the previous instance of the inspected variable to
    // determine the difference in state.
    let prevVariable = prevHierarchy.get(absoluteName);
    let changed = false;

    // If the inspected variable existed in a previous hierarchy, check if
    // the displayed value (a representation of the grip) has changed.
    if (prevVariable) {
      let prevString = prevVariable._valueString;
      let currString = currVariable._valueString;
      changed = prevString != currString;

      // Re-expand the variable if not previously collapsed.
      if (prevVariable.expanded) {
        currVariable.expand(true);
      }
    }

    // Make sure this variable is not handled in ulteror commits for the
    // same hierarchy.
    currVariable._committed = true;

    // This variable was either not changed or removed, no need to continue.
    if (!changed) {
      continue;
    }

    // Apply an attribute determining the flash type and duration.
    // Dispatch this action after all the nodes have been drawn, so that
    // the transition efects can take place.
    this.window.setTimeout(function(aTarget) {
      aTarget.setAttribute("changed", "");

      aTarget.addEventListener("transitionend", function onEvent() {
        aTarget.removeEventListener("transitionend", onEvent, false);
        aTarget.removeAttribute("changed");
      }, false);
    }.bind(this, currVariable.target), LAZY_EMPTY_DELAY + 1);
  }
};

/**
 * Returns true if the descriptor represents an undefined, null or
 * primitive value.
 *
 * @param object aDescriptor
 *        The variable's descriptor.
 */
VariablesView.isPrimitive = function VV_isPrimitive(aDescriptor) {
  if (!aDescriptor || typeof aDescriptor != "object") {
    return true;
  }

  // For accessor property descriptors, the getter and setter need to be
  // contained in 'get' and 'set' properties.
  let getter = aDescriptor.get;
  let setter = aDescriptor.set;
  if (getter || setter) {
    return false;
  }

  // As described in the remote debugger protocol, the value grip
  // must be contained in a 'value' property.
  let grip = aDescriptor.value;
  if (!grip || typeof grip != "object") {
    return true;
  }

  // For convenience, undefined and null are both considered types.
  let type = grip.type;
  if (["undefined", "null"].indexOf(type + "") != -1) {
    return true;
  }

  return false;
};

/**
 * Returns a custom formatted property string for a grip.
 *
 * @param any aGrip
 *        @see Variable._setGrip
 * @param boolean aConciseFlag
 *        Return a concisely formatted property string.
 * @return string
 *         The formatted property string.
 */
VariablesView.getString = function VV_getString(aGrip, aConciseFlag) {
  if (aGrip && typeof aGrip == "object") {
    switch (aGrip.type) {
      case "undefined":
        return "undefined";
      case "null":
        return "null";
      default:
        if (!aConciseFlag) {
          return "[" + aGrip.type + " " + aGrip.class + "]";
        } else {
          return aGrip.class;
        }
    }
  } else {
    switch (typeof aGrip) {
      case "string":
        return "\"" + aGrip + "\"";
      case "boolean":
        return aGrip ? "true" : "false";
    }
  }
  return aGrip + "";
};

/**
 * Returns a custom class style for a grip.
 *
 * @param any aGrip
 *        @see Variable._setGrip
 * @return string
 *         The custom class style.
 */
VariablesView.getClass = function VV_getClass(aGrip) {
  if (aGrip && typeof aGrip == "object") {
    switch (aGrip.type) {
      case "undefined":
        return "token-undefined";
      case "null":
        return "token-null";
    }
  } else {
    switch (typeof aGrip) {
      case "string":
        return "token-string";
      case "boolean":
        return "token-boolean";
      case "number":
        return "token-number";
    }
  }
  return "token-other";
};

/**
 * A monotonically-increasing counter, that guarantees the uniqueness of scope,
 * variables and properties ids.
 *
 * @param string aName
 *        An optional string to prefix the id with.
 * @return number
 *         A unique id.
 */
let generateId = (function() {
  let count = 0;
  return function(aName = "") {
    return aName.toLowerCase().trim().replace(/\s+/g, "-") + (++count);
  }
})();

/**
 * Sugar for prototypal inheritance using Object.create.
 * Creates a new object with the specified prototype object and properties.
 *
 * @param object target
 * @param object properties
 */
function create({ constructor, proto }, properties = {}) {
  let propertiesObject = {
    constructor: { value: constructor }
  };
  for (let name in properties) {
    propertiesObject[name] = Object.getOwnPropertyDescriptor(properties, name);
  }
  constructor.prototype = Object.create(proto, propertiesObject);
}
