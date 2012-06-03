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
 * The Original Code is the Mozilla Responsive UI Module.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Paul Rouget <paul@mozilla.com> (original author)
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

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var EXPORTED_SYMBOLS = ["ResponsiveUIManager"];

const MIN_WIDTH = 50;
const MIN_HEIGHT = 50;

let ResponsiveUIManager = {
  /**
   * Check if the a tab is in a responsive mode.
   * Leave the responsive mode if active,
   * active the responsive mode if not active.
   *
   * @param aWindow the main window.
   * @param aTab the tab targeted.
   */
  toggle: function(aWindow, aTab) {
    if (aTab.responsiveUI) {
      aTab.responsiveUI.close();
    } else {
      aTab.responsiveUI = new ResponsiveUI(aWindow, aTab);
    }
  },
}

let presets =  [
  // Phones
  {width: 320, height: 480},    // iPhone, B2G, with <meta viewport>
  {width: 360, height: 640},    // Android 4, phones, with <meta viewport>

  // Tablets
  {width: 768, height: 1024},   // iPad, with <meta viewport>
  {width: 800, height: 1280},   // Android 4, Tablet, with <meta viewport>

  // Default width for mobile browsers, no <meta viewport>
  {width: 980, height: 1280},

  // Computer
  {width: 1280, height: 600},
  {width: 1920, height: 900},
];

function ResponsiveUI(aWindow, aTab)
{
  this.mainWindow = aWindow;
  this.tab = aTab;
  this.browser = aTab.linkedBrowser;
  this.chromeDoc = aWindow.document;
  this.container = aWindow.gBrowser.getBrowserContainer(this.browser);
  this.stack = this.container.querySelector("[anonid=browserStack]");

  // Try to load presets from prefs
  if (Services.prefs.prefHasUserValue("devtools.responsiveUI.presets")) {
    try {
      presets = JSON.parse(Services.prefs.getCharPref("devtools.responsiveUI.presets"));
    } catch(e) {
      // User pref is malformated.
      Cu.reportError("Could not parse pref `devtools.responsiveUI.presets`: " + e);
    }
  }

  if (Array.isArray(presets)) {
    this.presets = [{custom: true}].concat(presets)
  } else {
    Cu.reportError("Presets value (devtools.responsiveUI.presets) is malformated.");
    this.presets = [{custom: true}];
  }

  // Default size. The first preset (custom) is the one that will be used.
  let bbox = this.stack.getBoundingClientRect();
  this.presets[0].width = bbox.width - 40; // horizontal padding of the container
  this.presets[0].height = bbox.height - 80; // vertical padding + toolbar height
  this.currentPreset = 0; // Custom

  this.container.setAttribute("responsivemode", "true");
  this.stack.setAttribute("responsivemode", "true");

  // Let's bind some callbacks.
  this.bound_presetSelected = this.presetSelected.bind(this);
  this.bound_rotate = this.rotate.bind(this);
  this.bound_startResizing = this.startResizing.bind(this);
  this.bound_stopResizing = this.stopResizing.bind(this);
  this.bound_onDrag = this.onDrag.bind(this);
  this.bound_onKeypress = this.onKeypress.bind(this);

  // Events
  this.tab.addEventListener("TabClose", this);
  this.tab.addEventListener("TabAttrModified", this);
  this.mainWindow.addEventListener("keypress", this.bound_onKeypress, true);

  this.buildUI();
  this.checkMenus();
}

