/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "WebConsoleUtils",
                                  "resource://gre/modules/devtools/WebConsoleUtils.jsm");

var EXPORTED_SYMBOLS = ["PropertyPanel", "PropertyTreeView"];

///////////////////////////////////////////////////////////////////////////
//// PropertyTreeView.

/**
 * This is an implementation of the nsITreeView interface. For comments on the
 * interface properties, see the documentation:
 * https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsITreeView
 */
var PropertyTreeView = function() {
  this._rows = [];
  this._objectActors = [];
};

PropertyTreeView.prototype = {
  /**
   * Stores the visible rows of the tree.
   * @private
   */
  _rows: null,

  /**
   * Stores the nsITreeBoxObject for this tree.
   * @private
   */
  _treeBox: null,

  /**
   * Track known object actor IDs. We clean these when the panel is
   * destroyed/cleaned up.
   *
   * @private
   * @type array
   */
  _objectActors: null,

  /**
   * Map fake object actors to their IDs. This is used when we inspect local
   * objects.
   * @private
   * @type Object
   */
  _localObjectActors: null,

  _releaseObject: null,
  _objectPropertiesProvider: null,

  /**
   * Use this setter to update the content of the tree.
   *
   * @param object aData
   *        A meta object that holds information about the object you want to
   *        display in the property panel. Object properties:
   *        - object:
   *        This is the raw object you want to display. You can only provide
   *        this object if you want the property panel to work in sync mode.
   *        - objectProperties:
   *        An array that holds information on the remote object being
   *        inspected. Each element in this array describes each property in the
   *        remote object. See WebConsoleUtils.inspectObject() for details.
   *        - objectPropertiesProvider:
   *        A function that is invoked when a new object is needed. This is
   *        called when the user tries to expand an inspectable property. The
   *        callback must take four arguments:
   *          - actorID:
   *          The object actor ID from which we request the properties.
   *          - callback:
   *          The callback function to be invoked when the remote object is
   *          received. This function takes one argument: the array of
   *          descriptors for each property in the object represented by the
   *          actor.
   *        - releaseObject:
   *        Function to invoke when an object actor should be released. The
   *        function must take one argument: the object actor ID.
   */
  set data(aData) {
    let oldLen = this._rows.length;

    this.cleanup();

    if (!aData) {
      return;
    }

    if (aData.objectPropertiesProvider) {
      this._objectPropertiesProvider = aData.objectPropertiesProvider;
      this._releaseObject = aData.releaseObject;
      this._propertiesToRows(aData.objectProperties, 0);
      this._rows = aData.objectProperties;
    }
    else if (aData.object) {
      this._localObjectActors = Object.create(null);
      this._rows = this._inspectObject(aData.object);
    }
    else {
      throw new Error("First argument must have an objectActor or an " +
                      "object property!");
    }

    if (this._treeBox) {
      this._treeBox.beginUpdateBatch();
      if (oldLen) {
        this._treeBox.rowCountChanged(0, -oldLen);
      }
      this._treeBox.rowCountChanged(0, this._rows.length);
      this._treeBox.endUpdateBatch();
    }
  },

  /**
   * Update a remote object so it can be used with the tree view. This method
   * adds properties to each array element.
   *
   * @private
   * @param array aObject
   *        The remote object you want prepared for use with the tree view.
   * @param number aLevel
   *        The level you want to give to each property in the remote object.
   */
  _propertiesToRows: function PTV__propertiesToRows(aObject, aLevel)
  {
    aObject.forEach(function(aItem) {
      aItem._level = aLevel;
      aItem._open = false;
      aItem._children = null;

      if (this._releaseObject) {
        ["value", "get", "set"].forEach(function(aProp) {
          let val = aItem[aProp];
          if (val && val.actor) {
            this._objectActors.push(val.actor);
          }
        }, this);
      }
    }, this);
  },

  /**
   * Inspect a local object.
   *
   * @private
   * @param object aObject
   *        The object you want to inspect.
   * @return array
   *         The array of properties, each being described in a way that is
   *         usable by the tree view.
   */
  _inspectObject: function PTV__inspectObject(aObject)
  {
    this._objectPropertiesProvider = this._localPropertiesProvider.bind(this);
    let children =
      WebConsoleUtils.inspectObject(aObject, this._localObjectGrip.bind(this));
    this._propertiesToRows(children, 0);
    return children;
  },

  /**
   * Make a local fake object actor for the given object.
   *
   * @private
   * @param object aObject
   *        The object to make an actor for.
   * @return object
   *         The fake actor grip that represents the given object.
   */
  _localObjectGrip: function PTV__localObjectGrip(aObject)
  {
    let grip = WebConsoleUtils.getObjectGrip(aObject);
    grip.actor = "obj" + gSequenceId();
    this._localObjectActors[grip.actor] = aObject;
    return grip;
  },

  /**
   * A properties provider for when the user inspects local objects (not remote
   * ones).
   *
   * @private
   * @param string aActor
   *        The ID of the object actor you want.
   * @param function aCallback
   *        The function you want to receive the list of properties.
   */
  _localPropertiesProvider:
  function PTV__localPropertiesProvider(aActor, aCallback)
  {
    let object = this._localObjectActors[aActor];
    let properties =
      WebConsoleUtils.inspectObject(object, this._localObjectGrip.bind(this));
    aCallback(properties);
  },

  /** nsITreeView interface implementation **/

  selection: null,

  get rowCount()                     { return this._rows.length; },
  setTree: function(treeBox)         { this._treeBox = treeBox;  },
  getCellText: function PTV_getCellText(idx, column)
  {
    let row = this._rows[idx];
    return row.name + ": " + WebConsoleUtils.getPropertyPanelValue(row);
  },
  getLevel: function(idx) {
    return this._rows[idx]._level;
  },
  isContainer: function(idx) {
    return typeof this._rows[idx].value == "object" && this._rows[idx].value &&
           this._rows[idx].value.inspectable;
  },
  isContainerOpen: function(idx) {
    return this._rows[idx]._open;
  },
  isContainerEmpty: function(idx)    { return false; },
  isSeparator: function(idx)         { return false; },
  isSorted: function()               { return false; },
  isEditable: function(idx, column)  { return false; },
  isSelectable: function(row, col)   { return true; },

  getParentIndex: function(idx)
  {
    if (this.getLevel(idx) == 0) {
      return -1;
    }
    for (var t = idx - 1; t >= 0; t--) {
      if (this.isContainer(t)) {
        return t;
      }
    }
    return -1;
  },

  hasNextSibling: function(idx, after)
  {
    let thisLevel = this.getLevel(idx);
    return this._rows.slice(after + 1).some(function (r) r._level == thisLevel);
  },

  toggleOpenState: function(idx)
  {
    let item = this._rows[idx];
    if (!this.isContainer(idx)) {
      return;
    }

    if (item._open) {
      this._treeBox.beginUpdateBatch();
      item._open = false;

      var thisLevel = item._level;
      var t = idx + 1, deleteCount = 0;
      while (t < this._rows.length && this.getLevel(t++) > thisLevel) {
        deleteCount++;
      }

      if (deleteCount) {
        this._rows.splice(idx + 1, deleteCount);
        this._treeBox.rowCountChanged(idx + 1, -deleteCount);
      }
      this._treeBox.invalidateRow(idx);
      this._treeBox.endUpdateBatch();
    }
    else {
      let levelUpdate = true;
      let callback = function _onRemoteResponse(aProperties) {
        this._treeBox.beginUpdateBatch();
        if (levelUpdate) {
          this._propertiesToRows(aProperties, item._level + 1);
          item._children = aProperties;
        }

        this._rows.splice.apply(this._rows, [idx + 1, 0].concat(item._children));

        this._treeBox.rowCountChanged(idx + 1, item._children.length);
        this._treeBox.invalidateRow(idx);
        this._treeBox.endUpdateBatch();
        item._open = true;
      }.bind(this);

      if (!item._children) {
        this._objectPropertiesProvider(item.value.actor, callback);
      }
      else {
        levelUpdate = false;
        callback(item._children);
      }
    }
  },

  getImageSrc: function(idx, column) { },
  getProgressMode : function(idx,column) { },
  getCellValue: function(idx, column) { },
  cycleHeader: function(col, elem) { },
  selectionChanged: function() { },
  cycleCell: function(idx, column) { },
  performAction: function(action) { },
  performActionOnCell: function(action, index, column) { },
  performActionOnRow: function(action, row) { },
  getRowProperties: function(idx, column, prop) { },
  getCellProperties: function(idx, column, prop) { },
  getColumnProperties: function(column, element, prop) { },

  setCellValue: function(row, col, value)               { },
  setCellText: function(row, col, value)                { },
  drop: function(index, orientation, dataTransfer)      { },
  canDrop: function(index, orientation, dataTransfer)   { return false; },

  /**
   * Cleanup the property tree view.
   */
  cleanup: function PTV_cleanup()
  {
    if (this._releaseObject) {
      this._objectActors.forEach(this._releaseObject);
      delete this._objectPropertiesProvider;
      delete this._releaseObject;
    }
    if (this._localObjectActors) {
      delete this._localObjectActors;
      delete this._objectPropertiesProvider;
    }

    this._rows = [];
    this._objectActors = [];
  },
};

