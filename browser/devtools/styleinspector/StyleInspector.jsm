/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/devtools/CssRuleView.jsm");
Cu.import("resource:///modules/devtools/ToolDefinitions.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
                                  "resource:///modules/devtools/gDevTools.jsm");

// This module doesn't currently export any symbols directly, it only
// registers inspector tools.
this.EXPORTED_SYMBOLS = ["RuleViewTool", "ComputedViewTool"];

this.RuleViewTool = function RVT_RuleViewTool(aInspector, aWindow, aIFrame)
{
  this.inspector = aInspector;
  this.doc = aWindow.document;
  this.outerIFrame = aIFrame;

  this.view = new CssRuleView(this.doc);
  this.doc.documentElement.appendChild(this.view.element);

  this._changeHandler = function() {
    this.inspector.markDirty();
  }.bind(this);

  this.view.element.addEventListener("CssRuleViewChanged", this._changeHandler)

  this._cssLinkHandler = function(aEvent) {
    let contentDoc = this.inspector.selection.document;
    let rule = aEvent.detail.rule;
    let line = rule.ruleLine || 0;
    let styleSheet = rule.sheet;
    let styleSheets = contentDoc.styleSheets;
    let contentSheet = false;

    // The style editor can only display stylesheets coming from content because
    // chrome stylesheets are not listed in the editor's stylesheet selector.
    //
    // If the stylesheet is a content stylesheet we send it to the style
    // editor else we display it in the view source window.
    //
    // Array.prototype.indexOf always returns -1 here so we loop through
    // the styleSheets object instead.
    for each (let sheet in styleSheets) {
      if (sheet == styleSheet) {
        contentSheet = true;
        break;
      }
    }

    if (contentSheet)  {
      let target = this.inspector.target;

      if (styleEditorDefinition.isTargetSupported(target)) {
        gDevTools.showToolbox(target, "styleeditor").then(function(toolbox) {
          toolbox.getCurrentPanel().selectStyleSheet(styleSheet, line);
        });
      }
    } else {
      let href = styleSheet ? styleSheet.href : "";
      if (rule.elementStyle.element) {
        href = rule.elementStyle.element.ownerDocument.location.href;
      }
      let viewSourceUtils = this.inspector.viewSourceUtils;
      viewSourceUtils.viewSource(href, null, contentDoc, line);
    }
  }.bind(this);

  this.view.element.addEventListener("CssRuleViewCSSLinkClicked",
                                     this._cssLinkHandler);

  this._onSelect = this.onSelect.bind(this);
  this.inspector.selection.on("detached", this._onSelect);
  this.inspector.selection.on("new-node", this._onSelect);
  this.refresh = this.refresh.bind(this);
  this.inspector.on("layout-change", this.refresh);
  this.inspector.sidebar.on("ruleview-selected", this.refresh);
  this.inspector.selection.on("pseudoclass", this.refresh);
  if (this.inspector.highlighter) {
    this.inspector.highlighter.on("locked", this._onSelect);
  }

  this.onSelect();
}

RuleViewTool.prototype = {
  onSelect: function RVT_onSelect(aEvent) {
    if (!this.inspector.selection.isConnected() ||
        !this.inspector.selection.isElementNode()) {
      this.view.highlight(null);
      return;
    }

    if (!aEvent || aEvent == "new-node") {
      if (this.inspector.selection.reason == "highlighter") {
        this.view.highlight(null);
      } else {
        this.view.highlight(this.inspector.selection.node);
      }
    }

    if (aEvent == "locked") {
      this.view.highlight(this.inspector.selection.node);
    }
  },

  isActive: function RVT_isActive() {
    return this.inspector.sidebar.getCurrentTabID() == "ruleview";
  },

  refresh: function RVT_refresh() {
    if (this.isActive()) {
      this.view.nodeChanged();
    }
  },

  destroy: function RVT_destroy() {
    this.inspector.off("layout-change", this.refresh);
    this.inspector.sidebar.off("ruleview-selected", this.refresh);
    this.inspector.selection.off("pseudoclass", this.refresh);
    this.inspector.selection.off("new-node", this._onSelect);
    if (this.inspector.highlighter) {
      this.inspector.highlighter.off("locked", this._onSelect);
    }

    this.view.element.removeEventListener("CssRuleViewCSSLinkClicked",
      this._cssLinkHandler);

    this.view.element.removeEventListener("CssRuleViewChanged",
      this._changeHandler);

    this.doc.documentElement.removeChild(this.view.element);

    this.view.destroy();

    delete this.outerIFrame;
    delete this.view;
    delete this.doc;
    delete this.inspector;
  }
}

this.ComputedViewTool = function CVT_ComputedViewTool(aInspector, aWindow, aIFrame)
{
  this.inspector = aInspector;
  this.window = aWindow;
  this.document = aWindow.document;
  this.outerIFrame = aIFrame;
  this.cssLogic = new CssLogic();
  this.view = new CssHtmlTree(this);

  this._onSelect = this.onSelect.bind(this);
  this.inspector.selection.on("detached", this._onSelect);
  this.inspector.selection.on("new-node", this._onSelect);
  if (this.inspector.highlighter) {
    this.inspector.highlighter.on("locked", this._onSelect);
  }
  this.refresh = this.refresh.bind(this);
  this.inspector.on("layout-change", this.refresh);
  this.inspector.sidebar.on("computedview-selected", this.refresh);
  this.inspector.selection.on("pseudoclass", this.refresh);

  this.cssLogic.highlight(null);
  this.view.highlight(null);

  this.onSelect();
}

ComputedViewTool.prototype = {
  onSelect: function CVT_onSelect(aEvent)
  {
    if (!this.inspector.selection.isConnected() ||
        !this.inspector.selection.isElementNode()) {
      this.view.highlight(null);
      return;
    }

    if (!aEvent || aEvent == "new-node") {
      if (this.inspector.selection.reason == "highlighter") {
        // FIXME: We should hide view's content
      } else {
        this.cssLogic.highlight(this.inspector.selection.node);
        this.view.highlight(this.inspector.selection.node);
      }
    }

    if (aEvent == "locked") {
      this.cssLogic.highlight(this.inspector.selection.node);
      this.view.highlight(this.inspector.selection.node);
    }
  },

  isActive: function CVT_isActive() {
    return this.inspector.sidebar.getCurrentTabID() == "computedview";
  },

  refresh: function CVT_refresh() {
    if (this.isActive()) {
      this.cssLogic.highlight(this.inspector.selection.node);
      this.view.refreshPanel();
    }
  },

  destroy: function CVT_destroy(aContext)
  {
    this.inspector.off("layout-change", this.refresh);
    this.inspector.sidebar.off("computedview-selected", this.refresh);
    this.inspector.selection.off("pseudoclass", this.refresh);
    this.inspector.selection.off("new-node", this._onSelect);
    if (this.inspector.highlighter) {
      this.inspector.highlighter.off("locked", this._onSelect);
    }

    this.view.destroy();
    delete this.view;

    delete this.outerIFrame;
    delete this.cssLogic;
    delete this.cssHtmlTree;
    delete this.window;
    delete this.document;
    delete this.inspector;
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
