/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Cu, Ci} = require("chrome");
const promise = require("promise");
const {Tools} = require("devtools/client/main");
Cu.import("resource://gre/modules/Services.jsm");
const {PREF_ORIG_SOURCES} = require("devtools/client/styleeditor/utils");

loader.lazyGetter(this, "gDevTools", () =>
  Cu.import("resource:///modules/devtools/client/framework/gDevTools.jsm", {}).gDevTools);
loader.lazyGetter(this, "RuleView",
  () => require("devtools/client/styleinspector/rule-view"));
loader.lazyGetter(this, "ComputedView",
  () => require("devtools/client/styleinspector/computed-view"));
loader.lazyGetter(this, "_strings", () => Services.strings
  .createBundle("chrome://global/locale/devtools/styleinspector.properties"));

// This module doesn't currently export any symbols directly, it only
// registers inspector tools.

function RuleViewTool(inspector, window) {
  this.inspector = inspector;
  this.document = window.document;

  this.view = new RuleView.CssRuleView(this.inspector, this.document);

  this.onLinkClicked = this.onLinkClicked.bind(this);
  this.onSelected = this.onSelected.bind(this);
  this.refresh = this.refresh.bind(this);
  this.clearUserProperties = this.clearUserProperties.bind(this);
  this.onPropertyChanged = this.onPropertyChanged.bind(this);
  this.onViewRefreshed = this.onViewRefreshed.bind(this);
  this.onPanelSelected = this.onPanelSelected.bind(this);

  this.view.on("ruleview-changed", this.onPropertyChanged);
  this.view.on("ruleview-refreshed", this.onViewRefreshed);
  this.view.on("ruleview-linked-clicked", this.onLinkClicked);

  this.inspector.selection.on("detached", this.onSelected);
  this.inspector.selection.on("new-node-front", this.onSelected);
  this.inspector.on("layout-change", this.refresh);
  this.inspector.selection.on("pseudoclass", this.refresh);
  this.inspector.target.on("navigate", this.clearUserProperties);
  this.inspector.sidebar.on("ruleview-selected", this.onPanelSelected);
  this.inspector.pageStyle.on("stylesheet-updated", this.refresh);

  this.onSelected();
}

RuleViewTool.prototype = {
  isSidebarActive: function() {
    if (!this.view) {
      return false;
    }
    return this.inspector.sidebar.getCurrentTabID() == "ruleview";
  },

  onSelected: function(event) {
    // Ignore the event if the view has been destroyed, or if it's inactive.
    // But only if the current selection isn't null. If it's been set to null,
    // let the update go through as this is needed to empty the view on
    // navigation.
    if (!this.view) {
      return;
    }

    let isInactive = !this.isSidebarActive() &&
                     this.inspector.selection.nodeFront;
    if (isInactive) {
      return;
    }

    this.view.setPageStyle(this.inspector.pageStyle);

    if (!this.inspector.selection.isConnected() ||
        !this.inspector.selection.isElementNode()) {
      this.view.selectElement(null);
      return;
    }

    if (!event || event == "new-node-front") {
      let done = this.inspector.updating("rule-view");
      this.view.selectElement(this.inspector.selection.nodeFront)
        .then(done, done);
    }
  },

  refresh: function() {
    if (this.isSidebarActive()) {
      this.view.refreshPanel();
    }
  },

  clearUserProperties: function() {
    if (this.view && this.view.store && this.view.store.userProperties) {
      this.view.store.userProperties.clear();
    }
  },

  onPanelSelected: function() {
    if (this.inspector.selection.nodeFront === this.view.viewedElement) {
      this.refresh();
    } else {
      this.onSelected();
    }
  },

  onLinkClicked: function(e, rule) {
    let sheet = rule.parentStyleSheet;

    // Chrome stylesheets are not listed in the style editor, so show
    // these sheets in the view source window instead.
    if (!sheet || sheet.isSystem) {
      let href = rule.nodeHref || rule.href;
      let toolbox = gDevTools.getToolbox(this.inspector.target);
      toolbox.viewSource(href, rule.line);
      return;
    }

    let location = promise.resolve(rule.location);
    if (Services.prefs.getBoolPref(PREF_ORIG_SOURCES)) {
      location = rule.getOriginalLocation();
    }
    location.then(({ source, href, line, column }) => {
      let target = this.inspector.target;
      if (Tools.styleEditor.isTargetSupported(target)) {
        gDevTools.showToolbox(target, "styleeditor").then(function(toolbox) {
          let sheet = source || href;
          toolbox.getCurrentPanel().selectStyleSheet(sheet, line, column);
        });
      }
      return;
    });
  },

  onPropertyChanged: function() {
    this.inspector.markDirty();
  },

  onViewRefreshed: function() {
    this.inspector.emit("rule-view-refreshed");
  },

  destroy: function() {
    this.inspector.off("layout-change", this.refresh);
    this.inspector.selection.off("pseudoclass", this.refresh);
    this.inspector.selection.off("new-node-front", this.onSelected);
    this.inspector.target.off("navigate", this.clearUserProperties);
    this.inspector.sidebar.off("ruleview-selected", this.onPanelSelected);
    if (this.inspector.pageStyle) {
      this.inspector.pageStyle.off("stylesheet-updated", this.refresh);
    }

    this.view.off("ruleview-linked-clicked", this.onLinkClicked);
    this.view.off("ruleview-changed", this.onPropertyChanged);
    this.view.off("ruleview-refreshed", this.onViewRefreshed);

    this.view.destroy();

    this.view = this.document = this.inspector = null;
  }
};

