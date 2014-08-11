/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cc, Cu, Ci} = require("chrome");
const {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});

let ToolDefinitions = require("main").Tools;

Cu.import("resource://gre/modules/Services.jsm");

loader.lazyGetter(this, "gDevTools", () => Cu.import("resource:///modules/devtools/gDevTools.jsm", {}).gDevTools);
loader.lazyGetter(this, "RuleView", () => require("devtools/styleinspector/rule-view"));
loader.lazyGetter(this, "ComputedView", () => require("devtools/styleinspector/computed-view"));
loader.lazyGetter(this, "_strings", () => Services.strings
  .createBundle("chrome://global/locale/devtools/styleinspector.properties"));

const { PREF_ORIG_SOURCES } = require("devtools/styleeditor/utils");

// This module doesn't currently export any symbols directly, it only
// registers inspector tools.

function RuleViewTool(aInspector, aWindow, aIFrame)
{
  this.inspector = aInspector;
  this.doc = aWindow.document;
  this.outerIFrame = aIFrame;

  this.view = new RuleView.CssRuleView(aInspector, this.doc);
  this.doc.documentElement.appendChild(this.view.element);

  this._changeHandler = () => {
    this.inspector.markDirty();
  };

  this.view.element.addEventListener("CssRuleViewChanged", this._changeHandler);

  this._refreshHandler = () => {
    this.inspector.emit("rule-view-refreshed");
  };

  this.view.element.addEventListener("CssRuleViewRefreshed", this._refreshHandler);

  this._cssLinkHandler = (aEvent) => {
    let rule = aEvent.detail.rule;
    let sheet = rule.parentStyleSheet;

    // Chrome stylesheets are not listed in the style editor, so show
    // these sheets in the view source window instead.
    if (!sheet || sheet.isSystem) {
      let contentDoc = this.inspector.selection.document;
      let viewSourceUtils = this.inspector.viewSourceUtils;
      let href = rule.nodeHref || rule.href;
      viewSourceUtils.viewSource(href, null, contentDoc, rule.line || 0);
      return;
    }

    let location = promise.resolve(rule.location);
    if (Services.prefs.getBoolPref(PREF_ORIG_SOURCES)) {
      location = rule.getOriginalLocation();
    }
    location.then(({ href, line, column }) => {
      let target = this.inspector.target;
      if (ToolDefinitions.styleEditor.isTargetSupported(target)) {
        gDevTools.showToolbox(target, "styleeditor").then(function(toolbox) {
          toolbox.getCurrentPanel().selectStyleSheet(href, line, column);
        });
      }
      return;
    })
  }

  this.view.element.addEventListener("CssRuleViewCSSLinkClicked",
                                     this._cssLinkHandler);

  this._onSelect = this.onSelect.bind(this);
  this.inspector.selection.on("detached", this._onSelect);
  this.inspector.selection.on("new-node-front", this._onSelect);
  this.refresh = this.refresh.bind(this);
  this.inspector.on("layout-change", this.refresh);

  this.inspector.selection.on("pseudoclass", this.refresh);

  this._clearUserProperties = this._clearUserProperties.bind(this);
  this.inspector.target.on("navigate", this._clearUserProperties);

  this.onSelect();
}

exports.RuleViewTool = RuleViewTool;

RuleViewTool.prototype = {
  onSelect: function RVT_onSelect(aEvent) {
    if (!this.view) {
      // Skip the event if RuleViewTool has been destroyed.
      return;
    }
    this.view.setPageStyle(this.inspector.pageStyle);

    if (!this.inspector.selection.isConnected() ||
        !this.inspector.selection.isElementNode()) {
      this.view.highlight(null);
      return;
    }

    if (!aEvent || aEvent == "new-node-front") {
      let done = this.inspector.updating("rule-view");
      this.view.highlight(this.inspector.selection.nodeFront).then(done, done);
    }
  },

  refresh: function RVT_refresh() {
    this.view.refreshPanel();
  },

  _clearUserProperties: function() {
    if (this.view && this.view.store && this.view.store.userProperties) {
      this.view.store.userProperties.clear();
    }
  },

  destroy: function RVT_destroy() {
    this.inspector.off("layout-change", this.refresh);
    this.inspector.selection.off("pseudoclass", this.refresh);
    this.inspector.selection.off("new-node-front", this._onSelect);
    this.inspector.target.off("navigate", this._clearUserProperties);

    this.view.element.removeEventListener("CssRuleViewCSSLinkClicked",
      this._cssLinkHandler);

    this.view.element.removeEventListener("CssRuleViewChanged",
      this._changeHandler);

    this.view.element.removeEventListener("CssRuleViewRefreshed",
      this._refreshHandler);

    this.doc.documentElement.removeChild(this.view.element);

    this.view.destroy();

    delete this.outerIFrame;
    delete this.view;
    delete this.doc;
    delete this.inspector;
  }
};

function ComputedViewTool(aInspector, aWindow, aIFrame)
{
  this.inspector = aInspector;
  this.window = aWindow;
  this.document = aWindow.document;
  this.outerIFrame = aIFrame;
  this.view = new ComputedView.CssHtmlTree(this, aInspector.pageStyle);

  this._onSelect = this.onSelect.bind(this);
  this.inspector.selection.on("detached", this._onSelect);
  this.inspector.selection.on("new-node-front", this._onSelect);
  this.refresh = this.refresh.bind(this);
  this.inspector.on("layout-change", this.refresh);
  this.inspector.selection.on("pseudoclass", this.refresh);

  this.view.highlight(null);

  this.onSelect();
}

exports.ComputedViewTool = ComputedViewTool;

ComputedViewTool.prototype = {
  onSelect: function CVT_onSelect(aEvent)
  {
    if (!this.view) {
      // Skip the event if ComputedViewTool has been destroyed.
      return;
    }
    this.view.setPageStyle(this.inspector.pageStyle);

    if (!this.inspector.selection.isConnected() ||
        !this.inspector.selection.isElementNode()) {
      this.view.highlight(null);
      return;
    }

    if (!aEvent || aEvent == "new-node-front") {
      let done = this.inspector.updating("computed-view");
      this.view.highlight(this.inspector.selection.nodeFront).then(() => {
        done();
      });
    }
  },

  refresh: function CVT_refresh() {
    this.view.refreshPanel();
  },

  destroy: function CVT_destroy(aContext)
  {
    this.inspector.off("layout-change", this.refresh);
    this.inspector.sidebar.off("computedview-selected", this.refresh);
    this.inspector.selection.off("pseudoclass", this.refresh);
    this.inspector.selection.off("new-node-front", this._onSelect);

    this.view.destroy();
    delete this.view;

    delete this.outerIFrame;
    delete this.cssLogic;
    delete this.cssHtmlTree;
    delete this.window;
    delete this.document;
    delete this.inspector;
  }
};
