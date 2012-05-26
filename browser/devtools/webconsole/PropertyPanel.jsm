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
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "WebConsoleUtils", function () {
  let obj = {};
  Cu.import("resource:///modules/WebConsoleUtils.jsm", obj);
  return obj.WebConsoleUtils;
});

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
  this._objectCache = {};
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
   * Stores cached information about local objects being inspected.
   * @private
   */
  _objectCache: null,

  /**
   * Use this setter to update the content of the tree.
   *
   * @param object aData
   *        A meta object that holds information about the object you want to
   *        display in the property panel. Object properties:
   *        - object:
   *        This is the raw object you want to display. You can only provide
   *        this object if you want the property panel to work in sync mode.
   *        - remoteObject:
   *        An array that holds information on the remote object being
   *        inspected. Each element in this array describes each property in the
   *        remote object. See WebConsoleUtils.namesAndValuesOf() for details.
   *        - rootCacheId:
   *        The cache ID where the objects referenced in remoteObject are found.
   *        - panelCacheId:
   *        The cache ID where any object retrieved by this property panel
   *        instance should be stored into.
   *        - remoteObjectProvider:
   *        A function that is invoked when a new object is needed. This is
   *        called when the user tries to expand an inspectable property. The
   *        callback must take four arguments:
   *          - fromCacheId:
   *          Tells from where to retrieve the object the user picked (from
   *          which cache ID).
   *          - objectId:
   *          The object ID the user wants.
   *          - panelCacheId:
   *          Tells in which cache ID to store the objects referenced by
   *          objectId so they can be retrieved later.
   *          - callback:
   *          The callback function to be invoked when the remote object is
   *          received. This function takes one argument: the raw message
   *          received from the Web Console content script.
   */
  set data(aData) {
    let oldLen = this._rows.length;

    this._cleanup();

    if (!aData) {
      return;
    }

    if (aData.remoteObject) {
      this._rootCacheId = aData.rootCacheId;
      this._panelCacheId = aData.panelCacheId;
      this._remoteObjectProvider = aData.remoteObjectProvider;
      this._rows = [].concat(aData.remoteObject);
      this._updateRemoteObject(this._rows, 0);
    }
    else if (aData.object) {
      this._rows = this._inspectObject(aData.object);
    }
    else {
      throw new Error("First argument must have a .remoteObject or " +
                      "an .object property!");
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
  _updateRemoteObject: function PTV__updateRemoteObject(aObject, aLevel)
  {
    aObject.forEach(function(aElement) {
      aElement.level = aLevel;
      aElement.isOpened = false;
      aElement.children = null;
    });
  },

  /**
   * Inspect a local object.
   *
   * @private
   * @param object aObject
   *        The object you want to inspect.
   */
  _inspectObject: function PTV__inspectObject(aObject)
  {
    this._objectCache = {};
    this._remoteObjectProvider = this._localObjectProvider.bind(this);
    let children = WebConsoleUtils.namesAndValuesOf(aObject, this._objectCache);
    this._updateRemoteObject(children, 0);
    return children;
  },

  /**
   * An object provider for when the user inspects local objects (not remote
   * ones).
   *
   * @private
   * @param string aFromCacheId
   *        The cache ID from where to retrieve the desired object.
   * @param string aObjectId
   *        The ID of the object you want.
   * @param string aDestCacheId
   *        The ID of the cache where to store any objects referenced by the
   *        desired object.
   * @param function aCallback
   *        The function you want to receive the object.
   */
  _localObjectProvider:
  function PTV__localObjectProvider(aFromCacheId, aObjectId, aDestCacheId,
                                    aCallback)
  {
    let object = WebConsoleUtils.namesAndValuesOf(this._objectCache[aObjectId],
                                                  this._objectCache);
    aCallback({cacheId: aFromCacheId,
               objectId: aObjectId,
               object: object,
               childrenCacheId: aDestCacheId || aFromCacheId,
    });
  },

  /** nsITreeView interface implementation **/

  selection: null,

  get rowCount()                     { return this._rows.length; },
  setTree: function(treeBox)         { this._treeBox = treeBox;  },
  getCellText: function(idx, column) {
    let row = this._rows[idx];
    return row.name + ": " + row.value;
  },
  getLevel: function(idx) {
    return this._rows[idx].level;
  },
  isContainer: function(idx) {
    return !!this._rows[idx].inspectable;
  },
  isContainerOpen: function(idx) {
    return this._rows[idx].isOpened;
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
    var thisLevel = this.getLevel(idx);
    return this._rows.slice(after + 1).some(function (r) r.level == thisLevel);
  },

  toggleOpenState: function(idx)
  {
    let item = this._rows[idx];
    if (!item.inspectable) {
      return;
    }

    if (item.isOpened) {
      this._treeBox.beginUpdateBatch();
      item.isOpened = false;

      var thisLevel = item.level;
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
      let callback = function _onRemoteResponse(aResponse) {
        this._treeBox.beginUpdateBatch();
        item.isOpened = true;

        if (levelUpdate) {
          this._updateRemoteObject(aResponse.object, item.level + 1);
          item.children = aResponse.object;
        }

        this._rows.splice.apply(this._rows, [idx + 1, 0].concat(item.children));

        this._treeBox.rowCountChanged(idx + 1, item.children.length);
        this._treeBox.invalidateRow(idx);
        this._treeBox.endUpdateBatch();
      }.bind(this);

      if (!item.children) {
        let fromCacheId = item.level > 0 ? this._panelCacheId :
                                           this._rootCacheId;
        this._remoteObjectProvider(fromCacheId, item.objectId,
                                   this._panelCacheId, callback);
      }
      else {
        levelUpdate = false;
        callback({object: item.children});
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

  _cleanup: function PTV__cleanup()
  {
    if (this._rows.length) {
      // Reset the existing _rows children to the initial state.
      this._updateRemoteObject(this._rows, 0);
      this._rows = [];
    }

    delete this._objectCache;
    delete this._rootCacheId;
    delete this._panelCacheId;
    delete this._remoteObjectProvider;
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

