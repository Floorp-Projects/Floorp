/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
#ifdef 0
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
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rob Campbell <rcampbell@mozilla.com> (original author)
 *   Mihai Șucan <mihai.sucan@gmail.com>
 *   Julian Viereck <jviereck@mozilla.com>
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
#endif

const INSPECTOR_INVISIBLE_ELEMENTS = {
  "head": true,
  "base": true,
  "basefont": true,
  "isindex": true,
  "link": true,
  "meta": true,
  "script": true,
  "style": true,
  "title": true,
};

///////////////////////////////////////////////////////////////////////////
//// PanelHighlighter

/**
 * A highlighter mechanism using xul panels.
 *
 * @param aBrowser
 *        The XUL browser object for the content window being highlighted.
 */
function PanelHighlighter(aBrowser)
{
  this.panel = document.getElementById("highlighter-panel");
  this.panel.hidden = false;
  this.browser = aBrowser;
  this.win = this.browser.contentWindow;
}

PanelHighlighter.prototype = {

  /**
   * Highlight this.node, unhilighting first if necessary.
   *
   * @param scroll
   *        Boolean determining whether to scroll or not.
   */
  highlight: function PanelHighlighter_highlight(scroll)
  {
    // node is not set or node is not highlightable, bail
    if (!this.isNodeHighlightable()) {
      return;
    }

    this.unhighlight();

    let rect = this.node.getBoundingClientRect();

    if (scroll) {
      this.node.scrollIntoView();
    }

    if (this.viewContainsRect(rect)) {
      // TODO check for offscreen boundaries, bug565301
      this.panel.openPopup(this.node, "overlap", 0, 0, false, false);
      this.panel.sizeTo(rect.width, rect.height);
    } else {
      this.highlightVisibleRegion(rect);
    }
  },

  /**
   * Highlight the given node.
   *
   * @param aNode
   *        a DOM element to be highlighted
   * @param aParams
   *        extra parameters object
   */
  highlightNode: function PanelHighlighter_highlightNode(aNode, aParams)
  {
    this.node = aNode;
    this.highlight(aParams && aParams.scroll);
  },

  /**
   * Highlight the visible region of the region described by aRect, if any.
   *
   * @param aRect
   * @returns boolean
   *          was a region highlighted?
   */
  highlightVisibleRegion: function PanelHighlighter_highlightVisibleRegion(aRect)
  {
    let offsetX = 0;
    let offsetY = 0;
    let width = 0;
    let height = 0;
    let visibleWidth = this.win.innerWidth;
    let visibleHeight = this.win.innerHeight;

    // If any of these edges are out-of-bounds, the node's rectangle is
    // completely out-of-view and we can return.
    if (aRect.top > visibleHeight || aRect.left > visibleWidth ||
        aRect.bottom < 0 || aRect.right < 0) {
      return false;
    }

    // Calculate node offsets, if values are negative, then start the offsets
    // at their absolute values from node origin. The delta should be the edge
    // of view.
    offsetX = aRect.left < 0 ? Math.abs(aRect.left) : 0;
    offsetY = aRect.top < 0 ? Math.abs(aRect.top) : 0;

    // Calculate actual node width, taking into account the available visible
    // width and then subtracting the offset for the final dimension.
    width = aRect.right > visibleWidth ? visibleWidth - aRect.left :
      aRect.width;
    width -= offsetX;

    // Calculate actual node height using the same formula as above for width.
    height = aRect.bottom > visibleHeight ? visibleHeight - aRect.top :
      aRect.height;
    height -= offsetY;

    // If width and height are non-negative, open the highlighter popup over the
    // node and sizeTo width and height.
    if (width > 0 && height > 0) {
      this.panel.openPopup(this.node, "overlap", offsetX, offsetY, false,
        false);
      this.panel.sizeTo(width, height);
      return true;
    }

    return false;
  },

  /**
   * Close the highlighter panel.
   */
  unhighlight: function PanelHighlighter_unhighlight()
  {
    if (this.isHighlighting) {
      this.panel.hidePopup();
    }
  },

  /**
   * Is the highlighter panel open?
   *
   * @returns boolean
   */
  get isHighlighting()
  {
    return this.panel.state == "open";
  },

  /**
   * Return the midpoint of a line from pointA to pointB.
   *
   * @param aPointA
   *        An object with x and y properties.
   * @param aPointB
   *        An object with x and y properties.
   * @returns aPoint
   *          An object with x and y properties.
   */
  midPoint: function PanelHighlighter_midPoint(aPointA, aPointB)
  {
    let pointC = { };
    pointC.x = (aPointB.x - aPointA.x) / 2 + aPointA.x;
    pointC.y = (aPointB.y - aPointA.y) / 2 + aPointA.y;
    return pointC;
  },

  /**
   * Return the node under the highlighter rectangle. Useful for testing.
   * Calculation based on midpoint of diagonal from top left to bottom right
   * of panel.
   *
   * @returns a DOM node or null if none
   */
  get highlitNode()
  {
    // No highlighter panel? Bail.
    if (!this.isHighlighting) {
      return null;
    }

    let browserRect = this.browser.getBoundingClientRect();
    let clientRect = this.panel.getBoundingClientRect();

    // Calculate top left point offset minus browser chrome.
    let a = {
      x: clientRect.left - browserRect.left,
      y: clientRect.top - browserRect.top
    };

    // Calculate bottom right point minus browser chrome.
    let b = {
      x: clientRect.right - browserRect.left,
      y: clientRect.bottom - browserRect.top
    };

    // Get midpoint of diagonal line.
    let midpoint = this.midPoint(a, b);

    return InspectorUI.elementFromPoint(this.win.document, midpoint.x,
      midpoint.y);
  },

  /**
   * Is this.node highlightable?
   *
   * @returns boolean
   */
  isNodeHighlightable: function PanelHighlighter_isNodeHighlightable()
  {
    if (!this.node) {
      return false;
    }
    let nodeName = this.node.nodeName.toLowerCase();
    if (nodeName[0] == '#') {
      return false;
    }
    return !INSPECTOR_INVISIBLE_ELEMENTS[nodeName];
  },

  /**
   * Returns true if the given viewport-relative rect is within the visible area
   * of the window.
   *
   * @param aRect
   *        a CSS rectangle object
   * @returns boolean
   */
  viewContainsRect: function PanelHighlighter_viewContainsRect(aRect)
  {
    let visibleWidth = this.win.innerWidth;
    let visibleHeight = this.win.innerHeight;

    return ((0 <= aRect.left) && (aRect.right <= visibleWidth) && 
        (0 <= aRect.top) && (aRect.bottom <= visibleHeight))
  },

  /////////////////////////////////////////////////////////////////////////
  //// Event Handling

  /**
   * Handle mousemoves in panel when InspectorUI.inspecting is true.
   *
   * @param aEvent
   *        The MouseEvent triggering the method.
   */
  handleMouseMove: function PanelHighlighter_handleMouseMove(aEvent)
  {
    if (!InspectorUI.inspecting) {
      return;
    }
    let browserRect = this.browser.getBoundingClientRect();
    let element = InspectorUI.elementFromPoint(this.win.document,
      aEvent.clientX - browserRect.left, aEvent.clientY - browserRect.top);
    if (element && element != this.node) {
      InspectorUI.inspectNode(element);
    }
  },

  /**
   * Handle MozMousePixelScroll in panel when InspectorUI.inspecting is true.
   *
   * @param aEvent
   *        The onMozMousePixelScrollEvent triggering the method.
   * @returns void
   */
  handlePixelScroll: function PanelHighlighter_handlePixelScroll(aEvent) {
    if (!InspectorUI.inspecting) {
      return;
    }
    let browserRect = this.browser.getBoundingClientRect();
    let element = InspectorUI.elementFromPoint(this.win.document,
      aEvent.clientX - browserRect.left, aEvent.clientY - browserRect.top);
    let win = element.ownerDocument.defaultView;

    if (aEvent.axis == aEvent.HORIZONTAL_AXIS) {
      win.scrollBy(aEvent.detail, 0);
    } else {
      win.scrollBy(0, aEvent.detail);
    }
  }
};

