/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = [ ];

Cu.import("resource:///modules/devtools/gcli.jsm");

/**
 * 'export' command
 */
gcli.addCommand({
  name: "export",
  description: gcli.lookup("exportDesc"),
});

/**
 * The 'export html' command. This command allows the user to export the page to
 * HTML after they do DOM changes.
 */
gcli.addCommand({
  name: "export html",
  description: gcli.lookup("exportHtmlDesc"),
  exec: function(args, context) {
    let document = context.environment.contentDocument;
    let window = document.defaultView;
    let page = document.documentElement.outerHTML;
    window.open('data:text/plain;charset=utf8,' + encodeURIComponent(page));
  }
});