///////////////////////////////////////////////////////////////////////////
//// Helper for creating the panel.

/**
 * Creates a DOMNode and sets all the attributes of aAttributes on the created
 * element.
 *
 * @param nsIDOMDocument aDocument
 *        Document to create the new DOMNode.
 * @param string aTag
 *        Name of the tag for the DOMNode.
 * @param object aAttributes
 *        Attributes set on the created DOMNode.
 * @returns nsIDOMNode
 */
function createElement(aDocument, aTag, aAttributes)
{
  let node = aDocument.createElement(aTag);
  for (var attr in aAttributes) {
    node.setAttribute(attr, aAttributes[attr]);
  }
  return node;
}

/**
 * Creates a new DOMNode and appends it to aParent.
 *
 * @param nsIDOMDocument aDocument
 *        Document to create the new DOMNode.
 * @param nsIDOMNode aParent
 *        A parent node to append the created element.
 * @param string aTag
 *        Name of the tag for the DOMNode.
 * @param object aAttributes
 *        Attributes set on the created DOMNode.
 * @returns nsIDOMNode
 */
function appendChild(aDocument, aParent, aTag, aAttributes)
{
  let node = createElement(aDocument, aTag, aAttributes);
  aParent.appendChild(node);
  return node;
}

///////////////////////////////////////////////////////////////////////////
//// PropertyPanel

