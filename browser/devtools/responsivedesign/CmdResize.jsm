/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = [ ];

Cu.import("resource:///modules/devtools/gcli.jsm");

/* Responsive Mode commands */
gcli.addCommand({
  name: 'resize',
  description: gcli.lookup('resizeModeDesc')
});

gcli.addCommand({
  name: 'resize on',
  description: gcli.lookup('resizeModeOnDesc'),
  manual: gcli.lookup('resizeModeManual'),
  exec: gcli_cmd_resize
});

gcli.addCommand({
  name: 'resize off',
  description: gcli.lookup('resizeModeOffDesc'),
  manual: gcli.lookup('resizeModeManual'),
  exec: gcli_cmd_resize
});

gcli.addCommand({
  name: 'resize toggle',
  buttonId: "command-button-responsive",
  buttonClass: "command-button devtools-toolbarbutton",
  tooltipText: gcli.lookup("resizeModeToggleTooltip"),
  description: gcli.lookup('resizeModeToggleDesc'),
  manual: gcli.lookup('resizeModeManual'),
  exec: gcli_cmd_resize
});

gcli.addCommand({
  name: 'resize to',
  description: gcli.lookup('resizeModeToDesc'),
  params: [
    {
      name: 'width',
      type: 'number',
      description: gcli.lookup("resizePageArgWidthDesc"),
    },
    {
      name: 'height',
      type: 'number',
      description: gcli.lookup("resizePageArgHeightDesc"),
    },
  ],
  exec: gcli_cmd_resize
});

function gcli_cmd_resize(args, context) {
  let browserDoc = context.environment.chromeDocument;
  let browserWindow = browserDoc.defaultView;
  let mgr = browserWindow.ResponsiveUI.ResponsiveUIManager;
  mgr.handleGcliCommand(browserWindow,
                        browserWindow.gBrowser.selectedTab,
                        this.name,
                        args);
}
