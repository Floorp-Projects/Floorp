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
 *   Paul Rouget <paul@mozilla.com>
 *   Kyle Simpson <ksimpson@mozilla.com>
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

// Inspector notifications dispatched through the nsIObserverService.
const INSPECTOR_NOTIFICATIONS = {
  // Fires once the Inspector highlights an element in the page.
  HIGHLIGHTING: "inspector-highlighting",

  // Fires once the Inspector stops highlighting any element.
  UNHIGHLIGHTING: "inspector-unhighlighting",

  // Fires once the Inspector completes the initialization and opens up on
  // screen.
  OPENED: "inspector-opened",

  // Fires once the Inspector is closed.
  CLOSED: "inspector-closed",

  // Fires when the Tree Panel is opened and initialized.
  TREEPANELREADY: "inspector-treepanel-ready",

  // Event notifications for the attribute-value editor
  EDITOR_OPENED: "inspector-editor-opened",
  EDITOR_CLOSED: "inspector-editor-closed",
  EDITOR_SAVED: "inspector-editor-saved",
};

///////////////////////////////////////////////////////////////////////////
//// Highlighter

/**
 * A highlighter mechanism.
 *
 * The highlighter is built dynamically once the Inspector is invoked:
 * <stack id="highlighter-container">
 *   <vbox id="highlighter-veil-container">...</vbox>
 *   <box id="highlighter-controls>...</vbox>
 * </stack>
 *
 * @param nsIDOMNode aBrowser
 *        The xul:browser object for the content window being highlighted.
 */
function Highlighter(aBrowser)
{
  this._init(aBrowser);
}

