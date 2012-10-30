/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [ "addDebuggerToGlobal" ];

/*
 * This is the js module for Debugger. Import it like so:
 *   Components.utils.import("resource://gre/modules/jsdebugger.jsm");
 *   addDebuggerToGlobal(this);
 *
 * This will create a 'Debugger' object, which provides an interface to debug
 * JavaScript code running in other compartments in the same process, on the
 * same thread.
 *
 * For documentation on the API, see:
 *   https://wiki.mozilla.org/Debugger
 */

const init = Components.classes["@mozilla.org/jsdebugger;1"].createInstance(Components.interfaces.IJSDebugger);
this.addDebuggerToGlobal = function addDebuggerToGlobal(global) {
  init.addClass(global);
};