///////////////////////////////////////////////////////////////////////////
//// InspectorTreeView

/**
 * TreeView object to manage the view of the DOM tree. Wraps and provides an
 * interface to an inIDOMView object
 *
 * @param aWindow
 *        a top-level window object
 */
function InspectorTreeView(aWindow)
{
  this.tree = document.getElementById("inspector-tree");
  this.treeBody = document.getElementById("inspector-tree-body");
  this.view = Cc["@mozilla.org/inspector/dom-view;1"]
              .createInstance(Ci.inIDOMView);
  this.view.showSubDocuments = true;
  this.view.whatToShow = NodeFilter.SHOW_ALL;
  this.tree.view = this.view;
  this.contentWindow = aWindow;
  this.view.rootNode = aWindow.document;
  this.view.rebuild();
}

InspectorTreeView.prototype = {
  get editable() { return false; },
  get selection() { return this.view.selection; },

  /**
   * Destroy the view.
   */
  destroy: function ITV_destroy()
  {
    this.tree.view = null;
    this.view = null;
    this.tree = null;
  },

  /**
   * Get the cell text at a given row and column.
   *
   * @param aRow
   *        The row index of the desired cell.
   * @param aCol
   *        The column index of the desired cell.
   * @returns string
   */
  getCellText: function ITV_getCellText(aRow, aCol)
  {
    return this.view.getCellText(aRow, aCol);
  },

  /**
   * Get the index of the selected row.
   *
   * @returns number -1 if there is no row selected.
   */
  get selectionIndex()
  {
    return this.selection ? this.selection.currentIndex : -1;
  },

  /**
   * Get the corresponding node for the currently-selected row in the tree.
   *
   * @returns DOMNode|null
   */
  get selectedNode()
  {
    let rowIndex = this.selectionIndex;
    return rowIndex > -1 ? this.view.getNodeFromRowIndex(rowIndex) : null;
  },

  /**
   * Set the selected row in the table to the specified index.
   *
   * @param anIndex
   *        The index to set the selection to.
   */
  set selectedRow(anIndex)
  {
    this.view.selection.select(anIndex);
    this.tree.treeBoxObject.ensureRowIsVisible(anIndex);
  },

  /**
   * Set the selected node to the specified document node.
   *
   * @param aNode
   *        The document node to select in the tree.
   */
  set selectedNode(aNode)
  {
    let rowIndex = this.view.getRowIndexFromNode(aNode);
    if (rowIndex > -1) {
      this.selectedRow = rowIndex;
    } else {
      this.selectElementInTree(aNode);
    }
  },

  /**
   * Select the given node in the tree, searching for and expanding rows
   * as-needed.
   *
   * @param aNode
   *        The document node to select in the three.
   * @returns boolean
   *          Whether a node was selected or not if not found.
   */
  selectElementInTree: function ITV_selectElementInTree(aNode)
  {
    if (!aNode) {
      this.view.selection.select(null);
      return false;      
    }

    // Keep searching until a pre-created ancestor is found, then 
    // open each ancestor until the found element is created.
    let domUtils = Cc["@mozilla.org/inspector/dom-utils;1"].
                    getService(Ci.inIDOMUtils);
    let line = [];
    let parent = aNode;
    let index = null;

    while (parent) {
      index = this.view.getRowIndexFromNode(parent);
      line.push(parent);
      if (index < 0) {
        // Row for this node hasn't been created yet.
        parent = domUtils.getParentForNode(parent,
          this.view.showAnonymousContent);
      } else {
        break;
      }
    }

    // We have all the ancestors, now open them one-by-one from the top
    // to bottom.
    let lastIndex;
    let view = this.tree.treeBoxObject.view;

    for (let i = line.length - 1; i >= 0; --i) {
      index = this.view.getRowIndexFromNode(line[i]);
      if (index < 0) {
        // Can't find the row, so stop trying to descend.
        break;
      }
      if (i > 0 && !view.isContainerOpen(index)) {
        view.toggleOpenState(index);
      }
      lastIndex = index;
    }

    if (lastIndex >= 0) {
      this.selectedRow = lastIndex;
      return true;
    }
    
    return false;
  },
};

