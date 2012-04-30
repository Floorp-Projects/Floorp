/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Original Code is DevTools test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mike Ratcliffe <mratcliffe@mozilla.com>
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

let tempScope = {};
Cu.import("resource:///modules/devtools/CssLogic.jsm", tempScope);
Cu.import("resource:///modules/devtools/CssHtmlTree.jsm", tempScope);
Cu.import("resource://gre/modules/HUDService.jsm", tempScope);
let HUDService = tempScope.HUDService;
let ConsoleUtils = tempScope.ConsoleUtils;
let CssLogic = tempScope.CssLogic;
let CssHtmlTree = tempScope.CssHtmlTree;

function log(aMsg)
{
  dump("*** WebConsoleTest: " + aMsg + "\n");
}

function pprint(aObj)
{
  for (let prop in aObj) {
    if (typeof aObj[prop] == "function") {
      log("function " + prop);
    }
    else {
      log(prop + ": " + aObj[prop]);
    }
  }
}

let tab, browser, hudId, hud, hudBox, filterBox, outputNode, cs;

function addTab(aURL)
{
  gBrowser.selectedTab = gBrowser.addTab();
  content.location = aURL;
  tab = gBrowser.selectedTab;
  browser = gBrowser.getBrowserForTab(tab);
}

function afterAllTabsLoaded(callback, win) {
  win = win || window;

  let stillToLoad = 0;

  function onLoad() {
    this.removeEventListener("load", onLoad, true);
    stillToLoad--;
    if (!stillToLoad)
      callback();
  }

  for (let a = 0; a < win.gBrowser.tabs.length; a++) {
    let browser = win.gBrowser.tabs[a].linkedBrowser;
    if (browser.contentDocument.readyState != "complete") {
      stillToLoad++;
      browser.addEventListener("load", onLoad, true);
    }
  }

  if (!stillToLoad)
    callback();
}

/**
 * Check if a log entry exists in the HUD output node.
 *
 * @param {Element} aOutputNode
 *        the HUD output node.
 * @param {string} aMatchString
 *        the string you want to check if it exists in the output node.
 * @param {string} aMsg
 *        the message describing the test
 * @param {boolean} [aOnlyVisible=false]
 *        find only messages that are visible, not hidden by the filter.
 * @param {boolean} [aFailIfFound=false]
 *        fail the test if the string is found in the output node.
 * @param {string} aClass [optional]
 *        find only messages with the given CSS class.
 */
function testLogEntry(aOutputNode, aMatchString, aMsg, aOnlyVisible,
                      aFailIfFound, aClass)
{
  let selector = ".hud-msg-node";
  // Skip entries that are hidden by the filter.
  if (aOnlyVisible) {
    selector += ":not(.hud-filtered-by-type)";
  }
  if (aClass) {
    selector += "." + aClass;
  }

  let msgs = aOutputNode.querySelectorAll(selector);
  let found = false;
  for (let i = 0, n = msgs.length; i < n; i++) {
    let message = msgs[i].textContent.indexOf(aMatchString);
    if (message > -1) {
      found = true;
      break;
    }

    // Search the labels too.
    let labels = msgs[i].querySelectorAll("label");
    for (let j = 0; j < labels.length; j++) {
      if (labels[j].getAttribute("value").indexOf(aMatchString) > -1) {
        found = true;
        break;
      }
    }
  }

  is(found, !aFailIfFound, aMsg);
}

/**
 * A convenience method to call testLogEntry().
 *
 * @param string aString
 *        The string to find.
 */
function findLogEntry(aString)
{
  testLogEntry(outputNode, aString, "found " + aString);
}

function addStyle(aDocument, aString)
{
  let node = aDocument.createElement('style');
  node.setAttribute("type", "text/css");
  node.textContent = aString;
  aDocument.getElementsByTagName("head")[0].appendChild(node);
  return node;
}

function openConsole()
{
  HUDService.activateHUDForContext(tab);
}

function closeConsole()
{
  HUDService.deactivateHUDForContext(tab);
}

function finishTest()
{
  finish();
}

