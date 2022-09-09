/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  VIEW_NODE_SELECTOR_TYPE,
  VIEW_NODE_PROPERTY_TYPE,
  VIEW_NODE_VALUE_TYPE,
  VIEW_NODE_IMAGE_URL_TYPE,
  VIEW_NODE_LOCATION_TYPE,
} = require("devtools/client/inspector/shared/node-types");

loader.lazyRequireGetter(this, "Menu", "devtools/client/framework/menu");
loader.lazyRequireGetter(
  this,
  "MenuItem",
  "devtools/client/framework/menu-item"
);
loader.lazyRequireGetter(
  this,
  "getRuleFromNode",
  "devtools/client/inspector/rules/utils/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "clipboardHelper",
  "devtools/shared/platform/clipboard"
);

const STYLE_INSPECTOR_PROPERTIES =
  "devtools/shared/locales/styleinspector.properties";
const { LocalizationHelper } = require("devtools/shared/l10n");
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
function StyleInspectorMenu(view, { isRuleView = false } = {}) {
  this.view = view;
  this.inspector = this.view.inspector;
  this.styleWindow = this.view.styleWindow || this.view.doc.defaultView;
  this.isRuleView = isRuleView;

  this._onCopy = this._onCopy.bind(this);
  this._onCopyColor = this._onCopyColor.bind(this);
  this._onCopyImageDataUrl = this._onCopyImageDataUrl.bind(this);
  this._onCopyLocation = this._onCopyLocation.bind(this);
  this._onCopyDeclaration = this._onCopyDeclaration.bind(this);
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
  show(event) {
    try {
      this._openMenu({
        target: event.target,
        screenX: event.screenX,
        screenY: event.screenY,
      });
    } catch (e) {
      console.error(e);
    }
  },

  _openMenu({ target, screenX = 0, screenY = 0 } = {}) {
    this.currentTarget = target;
    this.styleWindow.focus();

    const menu = new Menu();

    const menuitemCopy = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copy"),
      accesskey: STYLE_INSPECTOR_L10N.getStr(
        "styleinspector.contextmenu.copy.accessKey"
      ),
      click: () => {
        this._onCopy();
      },
      disabled: !this._hasTextSelected(),
    });
    const menuitemCopyLocation = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr(
        "styleinspector.contextmenu.copyLocation"
      ),
      click: () => {
        this._onCopyLocation();
      },
      visible: false,
    });
    const menuitemCopyRule = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copyRule"),
      click: () => {
        this._onCopyRule();
      },
      visible: this.isRuleView,
    });
    const copyColorAccessKey = "styleinspector.contextmenu.copyColor.accessKey";
    const menuitemCopyColor = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr(
        "styleinspector.contextmenu.copyColor"
      ),
      accesskey: STYLE_INSPECTOR_L10N.getStr(copyColorAccessKey),
      click: () => {
        this._onCopyColor();
      },
      visible: this._isColorPopup(),
    });
    const copyUrlAccessKey = "styleinspector.contextmenu.copyUrl.accessKey";
    const menuitemCopyUrl = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copyUrl"),
      accesskey: STYLE_INSPECTOR_L10N.getStr(copyUrlAccessKey),
      click: () => {
        this._onCopyUrl();
      },
      visible: this._isImageUrl(),
    });
    const copyImageAccessKey =
      "styleinspector.contextmenu.copyImageDataUrl.accessKey";
    const menuitemCopyImageDataUrl = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr(
        "styleinspector.contextmenu.copyImageDataUrl"
      ),
      accesskey: STYLE_INSPECTOR_L10N.getStr(copyImageAccessKey),
      click: () => {
        this._onCopyImageDataUrl();
      },
      visible: this._isImageUrl(),
    });
    const copyDeclarationLabel = "styleinspector.contextmenu.copyDeclaration";
    const menuitemCopyDeclaration = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr(copyDeclarationLabel),
      click: () => {
        this._onCopyDeclaration();
      },
      visible: false,
    });
    const menuitemCopyPropertyName = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr(
        "styleinspector.contextmenu.copyPropertyName"
      ),
      click: () => {
        this._onCopyPropertyName();
      },
      visible: false,
    });
    const menuitemCopyPropertyValue = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr(
        "styleinspector.contextmenu.copyPropertyValue"
      ),
      click: () => {
        this._onCopyPropertyValue();
      },
      visible: false,
    });
    const menuitemCopySelector = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr(
        "styleinspector.contextmenu.copySelector"
      ),
      click: () => {
        this._onCopySelector();
      },
      visible: false,
    });

    this._clickedNodeInfo = this._getClickedNodeInfo();
    if (this.isRuleView && this._clickedNodeInfo) {
      switch (this._clickedNodeInfo.type) {
        case VIEW_NODE_PROPERTY_TYPE:
          menuitemCopyDeclaration.visible = true;
          menuitemCopyPropertyName.visible = true;
          break;
        case VIEW_NODE_VALUE_TYPE:
          menuitemCopyDeclaration.visible = true;
          menuitemCopyPropertyValue.visible = true;
          break;
        case VIEW_NODE_SELECTOR_TYPE:
          menuitemCopySelector.visible = true;
          break;
        case VIEW_NODE_LOCATION_TYPE:
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
    menu.append(menuitemCopyDeclaration);
    menu.append(menuitemCopyPropertyName);
    menu.append(menuitemCopyPropertyValue);
    menu.append(menuitemCopySelector);

    menu.append(
      new MenuItem({
        type: "separator",
      })
    );

    // Select All
    const selectAllAccessKey = "styleinspector.contextmenu.selectAll.accessKey";
    const menuitemSelectAll = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr(
        "styleinspector.contextmenu.selectAll"
      ),
      accesskey: STYLE_INSPECTOR_L10N.getStr(selectAllAccessKey),
      click: () => {
        this._onSelectAll();
      },
    });
    menu.append(menuitemSelectAll);

    menu.append(
      new MenuItem({
        type: "separator",
      })
    );

    // Add new rule
    const addRuleAccessKey = "styleinspector.contextmenu.addNewRule.accessKey";
    const menuitemAddRule = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr(
        "styleinspector.contextmenu.addNewRule"
      ),
      accesskey: STYLE_INSPECTOR_L10N.getStr(addRuleAccessKey),
      click: () => this.view._onAddRule(),
      visible: this.isRuleView,
      disabled: !this.isRuleView || this.inspector.selection.isAnonymousNode(),
    });
    menu.append(menuitemAddRule);

    // Show Original Sources
    const sourcesAccessKey =
      "styleinspector.contextmenu.toggleOrigSources.accessKey";
    const menuitemSources = new MenuItem({
      label: STYLE_INSPECTOR_L10N.getStr(
        "styleinspector.contextmenu.toggleOrigSources"
      ),
      accesskey: STYLE_INSPECTOR_L10N.getStr(sourcesAccessKey),
      click: () => {
        this._onToggleOrigSources();
      },
      type: "checkbox",
      checked: Services.prefs.getBoolPref(PREF_ORIG_SOURCES),
    });
    menu.append(menuitemSources);

    menu.popup(screenX, screenY, this.inspector.toolbox.doc);
    return menu;
  },

  _hasTextSelected() {
    let hasTextSelected;
    const selection = this.styleWindow.getSelection();

    const node = this._getClickedNode();
    if (node.nodeName == "input" || node.nodeName == "textarea") {
      const { selectionStart, selectionEnd } = node;
      hasTextSelected =
        isFinite(selectionStart) &&
        isFinite(selectionEnd) &&
        selectionStart !== selectionEnd;
    } else {
      hasTextSelected = selection.toString() && !selection.isCollapsed;
    }

    return hasTextSelected;
  },

  /**
   * Get the type of the currently clicked node
   */
  _getClickedNodeInfo() {
    const node = this._getClickedNode();
    return this.view.getNodeInfo(node);
  },

  /**
   * A helper that determines if the popup was opened with a click to a color
   * value and saves the color to this._colorToCopy.
   *
   * @return {Boolean}
   *         true if click on color opened the popup, false otherwise.
   */
  _isColorPopup() {
    this._colorToCopy = "";

    const container = this._getClickedNode();
    if (!container) {
      return false;
    }

    const colorNode = container.closest("[data-color]");
    if (!colorNode) {
      return false;
    }

    this._colorToCopy = colorNode.dataset.color;
    return true;
  },

  /**
   * Check if the current node (clicked node) is an image URL
   *
   * @return {Boolean} true if the node is an image url
   */
  _isImageUrl() {
    const nodeInfo = this._getClickedNodeInfo();
    if (!nodeInfo) {
      return false;
    }
    return nodeInfo.type == VIEW_NODE_IMAGE_URL_TYPE;
  },

  /**
   * Get the DOM Node container for the current target node.
   * If the target node is a text node, return the parent node, otherwise return
   * the target node itself.
   *
   * @return {DOMNode}
   */
  _getClickedNode() {
    const node = this.currentTarget;

    if (!node) {
      return null;
    }

    return node.nodeType === node.TEXT_NODE ? node.parentElement : node;
  },

  /**
   * Select all text.
   */
  _onSelectAll() {
    const selection = this.styleWindow.getSelection();

    if (this.isRuleView) {
      selection.selectAllChildren(
        this.currentTarget.closest("#ruleview-container-focusable")
      );
    } else {
      selection.selectAllChildren(this.view.element);
    }
  },

  /**
   * Copy the most recently selected color value to clipboard.
   */
  _onCopy() {
    this.view.copySelection(this.currentTarget);
  },

  /**
   * Copy the most recently selected color value to clipboard.
   */
  _onCopyColor() {
    clipboardHelper.copyString(this._colorToCopy);
  },

  /*
   * Retrieve the url for the selected image and copy it to the clipboard
   */
  _onCopyUrl() {
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
      const inspectorFront = this.inspector.inspectorFront;
      const imageUrl = this._clickedNodeInfo.value.url;
      const data = await inspectorFront.getImageDataFromURL(imageUrl);
      message = await data.data.string();
    } catch (e) {
      message = STYLE_INSPECTOR_L10N.getStr(
        "styleinspector.copyImageDataUrlError"
      );
    }

    clipboardHelper.copyString(message);
  },

  /**
   * Copy the rule source location of the current clicked node.
   */
  _onCopyLocation() {
    if (!this._clickedNodeInfo) {
      return;
    }

    clipboardHelper.copyString(this._clickedNodeInfo.value);
  },

  /**
   * Copy the CSS declaration of the current clicked node.
   */
  _onCopyDeclaration() {
    if (!this._clickedNodeInfo) {
      return;
    }

    const textProp = this._clickedNodeInfo.value.textProperty;
    clipboardHelper.copyString(textProp.stringifyProperty());
  },

  /**
   * Copy the rule property name of the current clicked node.
   */
  _onCopyPropertyName() {
    if (!this._clickedNodeInfo) {
      return;
    }

    clipboardHelper.copyString(this._clickedNodeInfo.value.property);
  },

  /**
   * Copy the rule property value of the current clicked node.
   */
  _onCopyPropertyValue() {
    if (!this._clickedNodeInfo) {
      return;
    }

    clipboardHelper.copyString(this._clickedNodeInfo.value.value);
  },

  /**
   * Copy the rule of the current clicked node.
   */
  _onCopyRule() {
    const node = this._getClickedNode();
    const rule = getRuleFromNode(node, this.view._elementStyle);
    clipboardHelper.copyString(rule.stringifyRule());
  },

  /**
   * Copy the rule selector of the current clicked node.
   */
  _onCopySelector() {
    if (!this._clickedNodeInfo) {
      return;
    }

    clipboardHelper.copyString(this._clickedNodeInfo.value);
  },

  /**
   * Toggle the original sources pref.
   */
  _onToggleOrigSources() {
    const isEnabled = Services.prefs.getBoolPref(PREF_ORIG_SOURCES);
    Services.prefs.setBoolPref(PREF_ORIG_SOURCES, !isEnabled);
  },

  destroy() {
    this.currentTarget = null;
    this.view = null;
    this.inspector = null;
    this.styleWindow = null;
  },
};