///////////////////////////////////////////////////////////////////////////
//// InspectorUI

/**
 * Main controller class for the Inspector.
 */
var InspectorUI = {
  browser: null,
  selectEventsSuppressed: false,
  inspecting: false,

  /**
   * Toggle the inspector interface elements on or off.
   *
   * @param aEvent
   *        The event that requested the UI change. Toolbar button or menu.
   */
  toggleInspectorUI: function IUI_toggleInspectorUI(aEvent)
  {
    if (this.isPanelOpen) {
      this.closeInspectorUI(true);
    } else {
      this.openInspectorUI();
    }
  },

  /**
   * Toggle the status of the inspector, starting or stopping it. Invoked
   * from the toolbar's Inspect button.
   */
  toggleInspection: function IUI_toggleInspection()
  {
    if (this.inspecting) {
      this.stopInspecting();
    } else {
      this.startInspecting();
    }
  },

  /**
   * Toggle the style panel. Invoked from the toolbar's Style button.
   */
  toggleStylePanel: function IUI_toggleStylePanel()
  {
    if (this.isStylePanelOpen) {
      this.stylePanel.hidePopup();
    } else {
      this.openStylePanel();
      if (this.treeView.selectedNode) {
        this.updateStylePanel(this.treeView.selectedNode);
      }
    }
  },

  /**
   * Toggle the DOM panel. Invoked from the toolbar's DOM button.
   */
  toggleDOMPanel: function IUI_toggleDOMPanel()
  {
    if (this.isDOMPanelOpen) {
      this.domPanel.hidePopup();
    } else {
      this.clearDOMPanel();
      this.openDOMPanel();
      if (this.treeView.selectedNode) {
        this.updateDOMPanel(this.treeView.selectedNode);
      }
    }
  },

  /**
   * Is the tree panel open?
   *
   * @returns boolean
   */
  get isPanelOpen()
  {
    return this.treePanel && this.treePanel.state == "open";
  },

  /**
   * Is the style panel open?
   *
   * @returns boolean
   */
  get isStylePanelOpen()
  {
    return this.stylePanel && this.stylePanel.state == "open";
  },

  /**
   * Is the DOM panel open?
   *
   * @returns boolean
   */
  get isDOMPanelOpen()
  {
    return this.domPanel && this.domPanel.state == "open";
  },

  /**
   * Open the inspector's tree panel and initialize it.
   */
  openTreePanel: function IUI_openTreePanel()
  {
    if (!this.treePanel) {
      this.treePanel = document.getElementById("inspector-panel");
      this.treePanel.hidden = false;
    }
    if (!this.isPanelOpen) {
      const panelWidthRatio = 7 / 8;
      const panelHeightRatio = 1 / 5;
      let bar = document.getElementById("status-bar");
      this.treePanel.openPopupAtScreen(this.win.screenX + 80,
        this.win.outerHeight + this.win.screenY);
      this.treePanel.sizeTo(this.win.outerWidth * panelWidthRatio, 
        this.win.outerHeight * panelHeightRatio);
      this.tree = document.getElementById("inspector-tree");
      this.createDocumentModel();
    }
  },

  /**
   * Open the style panel if not already onscreen.
   */
  openStylePanel: function IUI_openStylePanel()
  {
    if (!this.stylePanel)
      this.stylePanel = document.getElementById("inspector-style-panel");
    if (!this.isStylePanelOpen) {
      this.stylePanel.hidden = false;
      // open at top right of browser panel, offset by 20px from top.
      this.stylePanel.openPopup(this.browser, "end_before", 0, 20, false, false);
      // size panel to 200px wide by half browser height - 60.
      this.stylePanel.sizeTo(200, this.win.outerHeight / 2 - 60);
    }
  },

  /**
   * Open the DOM panel if not already onscreen.
   */
  openDOMPanel: function IUI_openDOMPanel()
  {
    if (!this.isDOMPanelOpen) {
      this.domPanel.hidden = false;
      // open at middle right of browser panel, offset by 20px from middle.
      this.domPanel.openPopup(this.browser, "end_before", 0,
        this.win.outerHeight / 2 - 20, false, false);
      // size panel to 200px wide by half browser height - 60.
      this.domPanel.sizeTo(200, this.win.outerHeight / 2 - 60);
    }
  },

  /**
   * Toggle the dimmed (semi-transparent) state for a panel by setting or
   * removing a dimmed attribute.
   *
   * @param aDim
   *        The panel to be dimmed.
   */
  toggleDimForPanel: function IUI_toggleDimForPanel(aDim)
  {
    if (aDim.hasAttribute("dimmed")) {
      aDim.removeAttribute("dimmed");
    } else {
      aDim.setAttribute("dimmed", "true");
    }
  },

  /**
   * Open inspector UI. tree, style and DOM panels if enabled. Add listeners for
   * document scrolling, resize, tabContainer.TabSelect and others.
   */
  openInspectorUI: function IUI_openInspectorUI()
  {
    // initialization
    this.browser = gBrowser.selectedBrowser;
    this.win = this.browser.contentWindow;
    this.winID = this.getWindowID(this.win);

    // DOM panel initialization and loading (via PropertyPanel.jsm)
    let objectPanelTitle = this.strings.
      GetStringFromName("object.objectPanelTitle");
    let parent = document.getElementById("inspector-style-panel").parentNode;
    this.propertyPanel = new (this.PropertyPanel)(parent, document,
      objectPanelTitle, {});

    // additional DOM panel setup needed for unittest identification and use
    this.domPanel = this.propertyPanel.panel;
    this.domPanel.setAttribute("id", "inspector-dom-panel");
    this.domBox = this.propertyPanel.tree;
    this.domTreeView = this.propertyPanel.treeView;

    // open inspector UI
    this.openTreePanel();

    // style panel setup and activation
    this.styleBox = document.getElementById("inspector-style-listbox");
    this.clearStylePanel();
    this.openStylePanel();

    // DOM panel setup and activation
    this.clearDOMPanel();
    this.openDOMPanel();

    // setup highlighter and start inspecting
    this.initializeHighlighter();

    if (!InspectorStore.hasID(this.winID) ||
      InspectorStore.getValue(this.winID, "inspecting")) {
      this.startInspecting();
    }

    this.win.document.addEventListener("scroll", this, false);
    this.win.addEventListener("resize", this, false);
    this.inspectCmd.setAttribute("checked", true);

    if (InspectorStore.isEmpty()) {
      gBrowser.tabContainer.addEventListener("TabSelect", this, false);
    }

    if (InspectorStore.hasID(this.winID)) {
      let selectedNode = InspectorStore.getValue(this.winID, "selectedNode");
      if (selectedNode) {
        this.inspectNode(selectedNode);
      }
    } else {
      InspectorStore.addStore(this.winID);
      InspectorStore.setValue(this.winID, "selectedNode", null);
      this.win.addEventListener("pagehide", this, true);
    }
  },

  /**
   * Initialize highlighter.
   */
  initializeHighlighter: function IUI_initializeHighlighter()
  {
    this.highlighter = new PanelHighlighter(this.browser);
  },

  /**
   * Close inspector UI and associated panels. Unhighlight and stop inspecting.
   * Remove event listeners for document scrolling, resize,
   * tabContainer.TabSelect and others.
   *
   * @param boolean aClearStore tells if you want the store associated to the
   * current tab/window to be cleared or not.
   */
  closeInspectorUI: function IUI_closeInspectorUI(aClearStore)
  {
    if (aClearStore) {
      InspectorStore.deleteStore(this.winID);
      this.win.removeEventListener("pagehide", this, true);
    } else {
      // Update the store before closing.
      InspectorStore.setValue(this.winID, "selectedNode",
        this.treeView.selectedNode);
      InspectorStore.setValue(this.winID, "inspecting", this.inspecting);
    }

    if (InspectorStore.isEmpty()) {
      gBrowser.tabContainer.removeEventListener("TabSelect", this, false);
    }

    this.win.document.removeEventListener("scroll", this, false);
    this.win.removeEventListener("resize", this, false);
    this.stopInspecting();
    if (this.highlighter && this.highlighter.isHighlighting) {
      this.highlighter.unhighlight();
    }
    if (this.isPanelOpen) {
      this.treePanel.hidePopup();
      this.treeView.destroy();
    }
    if (this.isStylePanelOpen) {
      this.stylePanel.hidePopup();
    }
    if (this.domPanel) {
      this.domPanel.hidePopup();
      this.domBox = null;
      this.domTreeView = null;
      this.propertyPanel.destroy();
    }
    this.inspectCmd.setAttribute("checked", false);
    this.browser = this.win = null; // null out references to browser and window
    this.winID = null;
  },

  /**
   * Begin inspecting webpage, attach page event listeners, activate
   * highlighter event listeners.
   */
  startInspecting: function IUI_startInspecting()
  {
    this.attachPageListeners();
    this.inspecting = true;
    this.toggleDimForPanel(this.stylePanel);
    this.toggleDimForPanel(this.domPanel);
  },

  /**
   * Stop inspecting webpage, detach page listeners, disable highlighter
   * event listeners.
   */
  stopInspecting: function IUI_stopInspecting()
  {
    if (!this.inspecting)
      return;
    this.detachPageListeners();
    this.inspecting = false;
    this.toggleDimForPanel(this.stylePanel);
    this.toggleDimForPanel(this.domPanel);
    if (this.treeView.selection) {
      this.updateStylePanel(this.treeView.selectedNode);
      this.updateDOMPanel(this.treeView.selectedNode);
    }
  },

  /////////////////////////////////////////////////////////////////////////
  //// Model Creation Methods

  /**
   * Create treeView object from content window.
   */
  createDocumentModel: function IUI_createDocumentModel()
  {
    this.treeView = new InspectorTreeView(this.win);
  },

  /**
   * Add a new item to the style panel listbox.
   *
   * @param aLabel
   *        A bit of text to put in the listitem's label attribute.
   * @param aType
   *        The type of item.
   * @param content
   *        Text content or value of the listitem.
   */
  addStyleItem: function IUI_addStyleItem(aLabel, aType, aContent)
  {
    let itemLabelString = this.strings.GetStringFromName("style.styleItemLabel");
    let item = document.createElement("listitem");

    // Do not localize these strings
    let label = aLabel;
    item.className = "style-" + aType;
    if (aContent) {
      label = itemLabelString.replace("#1", aLabel);
      label = label.replace("#2", aContent);
    }
    item.setAttribute("label", label);

    this.styleBox.appendChild(item);
  },

  /**
   * Create items for each rule included in the given array.
   *
   * @param aRules
   *        an array of rule objects
   */
  createStyleRuleItems: function IUI_createStyleRuleItems(aRules)
  {
    let selectorLabel = this.strings.GetStringFromName("style.selectorLabel");

    aRules.forEach(function(rule) {
      this.addStyleItem(selectorLabel, "selector", rule.id);
      rule.properties.forEach(function(property) {
        if (property.overridden)
          return; // property marked overridden elsewhere
        // Do not localize the strings below this line
        let important = "";
        if (property.important)
          important += " !important";
        this.addStyleItem(property.name, "property", property.value + important);
      }, this);
    }, this);
  },

  /**
   * Create rule items for each section as well as the element's style rules,
   * if any.
   *
   * @param aRules
   *        Array of rules corresponding to the element's style object.
   * @param aSections
   *        Array of sections encapsulating the inherited rules for selectors
   *        and elements.
   */
  createStyleItems: function IUI_createStyleItems(aRules, aSections)
  {
    this.createStyleRuleItems(aRules);
    let inheritedString = 
        this.strings.GetStringFromName("style.inheritedFrom");
    aSections.forEach(function(section) {
      let sectionTitle = section.element.tagName;
      if (section.element.id)
        sectionTitle += "#" + section.element.id;
      let replacedString = inheritedString.replace("#1", sectionTitle);
      this.addStyleItem(replacedString, "section");
      this.createStyleRuleItems(section.rules);
    }, this);
  },

  /**
   * Remove all items from the Style Panel's listbox.
   */
  clearStylePanel: function IUI_clearStylePanel()
  {
    for (let i = this.styleBox.childElementCount; i >= 0; --i)
      this.styleBox.removeItemAt(i);
  },

  /**
   * Remove all items from the DOM Panel's listbox.
   */
  clearDOMPanel: function IUI_clearStylePanel()
  {
    this.domTreeView.data = {};
  },

  /**
   * Update the contents of the style panel with styles for the currently
   * inspected node.
   *
   * @param aNode
   *        The highlighted node to get styles for.
   */
  updateStylePanel: function IUI_updateStylePanel(aNode)
  {
    if (this.inspecting || !this.isStylePanelOpen) {
      return;
    }

    let rules = [], styleSections = [], usedProperties = {};
    this.style.getInheritedRules(aNode, styleSections, usedProperties);
    this.style.getElementRules(aNode, rules, usedProperties);
    this.clearStylePanel();
    this.createStyleItems(rules, styleSections);
  },

  /**
   * Update the contents of the DOM panel with name/value pairs for the
   * currently-inspected node.
   */
  updateDOMPanel: function IUI_updateDOMPanel(aNode)
  {
    if (this.inspecting || !this.isDOMPanelOpen) {
      return;
    }

    this.domTreeView.data = aNode;
  },

  /////////////////////////////////////////////////////////////////////////
  //// Event Handling

  /**
   * Main callback handler for events.
   *
   * @param event
   *        The event to be handled.
   */
  handleEvent: function IUI_handleEvent(event)
  {
    let winID = null;
    let win = null;

    switch (event.type) {
      case "TabSelect":
        winID = this.getWindowID(gBrowser.selectedBrowser.contentWindow);
        if (this.isPanelOpen && winID != this.winID) {
          this.closeInspectorUI(false);
        }

        if (winID && InspectorStore.hasID(winID)) {
          this.openInspectorUI();
        }

        if (InspectorStore.isEmpty()) {
          gBrowser.tabContainer.removeEventListener("TabSelect", this, false);
        }
        break;
      case "pagehide":
        win = event.originalTarget.defaultView;
        // Skip iframes/frames.
        if (!win || win.frameElement || win.top != win) {
          break;
        }

        win.removeEventListener(event.type, this, true);

        winID = this.getWindowID(win);
        if (winID && winID != this.winID) {
          InspectorStore.deleteStore(winID);
        }

        if (InspectorStore.isEmpty()) {
          gBrowser.tabContainer.removeEventListener("TabSelect", this, false);
        }
        break;
      case "keypress":
        switch (event.keyCode) {
          case KeyEvent.DOM_VK_RETURN:
          case KeyEvent.DOM_VK_ESCAPE:
            this.stopInspecting();
            break;
        }
        break;
      case "mousemove":
        let element = this.elementFromPoint(event.target.ownerDocument,
          event.clientX, event.clientY);
        if (element && element != this.node) {
          this.inspectNode(element);
        }
        break;
      case "click":
        this.stopInspecting();
        break;
      case "scroll":
      case "resize":
        this.highlighter.highlight();
        break;
    }
  },

  /**
   * Event fired when a tree row is selected in the tree panel.
   */
  onTreeSelected: function IUI_onTreeSelected()
  {
    if (this.selectEventsSuppressed) {
      return false;
    }

    let node = this.treeView.selectedNode;
    this.highlighter.highlightNode(node);
    this.stopInspecting();
    this.updateStylePanel(node);
    this.updateDOMPanel(node);
    return true;
  },

  /**
   * Attach event listeners to content window and child windows to enable 
   * highlighting and click to stop inspection.
   */
  attachPageListeners: function IUI_attachPageListeners()
  {
    this.win.addEventListener("keypress", this, true);
    this.browser.addEventListener("mousemove", this, true);
    this.browser.addEventListener("click", this, true);
  },

  /**
   * Detach event listeners from content window and child windows
   * to disable highlighting.
   */
  detachPageListeners: function IUI_detachPageListeners()
  {
    this.win.removeEventListener("keypress", this, true);
    this.browser.removeEventListener("mousemove", this, true);
    this.browser.removeEventListener("click", this, true);
  },

  /////////////////////////////////////////////////////////////////////////
  //// Utility Methods

  /**
   * inspect the given node, highlighting it on the page and selecting the
   * correct row in the tree panel
   *
   * @param aNode
   *        the element in the document to inspect
   */
  inspectNode: function IUI_inspectNode(aNode)
  {
    this.highlighter.highlightNode(aNode);
    this.selectEventsSuppressed = true;
    this.treeView.selectedNode = aNode;
    this.selectEventsSuppressed = false;
    this.updateStylePanel(aNode);
    this.updateDOMPanel(aNode);
  },

  /**
   * Find an element from the given coordinates. This method descends through 
   * frames to find the element the user clicked inside frames.
   *
   * @param DOMDocument aDocument the document to look into.
   * @param integer aX
   * @param integer aY
   * @returns Node|null the element node found at the given coordinates.
   */
  elementFromPoint: function IUI_elementFromPoint(aDocument, aX, aY)
  {
    let node = aDocument.elementFromPoint(aX, aY);
    if (node && node.contentDocument) {
      switch (node.nodeName.toLowerCase()) {
        case "iframe":
          let rect = node.getBoundingClientRect();
          aX -= rect.left;
          aY -= rect.top;

        case "frame":
          let subnode = this.elementFromPoint(node.contentDocument, aX, aY);
          if (subnode) {
            node = subnode;
          }
      }
    }
    return node;
  },

  ///////////////////////////////////////////////////////////////////////////
  //// Utility functions

  /**
   * Retrieve the unique ID of a window object.
   *
   * @param nsIDOMWindow aWindow
   * @returns integer ID
   */
  getWindowID: function IUI_getWindowID(aWindow)
  {
    if (!aWindow) {
      return null;
    }

    let util = {};

    try {
      util = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).
        getInterface(Ci.nsIDOMWindowUtils);
    } catch (ex) { }

    return util.currentInnerWindowID;
  },

  /**
   * debug logging facility
   * @param msg
   *        text message to send to the log
   */
  _log: function LOG(msg)
  {
    Services.console.logStringMessage(msg);
  },
}

