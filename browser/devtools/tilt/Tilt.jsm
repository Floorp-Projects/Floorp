/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Tilt: A WebGL-based 3D visualization of a webpage.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Victor Porof <vporof@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 ***** END LICENSE BLOCK *****/

/*global Components, Services, TiltGL, TiltUtils, TiltVisualizer */
"use strict";

const Cu = Components.utils;

// Tilt notifications dispatched through the nsIObserverService.
const TILT_NOTIFICATIONS = {

  // Fires when Tilt completes the initialization.
  INITIALIZED: "tilt-initialized",

  // Fires when Tilt is destroyed.
  DESTROYED: "tilt-destroyed",

  // Fires when Tilt is shown (after a tab-switch).
  SHOWN: "tilt-shown",

  // Fires when Tilt is hidden (after a tab-switch).
  HIDDEN: "tilt-hidden"
};

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/TiltGL.jsm");
Cu.import("resource:///modules/devtools/TiltUtils.jsm");
Cu.import("resource:///modules/devtools/TiltVisualizer.jsm");

let EXPORTED_SYMBOLS = ["Tilt"];

/**
 * Object managing instances of the visualizer.
 *
 * @param {Window} aWindow
 *                 the chrome window used by each visualizer instance
 */
function Tilt(aWindow)
{
  /**
   * Save a reference to the top-level window.
   */
  this.chromeWindow = aWindow;

  /**
   * All the instances of TiltVisualizer.
   */
  this.visualizers = {};

  /**
   * Shortcut for accessing notifications strings.
   */
  this.NOTIFICATIONS = TILT_NOTIFICATIONS;
}

