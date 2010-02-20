dump("Loading remote script!\n");
dump(content + "\n");

addEventListener("click",
  function(e) {
    dump(e.target + "\n");
    if (e.target instanceof Components.interfaces.nsIDOMHTMLAnchorElement) {
      var retval = sendSyncMessage("linkclick", { href : e.target.href });
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
