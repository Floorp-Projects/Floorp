// Forward iframe loaded event.
addEventListener("frames-loaded",
  e => sendAsyncMessage("test:frames-loaded"), true, true);

let requestObserver = {
  observe(subject, topic, data) {
    if (topic == "http-on-opening-request") {
      // Get DOMWindow on all child docshells to force about:blank
      // content viewers being created.
      getChildDocShells().map(ds => {
        let window = ds.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsILoadContext)
                      .associatedWindow;
      });
    }
  },
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIObserver
  ])
}
Services.obs.addObserver(requestObserver, "http-on-opening-request");
addEventListener("unload", e => {
  if (e.target == this) {
    Services.obs.removeObserver(requestObserver, "http-on-opening-request");
  }
});

function getChildDocShells() {
  let docShellsEnum = docShell.getDocShellEnumerator(
    Ci.nsIDocShellTreeItem.typeAll,
    Ci.nsIDocShell.ENUMERATE_FORWARDS
  );

  let docShells = [];
  while (docShellsEnum.hasMoreElements()) {
    let ds = docShellsEnum.getNext();
    docShells.push(ds);
  }
  return docShells;
}
