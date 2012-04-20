/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
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
 * The Original Code is the Mozilla Tree Panel.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rob Campbell <rcampbell@mozilla.com> (original author)
 *   Mihai Șucan <mihai.sucan@gmail.com>
 *   Julian Viereck <jviereck@mozilla.com>
 *   Paul Rouget <paul@mozilla.com>
 *   Kyle Simpson <getify@mozilla.com>
 *   Dave Camp <dcamp@mozilla.com>
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

const Cu = Components.utils;

Cu.import("resource:///modules/domplate.jsm");
Cu.import("resource:///modules/InsideOutBox.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/inspector.jsm");

var EXPORTED_SYMBOLS = ["TreePanel", "DOMHelpers"];

const INSPECTOR_URI = "chrome://browser/content/inspector.html";

/**
 * TreePanel
 * A container for the Inspector's HTML Tree Panel widget constructor function.
 * @param aContext nsIDOMWindow (xulwindow)
 * @param aIUI global InspectorUI object
 */
function TreePanel(aContext, aIUI) {
  this._init(aContext, aIUI);
};

TreePanel.prototype = {
  showTextNodesWithWhitespace: false,
  id: "treepanel", // DO NOT LOCALIZE
  _open: false,

  /**
   * The tree panel container element.
   * @returns xul:panel|xul:vbox|null
   *          xul:panel is returned when the tree panel is not docked, or
   *          xul:vbox when when the tree panel is docked.
   *          null is returned when no container is available.
   */
  get container()
  {
    return this.document.getElementById("inspector-tree-box");
  },

  /**
   * Main TreePanel boot-strapping method. Initialize the TreePanel with the
   * originating context and the InspectorUI global.
   * @param aContext nsIDOMWindow (xulwindow)
   * @param aIUI global InspectorUI object
   */
  _init: function TP__init(aContext, aIUI)
  {
    this.IUI = aIUI;
    this.window = aContext;
    this.document = this.window.document;
    this.button =
     this.IUI.chromeDoc.getElementById("inspector-treepanel-toolbutton");

    domplateUtils.setDOM(this.window);

    this.DOMHelpers = new DOMHelpers(this.window);

    let isOpen = this.isOpen.bind(this);

    this.editingEvents = {};
  },

  /**
   * Initialization function for the TreePanel.
   */
  initializeIFrame: function TP_initializeIFrame()
  {
    if (!this.initializingTreePanel || this.treeLoaded) {
      return;
    }
    this.treeBrowserDocument = this.treeIFrame.contentDocument;
    this.treePanelDiv = this.treeBrowserDocument.createElement("div");
    this.treeBrowserDocument.body.appendChild(this.treePanelDiv);
    this.treePanelDiv.ownerPanel = this;
    this.ioBox = new InsideOutBox(this, this.treePanelDiv);
    this.ioBox.createObjectBox(this.IUI.win.document.documentElement);
    this.treeLoaded = true;
    this.treeIFrame.addEventListener("click", this.onTreeClick.bind(this), false);
    this.treeIFrame.addEventListener("dblclick", this.onTreeDblClick.bind(this), false);
    this.treeIFrame.focus();
    delete this.initializingTreePanel;
    Services.obs.notifyObservers(null,
      this.IUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY, null);
    if (this.pendingSelection) {
      this.select(this.pendingSelection.node, this.pendingSelection.scroll);
      delete this.pendingSelection;
    }
  },

  /**
   * Open the inspector's tree panel and initialize it.
   */
  open: function TP_open()
  {
    if (this._open) {
      return;
    }

    this._open = true;

    this.button.setAttribute("checked", true);
    this.initializingTreePanel = true;

    this.treeIFrame = this.document.getElementById("inspector-tree-iframe");
    if (!this.treeIFrame) {
      this.treeIFrame = this.document.createElement("iframe");
      this.treeIFrame.setAttribute("id", "inspector-tree-iframe");
      this.treeIFrame.flex = 1;
      this.treeIFrame.setAttribute("type", "content");
      this.treeIFrame.setAttribute("context", "inspector-node-popup");
    }

    let treeBox = null;
    treeBox = this.document.createElement("vbox");
    treeBox.id = "inspector-tree-box";
    treeBox.state = "open";
    try {
      treeBox.height =
        Services.prefs.getIntPref("devtools.inspector.htmlHeight");
    } catch(e) {
      treeBox.height = 112;
    }

    treeBox.minHeight = 64;

    this.splitter = this.document.createElement("splitter");
    this.splitter.id = "inspector-tree-splitter";

    let container = this.document.getElementById("appcontent");
    container.appendChild(this.splitter);
    container.appendChild(treeBox);

    treeBox.appendChild(this.treeIFrame);

    this._boundLoadedInitializeTreePanel = function loadedInitializeTreePanel()
    {
      this.treeIFrame.removeEventListener("load",
        this._boundLoadedInitializeTreePanel, true);
      delete this._boundLoadedInitializeTreePanel;
      this.initializeIFrame();
    }.bind(this);

    this.treeIFrame.addEventListener("load",
      this._boundLoadedInitializeTreePanel, true);

    let src = this.treeIFrame.getAttribute("src");
    if (src != INSPECTOR_URI) {
      this.treeIFrame.setAttribute("src", INSPECTOR_URI);
    } else {
      this.treeIFrame.contentWindow.location.reload();
    }
  },

  /**
   * Close the TreePanel.
   */
  close: function TP_close()
  {
    this._open = false;

    // Stop caring about the tree iframe load if it's in progress.
    if (this._boundLoadedInitializeTreePanel) {
      this.treeIFrame.removeEventListener("load",
        this._boundLoadedInitializeTreePanel, true);
      delete this._boundLoadedInitializeTreePanel;
    }

    this.button.removeAttribute("checked");
    let treeBox = this.container;
    Services.prefs.setIntPref("devtools.inspector.htmlHeight", treeBox.height);
    let treeBoxParent = treeBox.parentNode;
    treeBoxParent.removeChild(this.splitter);
    treeBoxParent.removeChild(treeBox);

    if (this.treePanelDiv) {
      this.treePanelDiv.ownerPanel = null;
      let parent = this.treePanelDiv.parentNode;
      parent.removeChild(this.treePanelDiv);
      delete this.treePanelDiv;
      delete this.treeBrowserDocument;
    }

    if (this.ioBox) {
      this.ioBox.destroy();
      delete this.ioBox;
    }

    this.treeLoaded = false;
  },

  /**
   * Is the TreePanel open?
   * @returns boolean
   */
  isOpen: function TP_isOpen()
  {
    return this._open;
  },

  /**
   * Toggle the TreePanel.
   */
  toggle: function TP_toggle()
  {
    this.isOpen() ? this.close() : this.open();
  },

  /**
   * Create the ObjectBox for the given object.
   * @param object nsIDOMNode
   * @param isRoot boolean - Is this the root object?
   * @returns InsideOutBox
   */
  createObjectBox: function TP_createObjectBox(object, isRoot)
  {
    let tag = domplateUtils.getNodeTag(object);
    if (tag)
      return tag.replace({object: object}, this.treeBrowserDocument);
  },

  getParentObject: function TP_getParentObject(node)
  {
    return this.DOMHelpers.getParentObject(node);
  },

  getChildObject: function TP_getChildObject(node, index, previousSibling)
  {
    return this.DOMHelpers.getChildObject(node, index, previousSibling,
                                        this.showTextNodesWithWhitespace);
  },

  getFirstChild: function TP_getFirstChild(node)
  {
    return this.DOMHelpers.getFirstChild(node);
  },

  getNextSibling: function TP_getNextSibling(node)
  {
    return this.DOMHelpers.getNextSibling(node);
  },

  /////////////////////////////////////////////////////////////////////
  // Event Handling

  /**
   * Handle click events in the html tree panel.
   * @param aEvent
   *        The mouse event.
   */
  onTreeClick: function TP_onTreeClick(aEvent)
  {
    let node;
    let target = aEvent.target;
    let hitTwisty = false;
    if (this.hasClass(target, "twisty")) {
      node = this.getRepObject(aEvent.target.nextSibling);
      hitTwisty = true;
    } else {
      node = this.getRepObject(aEvent.target);
    }

    if (node) {
      if (hitTwisty) {
        this.ioBox.toggleObject(node);
      } else {
        if (this.IUI.inspecting) {
          this.IUI.stopInspecting(true);
        } else {
          this.IUI.select(node, true, false);
          this.IUI.highlighter.highlight(node);
        }
      }
    }
  },

  /**
   * Handle double-click events in the html tree panel.
   * Double-clicking an attribute name or value allows it to be edited.
   * @param aEvent
   *        The mouse event.
   */
  onTreeDblClick: function TP_onTreeDblClick(aEvent)
  {
    // if already editing an attribute value, double-clicking elsewhere
    // in the tree is the same as a click, which dismisses the editor
    if (this.editingContext)
      this.closeEditor();

    let target = aEvent.target;

    if (!this.hasClass(target, "editable")) {
      return;
    }

    let repObj = this.getRepObject(target);

    if (this.hasClass(target, "nodeValue")) {
      let attrName = target.getAttribute("data-attributeName");
      let attrVal = target.innerHTML;

      this.editAttribute(target, repObj, attrName, attrVal);
    }

    if (this.hasClass(target, "nodeName")) {
      let attrName = target.innerHTML;
      let attrValNode = target.nextSibling.nextSibling; // skip 2 (=)

      if (attrValNode)
        this.editAttribute(target, repObj, attrName, attrValNode.innerHTML);
    }
  },

  /**
   * Starts the editor for an attribute name or value.
   * @param aAttrObj
   *        The DOM object representing the attribute name or value in the HTML
   *        Tree.
   * @param aRepObj
   *        The original DOM (target) object being inspected/edited
   * @param aAttrName
   *        The name of the attribute being edited
   * @param aAttrVal
   *        The current value of the attribute being edited
   */
  editAttribute:
  function TP_editAttribute(aAttrObj, aRepObj, aAttrName, aAttrVal)
  {
    let editor = this.treeBrowserDocument.getElementById("attribute-editor");
    let editorInput =
      this.treeBrowserDocument.getElementById("attribute-editor-input");
    let attrDims = aAttrObj.getBoundingClientRect();
    // figure out actual viewable viewport dimensions (sans scrollbars)
    let viewportWidth = this.treeBrowserDocument.documentElement.clientWidth;
    let viewportHeight = this.treeBrowserDocument.documentElement.clientHeight;

    // saves the editing context for use when the editor is saved/closed
    this.editingContext = {
      attrObj: aAttrObj,
      repObj: aRepObj,
      attrName: aAttrName,
      attrValue: aAttrVal
    };

    // highlight attribute-value node in tree while editing
    this.addClass(aAttrObj, "editingAttributeValue");

    // show the editor
    this.addClass(editor, "editing");

    // offset the editor below the attribute-value node being edited
    let editorVerticalOffset = 2;

    // keep the editor comfortably within the bounds of the viewport
    let editorViewportBoundary = 5;

    // outer editor is sized based on the <input> box inside it
    editorInput.style.width = Math.min(attrDims.width, viewportWidth -
                                editorViewportBoundary) + "px";
    editorInput.style.height = Math.min(attrDims.height, viewportHeight -
                                editorViewportBoundary) + "px";
    let editorDims = editor.getBoundingClientRect();

    // calculate position for the editor according to the attribute node
    let editorLeft = attrDims.left + this.treeIFrame.contentWindow.scrollX -
                    // center the editor against the attribute value
                    ((editorDims.width - attrDims.width) / 2);
    let editorTop = attrDims.top + this.treeIFrame.contentWindow.scrollY +
                    attrDims.height + editorVerticalOffset;

    // but, make sure the editor stays within the visible viewport
    editorLeft = Math.max(0, Math.min(
                                      (this.treeIFrame.contentWindow.scrollX +
                                          viewportWidth - editorDims.width),
                                      editorLeft)
                          );
    editorTop = Math.max(0, Math.min(
                                      (this.treeIFrame.contentWindow.scrollY +
                                          viewportHeight - editorDims.height),
                                      editorTop)
                          );

    // position the editor
    editor.style.left = editorLeft + "px";
    editor.style.top = editorTop + "px";

    // set and select the text
    if (this.hasClass(aAttrObj, "nodeValue")) {
      editorInput.value = aAttrVal;
      editorInput.select();
    } else {
      editorInput.value = aAttrName;
      editorInput.select();
    }

    // listen for editor specific events
    this.bindEditorEvent(editor, "click", function(aEvent) {
      aEvent.stopPropagation();
    });
    this.bindEditorEvent(editor, "dblclick", function(aEvent) {
      aEvent.stopPropagation();
    });
    this.bindEditorEvent(editor, "keypress",
                          this.handleEditorKeypress.bind(this));

    // event notification
    Services.obs.notifyObservers(null, this.IUI.INSPECTOR_NOTIFICATIONS.EDITOR_OPENED,
                                  null);
  },

  /**
   * Handle binding an event handler for the editor.
   * (saves the callback for easier unbinding later)
   * @param aEditor
   *        The DOM object for the editor
   * @param aEventName
   *        The name of the event to listen for
   * @param aEventCallback
   *        The callback to bind to the event (and also to save for later
   *          unbinding)
   */
  bindEditorEvent:
  function TP_bindEditorEvent(aEditor, aEventName, aEventCallback)
  {
    this.editingEvents[aEventName] = aEventCallback;
    aEditor.addEventListener(aEventName, aEventCallback, false);
  },

  /**
   * Handle unbinding an event handler from the editor.
   * (unbinds the previously bound and saved callback)
   * @param aEditor
   *        The DOM object for the editor
   * @param aEventName
   *        The name of the event being listened for
   */
  unbindEditorEvent: function TP_unbindEditorEvent(aEditor, aEventName)
  {
    aEditor.removeEventListener(aEventName, this.editingEvents[aEventName],
                                  false);
    this.editingEvents[aEventName] = null;
  },

  /**
   * Handle keypress events in the editor.
   * @param aEvent
   *        The keyboard event.
   */
  handleEditorKeypress: function TP_handleEditorKeypress(aEvent)
  {
    if (aEvent.which == this.window.KeyEvent.DOM_VK_RETURN) {
      this.saveEditor();
      aEvent.preventDefault();
      aEvent.stopPropagation();
    } else if (aEvent.keyCode == this.window.KeyEvent.DOM_VK_ESCAPE) {
      this.closeEditor();
      aEvent.preventDefault();
      aEvent.stopPropagation();
    }
  },

  /**
   * Close the editor and cleanup.
   */
  closeEditor: function TP_closeEditor()
  {
    let editor = this.treeBrowserDocument.getElementById("attribute-editor");
    let editorInput =
      this.treeBrowserDocument.getElementById("attribute-editor-input");

    // remove highlight from attribute-value node in tree
    this.removeClass(this.editingContext.attrObj, "editingAttributeValue");

    // hide editor
    this.removeClass(editor, "editing");

    // stop listening for editor specific events
    this.unbindEditorEvent(editor, "click");
    this.unbindEditorEvent(editor, "dblclick");
    this.unbindEditorEvent(editor, "keypress");

    // clean up after the editor
    editorInput.value = "";
    editorInput.blur();
    this.editingContext = null;
    this.editingEvents = {};

    // event notification
    Services.obs.notifyObservers(null, this.IUI.INSPECTOR_NOTIFICATIONS.EDITOR_CLOSED,
                                  null);
  },

  /**
   * Commit the edits made in the editor, then close it.
   */
  saveEditor: function TP_saveEditor()
  {
    let editorInput =
      this.treeBrowserDocument.getElementById("attribute-editor-input");
    let dirty = false;

    if (this.hasClass(this.editingContext.attrObj, "nodeValue")) {
      // set the new attribute value on the original target DOM element
      this.editingContext.repObj.setAttribute(this.editingContext.attrName,
                                                editorInput.value);

      // update the HTML tree attribute value
      this.editingContext.attrObj.innerHTML = editorInput.value;
      dirty = true;
    }

    if (this.hasClass(this.editingContext.attrObj, "nodeName")) {
      // remove the original attribute from the original target DOM element
      this.editingContext.repObj.removeAttribute(this.editingContext.attrName);

      // set the new attribute value on the original target DOM element
      this.editingContext.repObj.setAttribute(editorInput.value,
                                              this.editingContext.attrValue);

      // update the HTML tree attribute value
      this.editingContext.attrObj.innerHTML = editorInput.value;
      dirty = true;
    }

    this.IUI.isDirty = dirty;
    this.IUI.nodeChanged(this.registrationObject);

    // event notification
    Services.obs.notifyObservers(null, this.IUI.INSPECTOR_NOTIFICATIONS.EDITOR_SAVED,
                                  null);

    this.closeEditor();
  },

  /**
   * Simple tree select method.
   * @param aNode the DOM node in the content document to select.
   * @param aScroll boolean scroll to the visible node?
   */
  select: function TP_select(aNode, aScroll)
  {
    if (this.ioBox) {
      this.ioBox.select(aNode, true, true, aScroll);
    } else {
      this.pendingSelection = { node: aNode, scroll: aScroll };
    }
  },

  ///////////////////////////////////////////////////////////////////////////
  //// Utility functions

  /**
   * Does the given object have a class attribute?
   * @param aNode
   *        the DOM node.
   * @param aClass
   *        The class string.
   * @returns boolean
   */
  hasClass: function TP_hasClass(aNode, aClass)
  {
    if (!(aNode instanceof this.window.Element))
      return false;
    return aNode.classList.contains(aClass);
  },

  /**
   * Add the class name to the given object.
   * @param aNode
   *        the DOM node.
   * @param aClass
   *        The class string.
   */
  addClass: function TP_addClass(aNode, aClass)
  {
    if (aNode instanceof this.window.Element)
      aNode.classList.add(aClass);
  },

  /**
   * Remove the class name from the given object
   * @param aNode
   *        the DOM node.
   * @param aClass
   *        The class string.
   */
  removeClass: function TP_removeClass(aNode, aClass)
  {
    if (aNode instanceof this.window.Element)
      aNode.classList.remove(aClass);
  },

  /**
   * Get the "repObject" from the HTML panel's domplate-constructed DOM node.
   * In this system, a "repObject" is the Object being Represented by the box
   * object. It is the "real" object that we're building our facade around.
   *
   * @param element
   *        The element in the HTML panel the user clicked.
   * @returns either a real node or null
   */
  getRepObject: function TP_getRepObject(element)
  {
    let target = null;
    for (let child = element; child; child = child.parentNode) {
      if (this.hasClass(child, "repTarget"))
        target = child;

      if (child.repObject) {
        if (!target && this.hasClass(child.repObject, "repIgnore"))
          break;
        else
          return child.repObject;
      }
    }
    return null;
  },

  /**
   * Remove a node box from the tree view.
   * @param aElement
   *        The DOM node to remove from the HTML IOBox.
   */
  deleteChildBox: function TP_deleteChildBox(aElement)
  {
    let childBox = this.ioBox.findObjectBox(aElement);
    if (!childBox) {
      return;
    }
    childBox.parentNode.removeChild(childBox);
  },

  /**
   * Destructor function. Cleanup.
   */
  destroy: function TP_destroy()
  {
    if (this.isOpen()) {
      this.close();
    }

    domplateUtils.setDOM(null);

    if (this.DOMHelpers) {
      this.DOMHelpers.destroy();
      delete this.DOMHelpers;
    }

    if (this.treePanelDiv) {
      this.treePanelDiv.ownerPanel = null;
      let parent = this.treePanelDiv.parentNode;
      parent.removeChild(this.treePanelDiv);
      delete this.treePanelDiv;
      delete this.treeBrowserDocument;
    }

    if (this.treeIFrame) {
      this.treeIFrame.removeEventListener("dblclick", this.onTreeDblClick, false);
      this.treeIFrame.removeEventListener("click", this.onTreeClick, false);
      let parent = this.treeIFrame.parentNode;
      parent.removeChild(this.treeIFrame);
      delete this.treeIFrame;
    }
  }
};


