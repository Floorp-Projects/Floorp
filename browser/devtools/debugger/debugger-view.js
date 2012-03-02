/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Victor Porof <vporof@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 ***** END LICENSE BLOCK *****/
"use strict";

const Cu = Components.utils;
const DBG_STRINGS_URI = "chrome://browser/locale/devtools/debugger.properties";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import('resource://gre/modules/Services.jsm');

/**
 * Object mediating visual changes and event listeners between the debugger and
 * the html view.
 */
let DebuggerView = {

  /**
   * L10N shortcut function
   *
   * @param string aName
   * @return string
   */
  getStr: function DV_getStr(aName) {
    return this.stringBundle.GetStringFromName(aName);
  },

  /**
   * L10N shortcut function
   *
   * @param string aName
   * @param array aArray
   * @return string
   */
  getFormatStr: function DV_getFormatStr(aName, aArray) {
    return this.stringBundle.formatStringFromName(aName, aArray, aArray.length);
  }
};

XPCOMUtils.defineLazyGetter(DebuggerView, "stringBundle", function() {
  return Services.strings.createBundle(DBG_STRINGS_URI);
});

/**
 * Functions handling the html stackframes UI.
 */
DebuggerView.Stackframes = {

  /**
   * Sets the current frames state based on the debugger active thread state.
   *
   * @param string aState
   *        Either "paused" or "attached".
   */
  updateState: function DVF_updateState(aState) {
    let resume = document.getElementById("resume");
    let status = document.getElementById("status");

    // If we're paused, show a pause label and a resume label on the button.
    if (aState === "paused") {
      status.textContent = DebuggerView.getStr("pausedState");
      resume.label = DebuggerView.getStr("resumeLabel");
    } else if (aState === "attached") {
      // If we're attached, do the opposite.
      status.textContent = DebuggerView.getStr("runningState");
      resume.label = DebuggerView.getStr("pauseLabel");
    } else {
      // No valid state parameter.
      status.textContent = "";
    }
  },

  /**
   * Sets the onClick listener for the stackframes container.
   *
   * @param function aHandler
   *        The delegate used as the event listener.
   */
  addClickListener: function DVF_addClickListener(aHandler) {
    // save the handler so it can be removed on shutdown
    this._onFramesClick = aHandler;
    this._frames.addEventListener("click", aHandler, false);
  },

  /**
   * Removes all elements from the stackframes container, leaving it empty.
   */
  empty: function DVF_empty() {
    while (this._frames.firstChild) {
      this._frames.removeChild(this._frames.firstChild);
    }
  },

  /**
   * Removes all elements from the stackframes container, and adds a child node
   * with an empty text note attached.
   */
  emptyText: function DVF_emptyText() {
    // make sure the container is empty first
    this.empty();

    let item = document.createElement("div");

    // the empty node should look grayed out to avoid confusion
    item.className = "empty list-item";
    item.appendChild(document.createTextNode(DebuggerView.getStr("emptyText")));

    this._frames.appendChild(item);
  },

  /**
   * Adds a frame to the stackframes container.
   * If the frame already exists (was previously added), null is returned.
   * Otherwise, the newly created element is returned.
   *
   * @param number aDepth
   *        The frame depth specified by the debugger.
   * @param string aFrameIdText
   *        The id to be displayed in the list.
   * @param string aFrameNameText
   *        The name to be displayed in the list.
   * @return object
   *         The newly created html node representing the added frame.
   */
  addFrame: function DVF_addFrame(aDepth, aFrameIdText, aFrameNameText) {
    // make sure we don't duplicate anything
    if (document.getElementById("stackframe-" + aDepth)) {
      return null;
    }

    let frame = document.createElement("div");
    let frameId = document.createElement("span");
    let frameName = document.createElement("span");

    // create a list item to be added to the stackframes container
    frame.id = "stackframe-" + aDepth;
    frame.className = "dbg-stackframe list-item";

    // this list should display the id and name of the frame
    frameId.className = "dbg-stackframe-id";
    frameName.className = "dbg-stackframe-name";
    frameId.appendChild(document.createTextNode(aFrameIdText));
    frameName.appendChild(document.createTextNode(aFrameNameText));

    frame.appendChild(frameId);
    frame.appendChild(frameName);

    this._frames.appendChild(frame);

    // return the element for later use if necessary
    return frame;
  },

  /**
   * Highlights a frame from the stackframe container as selected/deselected.
   *
   * @param number aDepth
   *        The frame depth specified by the debugger.
   * @param boolean aSelect
   *        True if the frame should be selected, false otherwise.
   */
  highlightFrame: function DVF_highlightFrame(aDepth, aSelect) {
    let frame = document.getElementById("stackframe-" + aDepth);

    // the list item wasn't found in the stackframe container
    if (!frame) {
      dump("The frame list item wasn't found in the stackframes container.");
      return;
    }

    // add the 'selected' css class if the frame isn't already selected
    if (aSelect && !frame.classList.contains("selected")) {
      frame.classList.add("selected");

    // remove the 'selected' css class if the frame is already selected
    } else if (!aSelect && frame.classList.contains("selected")) {
      frame.classList.remove("selected");
    }
  },

  /**
   * Gets the current dirty state.
   *
   * @return boolean value
   *         True if should load more frames.
   */
  get dirty() {
    return this._dirty;
  },

  /**
   * Sets if the active thread has more frames that need to be loaded.
   *
   * @param boolean aValue
   *        True if should load more frames.
   */
  set dirty(aValue) {
    this._dirty = aValue;
  },

  /**
   * The cached click listener for the stackframes container.
   */
  _onFramesClick: null,

  /**
   * Listener handling the stackframes container scroll event.
   */
  _onFramesScroll: function DVF__onFramesScroll(aEvent) {
    // update the stackframes container only if we have to
    if (this._dirty) {
      let clientHeight = this._frames.clientHeight;
      let scrollTop = this._frames.scrollTop;
      let scrollHeight = this._frames.scrollHeight;

      // if the stackframes container was scrolled past 95% of the height,
      // load more content
      if (scrollTop >= (scrollHeight - clientHeight) * 0.95) {
        this._dirty = false;

        StackFrames._addMoreFrames();
      }
    }
  },

  /**
   * Listener handling the close button click event.
   */
  _onCloseButtonClick: function DVF__onCloseButtonClick() {
    let root = document.documentElement;
    let debuggerClose = document.createEvent("Events");

    debuggerClose.initEvent("DebuggerClose", true, false);
    root.dispatchEvent(debuggerClose);
  },

  /**
   * Listener handling the pause/resume button click event.
   */
  _onResumeButtonClick: function DVF__onResumeButtonClick() {
    if (ThreadState.activeThread.paused) {
      ThreadState.activeThread.resume();
    } else {
      ThreadState.activeThread.interrupt();
    }
  },

  /**
   * Specifies if the active thread has more frames which need to be loaded.
   */
  _dirty: false,

  /**
   * The cached stackframes container.
   */
  _frames: null,

  /**
   * Initialization function, called when the debugger is initialized.
   */
  initialize: function DVF_initialize() {
    let close = document.getElementById("close");
    let resume = document.getElementById("resume");
    let frames = document.getElementById("stackframes");

    close.addEventListener("click", this._onCloseButtonClick, false);
    resume.addEventListener("click", this._onResumeButtonClick, false);
    frames.addEventListener("scroll", this._onFramesScroll, false);
    window.addEventListener("resize", this._onFramesScroll, false);

    this._frames = frames;
  },

  /**
   * Destruction function, called when the debugger is shut down.
   */
  destroy: function DVF_destroy() {
    let close = document.getElementById("close");
    let resume = document.getElementById("resume");
    let frames = this._frames;

    close.removeEventListener("click", this._onCloseButtonClick, false);
    resume.removeEventListener("click", this._onResumeButtonClick, false);
    frames.removeEventListener("click", this._onFramesClick, false);
    frames.removeEventListener("scroll", this._onFramesScroll, false);
    window.removeEventListener("resize", this._onFramesScroll, false);

    this._frames = null;
  }
};

