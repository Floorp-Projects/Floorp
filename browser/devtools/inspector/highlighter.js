/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cu, Cc, Ci} = require("chrome");

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/LayoutHelpers.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let EventEmitter = require("devtools/shared/event-emitter");

const PSEUDO_CLASSES = [":hover", ":active", ":focus"];
  // add ":visited" and ":link" after bug 713106 is fixed

/**
 * A highlighter mechanism.
 *
 * The highlighter is built dynamically into the browser element.
 * The caller is in charge of destroying the highlighter (ie, the highlighter
 * won't be destroyed if a new tab is selected for example).
 *
 * API:
 *
 *   // Constructor and destructor.
 *   Highlighter(aTab, aInspector)
 *   void destroy();
 *
 *   // Show and hide the highlighter
 *   void show();
 *   void hide();
 *   boolean isHidden();
 *
 *   // Redraw the highlighter if the visible portion of the node has changed.
 *   void invalidateSize(aScroll);
 *
 * Events:
 *
 *   "closed" - Highlighter is closing
 *   "highlighting" - Highlighter is highlighting
 *   "locked" - The selected node has been locked
 *   "unlocked" - The selected ndoe has been unlocked
 *
 * Structure:
 *  <stack class="highlighter-container">
 *    <box class="highlighter-outline-container">
 *      <box class="highlighter-outline" locked="true/false"/>
 *    </box>
 *    <box class="highlighter-controls">
 *      <box class="highlighter-nodeinfobar-container" position="top/bottom" locked="true/false">
 *        <box class="highlighter-nodeinfobar-arrow highlighter-nodeinfobar-arrow-top"/>
 *        <hbox class="highlighter-nodeinfobar">
 *          <toolbarbutton class="highlighter-nodeinfobar-inspectbutton highlighter-nodeinfobar-button"/>
 *          <hbox class="highlighter-nodeinfobar-text">tagname#id.class1.class2</hbox>
 *          <toolbarbutton class="highlighter-nodeinfobar-menu highlighter-nodeinfobar-button">…</toolbarbutton>
 *        </hbox>
 *        <box class="highlighter-nodeinfobar-arrow highlighter-nodeinfobar-arrow-bottom"/>
 *      </box>
 *    </box>
 *  </stack>
 *
 */


/**
 * Constructor.
 *
 * @param aTarget The inspection target.
 * @param aInspector Inspector panel.
 * @param aToolbox The toolbox holding the inspector.
 */
function Highlighter(aTarget, aInspector, aToolbox)
{
  this.target = aTarget;
  this.tab = aTarget.tab;
  this.toolbox = aToolbox;
  this.browser = this.tab.linkedBrowser;
  this.chromeDoc = this.tab.ownerDocument;
  this.chromeWin = this.chromeDoc.defaultView;
  this.inspector = aInspector

  EventEmitter.decorate(this);

  this._init();
}

exports.Highlighter = Highlighter;

