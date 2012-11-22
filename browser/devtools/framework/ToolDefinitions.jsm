/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "defaultTools" ];

Components.utils.import("resource:///modules/WebConsolePanel.jsm");
Components.utils.import("resource:///modules/devtools/DebuggerPanel.jsm");
Components.utils.import("resource:///modules/devtools/StyleEditorDefinition.jsm");
Components.utils.import("resource:///modules/devtools/InspectorDefinition.jsm");

this.defaultTools = [
  StyleEditorDefinition,
  WebConsoleDefinition,
  DebuggerDefinition,
  InspectorDefinition,
];