Tilt.prototype = {

  /**
   * Initializes a visualizer for the current tab.
   */
  initialize: function T_initialize()
  {
    let id = this.currentWindowId;

    // if the visualizer for the current tab is already open, destroy it now
    if (this.visualizers[id]) {
      this.destroy(id, true);
      return;
    }

    // create a visualizer instance for the current tab
    this.visualizers[id] = new TiltVisualizer({
      parentNode: this.chromeWindow.gBrowser.selectedBrowser.parentNode,
      contentWindow: this.chromeWindow.gBrowser.selectedBrowser.contentWindow,
      requestAnimationFrame: this.chromeWindow.mozRequestAnimationFrame,
      inspectorUI: this.chromeWindow.InspectorUI
    });

    // make sure the visualizer object was initialized properly
    if (!this.visualizers[id].isInitialized()) {
      this.destroy(id);
      return;
    }

    Services.obs.notifyObservers(null, TILT_NOTIFICATIONS.INITIALIZED, null);
  },

  /**
   * Destroys a specific instance of the visualizer.
   *
   * @param {String} aId
   *                 the identifier of the instance in the visualizers array
   * @param {Boolean} aAnimateFlag
   *                  optional, set to true to display a destruction transition
   */
  destroy: function T_destroy(aId, aAnimateFlag)
  {
    // if the visualizer is already destroyed, don't do anything
    if (!this.visualizers[aId]) {
      return;
    }

    if (!this.isDestroying) {
      this.isDestroying = true;

      let finalize = function T_finalize(aId) {
        this.visualizers[aId].removeOverlay();
        this.visualizers[aId].cleanup();
        this.visualizers[aId] = null;

        this.isDestroying = false;
        this.chromeWindow.gBrowser.selectedBrowser.focus();
        Services.obs.notifyObservers(null, TILT_NOTIFICATIONS.DESTROYED, null);
      };

      if (!aAnimateFlag) {
        finalize.call(this, aId);
        return;
      }

      let controller = this.visualizers[aId].controller;
      let presenter = this.visualizers[aId].presenter;

      TiltUtils.setDocumentZoom(presenter.transforms.zoom);

      let content = presenter.contentWindow;
      let pageXOffset = content.pageXOffset * TiltUtils.getDocumentZoom();
      let pageYOffset = content.pageYOffset * TiltUtils.getDocumentZoom();

      controller.removeEventListeners();
      controller.arcball.reset([-pageXOffset, -pageYOffset]);
      presenter.executeDestruction(finalize.bind(this, aId));
    }
  },

  /**
   * Handles any supplementary post-initialization work, done immediately
   * after a TILT_NOTIFICATIONS.INITIALIZED notification.
   */
  _whenInitialized: function T__whenInitialized()
  {
    this._whenShown();
  },

  /**
   * Handles any supplementary post-destruction work, done immediately
   * after a TILT_NOTIFICATIONS.DESTROYED notification.
   */
  _whenDestroyed: function T__whenDestroyed()
  {
    this._whenHidden();
  },

  /**
   * Handles any necessary changes done when the Tilt surface is shown,
   * after a TILT_NOTIFICATIONS.SHOWN notification.
   */
  _whenShown: function T__whenShown()
  {
    this.tiltButton.checked = true;
  },

  /**
   * Handles any necessary changes done when the Tilt surface is hidden,
   * after a TILT_NOTIFICATIONS.HIDDEN notification.
   */
  _whenHidden: function T__whenHidden()
  {
    this.tiltButton.checked = false;
  },

  /**
   * Handles the event fired when a tab is selected.
   */
  _onTabSelect: function T__onTabSelect()
  {
    if (this.visualizers[this.currentWindowId]) {
      Services.obs.notifyObservers(null, TILT_NOTIFICATIONS.SHOWN, null);
    } else {
      Services.obs.notifyObservers(null, TILT_NOTIFICATIONS.HIDDEN, null);
    }
  },

  /**
   * A node was selected in the Inspector.
   * Called from InspectorUI.
   *
   * @param {Element} aNode
   *                  the newly selected node
   */
  update: function T_update(aNode) {
    let id = this.currentWindowId;

    if (this.visualizers[id]) {
      this.visualizers[id].presenter.highlightNode(aNode);
    }
  },

  /**
   * Add the browser event listeners to handle state changes.
   * Called from InspectorUI.
   */
  setup: function T_setup()
  {
    if (this._setupFinished) {
      return;
    }

    // load the preferences from the devtools.tilt branch
    TiltVisualizer.Prefs.load();

    // hide the button in the Inspector toolbar if Tilt is not enabled
    this.tiltButton.hidden = !this.enabled;

    // add the necessary observers to handle specific notifications
    Services.obs.addObserver(
      this._whenInitialized.bind(this), TILT_NOTIFICATIONS.INITIALIZED, false);
    Services.obs.addObserver(
      this._whenDestroyed.bind(this), TILT_NOTIFICATIONS.DESTROYED, false);
    Services.obs.addObserver(
      this._whenShown.bind(this), TILT_NOTIFICATIONS.SHOWN, false);
    Services.obs.addObserver(
      this._whenHidden.bind(this), TILT_NOTIFICATIONS.HIDDEN, false);
    Services.obs.addObserver(function(aSubject, aTopic, aWinId) {
      this.destroy(aWinId); }.bind(this),
      this.chromeWindow.InspectorUI.INSPECTOR_NOTIFICATIONS.DESTROYED, false);

    this.chromeWindow.gBrowser.tabContainer.addEventListener("TabSelect",
      this._onTabSelect.bind(this), false);


    // FIXME: this shouldn't be done here, see bug #705131
    let onOpened = function() {
      if (this.visualizers[this.currentWindowId]) {
        this.chromeWindow.InspectorUI.stopInspecting();
        this.inspectButton.disabled = true;
        this.highlighterContainer.style.display = "none";
      }
    }.bind(this);

    let onClosed = function() {
      this.inspectButton.disabled = false;
      this.highlighterContainer.style.display = "";
    }.bind(this);

    Services.obs.addObserver(onOpened,
      this.chromeWindow.InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
    Services.obs.addObserver(onClosed,
      this.chromeWindow.InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);
    Services.obs.addObserver(onOpened,
      TILT_NOTIFICATIONS.INITIALIZED, false);
    Services.obs.addObserver(onClosed,
      TILT_NOTIFICATIONS.DESTROYED, false);


    this._setupFinished = true;
  },

  /**
   * Returns true if this tool is enabled.
   */
  get enabled()
  {
    return (TiltVisualizer.Prefs.enabled &&
           (TiltGL.isWebGLForceEnabled() || TiltGL.isWebGLSupported()));
  },

  /**
   * Gets the ID of the current window object to identify the visualizer.
   */
  get currentWindowId()
  {
    let gBrowser = this.chromeWindow.gBrowser;
    return TiltUtils.getWindowId(gBrowser.selectedBrowser.contentWindow);
  },

  /**
   * Gets the Tilt button in the Inspector toolbar.
   */
  get tiltButton()
  {
    return this.chromeWindow.document.getElementById(
      "inspector-3D-button");
  },

  /**
   * Gets the Inspect button in the Inspector toolbar.
   * FIXME: this shouldn't be needed here, remove after bug #705131
   */
  get inspectButton()
  {
    return this.chromeWindow.document.getElementById(
      "inspector-inspect-toolbutton");
  },

  /**
   * Gets the Highlighter contaniner stack.
   * FIXME: this shouldn't be needed here, remove after bug #705131
   */
  get highlighterContainer()
  {
    return this.chromeWindow.document.getElementById(
      "highlighter-container");
  }
};