/**
 * Creates a new PropertyPanel.
 *
 * @see PropertyTreeView
 * @param nsIDOMNode aParent
 *        Parent node to append the created panel to.
 * @param string aTitle
 *        Title for the panel.
 * @param string aObject
 *        Object to display in the tree. For details about this object please
 *        see the PropertyTreeView constructor in this file.
 * @param array of objects aButtons
 *        Array with buttons to display at the bottom of the panel.
 */
function PropertyPanel(aParent, aTitle, aObject, aButtons)
{
  let document = aParent.ownerDocument;

  // Create the underlying panel
  this.panel = createElement(document, "panel", {
    label: aTitle,
    titlebar: "normal",
    noautofocus: "true",
    noautohide: "true",
    close: "true",
  });

  // Create the tree.
  let tree = this.tree = createElement(document, "tree", {
    flex: 1,
    hidecolumnpicker: "true"
  });

  let treecols = document.createElement("treecols");
  appendChild(document, treecols, "treecol", {
    primary: "true",
    flex: 1,
    hideheader: "true",
    ignoreincolumnpicker: "true"
  });
  tree.appendChild(treecols);

  tree.appendChild(document.createElement("treechildren"));
  this.panel.appendChild(tree);

  // Create the footer.
  let footer = createElement(document, "hbox", { align: "end" });
  appendChild(document, footer, "spacer", { flex: 1 });

  // The footer can have butttons.
  let self = this;
  if (aButtons) {
    aButtons.forEach(function(button) {
      let buttonNode = appendChild(document, footer, "button", {
        label: button.label,
        accesskey: button.accesskey || "",
        class: button.class || "",
      });
      buttonNode.addEventListener("command", button.oncommand, false);
    });
  }

  appendChild(document, footer, "resizer", { dir: "bottomend" });
  this.panel.appendChild(footer);

  aParent.appendChild(this.panel);

  // Create the treeView object.
  this.treeView = new PropertyTreeView();
  this.treeView.data = aObject;

  // Set the treeView object on the tree view. This has to be done *after* the
  // panel is shown. This is because the tree binding must be attached first.
  this.panel.addEventListener("popupshown", function onPopupShow()
  {
    self.panel.removeEventListener("popupshown", onPopupShow, false);
    self.tree.view = self.treeView;
  }, false);

  this.panel.addEventListener("popuphidden", function onPopupHide()
  {
    self.panel.removeEventListener("popuphidden", onPopupHide, false);
    self.destroy();
  }, false);
}

/**
 * Destroy the PropertyPanel. This closes the panel and removes it from the
 * browser DOM.
 */
PropertyPanel.prototype.destroy = function PP_destroy()
{
  this.treeView.data = null;
  this.panel.parentNode.removeChild(this.panel);
  this.treeView = null;
  this.panel = null;
  this.tree = null;
}


function gSequenceId()
{
  return gSequenceId.n++;
}
gSequenceId.n = 0;
