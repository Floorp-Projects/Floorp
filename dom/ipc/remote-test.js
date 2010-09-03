dump("Loading remote script!\n");
dump(content + "\n");

var cpm = Components.classes["@mozilla.org/childprocessmessagemanager;1"]
                            .getService(Components.interfaces.nsISyncMessageSender);
cpm.addMessageListener("cpm-async",
  function(m) {
    cpm.sendSyncMessage("ppm-sync");
    dump(content.document.documentElement);
    cpm.sendAsyncMessage("ppm-async");
  });

var Cc = Components.classes;
var Ci = Components.interfaces;
var dshell = content.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIWebNavigation)
                    .QueryInterface(Ci.nsIDocShellTreeItem)
                    .rootTreeItem
                    .QueryInterface(Ci.nsIDocShell);


addEventListener("click",
  function(e) {
    dump(e.target + "\n");
    if (e.target instanceof Components.interfaces.nsIDOMHTMLAnchorElement &&
        dshell == docShell) {
      var retval = sendSyncMessage("linkclick", { href: e.target.href });
      dump(uneval(retval[0]) + "\n");
      // Test here also that both retvals are the same
      sendAsyncMessage("linkclick-reply-object", uneval(retval[0]) == uneval(retval[1]) ? retval[0] : "");
    }
  },
  true);

addMessageListener("chrome-message",
  function(m) {
    dump(uneval(m.json) + "\n");
    sendAsyncMessage("chrome-message-reply", m.json);
  });

addMessageListener("speed-test-start",
  function(m) {
    while (sendSyncMessage("speed-test")[0].message != "done");
  });

addMessageListener("async-echo", function(m) {
  sendAsyncMessage(m.name);
});