ResponsiveUI.prototype = {
  _transitionsEnabled: true,
  get transitionsEnabled() this._transitionsEnabled,
  set transitionsEnabled(aValue) {
    this._transitionsEnabled = aValue;
    if (aValue && !this._resizing && this.stack.hasAttribute("responsivemode")) {
      this.stack.removeAttribute("notransition");
    } else if (!aValue) {
      this.stack.setAttribute("notransition", "true");
    }
  },

  /**
   * Destroy the nodes. Remove listeners. Reset the style.
   */
  close: function RUI_unload() {
    this.unCheckMenus();
    // Reset style of the stack.
    let style = "max-width: none;" +
                "min-width: 0;" +
                "max-height: none;" +
                "min-height: 0;";
    this.stack.setAttribute("style", style);

    this.stopResizing();

    // Remove listeners.
    this.mainWindow.removeEventListener("keypress", this.bound_onKeypress, true);
    this.menulist.removeEventListener("select", this.bound_presetSelected, true);
    this.tab.removeEventListener("TabClose", this);
    this.tab.removeEventListener("TabAttrModified", this);
    this.rotatebutton.removeEventListener("command", this.bound_rotate, true);

    // Removed elements.
    this.container.removeChild(this.toolbar);
    this.stack.removeChild(this.resizer);
    this.stack.removeChild(this.resizeBar);

    // Unset the responsive mode.
    this.container.removeAttribute("responsivemode");
    this.stack.removeAttribute("responsivemode");

    delete this.tab.responsiveUI;
  },

  /**
   * Handle keypressed.
   *
   * @param aEvent
   */
  onKeypress: function RUI_onKeypress(aEvent) {
    if (aEvent.keyCode == this.mainWindow.KeyEvent.DOM_VK_ESCAPE &&
        this.mainWindow.gBrowser.selectedBrowser == this.browser) {
      aEvent.preventDefault();
      aEvent.stopPropagation();
      this.close();
    }
  },

  /**
   * Handle events
   */
  handleEvent: function (aEvent) {
    switch (aEvent.type) {
      case "TabClose":
        this.close();
        break;
      case "TabAttrModified":
        if (this.mainWindow.gBrowser.selectedBrowser == this.browser) {
          this.checkMenus();
        } else {
          this.unCheckMenus();
        }
        break;
    }
  },

  /**
   * Check the menu items.
   */
   checkMenus: function RUI_checkMenus() {
     this.chromeDoc.getElementById("Tools:ResponsiveUI").setAttribute("checked", "true");
   },

  /**
   * Uncheck the menu items.
   */
   unCheckMenus: function RUI_unCheckMenus() {
     this.chromeDoc.getElementById("Tools:ResponsiveUI").setAttribute("checked", "false");
   },

  /**
   * Build the toolbar and the resizers.
   *
   * <vbox anonid="browserContainer"> From tabbrowser.xml
   *  <toolbar class="devtools-toolbar devtools-responsiveui-toolbar">
   *    <menulist class="devtools-menulist"/> // presets
   *    <toolbarbutton tabindex="0" class="devtools-toolbarbutton" label="rotate"/> // rotate
   *  </toolbar>
   *  <stack anonid="browserStack"> From tabbrowser.xml
   *    <browser/>
   *    <box class="devtools-responsiveui-resizehandle" bottom="0" right="0"/>
   *    <box class="devtools-responsiveui-resizebar" top="0" right="0"/>
   *  </stack>
   * </vbox>
   */
  buildUI: function RUI_buildUI() {
    // Toolbar
    this.toolbar = this.chromeDoc.createElement("toolbar");
    this.toolbar.className = "devtools-toolbar devtools-responsiveui-toolbar";

    this.menulist = this.chromeDoc.createElement("menulist");
    this.menulist.className = "devtools-menulist";

    this.menulist.addEventListener("select", this.bound_presetSelected, true);

    let menupopup = this.chromeDoc.createElement("menupopup");
    this.registerPresets(menupopup);
    this.menulist.appendChild(menupopup);

    this.rotatebutton = this.chromeDoc.createElement("toolbarbutton");
    this.rotatebutton.setAttribute("tabindex", "0");
    this.rotatebutton.setAttribute("label", this.strings.GetStringFromName("responsiveUI.rotate"));
    this.rotatebutton.className = "devtools-toolbarbutton";
    this.rotatebutton.addEventListener("command", this.bound_rotate, true);

    this.toolbar.appendChild(this.menulist);
    this.toolbar.appendChild(this.rotatebutton);

    // Resizers
    this.resizer = this.chromeDoc.createElement("box");
    this.resizer.className = "devtools-responsiveui-resizehandle";
    this.resizer.setAttribute("right", "0");
    this.resizer.setAttribute("bottom", "0");
    this.resizer.onmousedown = this.bound_startResizing;

    this.resizeBar =  this.chromeDoc.createElement("box");
    this.resizeBar.className = "devtools-responsiveui-resizebar";
    this.resizeBar.setAttribute("top", "0");
    this.resizeBar.setAttribute("right", "0");
    this.resizeBar.onmousedown = this.bound_startResizing;

    this.container.insertBefore(this.toolbar, this.stack);
    this.stack.appendChild(this.resizer);
    this.stack.appendChild(this.resizeBar);
  },

  /**
   * Build the presets list and append it to the menupopup.
   *
   * @param aParent menupopup.
   */
  registerPresets: function RUI_registerPresets(aParent) {
    let fragment = this.chromeDoc.createDocumentFragment();
    let doc = this.chromeDoc;
    let self = this;
    this.presets.forEach(function(preset) {
        let menuitem = doc.createElement("menuitem");
        self.setMenuLabel(menuitem, preset);
        fragment.appendChild(menuitem);
    });
    aParent.appendChild(fragment);
  },

  /**
   * Set the menuitem label of a preset.
   *
   * @param aMenuitem menuitem to edit.
   * @param aPreset associated preset.
   */
  setMenuLabel: function RUI_setMenuLabel(aMenuitem, aPreset) {
    let size = Math.round(aPreset.width) + "x" + Math.round(aPreset.height);
    if (aPreset.custom) {
      let str = this.strings.formatStringFromName("responsiveUI.customResolution", [size], 1);
      aMenuitem.setAttribute("label", str);
    } else {
      aMenuitem.setAttribute("label", size);
    }
  },

  /**
   * When a preset is selected, apply it.
   */
  presetSelected: function RUI_presetSelected() {
    this.currentPreset = this.menulist.selectedIndex;
    let preset = this.presets[this.currentPreset];
    this.loadPreset(preset);
  },

  /**
   * Apply a preset.
   *
   * @param aPreset preset to apply.
   */
  loadPreset: function RUI_loadPreset(aPreset) {
    this.setSize(aPreset.width, aPreset.height);
  },

  /**
   * Swap width and height.
   */
  rotate: function RUI_rotate() {
    this.setSize(this.currentHeight, this.currentWidth);
  },

  /**
   * Change the size of the browser.
   *
   * @param aWidth width of the browser.
   * @param aHeight height of the browser.
   */
  setSize: function RUI_setSize(aWidth, aHeight) {
    this.currentWidth = aWidth;
    this.currentHeight = aHeight;

    // We resize the containing stack.
    let style = "max-width: %width;" +
                "min-width: %width;" +
                "max-height: %height;" +
                "min-height: %height;";

    style = style.replace(/%width/g, this.currentWidth + "px");
    style = style.replace(/%height/g, this.currentHeight + "px");

    this.stack.setAttribute("style", style);

    if (!this.ignoreY)
      this.resizeBar.setAttribute("top", Math.round(this.currentHeight / 2));

    // We uptate the Custom menuitem if we are not using a preset.
    if (this.presets[this.currentPreset].custom) {
      let preset = this.presets[this.currentPreset];
      preset.width = this.currentWidth;
      preset.height = this.currentHeight;

      let menuitem = this.menulist.firstChild.childNodes[this.currentPreset];
      this.setMenuLabel(menuitem, preset);
    }
  },

  /**
   * Start the process of resizing the browser.
   *
   * @param aEvent
   */
  startResizing: function RUI_startResizing(aEvent) {
    let preset = this.presets[this.currentPreset];
    if (!preset.custom) {
      this.currentPreset = 0;
      preset = this.presets[0];
      preset.width = this.currentWidth;
      preset.height = this.currentHeight;
      let menuitem = this.menulist.firstChild.childNodes[0];
      this.setMenuLabel(menuitem, preset);
      this.menulist.selectedIndex = 0;
    }
    this.mainWindow.addEventListener("mouseup", this.bound_stopResizing, true);
    this.mainWindow.addEventListener("mousemove", this.bound_onDrag, true);
    this.container.style.pointerEvents = "none";

    this._resizing = true;
    this.stack.setAttribute("notransition", "true");

    this.lastClientX = aEvent.clientX;
    this.lastClientY = aEvent.clientY;

    this.ignoreY = (aEvent.target === this.resizeBar);
  },

  /**
   * Resizing on mouse move.
   *
   * @param aEvent
   */
  onDrag: function RUI_onDrag(aEvent) {
    let deltaX = aEvent.clientX - this.lastClientX;
    let deltaY = aEvent.clientY - this.lastClientY;

    if (this.ignoreY)
      deltaY = 0;

    let width = this.currentWidth + deltaX;
    let height = this.currentHeight + deltaY;

    if (width < MIN_WIDTH) {
        width = MIN_WIDTH;
    } else {
        this.lastClientX = aEvent.clientX;
    }

    if (height < MIN_HEIGHT) {
        height = MIN_HEIGHT;
    } else {
        this.lastClientY = aEvent.clientY;
    }

    this.setSize(width, height);
  },

  /**
   * Stop End resizing
   */
  stopResizing: function RUI_stopResizing() {
    this.container.style.pointerEvents = "auto";

    this.mainWindow.removeEventListener("mouseup", this.bound_stopResizing, true);
    this.mainWindow.removeEventListener("mousemove", this.bound_onDrag, true);

    delete this._resizing;
    if (this.transitionsEnabled) {
      this.stack.removeAttribute("notransition");
    }
    this.ignoreY = false;
  },
}

XPCOMUtils.defineLazyGetter(ResponsiveUI.prototype, "strings", function () {
  return Services.strings.createBundle("chrome://browser/locale/devtools/responsiveUI.properties");
});