function tearDown()
{
  try {
    HUDService.deactivateHUDForContext(gBrowser.selectedTab);
  }
  catch (ex) {
    log(ex);
  }
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
  tab = browser = hudId = hud = filterBox = outputNode = cs = null;
}

/**
 * Shows the computed view in its own panel.
 */
function ComputedViewPanel(aContext)
{
  this._init(aContext);
}

ComputedViewPanel.prototype = {
  _init: function CVP_init(aContext)
  {
    this.window = aContext;
    this.document = this.window.document;
    this.cssLogic = new CssLogic();
    this.panelReady = false;
    this.iframeReady = false;
  },

  /**
   * Factory method to create the actual style panel
   * @param {function} aCallback (optional) callback to fire when ready.
   */
  createPanel: function SI_createPanel(aSelection, aCallback)
  {
    let popupSet = this.document.getElementById("mainPopupSet");
    let panel = this.document.createElement("panel");

    panel.setAttribute("class", "styleInspector");
    panel.setAttribute("orient", "vertical");
    panel.setAttribute("ignorekeys", "true");
    panel.setAttribute("noautofocus", "true");
    panel.setAttribute("noautohide", "true");
    panel.setAttribute("titlebar", "normal");
    panel.setAttribute("close", "true");
    panel.setAttribute("label", "Computed View");
    panel.setAttribute("width", 350);
    panel.setAttribute("height", this.window.screen.height / 2);

    this._openCallback = aCallback;
    this.selectedNode = aSelection;

    let iframe = this.document.createElement("iframe");
    let boundIframeOnLoad = function loadedInitializeIframe()
    {
      this.iframeReady = true;
      this.iframe.removeEventListener("load", boundIframeOnLoad, true);
      this.panel.openPopup(this.window.gBrowser.selectedBrowser, "end_before", 0, 0, false, false);
    }.bind(this);

    iframe.flex = 1;
    iframe.setAttribute("tooltip", "aHTMLTooltip");
    iframe.addEventListener("load", boundIframeOnLoad, true);
    iframe.setAttribute("src", "chrome://browser/content/devtools/csshtmltree.xul");

    panel.appendChild(iframe);
    popupSet.appendChild(panel);

    this._boundPopupShown = this.popupShown.bind(this);
    panel.addEventListener("popupshown", this._boundPopupShown, false);

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
    this.cssHtmlTree = new CssHtmlTree(this);
    let selectedNode = this.selectedNode || null;
    this.cssLogic.highlight(selectedNode);
    this.cssHtmlTree.highlight(selectedNode);
    if (this._openCallback) {
      this._openCallback();
      delete this._openCallback;
    }
  },

  isLoaded: function SI_isLoaded()
  {
    return this.iframeReady && this.panelReady;
  },

  /**
   * Select from Path (via CssHtmlTree_pathClick)
   * @param aNode The node to inspect.
   */
  selectFromPath: function SI_selectFromPath(aNode)
  {
    this.selectNode(aNode);
  },

  /**
   * Select a node to inspect in the Style Inspector panel
   * @param aNode The node to inspect.
   */
  selectNode: function SI_selectNode(aNode)
  {
    this.selectedNode = aNode;

    if (this.isLoaded()) {
      this.cssLogic.highlight(aNode);
      this.cssHtmlTree.highlight(aNode);
    }
  },

  /**
   * Destroy the style panel, remove listeners etc.
   */
  destroy: function SI_destroy()
  {
    this.panel.hidePopup();

    if (this.cssHtmlTree) {
      this.cssHtmlTree.destroy();
      delete this.cssHtmlTree;
    }

    if (this.iframe) {
      this.iframe.parentNode.removeChild(this.iframe);
      delete this.iframe;
    }

    delete this.cssLogic;
    this.panel.removeEventListener("popupshown", this._boundPopupShown, false);
    delete this._boundPopupShown;
    this.panel.parentNode.removeChild(this.panel);
    delete this.panel;
    delete this.doc;
    delete this.win;
    delete CssHtmlTree.win;
  },
};

function ruleView()
{
  return InspectorUI.sidebar._toolContext("ruleview").view;
}

registerCleanupFunction(tearDown);

waitForExplicitFinish();

