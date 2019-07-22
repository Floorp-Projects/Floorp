/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Frame script that runs in the layout debugger's <browser>.

var gDebuggingTools;

const NS_LAYOUT_DEBUGGINGTOOLS_CONTRACTID =
  "@mozilla.org/layout-debug/layout-debuggingtools;1";

gDebuggingTools = Cc[NS_LAYOUT_DEBUGGINGTOOLS_CONTRACTID].createInstance(
  Ci.nsILayoutDebuggingTools
);
gDebuggingTools.init(content);

addMessageListener("LayoutDebug:Call", function(msg) {
  gDebuggingTools[msg.data.name](msg.data.arg);
});