/**
 * DOMHelpers
 * Makes DOM traversal easier. Goes through iframes.
 *
 * @constructor
 * @param nsIDOMWindow aWindow
 *        The content window, owning the document to traverse.
 */
function DOMHelpers(aWindow) {
  this.window = aWindow;
};

DOMHelpers.prototype = {
  getParentObject: function Helpers_getParentObject(node)
  {
    let parentNode = node ? node.parentNode : null;

    if (!parentNode) {
      // Documents have no parentNode; Attr, Document, DocumentFragment, Entity,
      // and Notation. top level windows have no parentNode
      if (node && node == this.window.Node.DOCUMENT_NODE) {
        // document type
        if (node.defaultView) {
          let embeddingFrame = node.defaultView.frameElement;
          if (embeddingFrame)
            return embeddingFrame.parentNode;
        }
      }
      // a Document object without a parentNode or window
      return null;  // top level has no parent
    }

    if (parentNode.nodeType == this.window.Node.DOCUMENT_NODE) {
      if (parentNode.defaultView) {
        return parentNode.defaultView.frameElement;
      }
      // parent is document element, but no window at defaultView.
      return null;
    }

    if (!parentNode.localName)
      return null;

    return parentNode;
  },

  getChildObject: function Helpers_getChildObject(node, index, previousSibling,
                                                showTextNodesWithWhitespace)
  {
    if (!node)
      return null;

    if (node.contentDocument) {
      // then the node is a frame
      if (index == 0) {
        return node.contentDocument.documentElement;  // the node's HTMLElement
      }
      return null;
    }

    if (node instanceof this.window.GetSVGDocument) {
      let svgDocument = node.getSVGDocument();
      if (svgDocument) {
        // then the node is a frame
        if (index == 0) {
          return svgDocument.documentElement;  // the node's SVGElement
        }
        return null;
      }
    }

    let child = null;
    if (previousSibling)  // then we are walking
      child = this.getNextSibling(previousSibling);
    else
      child = this.getFirstChild(node);

    if (showTextNodesWithWhitespace)
      return child;

    for (; child; child = this.getNextSibling(child)) {
      if (!this.isWhitespaceText(child))
        return child;
    }

    return null;  // we have no children worth showing.
  },

  getFirstChild: function Helpers_getFirstChild(node)
  {
    let SHOW_ALL = Components.interfaces.nsIDOMNodeFilter.SHOW_ALL;
    this.treeWalker = node.ownerDocument.createTreeWalker(node,
      SHOW_ALL, null, false);
    return this.treeWalker.firstChild();
  },

  getNextSibling: function Helpers_getNextSibling(node)
  {
    let next = this.treeWalker.nextSibling();

    if (!next)
      delete this.treeWalker;

    return next;
  },

  isWhitespaceText: function Helpers_isWhitespaceText(node)
  {
    return node.nodeType == this.window.Node.TEXT_NODE &&
                            !/[^\s]/.exec(node.nodeValue);
  },

  destroy: function Helpers_destroy()
  {
    delete this.window;
    delete this.treeWalker;
  }
};
