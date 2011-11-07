/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Mike Ratcliffe <mratcliffe@mozilla.com> (Original Author)
 *   Rob Campbell <rcampbell@mozilla.com>
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
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var EXPORTED_SYMBOLS = ["StyleInspector"];

/**
 * StyleInspector Constructor Function.
 * @param {window} aContext, the chrome window context we're calling from.
 * @param {InspectorUI} aIUI (optional) An InspectorUI instance if called from the
 *        Highlighter.
 */
function StyleInspector(aContext, aIUI)
{
  this._init(aContext, aIUI);
};

StyleInspector.prototype = {

  /**
   * Initialization method called from constructor.
   * @param {window} aContext, the chrome window context we're calling from.
   * @param {InspectorUI} aIUI (optional) An InspectorUI instance if called from
   *        the Highlighter.
   */
  _init: function SI__init(aContext, aIUI)
  {
    this.window = aContext;
    this.IUI = aIUI;
    this.document = this.window.document;
    this.cssLogic = new CssLogic();
    this.panelReady = false;
    this.iframeReady = false;

    // Were we invoked from the Highlighter?
    if (this.IUI) {
      this.openDocked = true;
      let isOpen = this.isOpen.bind(this);

      this.registrationObject = {
        id: "styleinspector",
        label: this.l10n("style.highlighter.button.label1"),
        tooltiptext: this.l10n("style.highlighter.button.tooltip"),
        accesskey: this.l10n("style.highlighter.accesskey1"),
        context: this,
        get isOpen() isOpen(),
        onSelect: this.selectNode,
        onChanged: this.updateNode,
        show: this.open,
        hide: this.close,
        dim: this.dimTool,
        panel: null,
        unregister: this.destroy,
        sidebar: true,
      };

      // Register the registrationObject with the Highlighter
      this.IUI.registerTool(this.registrationObject);
      this.createSidebarContent(true);
    }
  },

  /**
   * Create the iframe in the IUI sidebar's tab panel.
   * @param {Boolean} aPreserveOnHide Prevents destroy from being called.
   */
  createSidebarContent: function SI_createSidebarContent(aPreserveOnHide)
  {
    this.preserveOnHide = !!aPreserveOnHide;

    let boundIframeOnLoad = function loadedInitializeIframe() {
      if (this.iframe &&
          this.iframe.getAttribute("src") ==
          "chrome://browser/content/devtools/csshtmltree.xul") {
        let selectedNode = this.selectedNode || null;
        this.cssHtmlTree = new CssHtmlTree(this);
        this.cssLogic.highlight(selectedNode);
        this.cssHtmlTree.highlight(selectedNode);
        this.iframe.removeEventListener("load", boundIframeOnLoad, true);
        this.iframeReady = true;
        Services.obs.notifyObservers(null, "StyleInspector-opened", null);
      }
    }.bind(this);

    this.iframe = this.IUI.getToolIframe(this.registrationObject);

    this.iframe.addEventListener("load", boundIframeOnLoad, true);
  },

  /**
   * Factory method to create the actual style panel
   * @param {Boolean} aPreserveOnHide Prevents destroy from being called
   * onpopuphide. USE WITH CAUTION: When this value is set to true then you are
   * responsible to manually call destroy from outside the style inspector.
   * @param {function} aCallback (optional) callback to fire when ready.
   */
  createPanel: function SI_createPanel(aPreserveOnHide, aCallback)
  {
    let popupSet = this.document.getElementById("mainPopupSet");
    let panel = this.document.createElement("panel");
    this.preserveOnHide = !!aPreserveOnHide;

    panel.setAttribute("class", "styleInspector");
    panel.setAttribute("orient", "vertical");
    panel.setAttribute("ignorekeys", "true");
    panel.setAttribute("noautofocus", "true");
    panel.setAttribute("noautohide", "true");
    panel.setAttribute("titlebar", "normal");
    panel.setAttribute("close", "true");
    panel.setAttribute("label", this.l10n("panelTitle"));
    panel.setAttribute("width", 350);
    panel.setAttribute("height", this.window.screen.height / 2);

    let iframe = this.document.createElement("iframe");
    let boundIframeOnLoad = function loadedInitializeIframe()
    {
      this.iframe.removeEventListener("load", boundIframeOnLoad, true);
      this.iframeReady = true;
      if (aCallback)
        aCallback(this);
    }.bind(this);

    iframe.flex = 1;
    iframe.setAttribute("tooltip", "aHTMLTooltip");
    iframe.addEventListener("load", boundIframeOnLoad, true);
    iframe.setAttribute("src", "chrome://browser/content/devtools/csshtmltree.xul");

    panel.appendChild(iframe);
    popupSet.appendChild(panel);

    this._boundPopupShown = this.popupShown.bind(this);
    this._boundPopupHidden = this.popupHidden.bind(this);
    panel.addEventListener("popupshown", this._boundPopupShown, false);
    panel.addEventListener("popuphidden", this._boundPopupHidden, false);

    this.panel = panel;
    this.iframe = iframe;

    return panel;
  },

  /**
   * Event handler for the popupshown event.
   */
  popupShown: function SI_popupShown()
  {
    this.panelReady = true;
    if (this.iframeReady) {
      this.cssHtmlTree = new CssHtmlTree(this);
      let selectedNode = this.selectedNode || null;
      this.cssLogic.highlight(selectedNode);
      this.cssHtmlTree.highlight(selectedNode);
      Services.obs.notifyObservers(null, "StyleInspector-opened", null);
    }
  },

  /**
   * Event handler for the popuphidden event.
   * Hide the popup and conditionally destroy it
   */
  popupHidden: function SI_popupHidden()
  {
    if (this.preserveOnHide) {
      Services.obs.notifyObservers(null, "StyleInspector-closed", null);
    } else {
      this.destroy();
    }
  },

  /**
   * Check if the style inspector is open.
   * @returns boolean
   */
  isOpen: function SI_isOpen()
  {
    return this.openDocked ? this.iframeReady && this.IUI.isSidebarOpen &&
            (this.IUI.sidebarDeck.selectedPanel == this.iframe) :
           this.panel && this.panel.state && this.panel.state == "open";
  },

  /**
   * Select from Path (via CssHtmlTree_pathClick)
   * @param aNode The node to inspect.
   */
  selectFromPath: function SI_selectFromPath(aNode)
  {
    if (this.IUI && this.IUI.selection) {
      if (aNode != this.IUI.selection) {
        this.IUI.inspectNode(aNode);
      }
    } else {
      this.selectNode(aNode);
    }
  },

  /**
   * Select a node to inspect in the Style Inspector panel
   * @param aNode The node to inspect.
   */
  selectNode: function SI_selectNode(aNode)
  {
    this.selectedNode = aNode;
    if (this.isOpen() && !this.dimmed) {
      this.cssLogic.highlight(aNode);
      this.cssHtmlTree.highlight(aNode);
    }
  },

  /**
   * Update the display for the currently-selected node.
   */
  updateNode: function SI_updateNode()
  {
    if (this.isOpen() && !this.dimmed) {
      this.cssLogic.highlight(this.selectedNode);
      this.cssHtmlTree.refreshPanel();
    }
  },

  /**
   * Dim or undim a panel by setting or removing a dimmed attribute.
   * @param aState
   *        true = dim, false = undim
   */
  dimTool: function SI_dimTool(aState)
  {
    this.dimmed = aState;
  },

  /**
   * Open the panel.
   * @param {DOMNode} aSelection the (optional) DOM node to select.
   */
  open: function SI_open(aSelection)
  {
    this.selectNode(aSelection);
    if (this.openDocked) {
      if (!this.iframeReady) {
        this.iframe.setAttribute("src", "chrome://browser/content/devtools/csshtmltree.xul");
      }
    } else {
      this.panel.openPopup(this.window.gBrowser.selectedBrowser, "end_before", 0, 0,
        false, false);
    }
  },

  /**
   * Close the panel.
   */
  close: function SI_close()
  {
    if (this.openDocked) {
      Services.obs.notifyObservers(null, "StyleInspector-closed", null);
    } else {
      this.panel.hidePopup();
    }
  },

  /**
   * Memoized lookup of a l10n string from a string bundle.
   * @param {string} aName The key to lookup.
   * @returns A localized version of the given key.
   */
  l10n: function SI_l10n(aName)
  {
    try {
      return _strings.GetStringFromName(aName);
    } catch (ex) {
      Services.console.logStringMessage("Error reading '" + aName + "'");
      throw new Error("l10n error with " + aName);
    }
  },

  /**
   * Destroy the style panel, remove listeners etc.
   */
  destroy: function SI_destroy()
  {
    if (this.isOpen())
      this.close();
    if (this.cssHtmlTree)
      this.cssHtmlTree.destroy();
    if (this.iframe) {
      this.iframe.parentNode.removeChild(this.iframe);
      delete this.iframe;
    }

    delete this.cssLogic;
    delete this.cssHtmlTree;
    if (this.panel) {
      this.panel.removeEventListener("popupshown", this._boundPopupShown, false);
      this.panel.removeEventListener("popuphidden", this._boundPopupHidden, false);
      delete this._boundPopupShown;
      delete this._boundPopupHidden;
      this.panel.parentNode.removeChild(this.panel);
      delete this.panel;
    }
    delete this.doc;
    delete this.win;
    delete CssHtmlTree.win;
    Services.obs.notifyObservers(null, "StyleInspector-closed", null);
  },
};

XPCOMUtils.defineLazyGetter(this, "_strings", function() Services.strings
  .createBundle("chrome://browser/locale/devtools/styleinspector.properties"));

XPCOMUtils.defineLazyGetter(this, "CssLogic", function() {
  let tmp = {};
  Cu.import("resource:///modules/devtools/CssLogic.jsm", tmp);
  return tmp.CssLogic;
});

XPCOMUtils.defineLazyGetter(this, "CssHtmlTree", function() {
  let tmp = {};
  Cu.import("resource:///modules/devtools/CssHtmlTree.jsm", tmp);
  return tmp.CssHtmlTree;
});
