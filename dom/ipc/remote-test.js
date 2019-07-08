/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

dump("Loading remote script!\n");
dump(content + "\n");

var cpm = Cc["@mozilla.org/childprocessmessagemanager;1"].getService();
cpm.addMessageListener("cpm-async", function(m) {
  cpm.sendSyncMessage("ppm-sync");
  dump(content.document.documentElement);
  cpm.sendAsyncMessage("ppm-async");
});

var dshell = content.docShell.rootTreeItem.QueryInterface(Ci.nsIDocShell);

addEventListener(
  "click",
  function(e) {
    dump(e.target + "\n");
    if (
      ChromeUtils.getClassName(e.target) === "HTMLAnchorElement" &&
      dshell == docShell
    ) {
      var retval = docShell.messageManager.sendSyncMessage("linkclick", {
        href: e.target.href,
      });
      dump(uneval(retval[0]) + "\n");
      // Test here also that both retvals are the same
      sendAsyncMessage(
        "linkclick-reply-object",
        uneval(retval[0]) == uneval(retval[1]) ? retval[0] : ""
      );
    }
  },
  true
);

addMessageListener("chrome-message", function(m) {
  dump(uneval(m.json) + "\n");
  sendAsyncMessage("chrome-message-reply", m.json);
});

addMessageListener("speed-test-start", function(m) {
  // eslint-disable-next-line no-empty
  while (sendSyncMessage("speed-test")[0].message != "done") {}
});

addMessageListener("async-echo", function(m) {
  sendAsyncMessage(m.name);
});