Highlighter.prototype = {

  _init: function Highlighter__init(aBrowser)
  {
    this.browser = aBrowser;
    let stack = this.browser.parentNode;
    this.win = this.browser.contentWindow;
    this._highlighting = false;

    this.highlighterContainer = document.createElement("stack");
    this.highlighterContainer.id = "highlighter-container";

    this.veilContainer = document.createElement("vbox");
    this.veilContainer.id = "highlighter-veil-container";

    let controlsBox = document.createElement("box");
    controlsBox.id = "highlighter-controls";

    // The veil will make the whole page darker except
    // for the region of the selected box.
    this.buildVeil(this.veilContainer);

    // The controlsBox will host the different interactive
    // elements of the highlighter (buttons, toolbars, ...).
    this.buildControls(controlsBox);

    this.highlighterContainer.appendChild(this.veilContainer);
    this.highlighterContainer.appendChild(controlsBox);

    stack.appendChild(this.highlighterContainer);

    this.browser.addEventListener("resize", this, true);
    this.browser.addEventListener("scroll", this, true);

    this.handleResize();
  },

  /**
   * Build the veil:
   *
   * <vbox id="highlighter-veil-container">
   *   <box id="highlighter-veil-topbox" class="highlighter-veil"/>
   *   <hbox id="highlighter-veil-middlebox">
   *     <box id="highlighter-veil-leftbox" class="highlighter-veil"/>
   *     <box id="highlighter-veil-transparentbox"/>
   *     <box id="highlighter-veil-rightbox" class="highlighter-veil"/>
   *   </hbox>
   *   <box id="highlighter-veil-bottombox" class="highlighter-veil"/>
   * </vbox>
   *
   * @param nsIDOMNode aParent
   */
  buildVeil: function Highlighter_buildVeil(aParent)
  {

    // We will need to resize these boxes to surround a node.
    // See highlightRectangle().

    this.veilTopBox = document.createElement("box");
    this.veilTopBox.id = "highlighter-veil-topbox";
    this.veilTopBox.className = "highlighter-veil";

    this.veilMiddleBox = document.createElement("hbox");
    this.veilMiddleBox.id = "highlighter-veil-middlebox";

    this.veilLeftBox = document.createElement("box");
    this.veilLeftBox.id = "highlighter-veil-leftbox";
    this.veilLeftBox.className = "highlighter-veil";

    this.veilTransparentBox = document.createElement("box");
    this.veilTransparentBox.id = "highlighter-veil-transparentbox";

    // We don't need any references to veilRightBox and veilBottomBox.
    // These boxes are automatically resized (flex=1)

    let veilRightBox = document.createElement("box");
    veilRightBox.id = "highlighter-veil-rightbox";
    veilRightBox.className = "highlighter-veil";

    let veilBottomBox = document.createElement("box");
    veilBottomBox.id = "highlighter-veil-bottombox";
    veilBottomBox.className = "highlighter-veil";

    this.veilMiddleBox.appendChild(this.veilLeftBox);
    this.veilMiddleBox.appendChild(this.veilTransparentBox);
    this.veilMiddleBox.appendChild(veilRightBox);

    aParent.appendChild(this.veilTopBox);
    aParent.appendChild(this.veilMiddleBox);
    aParent.appendChild(veilBottomBox);
  },

  /**
   * Build the controls:
   *
   * <box id="highlighter-close-button"/>
   *
   * @param nsIDOMNode aParent
   */
  buildControls: function Highlighter_buildControls(aParent)
  {
    let closeButton = document.createElement("box");
    closeButton.id = "highlighter-close-button";
    closeButton.appendChild(document.createElement("image"));

    closeButton.setAttribute("onclick", "InspectorUI.closeInspectorUI(false);");

    aParent.appendChild(closeButton);
  },

  /**
   * Destroy the nodes.
   */
  destroy: function Highlighter_destroy()
  {
    this.browser.removeEventListener("scroll", this, true);
    this.browser.removeEventListener("resize", this, true);
    this._highlightRect = null;
    this._highlighting = false;
    this.veilTopBox = null;
    this.veilLeftBox = null;
    this.veilMiddleBox = null;
    this.veilTransparentBox = null;
    this.veilContainer = null;
    this.node = null;
    this.highlighterContainer.parentNode.removeChild(this.highlighterContainer);
    this.highlighterContainer = null;
    this.win = null
    this.browser = null;
    this.toolbar = null;
  },

  /**
   * Is the highlighter highlighting? Public method for querying the state
   * of the highlighter.
   */
  get isHighlighting() {
    return this._highlighting;
  },

  /**
   * Highlight this.node, unhilighting first if necessary.
   *
   * @param boolean aScroll
   *        Boolean determining whether to scroll or not.
   */
  highlight: function Highlighter_highlight(aScroll)
  {
    // node is not set or node is not highlightable, bail
    if (!this.node || !this.isNodeHighlightable()) {
      return;
    }

    if (aScroll) {
      this.node.scrollIntoView();
    }

    let clientRect = this.node.getBoundingClientRect();

    // Go up in the tree of frames to determine the correct rectangle.
    // clientRect is read-only, we need to be able to change properties.
    let rect = {top: clientRect.top,
                left: clientRect.left,
                width: clientRect.width,
                height: clientRect.height};

    let frameWin = this.node.ownerDocument.defaultView;

    // We iterate through all the parent windows.
    while (true) {

      // Does the selection overflow on the right of its window?
      let diffx = frameWin.innerWidth - (rect.left + rect.width);
      if (diffx < 0) {
        rect.width += diffx;
      }

      // Does the selection overflow on the bottom of its window?
      let diffy = frameWin.innerHeight - (rect.top + rect.height);
      if (diffy < 0) {
        rect.height += diffy;
      }

      // Does the selection overflow on the left of its window?
      if (rect.left < 0) {
        rect.width += rect.left;
        rect.left = 0;
      }

      // Does the selection overflow on the top of its window?
      if (rect.top < 0) {
        rect.height += rect.top;
        rect.top = 0;
      }

      // Selection has been clipped to fit in its own window.

      // Are we in the top-level window?
      if (frameWin.parent === frameWin || !frameWin.frameElement) {
        break;
      }

      // We are in an iframe.
      // We take into account the parent iframe position and its
      // offset (borders and padding).
      let frameRect = frameWin.frameElement.getBoundingClientRect();

      let [offsetTop, offsetLeft] =
        InspectorUI.getIframeContentOffset(frameWin.frameElement);

      rect.top += frameRect.top + offsetTop;
      rect.left += frameRect.left + offsetLeft;

      frameWin = frameWin.parent;
    }

    this.highlightRectangle(rect);

    if (this._highlighting) {
      Services.obs.notifyObservers(null,
        INSPECTOR_NOTIFICATIONS.HIGHLIGHTING, null);
    }
  },

  /**
   * Highlight the given node.
   *
   * @param nsIDOMNode aNode
   *        a DOM element to be highlighted
   * @param object aParams
   *        extra parameters object
   */
  highlightNode: function Highlighter_highlightNode(aNode, aParams)
  {
    this.node = aNode;
    this.highlight(aParams && aParams.scroll);
  },

  /**
   * Highlight a rectangular region.
   *
   * @param object aRect
   *        The rectangle region to highlight.
   * @returns boolean
   *          True if the rectangle was highlighted, false otherwise.
   */
  highlightRectangle: function Highlighter_highlightRectangle(aRect)
  {
    let oldRect = this._highlightRect;

    if (oldRect && aRect.top == oldRect.top && aRect.left == oldRect.left &&
        aRect.width == oldRect.width && aRect.height == oldRect.height) {
      return this._highlighting; // same rectangle
    }

    if (aRect.left >= 0 && aRect.top >= 0 &&
        aRect.width > 0 && aRect.height > 0) {
      // The bottom div and the right div are flexibles (flex=1).
      // We don't need to resize them.
      this.veilTopBox.style.height = aRect.top + "px";
      this.veilLeftBox.style.width = aRect.left + "px";
      this.veilMiddleBox.style.height = aRect.height + "px";
      this.veilTransparentBox.style.width = aRect.width + "px";

      this._highlighting = true;
    } else {
      this.unhighlight();
    }

    this._highlightRect = aRect;

    return this._highlighting;
  },

  /**
   * Clear the highlighter surface.
   */
  unhighlight: function Highlighter_unhighlight()
  {
    this._highlighting = false;
    this.veilMiddleBox.style.height = 0;
    this.veilTransparentBox.style.width = 0;
    Services.obs.notifyObservers(null,
      INSPECTOR_NOTIFICATIONS.UNHIGHLIGHTING, null);
  },

  /**
   * Return the midpoint of a line from pointA to pointB.
   *
   * @param object aPointA
   *        An object with x and y properties.
   * @param object aPointB
   *        An object with x and y properties.
   * @returns object
   *          An object with x and y properties.
   */
  midPoint: function Highlighter_midPoint(aPointA, aPointB)
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
   * @returns nsIDOMNode|null
   *          Returns the node under the current highlighter rectangle. Null is
   *          returned if there is no node highlighted.
   */
  get highlitNode()
  {
    // Not highlighting? Bail.
    if (!this._highlighting || !this._highlightRect) {
      return null;
    }

    let a = {
      x: this._highlightRect.left,
      y: this._highlightRect.top
    };

    let b = {
      x: a.x + this._highlightRect.width,
      y: a.y + this._highlightRect.height
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
   *          True if the node is highlightable or false otherwise.
   */
  isNodeHighlightable: function Highlighter_isNodeHighlightable()
  {
    if (!this.node || this.node.nodeType != Node.ELEMENT_NODE) {
      return false;
    }
    let nodeName = this.node.nodeName.toLowerCase();
    return !INSPECTOR_INVISIBLE_ELEMENTS[nodeName];
  },

  /////////////////////////////////////////////////////////////////////////
  //// Event Handling

  attachInspectListeners: function Highlighter_attachInspectListeners()
  {
    this.browser.addEventListener("mousemove", this, true);
    this.browser.addEventListener("click", this, true);
    this.browser.addEventListener("dblclick", this, true);
    this.browser.addEventListener("mousedown", this, true);
    this.browser.addEventListener("mouseup", this, true);
  },

  detachInspectListeners: function Highlighter_detachInspectListeners()
  {
    this.browser.removeEventListener("mousemove", this, true);
    this.browser.removeEventListener("click", this, true);
    this.browser.removeEventListener("dblclick", this, true);
    this.browser.removeEventListener("mousedown", this, true);
    this.browser.removeEventListener("mouseup", this, true);
  },


  /**
   * Generic event handler.
   *
   * @param nsIDOMEvent aEvent
   *        The DOM event object.
   */
  handleEvent: function Highlighter_handleEvent(aEvent)
  {
    switch (aEvent.type) {
      case "click":
        this.handleClick(aEvent);
        break;
      case "mousemove":
        this.handleMouseMove(aEvent);
        break;
      case "resize":
        this.handleResize(aEvent);
        break;
      case "dblclick":
      case "mousedown":
      case "mouseup":
        aEvent.stopPropagation();
        aEvent.preventDefault();
        break;
      case "scroll":
        this.highlight();
        break;
    }
  },

  /**
   * Handle clicks.
   *
   * @param nsIDOMEvent aEvent
   *        The DOM event.
   */
  handleClick: function Highlighter_handleClick(aEvent)
  {
    // Stop inspection when the user clicks on a node.
    if (aEvent.button == 0) {
      let win = aEvent.target.ownerDocument.defaultView;
      InspectorUI.stopInspecting();
      win.focus();
    }
    aEvent.preventDefault();
    aEvent.stopPropagation();
  },

  /**
   * Handle mousemoves in panel when InspectorUI.inspecting is true.
   *
   * @param nsiDOMEvent aEvent
   *        The MouseEvent triggering the method.
   */
  handleMouseMove: function Highlighter_handleMouseMove(aEvent)
  {
    let element = InspectorUI.elementFromPoint(aEvent.target.ownerDocument,
      aEvent.clientX, aEvent.clientY);
    if (element && element != this.node) {
      InspectorUI.inspectNode(element);
    }
  },

  /**
   * Handle window resize events.
   */
  handleResize: function Highlighter_handleResize()
  {
    this.highlight();
  },
};

///////////////////////////////////////////////////////////////////////////
//// InspectorUI

/**
 * Main controller class for the Inspector.
 */
var InspectorUI = {
  browser: null,
  tools: {},
  toolEvents: {},
  inspecting: false,
  treePanelEnabled: true,
  get enabled()
  {
    return gPrefService.getBoolPref("devtools.inspector.enabled");
  },
  isDirty: false,

  /**
   * Toggle the inspector interface elements on or off.
   *
   * @param aEvent
   *        The event that requested the UI change. Toolbar button or menu.
   */
  toggleInspectorUI: function IUI_toggleInspectorUI(aEvent)
  {
    if (this.isInspectorOpen) {
      this.closeInspectorUI();
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
   * Is the inspector UI open? Simply check if the toolbar is visible or not.
   *
   * @returns boolean
   */
  get isInspectorOpen()
  {
    return this.toolbar && !this.toolbar.hidden;
  },

  /**
   * Return the default selection element for the inspected document.
   */
  get defaultSelection()
  {
    let doc = this.win.document;
    return doc.documentElement ? doc.documentElement.lastElementChild : null;
  },

  /**
   * Open inspector UI and HTML tree. Add listeners for document scrolling,
   * resize, tabContainer.TabSelect and others. If a node is provided, then
   * start inspecting it.
   *
   * @param [optional] aNode
   *        The node to inspect.
   */
  openInspectorUI: function IUI_openInspectorUI(aNode)
  {
    // InspectorUI is already up and running. Lock a node if asked (via context).
    if (this.highlighter && aNode) {
      this.inspectNode(aNode);
      this.stopInspecting();
      return;
    }
    // Observer used to inspect the specified element from content after the
    // inspector UI has been opened.
    function inspectObserver(aElement) {
      Services.obs.removeObserver(boundInspectObserver,
                                  INSPECTOR_NOTIFICATIONS.OPENED,
                                  false);
      this.inspectNode(aElement);
      this.stopInspecting();
    };
    var boundInspectObserver = inspectObserver.bind(this, aNode);

    if (aNode) {
      // Add the observer to inspect the node after initialization finishes.
      Services.obs.addObserver(boundInspectObserver,
                               INSPECTOR_NOTIFICATIONS.OPENED,
                               false);
    }
    // Start initialization.
    this.browser = gBrowser.selectedBrowser;
    this.win = this.browser.contentWindow;
    this.winID = this.getWindowID(this.win);
    this.toolbar = document.getElementById("inspector-toolbar");

    this.initTools();

    if (!this.TreePanel && this.treePanelEnabled) {
      Cu.import("resource:///modules/TreePanel.jsm", this);
      this.treePanel = new this.TreePanel(window, this);
    }

    this.toolbar.hidden = false;
    this.inspectMenuitem.setAttribute("checked", true);

    this.isDirty = false;

    gBrowser.addProgressListener(InspectorProgressListener);

    // initialize the highlighter
    this.initializeHighlighter();
  },

  /**
   * Register and initialize any included tools.
   */
  initTools: function IUI_initTools()
  {
    // Style inspector
    if (Services.prefs.getBoolPref("devtools.styleinspector.enabled") &&
        !this.toolRegistered("styleinspector")) {
      let stylePanel = this.StyleInspector.createPanel(true);
      this.registerTool({
        id: "styleinspector",
        label: InspectorUI.StyleInspector.l10n("style.highlighter.button.label"),
        tooltiptext: InspectorUI.StyleInspector.l10n("style.highlighter.button.tooltip"),
        accesskey: InspectorUI.StyleInspector.l10n("style.highlighter.accesskey"),
        context: stylePanel,
        get isOpen() stylePanel.isOpen(),
        onSelect: stylePanel.selectNode,
        show: stylePanel.showTool,
        hide: stylePanel.hideTool,
        dim: stylePanel.dimTool,
        panel: stylePanel,
        unregister: stylePanel.destroy,
      });
      this.stylePanel = stylePanel;
    }
  },

  /**
   * Initialize highlighter.
   */
  initializeHighlighter: function IUI_initializeHighlighter()
  {
    this.highlighter = new Highlighter(this.browser);
    this.highlighterReady();
  },

  /**
   * Initialize the InspectorStore.
   */
  initializeStore: function IUI_initializeStore()
  {
    // First time opened, add the TabSelect listener
    if (InspectorStore.isEmpty())
      gBrowser.tabContainer.addEventListener("TabSelect", this, false);

    // Has this windowID been inspected before?
    if (InspectorStore.hasID(this.winID)) {
      let selectedNode = InspectorStore.getValue(this.winID, "selectedNode");
      if (selectedNode) {
        this.inspectNode(selectedNode);
      }
      this.isDirty = InspectorStore.getValue(this.winID, "isDirty");
    } else {
      // First time inspecting, set state to no selection + live inspection.
      InspectorStore.addStore(this.winID);
      InspectorStore.setValue(this.winID, "selectedNode", null);
      InspectorStore.setValue(this.winID, "inspecting", true);
      InspectorStore.setValue(this.winID, "isDirty", this.isDirty);
      this.win.addEventListener("pagehide", this, true);
    }
  },

  /**
   * Close inspector UI and associated panels. Unhighlight and stop inspecting.
   * Remove event listeners for document scrolling, resize,
   * tabContainer.TabSelect and others.
   *
   * @param boolean aKeepStore
   *        Tells if you want the store associated to the current tab/window to
   *        be cleared or not. Set this to true to not clear the store, or false
   *        otherwise.
   */
  closeInspectorUI: function IUI_closeInspectorUI(aKeepStore)
  {
    // if currently editing an attribute value, closing the
    // highlighter/HTML panel dismisses the editor
    if (this.treePanel && this.treePanel.editingContext)
      this.treePanel.closeEditor();

    if (this.closing || !this.win || !this.browser) {
      return;
    }

    this.closing = true;
    this.toolbar.hidden = true;

    gBrowser.removeProgressListener(InspectorProgressListener);

    if (!aKeepStore) {
      InspectorStore.deleteStore(this.winID);
      this.win.removeEventListener("pagehide", this, true);
    } else {
      // Update the store before closing.
      if (this.selection) {
        InspectorStore.setValue(this.winID, "selectedNode",
          this.selection);
      }
      InspectorStore.setValue(this.winID, "inspecting", this.inspecting);
      InspectorStore.setValue(this.winID, "isDirty", this.isDirty);
    }

    if (InspectorStore.isEmpty()) {
      gBrowser.tabContainer.removeEventListener("TabSelect", this, false);
    }

    this.stopInspecting();

    this.saveToolState(this.winID);
    this.toolsDo(function IUI_toolsHide(aTool) {
      this.unregisterTool(aTool);
    }.bind(this));

    if (this.highlighter) {
      this.highlighter.destroy();
      this.highlighter = null;
    }

    this.inspectMenuitem.setAttribute("checked", false);
    this.browser = this.win = null; // null out references to browser and window
    this.winID = null;
    this.selection = null;
    this.closing = false;
    this.isDirty = false;

    delete this.treePanel;
    delete this.stylePanel;
    delete this.toolbar;
    delete this.TreePanel;
    Services.obs.notifyObservers(null, INSPECTOR_NOTIFICATIONS.CLOSED, null);
  },

  /**
   * Begin inspecting webpage, attach page event listeners, activate
   * highlighter event listeners.
   */
  startInspecting: function IUI_startInspecting()
  {
    // if currently editing an attribute value, starting
    // "live inspection" mode closes the editor
    if (this.treePanel && this.treePanel.editingContext)
      this.treePanel.closeEditor();

    this.inspectToolbutton.checked = true;
    this.attachPageListeners();
    this.inspecting = true;
    this.toolsDim(true);
    this.highlighter.veilContainer.removeAttribute("locked");
  },

  /**
   * Stop inspecting webpage, detach page listeners, disable highlighter
   * event listeners.
   * @param aPreventScroll
   *        Prevent scroll in the HTML tree?
   */
  stopInspecting: function IUI_stopInspecting(aPreventScroll)
  {
    if (!this.inspecting) {
      return;
    }

    this.inspectToolbutton.checked = false;
    this.detachPageListeners();
    this.inspecting = false;
    this.toolsDim(false);
    if (this.highlighter.node) {
      this.select(this.highlighter.node, true, true, !aPreventScroll);
    } else {
      this.select(null, true, true);
    }
    this.highlighter.veilContainer.setAttribute("locked", true);
  },

  /**
   * Select an object in the tree view.
   * @param aNode
   *        node to inspect
   * @param forceUpdate
   *        force an update?
   * @param aScroll boolean
   *        scroll the tree panel?
   */
  select: function IUI_select(aNode, forceUpdate, aScroll)
  {
    // if currently editing an attribute value, using the
    // highlighter dismisses the editor
    if (this.treePanel && this.treePanel.editingContext)
      this.treePanel.closeEditor();

    if (!aNode)
      aNode = this.defaultSelection;

    if (forceUpdate || aNode != this.selection) {
      this.selection = aNode;
      if (!this.inspecting) {
        this.highlighter.highlightNode(this.selection);
      }
    }

    this.toolsSelect(aScroll);
  },

  /////////////////////////////////////////////////////////////////////////
  //// Event Handling

  highlighterReady: function IUI_highlighterReady()
  {
    // Setup the InspectorStore or restore state
    this.initializeStore();

    if (InspectorStore.getValue(this.winID, "inspecting")) {
      this.startInspecting();
    }

    this.restoreToolState(this.winID);

    this.win.focus();
    Services.obs.notifyObservers(null, INSPECTOR_NOTIFICATIONS.OPENED, null);
  },

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
    let inspectorClosed = false;

    switch (event.type) {
      case "TabSelect":
        winID = this.getWindowID(gBrowser.selectedBrowser.contentWindow);
        if (this.isInspectorOpen && winID != this.winID) {
          this.closeInspectorUI(true);
          inspectorClosed = true;
        }

        if (winID && InspectorStore.hasID(winID)) {
          if (inspectorClosed && this.closing) {
            Services.obs.addObserver(function reopenInspectorForTab() {
              Services.obs.removeObserver(reopenInspectorForTab,
                INSPECTOR_NOTIFICATIONS.CLOSED, false);

              InspectorUI.openInspectorUI();
            }, INSPECTOR_NOTIFICATIONS.CLOSED, false);
          } else {
            this.openInspectorUI();
          }
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
            if (this.inspecting) {
              this.stopInspecting();
              event.preventDefault();
              event.stopPropagation();
            }
            break;
        }
        break;
    }
  },

  /**
   * Attach event listeners to content window and child windows to enable
   * highlighting and click to stop inspection.
   */
  attachPageListeners: function IUI_attachPageListeners()
  {
    this.browser.addEventListener("keypress", this, true);
    this.highlighter.attachInspectListeners();
  },

  /**
   * Detach event listeners from content window and child windows
   * to disable highlighting.
   */
  detachPageListeners: function IUI_detachPageListeners()
  {
    this.browser.removeEventListener("keypress", this, true);
    this.highlighter.detachInspectListeners();
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
    this.select(aNode, true, true);
    this.highlighter.highlightNode(aNode);
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
      if (node instanceof HTMLIFrameElement) {
        let rect = node.getBoundingClientRect();

        // Gap between the iframe and its content window.
        let [offsetTop, offsetLeft] = this.getIframeContentOffset(node);

        aX -= rect.left + offsetLeft;
        aY -= rect.top + offsetTop;

        if (aX < 0 || aY < 0) {
          // Didn't reach the content document, still over the iframe.
          return node;
        }
      }
      if (node instanceof HTMLIFrameElement ||
          node instanceof HTMLFrameElement) {
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
   * Returns iframe content offset (iframe border + padding).
   * Note: this function shouldn't need to exist, had the platform provided a
   * suitable API for determining the offset between the iframe's content and
   * its bounding client rect. Bug 626359 should provide us with such an API.
   *
   * @param aIframe
   *        The iframe.
   * @returns array [offsetTop, offsetLeft]
   *          offsetTop is the distance from the top of the iframe and the
   *            top of the content document.
   *          offsetLeft is the distance from the left of the iframe and the
   *            left of the content document.
   */
  getIframeContentOffset: function IUI_getIframeContentOffset(aIframe)
  {
    let style = aIframe.contentWindow.getComputedStyle(aIframe, null);

    let paddingTop = parseInt(style.getPropertyValue("padding-top"));
    let paddingLeft = parseInt(style.getPropertyValue("padding-left"));

    let borderTop = parseInt(style.getPropertyValue("border-top-width"));
    let borderLeft = parseInt(style.getPropertyValue("border-left-width"));

    return [borderTop + paddingTop, borderLeft + paddingLeft];
  },

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
   * @param msg
   *        text message to send to the log
   */
  _log: function LOG(msg)
  {
    Services.console.logStringMessage(msg);
  },

  /**
   * Debugging function.
   * @param msg
   *        text to show with the stack trace.
   */
  _trace: function TRACE(msg)
  {
    this._log("TRACE: " + msg);
    let frame = Components.stack.caller;
    while (frame = frame.caller) {
      if (frame.language == Ci.nsIProgrammingLanguage.JAVASCRIPT ||
          frame.language == Ci.nsIProgrammingLanguage.JAVASCRIPT2) {
        this._log("filename: " + frame.filename + " lineNumber: " + frame.lineNumber +
          " functionName: " + frame.name);
      }
    }
    this._log("END TRACE");
  },

  /**
   * Get the toolbar button name for a given id string. Used by the
   * registerTools API to retrieve a consistent name for toolbar buttons
   * based on the ID of the tool.
   * @param anId String
   *        id of the tool to be buttonized
   * @returns String
   */
  getToolbarButtonId: function IUI_createButtonId(anId)
  {
    return "inspector-" + anId + "-toolbutton";
  },

  /**
   * Register an external tool with the inspector.
   *
   * aRegObj = {
   *   id: "toolname",
   *   context: myTool,
   *   label: "Button label",
   *   icon: "chrome://somepath.png",
   *   tooltiptext: "Button tooltip",
   *   accesskey: "S",
   *   isOpen: object.property, (getter) returning true if tool is open.
   *   onSelect: object.method,
   *   show: object.method, called to show the tool when button is pressed.
   *   hide: object.method, called to hide the tool when button is pressed.
   *   dim: object.method, called to disable a tool during highlighting.
   *   unregister: object.method, called when tool should be destroyed.
   *   panel: myTool.panel
   * }
   *
   * @param aRegObj Object
   *        The Registration Object used to register this tool described
   *        above. The tool should cache this object for later deregistration.
   */
  registerTool: function IUI_registerTool(aRegObj)
  {
    if (this.toolRegistered(aRegObj.id)) {
      return;
    }

    this.tools[aRegObj.id] = aRegObj;

    let buttonContainer = document.getElementById("inspector-tools");
    let btn = document.createElement("toolbarbutton");
    let buttonId = this.getToolbarButtonId(aRegObj.id);
    btn.setAttribute("id", buttonId);
    btn.setAttribute("label", aRegObj.label);
    btn.setAttribute("tooltiptext", aRegObj.tooltiptext);
    btn.setAttribute("accesskey", aRegObj.accesskey);
    btn.setAttribute("image", aRegObj.icon || "");
    buttonContainer.appendChild(btn);

    /**
     * Save the registered tool's toolbar button's click handler so we can remove
     * it at deregistration time.
     * @param aButton XUL:toolbarbutton
     * @param aCallback Function the click event handler for the button
     */
    function bindToolEvent(aWidget, aEvent, aCallback)
    {
      let toolEvent = aWidget.id + "_" + aEvent;
      InspectorUI.toolEvents[toolEvent] = aCallback;
      aWidget.addEventListener(aEvent, aCallback, false);
    }

    bindToolEvent(btn, "click",
      function IUI_toolButtonClick(aEvent) {
        if (btn.checked) {
          this.toolHide(aRegObj);
        } else {
          this.toolShow(aRegObj);
        }
      }.bind(this));

    if (aRegObj.panel) {
      bindToolEvent(aRegObj.panel, "popuphiding",
        function IUI_toolPanelHiding() {
          btn.checked = false;
        });
    }
  },

  /**
   * Show the specified tool.
   * @param aTool Object (see comment for IUI_registerTool)
   */
  toolShow: function IUI_toolShow(aTool)
  {
    aTool.show.call(aTool.context, this.selection);
    document.getElementById(this.getToolbarButtonId(aTool.id)).checked = true;
  },

  /**
   * Hide the specified tool.
   * @param aTool Object (see comment for IUI_registerTool)
   */
  toolHide: function IUI_toolHide(aTool)
  {
    aTool.hide.call(aTool.context);
    document.getElementById(this.getToolbarButtonId(aTool.id)).checked = false;
  },

  /**
   * Unregister the registered tool, unbinding click events for the buttons
   * and showing and hiding events for the panel.
   * @param aRegObj Object
   *        The registration object used to register the tool.
   */
  unregisterTool: function IUI_unregisterTool(aRegObj)
  {
    let button = document.getElementById(this.getToolbarButtonId(aRegObj.id));

    /**
     * Unregister the click handler for the registered tool's button.
     * @param aButton XUL:toolbarbutton
     */
    function unbindToolEvent(aWidget, aEvent)
    {
      let toolEvent = aWidget.id + "_" + aEvent;
      if (!InspectorUI.toolEvents[toolEvent]) {
        return;
      }

      aWidget.removeEventListener(aEvent, InspectorUI.toolEvents[toolEvent], false);
      delete InspectorUI.toolEvents[toolEvent]
    }

    let buttonContainer = document.getElementById("inspector-tools");
    unbindToolEvent(button, "click");

    if (aRegObj.panel)
      unbindToolEvent(aRegObj.panel, "popuphiding");

    buttonContainer.removeChild(button);

    if (aRegObj.unregister)
      aRegObj.unregister.call(aRegObj.context);

    delete this.tools[aRegObj.id];
  },

  /**
   * Save a list of open tools to the inspector store.
   *
   * @param aWinID The ID of the window used to save the associated tools
   */
  saveToolState: function IUI_saveToolState(aWinID)
  {
    let openTools = {};
    this.toolsDo(function IUI_toolsSetId(aTool) {
      if (aTool.isOpen) {
        openTools[aTool.id] = true;
      }
    });
    InspectorStore.setValue(aWinID, "openTools", openTools);
  },

  /**
   * Restore tools previously save using saveToolState().
   *
   * @param aWinID The ID of the window to which the associated tools are to be
   *               restored.
   */
  restoreToolState: function IUI_restoreToolState(aWinID)
  {
    let openTools = InspectorStore.getValue(aWinID, "openTools");
    if (openTools) {
      this.toolsDo(function IUI_toolsOnShow(aTool) {
        if (aTool.id in openTools) {
          this.toolShow(aTool);
        }
      }.bind(this));
    }
  },

  /**
   * For each tool in the tools collection select the current node that is
   * selected in the highlighter
   * @param aScroll boolean
   *        Do you want to scroll the treepanel?
   */
  toolsSelect: function IUI_toolsSelect(aScroll)
  {
    this.toolsDo(function IUI_toolsOnSelect(aTool) {
      if (aTool.isOpen) {
        aTool.onSelect.call(aTool.context, InspectorUI.selection, aScroll);
      }
    });
  },

  /**
   * Dim or undim each tool in the tools collection
   * @param aState true = dim, false = undim
   */
  toolsDim: function IUI_toolsDim(aState)
  {
    this.toolsDo(function IUI_toolsOnSelect(aTool) {
      if (aTool.isOpen && "dim" in aTool) {
        aTool.dim.call(aTool.context, aState);
      }
    });
  },

  /**
   * Loop through all registered tools and pass each into the provided function
   * @param aFunction The function to which each tool is to be passed
   */
  toolsDo: function IUI_toolsDo(aFunction)
  {
    for each (let tool in this.tools) {
      aFunction(tool);
    }
  },

  /**
   * Check if a tool is registered?
   * @param aId The id of the tool to check
   */
  toolRegistered: function IUI_toolRegistered(aId)
  {
    return aId in this.tools;
  },
};

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
    if (!this.hasID(aID))
      return null;
    if (aKey in this.store[aID])
      return this.store[aID][aKey];
    return null;
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

/**
 * The InspectorProgressListener object is an nsIWebProgressListener which
 * handles onStateChange events for the inspected browser. If the user makes
 * changes to the web page and he tries to navigate away, he is prompted to
 * confirm page navigation, such that he's given the chance to prevent the loss
 * of edits.
 */
var InspectorProgressListener = {
  onStateChange:
  function IPL_onStateChange(aProgress, aRequest, aFlag, aStatus)
  {
    // Remove myself if the Inspector is no longer open.
    if (!InspectorUI.isInspectorOpen) {
      gBrowser.removeProgressListener(InspectorProgressListener);
      return;
    }

    // Skip non-start states.
    if (!(aFlag & Ci.nsIWebProgressListener.STATE_START)) {
      return;
    }

    // If the request is about to happen in a new window, we are not concerned
    // about the request.
    if (aProgress.DOMWindow != InspectorUI.win) {
      return;
    }

    if (InspectorUI.isDirty) {
      this.showNotification(aRequest);
    } else {
      InspectorUI.closeInspectorUI();
    }
  },

  /**
   * Show an asynchronous notification which asks the user to confirm or cancel
   * the page navigation request.
   *
   * @param nsIRequest aRequest
   *        The request initiated by the user or by the page itself.
   * @returns void
   */
  showNotification: function IPL_showNotification(aRequest)
  {
    aRequest.suspend();

    let notificationBox = gBrowser.getNotificationBox(InspectorUI.browser);
    let notification = notificationBox.
      getNotificationWithValue("inspector-page-navigation");

    if (notification) {
      notificationBox.removeNotification(notification, true);
    }

    let cancelRequest = function onCancelRequest() {
      if (aRequest) {
        aRequest.cancel(Cr.NS_BINDING_ABORTED);
        aRequest.resume(); // needed to allow the connection to be cancelled.
        aRequest = null;
      }
    };

    let eventCallback = function onNotificationCallback(aEvent) {
      if (aEvent == "removed") {
        cancelRequest();
      }
    };

    let buttons = [
      {
        id: "inspector.confirmNavigationAway.buttonLeave",
        label: InspectorUI.strings.
          GetStringFromName("confirmNavigationAway.buttonLeave"),
        accessKey: InspectorUI.strings.
          GetStringFromName("confirmNavigationAway.buttonLeaveAccesskey"),
        callback: function onButtonLeave() {
          if (aRequest) {
            aRequest.resume();
            aRequest = null;
            InspectorUI.closeInspectorUI();
          }
        },
      },
      {
        id: "inspector.confirmNavigationAway.buttonStay",
        label: InspectorUI.strings.
          GetStringFromName("confirmNavigationAway.buttonStay"),
        accessKey: InspectorUI.strings.
          GetStringFromName("confirmNavigationAway.buttonStayAccesskey"),
        callback: cancelRequest
      },
    ];

    let message = InspectorUI.strings.
      GetStringFromName("confirmNavigationAway.message");

    notification = notificationBox.appendNotification(message,
      "inspector-page-navigation", "chrome://browser/skin/Info.png",
      notificationBox.PRIORITY_WARNING_HIGH, buttons, eventCallback);

    // Make sure this not a transient notification, to avoid the automatic
    // transient notification removal.
    notification.persistence = -1;
  },
};

/////////////////////////////////////////////////////////////////////////
//// Initializers

XPCOMUtils.defineLazyGetter(InspectorUI, "inspectMenuitem", function () {
  return document.getElementById("Tools:Inspect");
});

XPCOMUtils.defineLazyGetter(InspectorUI, "inspectToolbutton", function () {
  return document.getElementById("inspector-inspect-toolbutton");
});

XPCOMUtils.defineLazyGetter(InspectorUI, "strings", function () {
  return Services.strings.
         createBundle("chrome://browser/locale/inspector.properties");
});

XPCOMUtils.defineLazyGetter(InspectorUI, "StyleInspector", function () {
  var obj = {};
  Cu.import("resource:///modules/devtools/StyleInspector.jsm", obj);
  return obj.StyleInspector;
});

