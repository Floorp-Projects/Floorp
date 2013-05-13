/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.EXPORTED_SYMBOLS = [ ];

Components.utils.import('resource://gre/modules/XPCOMUtils.jsm');
Components.utils.import("resource://gre/modules/devtools/gcli.jsm");
Components.utils.import("resource:///modules/devtools/gDevTools.jsm");

// Fetch TiltManager using the current loader, but don't save a
// reference to it, because it might change with a tool reload.
// We can clean this up once the command line is loadered.
Object.defineProperty(this, "TiltManager", {
  get: function() {
    return devtools.require("devtools/tilt/tilt").TiltManager;
  },
  enumerable: true
});

/**
 * 'tilt' command
 */
gcli.addCommand({
  name: 'tilt',
  description: gcli.lookup("tiltDesc"),
  manual: gcli.lookup("tiltManual")
});


/**
 * 'tilt open' command
 */
gcli.addCommand({
  name: 'tilt open',
  description: gcli.lookup("tiltOpenDesc"),
  manual: gcli.lookup("tiltOpenManual"),
  exec: function(args, context) {
    let chromeWindow = context.environment.chromeDocument.defaultView;
    let Tilt = TiltManager.getTiltForBrowser(chromeWindow);
    if (!Tilt.currentInstance) {
      Tilt.toggle();
    }
  }
});


/**
 * 'tilt toggle' command
 */
gcli.addCommand({
  name: "tilt toggle",
  buttonId: "command-button-tilt",
  buttonClass: "command-button",
  tooltipText: gcli.lookup("tiltToggleTooltip"),
  hidden: true,
  state: {
    isChecked: function(aTarget) {
      let browserWindow = aTarget.tab.ownerDocument.defaultView;
      return !!TiltManager.getTiltForBrowser(browserWindow).currentInstance;
    },
    onChange: function(aTarget, aChangeHandler) {
      let browserWindow = aTarget.tab.ownerDocument.defaultView;
      let tilt = TiltManager.getTiltForBrowser(browserWindow);
      tilt.on("change", aChangeHandler);
    },
    offChange: function(aTarget, aChangeHandler) {
      if (aTarget.tab) {
        let browserWindow = aTarget.tab.ownerDocument.defaultView;
        let tilt = TiltManager.getTiltForBrowser(browserWindow);
        tilt.off("change", aChangeHandler);
      }
    },
  },
  exec: function(args, context) {
    let chromeWindow = context.environment.chromeDocument.defaultView;
    let Tilt = TiltManager.getTiltForBrowser(chromeWindow);
    Tilt.toggle();
  }
});


/**
 * 'tilt translate' command
 */
gcli.addCommand({
  name: 'tilt translate',
  description: gcli.lookup("tiltTranslateDesc"),
  manual: gcli.lookup("tiltTranslateManual"),
  params: [
    {
      name: "x",
      type: "number",
      defaultValue: 0,
      description: gcli.lookup("tiltTranslateXDesc"),
      manual: gcli.lookup("tiltTranslateXManual")
    },
    {
      name: "y",
      type: "number",
      defaultValue: 0,
      description: gcli.lookup("tiltTranslateYDesc"),
      manual: gcli.lookup("tiltTranslateYManual")
    }
  ],
  exec: function(args, context) {
    let chromeWindow = context.environment.chromeDocument.defaultView;
    let Tilt = TiltManager.getTiltForBrowser(chromeWindow);
    if (Tilt.currentInstance) {
      Tilt.currentInstance.controller.arcball.translate([args.x, args.y]);
    }
  }
});


/**
 * 'tilt rotate' command
 */
gcli.addCommand({
  name: 'tilt rotate',
  description: gcli.lookup("tiltRotateDesc"),
  manual: gcli.lookup("tiltRotateManual"),
  params: [
    {
      name: "x",
      type: { name: 'number', min: -360, max: 360, step: 10 },
      defaultValue: 0,
      description: gcli.lookup("tiltRotateXDesc"),
      manual: gcli.lookup("tiltRotateXManual")
    },
    {
      name: "y",
      type: { name: 'number', min: -360, max: 360, step: 10 },
      defaultValue: 0,
      description: gcli.lookup("tiltRotateYDesc"),
      manual: gcli.lookup("tiltRotateYManual")
    },
    {
      name: "z",
      type: { name: 'number', min: -360, max: 360, step: 10 },
      defaultValue: 0,
      description: gcli.lookup("tiltRotateZDesc"),
      manual: gcli.lookup("tiltRotateZManual")
    }
  ],
  exec: function(args, context) {
    let chromeWindow = context.environment.chromeDocument.defaultView;
    let Tilt = TiltManager.getTiltForBrowser(chromeWindow);
    if (Tilt.currentInstance) {
      Tilt.currentInstance.controller.arcball.rotate([args.x, args.y, args.z]);
    }
  }
});


/**
 * 'tilt zoom' command
 */
gcli.addCommand({
  name: 'tilt zoom',
  description: gcli.lookup("tiltZoomDesc"),
  manual: gcli.lookup("tiltZoomManual"),
  params: [
    {
      name: "zoom",
      type: { name: 'number' },
      description: gcli.lookup("tiltZoomAmountDesc"),
      manual: gcli.lookup("tiltZoomAmountManual")
    }
  ],
  exec: function(args, context) {
    let chromeWindow = context.environment.chromeDocument.defaultView;
    let Tilt = TiltManager.getTiltForBrowser(chromeWindow);

    if (Tilt.currentInstance) {
      Tilt.currentInstance.controller.arcball.zoom(-args.zoom);
    }
  }
});


/**
 * 'tilt reset' command
 */
gcli.addCommand({
  name: 'tilt reset',
  description: gcli.lookup("tiltResetDesc"),
  manual: gcli.lookup("tiltResetManual"),
  exec: function(args, context) {
    let chromeWindow = context.environment.chromeDocument.defaultView;
    let Tilt = TiltManager.getTiltForBrowser(chromeWindow);

    if (Tilt.currentInstance) {
      Tilt.currentInstance.controller.arcball.reset();
    }
  }
});


/**
 * 'tilt close' command
 */
gcli.addCommand({
  name: 'tilt close',
  description: gcli.lookup("tiltCloseDesc"),
  manual: gcli.lookup("tiltCloseManual"),
  exec: function(args, context) {
    let chromeWindow = context.environment.chromeDocument.defaultView;
    let Tilt = TiltManager.getTiltForBrowser(chromeWindow);

    Tilt.destroy(Tilt.currentWindowId);
  }
});