Highlighter.prototype = {
  get selection() {
    return this.inspector.selection;
  },

  _init: function Highlighter__init()
  {
    this.unlockAndFocus = this.unlockAndFocus.bind(this);
    this.updateInfobar = this.updateInfobar.bind(this);
    this.highlight = this.highlight.bind(this);

    let stack = this.browser.parentNode;
    this.win = this.browser.contentWindow;
    this._highlighting = false;

    this.highlighterContainer = this.chromeDoc.createElement("stack");
    this.highlighterContainer.className = "highlighter-container";

    this.outline = this.chromeDoc.createElement("box");
    this.outline.className = "highlighter-outline";

    let outlineContainer = this.chromeDoc.createElement("box");
    outlineContainer.appendChild(this.outline);
    outlineContainer.className = "highlighter-outline-container";

    // The controlsBox will host the different interactive
    // elements of the highlighter (buttons, toolbars, ...).
    let controlsBox = this.chromeDoc.createElement("box");
    controlsBox.className = "highlighter-controls";
    this.highlighterContainer.appendChild(outlineContainer);
    this.highlighterContainer.appendChild(controlsBox);

    // Insert the highlighter right after the browser
    stack.insertBefore(this.highlighterContainer, stack.childNodes[1]);

    this.buildInfobar(controlsBox);

    this.transitionDisabler = null;
    this.pageEventsMuter = null;

    this.unlockAndFocus();

    this.selection.on("new-node", this.highlight);
    this.selection.on("new-node", this.updateInfobar);
    this.selection.on("pseudoclass", this.updateInfobar);
    this.selection.on("attribute-changed", this.updateInfobar);

    this.onToolSelected = function(event, id) {
      if (id != "inspector") {
        this.chromeWin.clearTimeout(this.pageEventsMuter);
        this.detachMouseListeners();
        this.disabled = true;
        this.hide();
      } else {
        if (!this.locked) {
          this.attachMouseListeners();
        }
        this.disabled = false;
        this.show();
      }
    }.bind(this);
    this.toolbox.on("select", this.onToolSelected);

    this.hidden = true;
    this.highlight();
  },

  /**
   * Destroy the nodes. Remove listeners.
   */
  destroy: function Highlighter_destroy()
  {
    this.inspectButton.removeEventListener("command", this.unlockAndFocus);
    this.inspectButton = null;

    this.toolbox.off("select", this.onToolSelected);
    this.toolbox = null;

    this.selection.off("new-node", this.highlight);
    this.selection.off("new-node", this.updateInfobar);
    this.selection.off("pseudoclass", this.updateInfobar);
    this.selection.off("attribute-changed", this.updateInfobar);

    this.detachMouseListeners();
    this.detachPageListeners();

    this.chromeWin.clearTimeout(this.transitionDisabler);
    this.chromeWin.clearTimeout(this.pageEventsMuter);
    this.boundCloseEventHandler = null;
    this._contentRect = null;
    this._highlightRect = null;
    this._highlighting = false;
    this.outline = null;
    this.nodeInfo = null;
    this.highlighterContainer.parentNode.removeChild(this.highlighterContainer);
    this.highlighterContainer = null;
    this.win = null
    this.browser = null;
    this.chromeDoc = null;
    this.chromeWin = null;
    this.tabbrowser = null;

    this.emit("closed");
  },

  /**
   * Show the outline, and select a node.
   */
  highlight: function Highlighter_highlight()
  {
    if (this.selection.reason != "highlighter") {
      this.lock();
    }

    let canHighlightNode = this.selection.isNode() &&
                          this.selection.isConnected() &&
                          this.selection.isElementNode();

    if (canHighlightNode) {
      if (this.selection.reason != "navigateaway") {
        this.disabled = false;
      }
      this.show();
      this.updateInfobar();
      this.invalidateSize();
      if (!this._highlighting &&
          this.selection.reason != "highlighter") {
        LayoutHelpers.scrollIntoViewIfNeeded(this.selection.node);
      }
    } else {
      this.disabled = true;
      this.hide();
    }
  },

  /**
   * Update the highlighter size and position.
   */
  invalidateSize: function Highlighter_invalidateSize()
  {
    let canHiglightNode = this.selection.isNode() &&
                          this.selection.isConnected() &&
                          this.selection.isElementNode();

    if (!canHiglightNode)
      return;

    let clientRect = this.selection.node.getBoundingClientRect();
    let rect = LayoutHelpers.getDirtyRect(this.selection.node);
    this.highlightRectangle(rect);

    this.moveInfobar();

    if (this._highlighting) {
      this.showOutline();
      this.emit("highlighting");
    }
  },

  /**
   * Show the highlighter if it has been hidden.
   */
  show: function() {
    if (!this.hidden || this.disabled) return;
    this.showOutline();
    this.showInfobar();
    this.computeZoomFactor();
    this.attachPageListeners();
    this.invalidateSize();
    this.hidden = false;
  },

  /**
   * Hide the highlighter, the outline and the infobar.
   */
  hide: function() {
    if (this.hidden) return;
    this.hideOutline();
    this.hideInfobar();
    this.detachPageListeners();
    this.hidden = true;
  },

  /**
   * Is the highlighter visible?
   *
   * @return boolean
   */
  isHidden: function() {
    return this.hidden;
  },

  /**
   * Lock a node. Stops the inspection.
   */
  lock: function() {
    if (this.locked === true) return;
    this.outline.setAttribute("locked", "true");
    this.nodeInfo.container.setAttribute("locked", "true");
    this.detachMouseListeners();
    this.locked = true;
    this.emit("locked");
  },

  /**
   * Start inspecting.
   * Unlock the current node (if any), and select any node being hovered.
   */
  unlock: function() {
    if (this.locked === false) return;
    this.outline.removeAttribute("locked");
    this.nodeInfo.container.removeAttribute("locked");
    this.attachMouseListeners();
    this.locked = false;
    if (this.selection.isElementNode() &&
        this.selection.isConnected()) {
      this.showOutline();
    }
    this.emit("unlocked");
  },

  /**
   * Focus the browser before unlocking.
   */
  unlockAndFocus: function Highlighter_unlockAndFocus() {
    if (this.locked === false) return;
    this.chromeWin.focus();
    this.unlock();
  },

  /**
   * Hide the infobar
   */
   hideInfobar: function Highlighter_hideInfobar() {
     this.nodeInfo.container.setAttribute("force-transitions", "true");
     this.nodeInfo.container.setAttribute("hidden", "true");
   },

  /**
   * Show the infobar
   */
   showInfobar: function Highlighter_showInfobar() {
     this.nodeInfo.container.removeAttribute("hidden");
     this.moveInfobar();
     this.nodeInfo.container.removeAttribute("force-transitions");
   },

  /**
   * Hide the outline
   */
   hideOutline: function Highlighter_hideOutline() {
     this.outline.setAttribute("hidden", "true");
   },

  /**
   * Show the outline
   */
   showOutline: function Highlighter_showOutline() {
     if (this._highlighting)
       this.outline.removeAttribute("hidden");
   },

  /**
   * Build the node Infobar.
   *
   * <box class="highlighter-nodeinfobar-container">
   *   <box class="Highlighter-nodeinfobar-arrow-top"/>
   *   <hbox class="highlighter-nodeinfobar">
   *     <toolbarbutton class="highlighter-nodeinfobar-button highlighter-nodeinfobar-inspectbutton"/>
   *     <hbox class="highlighter-nodeinfobar-text">
   *       <xhtml:span class="highlighter-nodeinfobar-tagname"/>
   *       <xhtml:span class="highlighter-nodeinfobar-id"/>
   *       <xhtml:span class="highlighter-nodeinfobar-classes"/>
   *       <xhtml:span class="highlighter-nodeinfobar-pseudo-classes"/>
   *     </hbox>
   *     <toolbarbutton class="highlighter-nodeinfobar-button highlighter-nodeinfobar-menu"/>
   *   </hbox>
   *   <box class="Highlighter-nodeinfobar-arrow-bottom"/>
   * </box>
   *
   * @param nsIDOMElement aParent
   *        The container of the infobar.
   */
  buildInfobar: function Highlighter_buildInfobar(aParent)
  {
    let container = this.chromeDoc.createElement("box");
    container.className = "highlighter-nodeinfobar-container";
    container.setAttribute("position", "top");
    container.setAttribute("disabled", "true");

    let nodeInfobar = this.chromeDoc.createElement("hbox");
    nodeInfobar.className = "highlighter-nodeinfobar";

    let arrowBoxTop = this.chromeDoc.createElement("box");
    arrowBoxTop.className = "highlighter-nodeinfobar-arrow highlighter-nodeinfobar-arrow-top";

    let arrowBoxBottom = this.chromeDoc.createElement("box");
    arrowBoxBottom.className = "highlighter-nodeinfobar-arrow highlighter-nodeinfobar-arrow-bottom";

    let tagNameLabel = this.chromeDoc.createElementNS("http://www.w3.org/1999/xhtml", "span");
    tagNameLabel.className = "highlighter-nodeinfobar-tagname";

    let idLabel = this.chromeDoc.createElementNS("http://www.w3.org/1999/xhtml", "span");
    idLabel.className = "highlighter-nodeinfobar-id";

    let classesBox = this.chromeDoc.createElementNS("http://www.w3.org/1999/xhtml", "span");
    classesBox.className = "highlighter-nodeinfobar-classes";

    let pseudoClassesBox = this.chromeDoc.createElementNS("http://www.w3.org/1999/xhtml", "span");
    pseudoClassesBox.className = "highlighter-nodeinfobar-pseudo-classes";

    // Add some content to force a better boundingClientRect down below.
    pseudoClassesBox.textContent = "&nbsp;";

    // Create buttons

    this.inspectButton = this.chromeDoc.createElement("toolbarbutton");
    this.inspectButton.className = "highlighter-nodeinfobar-button highlighter-nodeinfobar-inspectbutton"
    let toolbarInspectButton = this.inspector.panelDoc.getElementById("inspector-inspect-toolbutton");
    this.inspectButton.setAttribute("tooltiptext", toolbarInspectButton.getAttribute("tooltiptext"));
    this.inspectButton.addEventListener("command", this.unlockAndFocus);

    let nodemenu = this.chromeDoc.createElement("toolbarbutton");
    nodemenu.setAttribute("type", "menu");
    nodemenu.className = "highlighter-nodeinfobar-button highlighter-nodeinfobar-menu"
    nodemenu.setAttribute("tooltiptext",
                          this.strings.GetStringFromName("nodeMenu.tooltiptext"));

    nodemenu.onclick = function() {
      this.inspector.showNodeMenu(nodemenu, "after_start");
    }.bind(this);

    // <hbox class="highlighter-nodeinfobar-text"/>
    let texthbox = this.chromeDoc.createElement("hbox");
    texthbox.className = "highlighter-nodeinfobar-text";
    texthbox.setAttribute("align", "center");
    texthbox.setAttribute("flex", "1");

    texthbox.addEventListener("mousedown", function(aEvent) {
      // On click, show the node:
      if (this.selection.isElementNode()) {
        LayoutHelpers.scrollIntoViewIfNeeded(this.selection.node);
      }
    }.bind(this), true);

    texthbox.appendChild(tagNameLabel);
    texthbox.appendChild(idLabel);
    texthbox.appendChild(classesBox);
    texthbox.appendChild(pseudoClassesBox);

    nodeInfobar.appendChild(this.inspectButton);
    nodeInfobar.appendChild(texthbox);
    nodeInfobar.appendChild(nodemenu);

    container.appendChild(arrowBoxTop);
    container.appendChild(nodeInfobar);
    container.appendChild(arrowBoxBottom);

    aParent.appendChild(container);

    let barHeight = container.getBoundingClientRect().height;

    this.nodeInfo = {
      tagNameLabel: tagNameLabel,
      idLabel: idLabel,
      classesBox: classesBox,
      pseudoClassesBox: pseudoClassesBox,
      container: container,
      barHeight: barHeight,
    };
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
    if (!aRect) {
      this.unhighlight();
      return;
    }

    let oldRect = this._contentRect;

    if (oldRect && aRect.top == oldRect.top && aRect.left == oldRect.left &&
        aRect.width == oldRect.width && aRect.height == oldRect.height) {
      return; // same rectangle
    }

    let aRectScaled = LayoutHelpers.getZoomedRect(this.win, aRect);

    if (aRectScaled.left >= 0 && aRectScaled.top >= 0 &&
        aRectScaled.width > 0 && aRectScaled.height > 0) {

      this.showOutline();

      // The bottom div and the right div are flexibles (flex=1).
      // We don't need to resize them.
      let top = "top:" + aRectScaled.top + "px;";
      let left = "left:" + aRectScaled.left + "px;";
      let width = "width:" + aRectScaled.width + "px;";
      let height = "height:" + aRectScaled.height + "px;";
      this.outline.setAttribute("style", top + left + width + height);

      this._highlighting = true;
    } else {
      this.unhighlight();
    }

    this._contentRect = aRect; // save orig (non-scaled) rect
    this._highlightRect = aRectScaled; // and save the scaled rect.

    return;
  },

  /**
   * Clear the highlighter surface.
   */
  unhighlight: function Highlighter_unhighlight()
  {
    this._highlighting = false;
    this.hideOutline();
  },

  /**
   * Update node information (tagName#id.class)
   */
  updateInfobar: function Highlighter_updateInfobar()
  {
    if (!this.selection.isElementNode()) {
      this.nodeInfo.tagNameLabel.textContent = "";
      this.nodeInfo.idLabel.textContent = "";
      this.nodeInfo.classesBox.textContent = "";
      this.nodeInfo.pseudoClassesBox.textContent = "";
      return;
    }

    let node = this.selection.node;

    // Tag name
    this.nodeInfo.tagNameLabel.textContent = node.tagName;

    // ID
    this.nodeInfo.idLabel.textContent = node.id ? "#" + node.id : "";

    // Classes
    let classes = this.nodeInfo.classesBox;

    classes.textContent = node.classList.length ?
                            "." + Array.join(node.classList, ".") : "";

    // Pseudo-classes
    let pseudos = PSEUDO_CLASSES.filter(function(pseudo) {
      return DOMUtils.hasPseudoClassLock(node, pseudo);
    }, this);

    let pseudoBox = this.nodeInfo.pseudoClassesBox;
    pseudoBox.textContent = pseudos.join("");
  },

  /**
   * Move the Infobar to the right place in the highlighter.
   */
  moveInfobar: function Highlighter_moveInfobar()
  {
    if (this._highlightRect) {
      let winHeight = this.win.innerHeight * this.zoom;
      let winWidth = this.win.innerWidth * this.zoom;

      let rect = {top: this._highlightRect.top,
                  left: this._highlightRect.left,
                  width: this._highlightRect.width,
                  height: this._highlightRect.height};

      rect.top = Math.max(rect.top, 0);
      rect.left = Math.max(rect.left, 0);
      rect.width = Math.max(rect.width, 0);
      rect.height = Math.max(rect.height, 0);

      rect.top = Math.min(rect.top, winHeight);
      rect.left = Math.min(rect.left, winWidth);

      this.nodeInfo.container.removeAttribute("disabled");
      // Can the bar be above the node?
      if (rect.top < this.nodeInfo.barHeight) {
        // No. Can we move the toolbar under the node?
        if (rect.top + rect.height +
            this.nodeInfo.barHeight > winHeight) {
          // No. Let's move it inside.
          this.nodeInfo.container.style.top = rect.top + "px";
          this.nodeInfo.container.setAttribute("position", "overlap");
        } else {
          // Yes. Let's move it under the node.
          this.nodeInfo.container.style.top = rect.top + rect.height + "px";
          this.nodeInfo.container.setAttribute("position", "bottom");
        }
      } else {
        // Yes. Let's move it on top of the node.
        this.nodeInfo.container.style.top =
          rect.top - this.nodeInfo.barHeight + "px";
        this.nodeInfo.container.setAttribute("position", "top");
      }

      let barWidth = this.nodeInfo.container.getBoundingClientRect().width;
      let left = rect.left + rect.width / 2 - barWidth / 2;

      // Make sure the whole infobar is visible
      if (left < 0) {
        left = 0;
        this.nodeInfo.container.setAttribute("hide-arrow", "true");
      } else {
        if (left + barWidth > winWidth) {
          left = winWidth - barWidth;
          this.nodeInfo.container.setAttribute("hide-arrow", "true");
        } else {
          this.nodeInfo.container.removeAttribute("hide-arrow");
        }
      }
      this.nodeInfo.container.style.left = left + "px";
    } else {
      this.nodeInfo.container.style.left = "0";
      this.nodeInfo.container.style.top = "0";
      this.nodeInfo.container.setAttribute("position", "top");
      this.nodeInfo.container.setAttribute("hide-arrow", "true");
    }
  },

  /**
   * Store page zoom factor.
   */
  computeZoomFactor: function Highlighter_computeZoomFactor() {
    this.zoom =
      this.win.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils)
      .fullZoom;
  },

  /////////////////////////////////////////////////////////////////////////
  //// Event Handling

  attachMouseListeners: function Highlighter_attachMouseListeners()
  {
    this.browser.addEventListener("mousemove", this, true);
    this.browser.addEventListener("click", this, true);
    this.browser.addEventListener("dblclick", this, true);
    this.browser.addEventListener("mousedown", this, true);
    this.browser.addEventListener("mouseup", this, true);
  },

  detachMouseListeners: function Highlighter_detachMouseListeners()
  {
    this.browser.removeEventListener("mousemove", this, true);
    this.browser.removeEventListener("click", this, true);
    this.browser.removeEventListener("dblclick", this, true);
    this.browser.removeEventListener("mousedown", this, true);
    this.browser.removeEventListener("mouseup", this, true);
  },

  attachPageListeners: function Highlighter_attachPageListeners()
  {
    this.browser.addEventListener("resize", this, true);
    this.browser.addEventListener("scroll", this, true);
    this.browser.addEventListener("MozAfterPaint", this, true);
  },

  detachPageListeners: function Highlighter_detachPageListeners()
  {
    this.browser.removeEventListener("resize", this, true);
    this.browser.removeEventListener("scroll", this, true);
    this.browser.removeEventListener("MozAfterPaint", this, true);
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
        this.brieflyIgnorePageEvents();
        this.handleMouseMove(aEvent);
        break;
      case "resize":
        this.computeZoomFactor();
        break;
      case "MozAfterPaint":
      case "scroll":
        this.brieflyDisableTransitions();
        this.invalidateSize();
        break;
      case "dblclick":
      case "mousedown":
      case "mouseup":
        aEvent.stopPropagation();
        aEvent.preventDefault();
        break;
    }
  },

  /**
   * Disable the CSS transitions for a short time to avoid laggy animations
   * during scrolling or resizing.
   */
  brieflyDisableTransitions: function Highlighter_brieflyDisableTransitions()
  {
    if (this.transitionDisabler) {
      this.chromeWin.clearTimeout(this.transitionDisabler);
    } else {
      this.outline.setAttribute("disable-transitions", "true");
      this.nodeInfo.container.setAttribute("disable-transitions", "true");
    }
    this.transitionDisabler =
      this.chromeWin.setTimeout(function() {
        this.outline.removeAttribute("disable-transitions");
        this.nodeInfo.container.removeAttribute("disable-transitions");
        this.transitionDisabler = null;
      }.bind(this), 500);
  },

  /**
   * Don't listen to page events while inspecting with the mouse.
   */
  brieflyIgnorePageEvents: function Highlighter_brieflyIgnorePageEvents()
  {
    // The goal is to keep smooth animations while inspecting.
    // CSS Transitions might be interrupted because of a MozAfterPaint
    // event that would triger an invalidateSize() call.
    // So we don't listen to events that would trigger an invalidateSize()
    // call.
    //
    // Side effect, zoom levels are not updated during this short period.
    // It's very unlikely this would happen, but just in case, we call
    // computeZoomFactor() when reattaching the events.
    if (this.pageEventsMuter) {
      this.chromeWin.clearTimeout(this.pageEventsMuter);
    } else {
      this.detachPageListeners();
    }
    this.pageEventsMuter =
      this.chromeWin.setTimeout(function() {
        this.attachPageListeners();
        // Just in case the zoom level changed while ignoring the paint events
        this.computeZoomFactor();
        this.pageEventsMuter = null;
      }.bind(this), 500);
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
      this.lock();
      let node = this.selection.node;
      this.selection.setNode(node, "highlighter-lock");
      aEvent.preventDefault();
      aEvent.stopPropagation();
    }
  },

  /**
   * Handle mousemoves in panel.
   *
   * @param nsiDOMEvent aEvent
   *        The MouseEvent triggering the method.
   */
  handleMouseMove: function Highlighter_handleMouseMove(aEvent)
  {
    let doc = aEvent.target.ownerDocument;

    // This should never happen, but just in case, we don't let the
    // highlighter highlight browser nodes.
    if (doc && doc != this.chromeDoc) {
      let element = LayoutHelpers.getElementFromPoint(aEvent.target.ownerDocument,
        aEvent.clientX, aEvent.clientY);
      if (element && element != this.selection.node) {
        this.selection.setNode(element, "highlighter");
      }
    }
  },
};

///////////////////////////////////////////////////////////////////////////

XPCOMUtils.defineLazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils)
});

XPCOMUtils.defineLazyGetter(Highlighter.prototype, "strings", function () {
    return Services.strings.createBundle(
            "chrome://browser/locale/devtools/inspector.properties");
});