/**
 * The Inspector store is used for storing data specific to each tab window.
 */
var InspectorStore = {
  store: {},
  length: 0,

  /**
   * Check if there is any data recorded for any tab/window.
   *
   * @returns boolean True if there are no stores for any window/tab, or false
   * otherwise.
   */
  isEmpty: function IS_isEmpty()
  {
    return this.length == 0 ? true : false;
  },

  /**
   * Add a new store.
   *
   * @param string aID The Store ID you want created.
   * @returns boolean True if the store was added successfully, or false
   * otherwise.
   */
  addStore: function IS_addStore(aID)
  {
    let result = false;

    if (!(aID in this.store)) {
      this.store[aID] = {};
      this.length++;
      result = true;
    }

    return result;
  },

  /**
   * Delete a store by ID.
   *
   * @param string aID The store ID you want deleted.
   * @returns boolean True if the store was removed successfully, or false
   * otherwise.
   */
  deleteStore: function IS_deleteStore(aID)
  {
    let result = false;

    if (aID in this.store) {
      delete this.store[aID];
      this.length--;
      result = true;
    }

    return result;
  },

  /**
   * Check store existence.
   *
   * @param string aID The store ID you want to check.
   * @returns boolean True if the store ID is registered, or false otherwise.
   */
  hasID: function IS_hasID(aID)
  {
    return (aID in this.store);
  },

  /**
   * Retrieve a value from a store for a given key.
   *
   * @param string aID The store ID you want to read the value from.
   * @param string aKey The key name of the value you want.
   * @returns mixed the value associated to your store and key.
   */
  getValue: function IS_getValue(aID, aKey)
  {
    return aID in this.store ? this.store[aID][aKey] : null;
  },

  /**
   * Set a value for a given key and store.
   *
   * @param string aID The store ID where you want to store the value into.
   * @param string aKey The key name for which you want to save the value.
   * @param mixed aValue The value you want stored.
   * @returns boolean True if the value was stored successfully, or false
   * otherwise.
   */
  setValue: function IS_setValue(aID, aKey, aValue)
  {
    let result = false;

    if (aID in this.store) {
      this.store[aID][aKey] = aValue;
      result = true;
    }

    return result;
  },

  /**
   * Delete a value for a given key and store.
   *
   * @param string aID The store ID where you want to store the value into.
   * @param string aKey The key name for which you want to save the value.
   * @returns boolean True if the value was removed successfully, or false
   * otherwise.
   */
  deleteValue: function IS_deleteValue(aID, aKey)
  {
    let result = false;

    if (aID in this.store && aKey in this.store[aID]) {
      delete this.store[aID][aKey];
      result = true;
    }

    return result;
  }
};

/////////////////////////////////////////////////////////////////////////
//// Initializors

XPCOMUtils.defineLazyGetter(InspectorUI, "inspectCmd", function () {
  return document.getElementById("Tools:Inspect");
});

XPCOMUtils.defineLazyGetter(InspectorUI, "strings", function () {
  return Services.strings.createBundle("chrome://browser/locale/inspector.properties");
});

XPCOMUtils.defineLazyGetter(InspectorUI, "PropertyTreeView", function () {
  var obj = {};
  Cu.import("resource://gre/modules/PropertyPanel.jsm", obj);
  return obj.PropertyTreeView;
});

XPCOMUtils.defineLazyGetter(InspectorUI, "PropertyPanel", function () {
  var obj = {};
  Cu.import("resource://gre/modules/PropertyPanel.jsm", obj);
  return obj.PropertyPanel;
});

XPCOMUtils.defineLazyGetter(InspectorUI, "style", function () {
  var obj = {};
  Cu.import("resource:///modules/stylePanel.jsm", obj);
  obj.style.initialize();
  return obj.style;
});

