/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This component is used to build the menus for the HTML contextmenu attribute.

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;

// A global value that is used to identify each menu item. It is
// incremented with each one that is found.
var gGeneratedId = 1;

function HTMLMenuBuilder() {
  this.currentNode = null;
  this.root = null;
  this.items = {};
  this.nestedStack = [];
};

// Building is done in two steps:
// The first generates a hierarchical JS object that contains the menu structure.
// This object is returned by toJSONString.
//
// The second step can take this structure and generate a XUL menu hierarchy or
// other UI from this object. The default UI is done in PageMenu.jsm.
//
// When a multi-process browser is used, the first step is performed by the child
// process and the second step is performed by the parent process.

HTMLMenuBuilder.prototype =
{
  classID:        Components.ID("{51c65f5d-0de5-4edc-9058-60e50cef77f8}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMenuBuilder]),

  currentNode: null,
  root: null,
  items: {},
  nestedStack: [],

  toJSONString: function() {
    return JSON.stringify(this.root);
  },

  openContainer: function(aLabel) {
    if (!this.currentNode) {
      this.root = {
        type: "menu",
        children: []
      };
      this.currentNode = this.root;
    }
    else {
      let parent = this.currentNode;
      this.currentNode = {
        type: "menu",
        label: aLabel,
        children: []
      };
      parent.children.push(this.currentNode);
      this.nestedStack.push(parent);
    }
  },

  addItemFor: function(aElement, aCanLoadIcon) {
    if (!("children" in this.currentNode)) {
      return;
    }

    let item = {
      type: "menuitem",
      label: aElement.label
    };

    let elementType = aElement.type;
    if (elementType == "checkbox" || elementType == "radio") {
      item.checkbox = true;

      if (aElement.checked) {
        item.checked = true;
      }
    }

    let icon = aElement.icon;
    if (icon.length > 0 && aCanLoadIcon) {
      item.icon = icon;
    }

    if (aElement.disabled) {
      item.disabled = true;
    }

    item.id = gGeneratedId++;
    this.currentNode.children.push(item);

    this.items[item.id] = aElement;
  },

  addSeparator: function() {
    if (!("children" in this.currentNode)) {
      return;
    }

    this.currentNode.children.push({ type: "separator"});
  },

  undoAddSeparator: function() {
    if (!("children" in this.currentNode)) {
      return;
    }

    let children = this.currentNode.children;
    if (children.length && children[children.length - 1].type == "separator") {
      children.pop();
    }
  },

  closeContainer: function() {
    this.currentNode = this.nestedStack.length ? this.nestedStack.pop() : this.root;
  },

  click: function(id) {
    let item = this.items[id];
    if (item) {
      item.click();
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([HTMLMenuBuilder]);
