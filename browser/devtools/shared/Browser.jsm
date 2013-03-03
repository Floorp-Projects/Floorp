/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Define various constants to match the globals provided by the browser.
 * This module helps cases where code is shared between the web and Firefox.
 * See also Console.jsm for an implementation of the Firefox console that
 * forwards to dump();
 */

this.EXPORTED_SYMBOLS = [ "Node", "HTMLElement", "setTimeout", "clearTimeout" ];

/**
 * Expose Node/HTMLElement objects. This allows us to use the Node constants
 * without resorting to hardcoded numbers
 */
this.Node = Components.interfaces.nsIDOMNode;
this.HTMLElement = Components.interfaces.nsIDOMHTMLElement;

/*
 * Import and re-export the timeout functions from Timer.jsm.
 */
let Timer = Components.utils.import("resource://gre/modules/Timer.jsm", {});
this.setTimeout = Timer.setTimeout;
this.clearTimeout = Timer.clearTimeout;