/**
 * Functions handling the properties view.
 */
DebuggerView.Properties = {

  /**
   * Adds a scope to contain any inspected variables.
   * If the optional id is not specified, the scope html node will have a
   * default id set as aName-scope.
   *
   * @param string aName
   *        The scope name (e.g. "Local", "Global" or "With block").
   * @param string aId
   *        Optional, an id for the scope html node.
   * @return object
   *         The newly created html node representing the added scope or null
   *         if a node was not created.
   */
  _addScope: function DVP__addScope(aName, aId) {
    // make sure the parent container exists
    if (!this._vars) {
      return null;
    }

    // compute the id of the element if not specified
    aId = aId || (aName.toLowerCase().trim().replace(" ", "-") + "-scope");

    // contains generic nodes and functionality
    let element = this._createPropertyElement(aName, aId, "scope", this._vars);

    // make sure the element was created successfully
    if (!element) {
      dump("The debugger scope container wasn't created properly: " + aId);
      return null;
    }

    /**
     * @see DebuggerView.Properties._addVar
     */
    element.addVar = this._addVar.bind(this, element);

    // return the element for later use if necessary
    return element;
  },

  /**
   * Adds a variable to a specified scope.
   * If the optional id is not specified, the variable html node will have a
   * default id set as aScope.id->aName-variable.
   *
   * @param object aScope
   *        The parent scope element.
   * @param string aName
   *        The variable name.
   * @param string aId
   *        Optional, an id for the variable html node.
   * @return object
   *         The newly created html node representing the added var.
   */
  _addVar: function DVP__addVar(aScope, aName, aId) {
    // make sure the scope container exists
    if (!aScope) {
      return null;
    }

    // compute the id of the element if not specified
    aId = aId || (aScope.id + "->" + aName + "-variable");

    // contains generic nodes and functionality
    let element = this._createPropertyElement(aName, aId, "variable",
                                              aScope.querySelector(".details"));

    // make sure the element was created successfully
    if (!element) {
      dump("The debugger variable container wasn't created properly: " + aId);
      return null;
    }

    /**
     * @see DebuggerView.Properties._setGrip
     */
    element.setGrip = this._setGrip.bind(this, element);

    /**
     * @see DebuggerView.Properties._addProperties
     */
    element.addProperties = this._addProperties.bind(this, element);

    // setup the additional elements specific for a variable node
    element.refresh(function() {
      let separator = document.createElement("span");
      let info = document.createElement("span");
      let title = element.querySelector(".title");
      let arrow = element.querySelector(".arrow");

      // separator shouldn't be selectable
      separator.className = "unselectable";
      separator.appendChild(document.createTextNode(": "));

      // the variable information (type, class and/or value)
      info.className = "info";

      title.appendChild(separator);
      title.appendChild(info);

    }.bind(this));

    // return the element for later use if necessary
    return element;
  },

  /**
   * Sets the specific grip for a variable.
   * The grip should contain the value or the type & class, as defined in the
   * remote debugger protocol. For convenience, undefined and null are
   * both considered types.
   *
   * @param object aVar
   *        The parent variable element.
   * @param object aGrip
   *        The primitive or object defining the grip, specifying
   *        the value and/or type & class of the variable (if the type
   *        is not specified, it will be inferred from the value).
   *        e.g. 42
   *             true
   *             "nasu"
   *             { type: "undefined" } }
   *             { type: "null" } }
   *             { type: "object", class: "Object" } }
   * @return object
   *         The same variable.
   */
  _setGrip: function DVP__setGrip(aVar, aGrip) {
    // make sure the variable container exists
    if (!aVar) {
      return null;
    }

    let info = aVar.querySelector(".info") || aVar.target.info;

    // make sure the info node exists
    if (!info) {
      dump("Could not set the grip for the corresponding variable: " + aVar.id);
      return null;
    }

    info.textContent = this._propertyString(aGrip);
    info.classList.add(this._propertyColor(aGrip));

    return aVar;
  },

  /**
   * Adds multiple properties to a specified variable.
   * This function handles two types of properties: data properties and
   * accessor properties, as defined in the remote debugger protocol spec.
   *
   * @param object aVar
   *        The parent variable element.
   * @param object aProperties
   *        An object containing the key: descriptor data properties,
   *        specifying the value and/or type & class of the variable,
   *        or 'get' & 'set' accessor properties.
   *        e.g. { "someProp0": { value: 42 },
   *               "someProp1": { value: true },
   *               "someProp2": { value: "nasu" },
   *               "someProp3": { value: { type: "undefined" } },
   *               "someProp4": { value: { type: "null" } },
   *               "someProp5": { value: { type: "object", class: "Object" } },
   *               "someProp6": { get: { "type": "object", "class": "Function" },
   *                              set: { "type": "undefined" } } }
   * @return object
   *         The same variable.
   */
  _addProperties: function DVP__addProperties(aVar, aProperties) {
    // for each property, add it using the passed object key/grip
    for (let i in aProperties) {
      // Can't use aProperties.hasOwnProperty(i), because it may be overridden.
      if (Object.getOwnPropertyDescriptor(aProperties, i)) {

        // get the specified descriptor for current property
        let desc = aProperties[i];

        // as described in the remote debugger protocol, the value grip must be
        // contained in a 'value' property
        let value = desc["value"];

        // for accessor property descriptors, the two grips need to be
        // contained in 'get' and 'set' properties
        let getter = desc["get"];
        let setter = desc["set"];

        // handle data property and accessor property descriptors
        if (value !== undefined) {
          this._addProperty(aVar, [i, value]);
        }
        if (getter !== undefined || setter !== undefined) {
          let prop = this._addProperty(aVar, [i]).expand();
          prop.getter = this._addProperty(prop, ["get", getter]);
          prop.setter = this._addProperty(prop, ["set", setter]);
        }
      }
    }
    return aVar;
  },

  /**
   * Adds a property to a specified variable.
   * If the optional id is not specified, the property html node will have a
   * default id set as aVar.id->aKey-property.
   *
   * @param object aVar
   *        The parent variable element.
   * @param {Array} aProperty
   *        An array containing the key and grip properties, specifying
   *        the value and/or type & class of the variable (if the type
   *        is not specified, it will be inferred from the value).
   *        e.g. ["someProp0", 42]
   *             ["someProp1", true]
   *             ["someProp2", "nasu"]
   *             ["someProp3", { type: "undefined" }]
   *             ["someProp4", { type: "null" }]
   *             ["someProp5", { type: "object", class: "Object" }]
   * @param string aName
   *        Optional, the property name.
   * @paarm string aId
   *        Optional, an id for the property html node.
   * @return object
   *         The newly created html node representing the added prop.
   */
  _addProperty: function DVP__addProperty(aVar, aProperty, aName, aId) {
    // make sure the variable container exists
    if (!aVar) {
      return null;
    }

    // compute the id of the element if not specified
    aId = aId || (aVar.id + "->" + aProperty[0] + "-property");

    // contains generic nodes and functionality
    let element = this._createPropertyElement(aName, aId, "property",
                                              aVar.querySelector(".details"));

    // make sure the element was created successfully
    if (!element) {
      dump("The debugger property container wasn't created properly.");
      return null;
    }

    /**
     * @see DebuggerView.Properties._setGrip
     */
    element.setGrip = this._setGrip.bind(this, element);

    /**
     * @see DebuggerView.Properties._addProperties
     */
    element.addProperties = this._addProperties.bind(this, element);

    // setup the additional elements specific for a variable node
    element.refresh(function(pKey, pGrip) {
      let propertyString = this._propertyString(pGrip);
      let propertyColor = this._propertyColor(pGrip);
      let key = document.createElement("div");
      let value = document.createElement("div");
      let separator = document.createElement("span");
      let title = element.querySelector(".title");
      let arrow = element.querySelector(".arrow");

      // use a key element to specify the property name
      key.className = "key";
      key.appendChild(document.createTextNode(pKey));

      // use a value element to specify the property value
      value.className = "value";
      value.appendChild(document.createTextNode(propertyString));
      value.classList.add(propertyColor);

      // separator shouldn't be selected
      separator.className = "unselectable";
      separator.appendChild(document.createTextNode(": "));

      if (pKey) {
        title.appendChild(key);
      }
      if (pGrip) {
        title.appendChild(separator);
        title.appendChild(value);
      }

      // make the property also behave as a variable, to allow
      // recursively adding properties to properties
      element.target = {
        info: value
      };

      // save the property to the variable for easier access
      Object.defineProperty(aVar, pKey, { value: element,
                                          writable: false,
                                          enumerable: true,
                                          configurable: true });
    }.bind(this), aProperty);

    // return the element for later use if necessary
    return element;
  },

  /**
   * Returns a custom formatted property string for a type and a value.
   *
   * @param string | object aGrip
   *        The variable grip.
   * @return string
   *         The formatted property string.
   */
  _propertyString: function DVP__propertyString(aGrip) {
    if (aGrip && "object" === typeof aGrip) {
      switch (aGrip["type"]) {
        case "undefined":
          return "undefined";
        case "null":
          return "null";
        default:
          return "[" + aGrip["type"] + " " + aGrip["class"] + "]";
      }
    } else {
      switch (typeof aGrip) {
        case "string":
          return "\"" + aGrip + "\"";
        case "boolean":
          return aGrip ? "true" : "false";
        default:
          return aGrip + "";
      }
    }
    return aGrip + "";
  },

  /**
   * Returns a custom class style for a type and a value.
   *
   * @param string | object aGrip
   *        The variable grip.
   *
   * @return string
   *         The css class style.
   */
  _propertyColor: function DVP__propertyColor(aGrip) {
    if (aGrip && "object" === typeof aGrip) {
      switch (aGrip["type"]) {
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
  },

  /**
   * Creates an element which contains generic nodes and functionality used by
   * any scope, variable or property added to the tree.
   *
   * @param string aName
   *        A generic name used in a title strip.
   * @param string aId
   *        id used by the created element node.
   * @param string aClass
   *        Recommended style class used by the created element node.
   * @param object aParent
   *        The parent node which will contain the element.
   * @return object
   *         The newly created html node representing the generic elem.
   */
  _createPropertyElement: function DVP__createPropertyElement(aName, aId, aClass, aParent) {
    // make sure we don't duplicate anything and the parent exists
    if (document.getElementById(aId)) {
      dump("Duplicating a property element id is not allowed.");
      return null;
    }
    if (!aParent) {
      dump("A property element must have a valid parent node specified.");
      return null;
    }

    let element = document.createElement("div");
    let arrow = document.createElement("span");
    let name = document.createElement("span");
    let title = document.createElement("div");
    let details = document.createElement("div");

    // create a scope node to contain all the elements
    element.id = aId;
    element.className = aClass;

    // the expand/collapse arrow
    arrow.className = "arrow";
    arrow.style.visibility = "hidden";

    // the name element
    name.className = "name unselectable";
    name.appendChild(document.createTextNode(aName || ""));

    // the title element, containing the arrow and the name
    title.className = "title";
    title.addEventListener("click", function() { element.toggle(); }, true);

    // the node element which will contain any added scope variables
    details.className = "details";

    title.appendChild(arrow);
    title.appendChild(name);

    element.appendChild(title);
    element.appendChild(details);

    aParent.appendChild(element);

    /**
     * Shows the element, setting the display style to "block".
     * @return object
     *         The same element.
     */
    element.show = function DVP_element_show() {
      element.style.display = "-moz-box";

      if ("function" === typeof element.onshow) {
        element.onshow(element);
      }
      return element;
    };

    /**
     * Hides the element, setting the display style to "none".
     * @return object
     *         The same element.
     */
    element.hide = function DVP_element_hide() {
      element.style.display = "none";

      if ("function" === typeof element.onhide) {
        element.onhide(element);
      }
      return element;
    };

    /**
     * Expands the element, showing all the added details.
     * @return object
     *         The same element.
     */
    element.expand = function DVP_element_expand() {
      arrow.setAttribute("open", "");
      details.setAttribute("open", "");

      if ("function" === typeof element.onexpand) {
        element.onexpand(element);
      }
      return element;
    };

    /**
     * Collapses the element, hiding all the added details.
     * @return object
     *         The same element.
     */
    element.collapse = function DVP_element_collapse() {
      arrow.removeAttribute("open");
      details.removeAttribute("open");

      if ("function" === typeof element.oncollapse) {
        element.oncollapse(element);
      }
      return element;
    };

    /**
     * Toggles between the element collapse/expand state.
     * @return object
     *         The same element.
     */
    element.toggle = function DVP_element_toggle() {
      element.expanded = !element.expanded;

      if ("function" === typeof element.ontoggle) {
        element.ontoggle(element);
      }
      return element;
    };

    /**
     * Returns if the element is visible.
     * @return boolean
     *         True if the element is visible.
     */
    Object.defineProperty(element, "visible", {
      get: function DVP_element_getVisible() {
        return element.style.display !== "none";
      },
      set: function DVP_element_setVisible(value) {
        if (value) {
          element.show();
        } else {
          element.hide();
        }
      }
    });

    /**
     * Returns if the element is expanded.
     * @return boolean
     *         True if the element is expanded.
     */
    Object.defineProperty(element, "expanded", {
      get: function DVP_element_getExpanded() {
        return arrow.hasAttribute("open");
      },
      set: function DVP_element_setExpanded(value) {
        if (value) {
          element.expand();
        } else {
          element.collapse();
        }
      }
    });

    /**
     * Removes all added children in the details container tree.
     * @return object
     *         The same element.
     */
    element.empty = function DVP_element_empty() {
      // this details node won't have any elements, so hide the arrow
      arrow.style.visibility = "hidden";
      while (details.firstChild) {
        details.removeChild(details.firstChild);
      }

      if ("function" === typeof element.onempty) {
        element.onempty(element);
      }
      return element;
    };

    /**
     * Removes the element from the parent node details container tree.
     * @return object
     *         The same element.
     */
    element.remove = function DVP_element_remove() {
      element.parentNode.removeChild(element);

      if ("function" === typeof element.onremove) {
        element.onremove(element);
      }
      return element;
    };

    /**
     * Generic function refreshing the internal state of the element when
     * it's modified (e.g. a child detail, variable, property is added).
     *
     * @param function aFunction
     *        The function logic used to modify the internal state.
     * @param array aArguments
     *        Optional arguments array to be applied to aFunction.
     */
    element.refresh = function DVP_element_refresh(aFunction, aArguments) {
      if ("function" === typeof aFunction) {
        aFunction.apply(this, aArguments);
      }

      let node = aParent.parentNode;
      let arrow = node.querySelector(".arrow");
      let children = node.querySelector(".details").childNodes.length;

      // if the parent details node has at least one element, set the
      // expand/collapse arrow visible
      if (children) {
        arrow.style.visibility = "visible";
      } else {
        arrow.style.visibility = "hidden";
      }
    }.bind(this);

    // return the element for later use and customization
    return element;
  },

  /**
   * Returns the global scope container.
   */
  get globalScope() {
    return this._globalScope;
  },

  /**
   * Sets the display mode for the global scope container.
   *
   * @param boolean value
   *        False to hide the container, true to show.
   */
  set globalScope(value) {
    if (value) {
      this._globalScope.show();
    } else {
      this._globalScope.hide();
    }
  },

  /**
   * Returns the local scope container.
   */
  get localScope() {
    return this._localScope;
  },

  /**
   * Sets the display mode for the local scope container.
   *
   * @param boolean value
   *        False to hide the container, true to show.
   */
  set localScope(value) {
    if (value) {
      this._localScope.show();
    } else {
      this._localScope.hide();
    }
  },

  /**
   * Returns the with block scope container.
   */
  get withScope() {
    return this._withScope;
  },

  /**
   * Sets the display mode for the with block scope container.
   *
   * @param boolean value
   *        False to hide the container, true to show.
   */
  set withScope(value) {
    if (value) {
      this._withScope.show();
    } else {
      this._withScope.hide();
    }
  },

  /**
   * Returns the closure scope container.
   */
  get closureScope() {
    return this._closureScope;
  },

  /**
   * Sets the display mode for the with block scope container.
   *
   * @param boolean value
   *        False to hide the container, true to show.
   */
  set closureScope(value) {
    if (value) {
      this._closureScope.show();
    } else {
      this._closureScope.hide();
    }
  },

  /**
   * The cached variable properties container.
   */
  _vars: null,

  /**
   * Auto-created global, local, with block and closure scopes containing vars.
   */
  _globalScope: null,
  _localScope: null,
  _withScope: null,
  _closureScope: null,

  /**
   * Initialization function, called when the debugger is initialized.
   */
  initialize: function DVP_initialize() {
    this._vars = document.getElementById("variables");
    this._localScope = this._addScope(DebuggerView.getStr("localScope")).expand();
    this._withScope = this._addScope(DebuggerView.getStr("withScope")).hide();
    this._closureScope = this._addScope(DebuggerView.getStr("closureScope")).hide();
    this._globalScope = this._addScope(DebuggerView.getStr("globalScope"));
  },

  /**
   * Destruction function, called when the debugger is shut down.
   */
  destroy: function DVP_destroy() {
    this._vars = null;
    this._globalScope = null;
    this._localScope = null;
    this._withScope = null;
    this._closureScope = null;
  }
};

/**
 * Functions handling the html scripts UI.
 */
DebuggerView.Scripts = {

  /**
   * Sets the change event listener for the source scripts container.
   *
   * @param function aHandler
   *        The delegate used as the event listener.
   */
  addChangeListener: function DVS_addChangeListener(aHandler) {
    // Save the handler so it can be removed on shutdown.
    this._onScriptsChange = aHandler;
    this._scripts.addEventListener("select", aHandler, false);
  },

  /**
   * Removes all elements from the scripts container, leaving it empty.
   */
  empty: function DVS_empty() {
    while (this._scripts.firstChild) {
      this._scripts.removeChild(this._scripts.firstChild);
    }
  },

  /**
   * Checks whether the script with the specified URL is among the scripts
   * known to the debugger and shown in the list.
   *
   * @param string aUrl
   *        The script URL.
   */
  contains: function DVS_contains(aUrl) {
    if (this._scripts.getElementsByAttribute("value", aUrl).length > 0) {
      return true;
    }
    return false;
  },

  /**
   * Checks whether the script with the specified URL is selected in the list.
   *
   * @param string aUrl
   *        The script URL.
   */
  isSelected: function DVS_isSelected(aUrl) {
    if (this._scripts.selectedItem &&
        this._scripts.selectedItem.value == aUrl) {
      return true;
    }
    return false;
  },

  /**
   * Selects the script with the specified URL from the list.
   *
   * @param string aUrl
   *        The script URL.
   */
   selectScript: function DVS_selectScript(aUrl) {
    for (let i = 0; i < this._scripts.itemCount; i++) {
      if (this._scripts.getItemAtIndex(i).value == aUrl) {
        this._scripts.selectedIndex = i;
        break;
      }
    }
   },

  /**
   * Adds a script to the scripts container.
   * If the script already exists (was previously added), null is returned.
   * Otherwise, the newly created element is returned.
   *
   * @param string aUrl
   *        The script url.
   * @param string aScript
   *        The source script.
   * @param string aScriptNameText
   *        Optional, title displayed instead of url.
   * @return object
   *         The newly created html node representing the added script.
   */
  addScript: function DVS_addScript(aUrl, aSource, aScriptNameText) {
    // make sure we don't duplicate anything
    if (this.contains(aUrl)) {
      return null;
    }

    let script = this._scripts.appendItem(aScriptNameText || aUrl, aUrl);
    script.setUserData("sourceScript", aSource, null);
    this._scripts.selectedItem = script;
    return script;
  },

  /**
   * Returns the list of URIs for scripts in the page.
   */
  scriptLocations: function DVS_scriptLocations() {
    let locations = [];
    for (let i = 0; i < this._scripts.itemCount; i++) {
      locations.push(this._scripts.getItemAtIndex(i).value);
    }
    return locations;
  },

  /**
   * The cached click listener for the scripts container.
   */
  _onScriptsChange: null,

  /**
   * The cached scripts container.
   */
  _scripts: null,

  /**
   * Initialization function, called when the debugger is initialized.
   */
  initialize: function DVS_initialize() {
    this._scripts = document.getElementById("scripts");
  },

  /**
   * Destruction function, called when the debugger is shut down.
   */
  destroy: function DVS_destroy() {
    this._scripts.removeEventListener("select", this._onScriptsChange, false);
    this._scripts = null;
  }
};


let DVF = DebuggerView.Stackframes;
DVF._onFramesScroll = DVF._onFramesScroll.bind(DVF);
DVF._onCloseButtonClick = DVF._onCloseButtonClick.bind(DVF);
DVF._onResumeButtonClick = DVF._onResumeButtonClick.bind(DVF);