function ComputedViewTool(inspector, window) {
  this.inspector = inspector;
  this.document = window.document;

  this.view = new ComputedView.CssComputedView(this.inspector, this.document,
                                               this.inspector.pageStyle);

  this.onSelected = this.onSelected.bind(this);
  this.refresh = this.refresh.bind(this);
  this.onPanelSelected = this.onPanelSelected.bind(this);

  this.inspector.selection.on("detached", this.onSelected);
  this.inspector.selection.on("new-node-front", this.onSelected);
  this.inspector.on("layout-change", this.refresh);
  this.inspector.selection.on("pseudoclass", this.refresh);
  this.inspector.sidebar.on("computedview-selected", this.onPanelSelected);
  this.inspector.pageStyle.on("stylesheet-updated", this.refresh);

  this.view.selectElement(null);

  this.onSelected();
}

ComputedViewTool.prototype = {
  isSidebarActive: function() {
    if (!this.view) {
      return false;
    }
    return this.inspector.sidebar.getCurrentTabID() == "computedview";
  },

  onSelected: function(event) {
    // Ignore the event if the view has been destroyed, or if it's inactive.
    // But only if the current selection isn't null. If it's been set to null,
    // let the update go through as this is needed to empty the view on
    // navigation.
    if (!this.view) {
      return;
    }

    let isInactive = !this.isSidebarActive() &&
                     this.inspector.selection.nodeFront;
    if (isInactive) {
      return;
    }

    this.view.setPageStyle(this.inspector.pageStyle);

    if (!this.inspector.selection.isConnected() ||
        !this.inspector.selection.isElementNode()) {
      this.view.selectElement(null);
      return;
    }

    if (!event || event == "new-node-front") {
      let done = this.inspector.updating("computed-view");
      this.view.selectElement(this.inspector.selection.nodeFront).then(() => {
        done();
      });
    }
  },

  refresh: function() {
    if (this.isSidebarActive()) {
      this.view.refreshPanel();
    }
  },

  onPanelSelected: function() {
    if (this.inspector.selection.nodeFront === this.view.viewedElement) {
      this.refresh();
    } else {
      this.onSelected();
    }
  },

  destroy: function() {
    this.inspector.off("layout-change", this.refresh);
    this.inspector.sidebar.off("computedview-selected", this.refresh);
    this.inspector.selection.off("pseudoclass", this.refresh);
    this.inspector.selection.off("new-node-front", this.onSelected);
    this.inspector.sidebar.off("computedview-selected", this.onPanelSelected);
    if (this.inspector.pageStyle) {
      this.inspector.pageStyle.off("stylesheet-updated", this.refresh);
    }

    this.view.destroy();

    this.view = this.document = this.inspector = null;
  }
};

exports.RuleViewTool = RuleViewTool;
exports.ComputedViewTool = ComputedViewTool;
