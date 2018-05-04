/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  VIEW_NODE_SELECTOR_TYPE,
  VIEW_NODE_PROPERTY_TYPE,
  VIEW_NODE_VALUE_TYPE,
  VIEW_NODE_IMAGE_URL_TYPE,
  VIEW_NODE_LOCATION_TYPE,
} = require("devtools/client/inspector/shared/node-types");

loader.lazyRequireGetter(this, "Menu", "devtools/client/framework/menu");
loader.lazyRequireGetter(this, "MenuItem", "devtools/client/framework/menu-item");
loader.lazyRequireGetter(this, "clipboardHelper", "devtools/shared/platform/clipboard");

const STYLE_INSPECTOR_PROPERTIES = "devtools/shared/locales/styleinspector.properties";
const {LocalizationHelper} = require("devtools/shared/l10n");
const STYLE_INSPECTOR_L10N = new LocalizationHelper(STYLE_INSPECTOR_PROPERTIES);

const PREF_ORIG_SOURCES = "devtools.source-map.client-service.enabled";

/**
 * Style inspector context menu
 *
 * @param {RuleView|ComputedView} view
 *        RuleView or ComputedView instance controlling this menu
 * @param {Object} options
 *        Option menu configuration
 */
function StyleInspectorMenu(view, options) {
  this.view = view;
  this.inspector = this.view.inspector;
  this.styleDocument = this.view.styleDocument;
  this.styleWindow = this.view.styleWindow;

  this.isRuleView = options.isRuleView;

  this._onAddNewRule = this._onAddNewRule.bind(this);
  this._onCopy = this._onCopy.bind(this);
  this._onCopyColor = this._onCopyColor.bind(this);
  this._onCopyImageDataUrl = this._onCopyImageDataUrl.bind(this);
  this._onCopyLocation = this._onCopyLocation.bind(this);
  this._onCopyPropertyDeclaration = this._onCopyPropertyDeclaration.bind(this);
  this._onCopyPropertyName = this._onCopyPropertyName.bind(this);
  this._onCopyPropertyValue = this._onCopyPropertyValue.bind(this);
  this._onCopyRule = this._onCopyRule.bind(this);
  this._onCopySelector = this._onCopySelector.bind(this);
  this._onCopyUrl = this._onCopyUrl.bind(this);
  this._onSelectAll = this._onSelectAll.bind(this);
  this._onToggleOrigSources = this._onToggleOrigSources.bind(this);
}

module.exports = StyleInspectorMenu;

