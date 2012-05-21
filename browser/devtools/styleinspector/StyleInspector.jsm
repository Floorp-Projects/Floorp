/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/devtools/CssRuleView.jsm");
Cu.import("resource:///modules/inspector.jsm");

// This module doesn't currently export any symbols directly, it only
// registers inspector tools.
var EXPORTED_SYMBOLS = [];

/**
 * Lookup l10n string from a string bundle.
 * @param {string} aName The key to lookup.
 * @returns A localized version of the given key.
 */
function l10n(aName)
{
  try {
    return _strings.GetStringFromName(aName);
  } catch (ex) {
    Services.console.logStringMessage("Error reading '" + aName + "'");
    throw new Error("l10n error with " + aName);
  }
}

function RegisterStyleTools()
{
  // Register the rules view
  if (Services.prefs.getBoolPref("devtools.ruleview.enabled")) {
    InspectorUI.registerSidebar({
      id: "ruleview",
      label: l10n("ruleView.label"),
      tooltiptext: l10n("ruleView.tooltiptext"),
      accesskey: l10n("ruleView.accesskey"),
      contentURL: "chrome://browser/content/devtools/cssruleview.xul",
      load: function(aInspector, aFrame) new RuleViewTool(aInspector, aFrame),
      destroy: function(aContext) aContext.destroy()
    });
  }

  // Register the computed styles view
  if (Services.prefs.getBoolPref("devtools.styleinspector.enabled")) {
    InspectorUI.registerSidebar({
      id: "computedview",
      label: this.l10n("style.highlighter.button.label2"),
      tooltiptext: this.l10n("style.highlighter.button.tooltip2"),
      accesskey: this.l10n("style.highlighter.accesskey2"),
      contentURL: "chrome://browser/content/devtools/csshtmltree.xul",
      load: function(aInspector, aFrame) new ComputedViewTool(aInspector, aFrame),
      destroy: function(aContext) aContext.destroy()
    });
  }
}

function RuleViewTool(aInspector, aFrame)
{
  this.inspector = aInspector;
  this.chromeWindow = this.inspector.chromeWindow;
  this.doc = aFrame.contentDocument;

  if (!this.inspector._ruleViewStore) {
   this.inspector._ruleViewStore = {};
  }
  this.view = new CssRuleView(this.doc, this.inspector._ruleViewStore);
  this.doc.documentElement.appendChild(this.view.element);

  this._changeHandler = function() {
    this.inspector.markDirty();
    this.inspector.change("ruleview");
  }.bind(this);

  this.view.element.addEventListener("CssRuleViewChanged", this._changeHandler)

  this._cssLinkHandler = function(aEvent) {
    let rule = aEvent.detail.rule;
    let styleSheet = rule.sheet;
    let doc = this.chromeWindow.content.document;
    let styleSheets = doc.styleSheets;
    let contentSheet = false;
    let line = rule.ruleLine || 0;

    // Array.prototype.indexOf always returns -1 here so we loop through
    // the styleSheets object instead.
    for each (let sheet in styleSheets) {
      if (sheet == styleSheet) {
        contentSheet = true;
        break;
      }
    }

    if (contentSheet)  {
      this.chromeWindow.StyleEditor.openChrome(styleSheet, line);
    } else {
      let href = styleSheet ? styleSheet.href : "";
      if (rule.elementStyle.element) {
        href = rule.elementStyle.element.ownerDocument.location.href;
      }
      let viewSourceUtils = this.chromeWindow.gViewSourceUtils;
      viewSourceUtils.viewSource(href, null, doc, line);
    }
  }.bind(this);

  this.view.element.addEventListener("CssRuleViewCSSLinkClicked",
                                     this._cssLinkHandler);

  this._onSelect = this.onSelect.bind(this);
  this.inspector.on("select", this._onSelect);

  this._onChange = this.onChange.bind(this);
  this.inspector.on("change", this._onChange);

  this.onSelect();
}

RuleViewTool.prototype = {
  onSelect: function RVT_onSelect(aEvent, aFrom) {
    let node = this.inspector.selection;
    if (!node) {
      this.view.highlight(null);
      return;
    }

    if (this.inspector.locked) {
      this.view.highlight(node);
    }
  },

  onChange: function RVT_onChange(aEvent, aFrom) {
    // We're not that good yet at refreshing, only
    // refresh when we really need to.
    if (aFrom != "pseudoclass") {
      return;
    }

    if (this.inspector.locked) {
      this.view.nodeChanged();
    }
  },

  destroy: function RVT_destroy() {
    this.inspector.removeListener("select", this._onSelect);
    this.inspector.removeListener("change", this._onChange);
    this.view.element.removeEventListener("CssRuleViewChanged",
                                          this._changeHandler);
    this.view.element.removeEventListener("CssRuleViewCSSLinkClicked",
                                          this._cssLinkHandler);
    this.doc.documentElement.removeChild(this.view.element);

    this.view.destroy();

    delete this._changeHandler;
    delete this.view;
    delete this.doc;
    delete this.inspector;
  }
}

function ComputedViewTool(aInspector, aFrame)
{
  this.inspector = aInspector;
  this.iframe = aFrame;
  this.window = aInspector.chromeWindow;
  this.document = this.window.document;
  this.cssLogic = new CssLogic();
  this.view = new CssHtmlTree(this);

  this._onSelect = this.onSelect.bind(this);
  this.inspector.on("select", this._onSelect);

  this._onChange = this.onChange.bind(this);
  this.inspector.on("change", this._onChange);

  // Since refreshes of the computed view are non-destructive,
  // refresh when the tab is changed so we can notice script-driven
  // changes.
  this.inspector.on("sidebaractivated", this._onChange);

  this.cssLogic.highlight(null);
  this.view.highlight(null);

  this.onSelect();
}

ComputedViewTool.prototype = {
  onSelect: function CVT_onSelect(aEvent)
  {
    if (this.inspector.locked) {
      this.cssLogic.highlight(this.inspector.selection);
      this.view.highlight(this.inspector.selection);
    }
  },

  onChange: function CVT_change(aEvent, aFrom)
  {
    if (aFrom == "computedview" ||
        this.inspector.selection != this.cssLogic.viewedElement) {
      return;
    }

    this.cssLogic.highlight(this.inspector.selection);
    this.view.refreshPanel();
  },

  destroy: function CVT_destroy(aContext)
  {
    this.inspector.removeListener("select", this._onSelect);
    this.inspector.removeListener("change", this._onChange);
    this.inspector.removeListener("sidebaractivated", this._onChange);
    this.view.destroy();
    delete this.view;

    delete this.cssLogic;
    delete this.cssHtmlTree;
    delete this.iframe;
    delete this.window;
    delete this.document;
  }
}

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

RegisterStyleTools();
