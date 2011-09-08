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
 *   Mike Ratcliffe <mratcliffe@mozilla.com>
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

var StyleInspector = {
  /**
   * Is the Style Inspector enabled?
   * @returns {Boolean} true or false
   */
  get isEnabled()
  {
    return Services.prefs.getBoolPref("devtools.styleinspector.enabled");
  },

  /**
   * Factory method to create the actual style panel
   * @param {Boolean} aPreserveOnHide Prevents destroy from being called
   * onpopuphide. USE WITH CAUTION: When this value is set to true then you are
   * responsible to manually call destroy from outside the style inspector.
   */
  createPanel: function SI_createPanel(aPreserveOnHide)
  {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    let popupSet = win.document.getElementById("mainPopupSet");
    let ns = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    let panel = win.document.createElementNS(ns, "panel");

    panel.setAttribute("orient", "vertical");
    panel.setAttribute("ignorekeys", "true");
    panel.setAttribute("noautofocus", "true");
    panel.setAttribute("noautohide", "true");
    panel.setAttribute("titlebar", "normal");
    panel.setAttribute("close", "true");
    panel.setAttribute("label", StyleInspector.l10n("panelTitle"));

    // size panel to 200px wide by half browser height - 60.
    let contentWindow = win.gBrowser.selectedBrowser.contentWindow;
    panel.setAttribute("width", 200);
    panel.setAttribute("height", contentWindow.outerHeight / 2 - 60);

    let vbox = win.document.createElement("vbox");
    vbox.setAttribute("flex", "1");
    panel.appendChild(vbox);

    let iframe = win.document.createElementNS(ns, "iframe");
    iframe.setAttribute("flex", "1");
    iframe.setAttribute("tooltip", "aHTMLTooltip");
    iframe.setAttribute("src", "chrome://browser/content/csshtmltree.xhtml");
    iframe.addEventListener("load", SI_iframeOnload, true);
    vbox.appendChild(iframe);

    let hbox = win.document.createElement("hbox");
    hbox.setAttribute("class", "resizerbox");
    vbox.appendChild(hbox);

    let spacer = win.document.createElement("spacer");
    spacer.setAttribute("flex", "1");
    hbox.appendChild(spacer);

    let resizer = win.document.createElement("resizer");
    resizer.setAttribute("dir", "bottomend");
    hbox.appendChild(resizer);
    popupSet.appendChild(panel);

    /**
     * Iframe's onload event
     */
    let iframeReady = false;
    function SI_iframeOnload() {
      iframe.removeEventListener("load", SI_iframeOnload, true);
      panel.cssLogic = new CssLogic();
      panel.cssHtmlTree = new CssHtmlTree(iframe, panel.cssLogic, panel);
      iframeReady = true;
      if (panelReady) {
        SI_popupShown.call(panel);
      }
    }

    /**
     * Initialize the popup when it is first shown
     */
    let panelReady = false;
    function SI_popupShown() {
      panelReady = true;
      if (iframeReady) {
        let selectedNode = this.selectedNode || null;
        this.cssLogic.highlight(selectedNode);
        this.cssHtmlTree.highlight(selectedNode);
        Services.obs.notifyObservers(null, "StyleInspector-opened", null);
      }
    }

    /**
     * Hide the popup and conditionally destroy it
     */
    function SI_popupHidden() {
      if (panel.preserveOnHide) {
        Services.obs.notifyObservers(null, "StyleInspector-closed", null);
      } else {
        panel.destroy();
      }
    }

    panel.addEventListener("popupshown", SI_popupShown);
    panel.addEventListener("popuphidden", SI_popupHidden);
    panel.preserveOnHide = !!aPreserveOnHide;

    /**
     * Check if the style inspector is open
     */
    panel.isOpen = function SI_isOpen()
    {
      return this.state && this.state == "open";
    };

    /**
     * Select a node to inspect in the Style Inspector panel
     *
     * @param aNode The node to inspect
     */
    panel.selectNode = function SI_selectNode(aNode)
    {
      this.selectedNode = aNode;
      if (this.isOpen()) {
        this.cssLogic.highlight(aNode);
        this.cssHtmlTree.highlight(aNode);
      } else {
        let win = Services.wm.getMostRecentWindow("navigator:browser");
        this.openPopup(win.gBrowser.selectedBrowser, "end_before", 0, 0, false, false);
      }
    };

    /**
     * Destroy the style panel, remove listeners etc.
     */
    panel.destroy = function SI_destroy()
    {
      this.cssLogic = null;
      this.cssHtmlTree = null;
      this.removeEventListener("popupshown", SI_popupShown);
      this.removeEventListener("popuphidden", SI_popupHidden);
      this.parentNode.removeChild(this);
      Services.obs.notifyObservers(null, "StyleInspector-closed", null);
    };

    /**
     * Is the Style Inspector initialized?
     * @returns {Boolean} true or false
     */
    function isInitialized()
    {
      return panel.cssLogic && panel.cssHtmlTree;
    }

    return panel;
  },

  /**
   * Memonized lookup of a l10n string from a string bundle.
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
};

XPCOMUtils.defineLazyGetter(this, "_strings", function() Services.strings
          .createBundle("chrome://browser/locale/styleinspector.properties"));

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