StyleInspectorMenu.prototype = {
  /**
   * Display the style inspector context menu
   */
  show: function(event) {
    try {
      this._openMenu({
        target: event.explicitOriginalTarget,
        screenX: event.screenX,
        screenY: event.screenY,
      });
    } catch (e) {
      console.error(e);
    }
  },

  _openMenu: function({ target, screenX = 0, screenY = 0 } = { }) {
    // In the sidebar we do not have this.styleDocument.popupNode
    // so we need to save the node ourselves.
    this.styleDocument.popupNode = target;
    this.styleWindow.focus();

    let menu = new Menu();

    let menuitemCopy = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copy"),
      accesskey: STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copy.accessKey"),
      click: () => {
        this._onCopy();
      },
      disabled: !this._hasTextSelected(),
    });
    let menuitemCopyLocation = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copyLocation"),
      click: () => {
        this._onCopyLocation();
      },
      visible: false,
    });
    let menuitemCopyRule = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copyRule"),
      click: () => {
        this._onCopyRule();
      },
      visible: this.isRuleView,
    });
    let copyColorAccessKey = "styleinspector.contextmenu.copyColor.accessKey";
    let menuitemCopyColor = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copyColor"),
      accesskey: STYLE_INSPECTOR_L10N.getStr(copyColorAccessKey),
      click: () => {
        this._onCopyColor();
      },
      visible: this._isColorPopup(),
    });
    let copyUrlAccessKey = "styleinspector.contextmenu.copyUrl.accessKey";
    let menuitemCopyUrl = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copyUrl"),
      accesskey: STYLE_INSPECTOR_L10N.getStr(copyUrlAccessKey),
      click: () => {
        this._onCopyUrl();
      },
      visible: this._isImageUrl(),
    });
    let copyImageAccessKey = "styleinspector.contextmenu.copyImageDataUrl.accessKey";
    let menuitemCopyImageDataUrl = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copyImageDataUrl"),
      accesskey: STYLE_INSPECTOR_L10N.getStr(copyImageAccessKey),
      click: () => {
        this._onCopyImageDataUrl();
      },
      visible: this._isImageUrl(),
    });
    let copyPropDeclarationLabel = "styleinspector.contextmenu.copyPropertyDeclaration";
    let menuitemCopyPropertyDeclaration = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr(copyPropDeclarationLabel),
      click: () => {
        this._onCopyPropertyDeclaration();
      },
      visible: false,
    });
    let menuitemCopyPropertyName = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copyPropertyName"),
      click: () => {
        this._onCopyPropertyName();
      },
      visible: false,
    });
    let menuitemCopyPropertyValue = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copyPropertyValue"),
      click: () => {
        this._onCopyPropertyValue();
      },
      visible: false,
    });
    let menuitemCopySelector = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copySelector"),
      click: () => {
        this._onCopySelector();
      },
      visible: false,
    });

    this._clickedNodeInfo = this._getClickedNodeInfo();
    if (this.isRuleView && this._clickedNodeInfo) {
      switch (this._clickedNodeInfo.type) {
        case VIEW_NODE_PROPERTY_TYPE :
          menuitemCopyPropertyDeclaration.visible = true;
          menuitemCopyPropertyName.visible = true;
          break;
        case VIEW_NODE_VALUE_TYPE :
          menuitemCopyPropertyDeclaration.visible = true;
          menuitemCopyPropertyValue.visible = true;
          break;
        case VIEW_NODE_SELECTOR_TYPE :
          menuitemCopySelector.visible = true;
          break;
        case VIEW_NODE_LOCATION_TYPE :
          menuitemCopyLocation.visible = true;
          break;
      }
    }

    menu.append(menuitemCopy);
    menu.append(menuitemCopyLocation);
    menu.append(menuitemCopyRule);
    menu.append(menuitemCopyColor);
    menu.append(menuitemCopyUrl);
    menu.append(menuitemCopyImageDataUrl);
    menu.append(menuitemCopyPropertyDeclaration);
    menu.append(menuitemCopyPropertyName);
    menu.append(menuitemCopyPropertyValue);
    menu.append(menuitemCopySelector);

    menu.append(new MenuItem({
      type: "separator",
    }));

    // Select All
    let selectAllAccessKey = "styleinspector.contextmenu.selectAll.accessKey";
    let menuitemSelectAll = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.selectAll"),
      accesskey: STYLE_INSPECTOR_L10N.getStr(selectAllAccessKey),
      click: () => {
        this._onSelectAll();
      },
    });
    menu.append(menuitemSelectAll);

    menu.append(new MenuItem({
      type: "separator",
    }));

    // Add new rule
    let addRuleAccessKey = "styleinspector.contextmenu.addNewRule.accessKey";
    let menuitemAddRule = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.addNewRule"),
      accesskey: STYLE_INSPECTOR_L10N.getStr(addRuleAccessKey),
      click: () => {
        this._onAddNewRule();
      },
      visible: this.isRuleView,
      disabled: !this.isRuleView ||
                this.inspector.selection.isAnonymousNode(),
    });
    menu.append(menuitemAddRule);

    // Show Original Sources
    let sourcesAccessKey = "styleinspector.contextmenu.toggleOrigSources.accessKey";
    let menuitemSources = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.toggleOrigSources"),
      accesskey: STYLE_INSPECTOR_L10N.getStr(sourcesAccessKey),
      click: () => {
        this._onToggleOrigSources();
      },
      type: "checkbox",
      checked: Services.prefs.getBoolPref(PREF_ORIG_SOURCES),
    });
    menu.append(menuitemSources);

    menu.popup(screenX, screenY, this.inspector._toolbox);
    return menu;
  },

  _hasTextSelected: function() {
    let hasTextSelected;
    let selection = this.styleWindow.getSelection();

    let node = this._getClickedNode();
    if (node.nodeName == "input" || node.nodeName == "textarea") {
      let { selectionStart, selectionEnd } = node;
      hasTextSelected = isFinite(selectionStart) && isFinite(selectionEnd)
        && selectionStart !== selectionEnd;
    } else {
      hasTextSelected = selection.toString() && !selection.isCollapsed;
    }

    return hasTextSelected;
  },

  /**
   * Get the type of the currently clicked node
   */
  _getClickedNodeInfo: function() {
    let node = this._getClickedNode();
    return this.view.getNodeInfo(node);
  },

  /**
   * A helper that determines if the popup was opened with a click to a color
   * value and saves the color to this._colorToCopy.
   *
   * @return {Boolean}
   *         true if click on color opened the popup, false otherwise.
   */
  _isColorPopup: function() {
    this._colorToCopy = "";

    let container = this._getClickedNode();
    if (!container) {
      return false;
    }

    let isColorNode = el => el.dataset && "color" in el.dataset;

    while (!isColorNode(container)) {
      container = container.parentNode;
      if (!container) {
        return false;
      }
    }

    this._colorToCopy = container.dataset.color;
    return true;
  },

  _isPropertyName: function() {
    let nodeInfo = this._getClickedNodeInfo();
    if (!nodeInfo) {
      return false;
    }
    return nodeInfo.type == VIEW_NODE_PROPERTY_TYPE;
  },

  /**
   * Check if the current node (clicked node) is an image URL
   *
   * @return {Boolean} true if the node is an image url
   */
  _isImageUrl: function() {
    let nodeInfo = this._getClickedNodeInfo();
    if (!nodeInfo) {
      return false;
    }
    return nodeInfo.type == VIEW_NODE_IMAGE_URL_TYPE;
  },

  /**
   * Get the DOM Node container for the current popupNode.
   * If popupNode is a textNode, return the parent node, otherwise return
   * popupNode itself.
   *
   * @return {DOMNode}
   */
  _getClickedNode: function() {
    let container = null;
    let node = this.styleDocument.popupNode;

    if (node) {
      let isTextNode = node.nodeType == node.TEXT_NODE;
      container = isTextNode ? node.parentElement : node;
    }

    return container;
  },

  /**
   * Select all text.
   */
  _onSelectAll: function() {
    let selection = this.styleWindow.getSelection();
    selection.selectAllChildren(this.view.element);
  },

  /**
   * Copy the most recently selected color value to clipboard.
   */
  _onCopy: function() {
    this.view.copySelection(this.styleDocument.popupNode);
  },

  /**
   * Copy the most recently selected color value to clipboard.
   */
  _onCopyColor: function() {
    clipboardHelper.copyString(this._colorToCopy);
  },

  /*
   * Retrieve the url for the selected image and copy it to the clipboard
   */
  _onCopyUrl: function() {
    if (!this._clickedNodeInfo) {
      return;
    }

    clipboardHelper.copyString(this._clickedNodeInfo.value.url);
  },

  /**
   * Retrieve the image data for the selected image url and copy it to the
   * clipboard
   */
  async _onCopyImageDataUrl() {
    if (!this._clickedNodeInfo) {
      return;
    }

    let message;
    try {
      let inspectorFront = this.inspector.inspector;
      let imageUrl = this._clickedNodeInfo.value.url;
      let data = await inspectorFront.getImageDataFromURL(imageUrl);
      message = await data.data.string();
    } catch (e) {
      message =
        STYLE_INSPECTOR_L10N.getStr("styleinspector.copyImageDataUrlError");
    }

    clipboardHelper.copyString(message);
  },

  /**
   * Add a new rule to the current element.
   */
  _onAddNewRule: function() {
    this.view._onAddRule();
  },

  /**
   * Copy the rule source location of the current clicked node.
   */
  _onCopyLocation: function() {
    if (!this._clickedNodeInfo) {
      return;
    }

    clipboardHelper.copyString(this._clickedNodeInfo.value);
  },

  /**
   * Copy the rule property declaration of the current clicked node.
   */
  _onCopyPropertyDeclaration: function() {
    if (!this._clickedNodeInfo) {
      return;
    }

    let textProp = this._clickedNodeInfo.value.textProperty;
    clipboardHelper.copyString(textProp.stringifyProperty());
  },

  /**
   * Copy the rule property name of the current clicked node.
   */
  _onCopyPropertyName: function() {
    if (!this._clickedNodeInfo) {
      return;
    }

    clipboardHelper.copyString(this._clickedNodeInfo.value.property);
  },

  /**
   * Copy the rule property value of the current clicked node.
   */
  _onCopyPropertyValue: function() {
    if (!this._clickedNodeInfo) {
      return;
    }

    clipboardHelper.copyString(this._clickedNodeInfo.value.value);
  },

  /**
   * Copy the rule of the current clicked node.
   */
  _onCopyRule: function() {
    let ruleEditor =
      this.styleDocument.popupNode.parentNode.offsetParent._ruleEditor;
    let rule = ruleEditor.rule;
    clipboardHelper.copyString(rule.stringifyRule());
  },

  /**
   * Copy the rule selector of the current clicked node.
   */
  _onCopySelector: function() {
    if (!this._clickedNodeInfo) {
      return;
    }

    clipboardHelper.copyString(this._clickedNodeInfo.value);
  },

  /**
   *  Toggle the original sources pref.
   */
  _onToggleOrigSources: function() {
    let isEnabled = Services.prefs.getBoolPref(PREF_ORIG_SOURCES);
    Services.prefs.setBoolPref(PREF_ORIG_SOURCES, !isEnabled);
  },

  destroy: function() {
    this.popupNode = null;
    this.styleDocument.popupNode = null;
    this.view = null;
    this.inspector = null;
    this.styleDocument = null;
    this.styleWindow = null;
  }
};
