// Forward iframe loaded event.

/* eslint-env mozilla/frame-script */

addEventListener(
  "frames-loaded",
  () => sendAsyncMessage("test:frames-loaded"),
  true,
  true
);

let requestObserver = {
  observe(subject, topic) {
    if (topic == "http-on-opening-request") {
      // Get DOMWindow on all child docshells to force about:blank
      // content viewers being created.
      getChildDocShells().map(ds => {
        ds
          .QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsILoadContext).associatedWindow;
      });
    }
  },
  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
};
Services.obs.addObserver(requestObserver, "http-on-opening-request");
addEventListener("unload", e => {
  if (e.target == this) {
    Services.obs.removeObserver(requestObserver, "http-on-opening-request");
  }
});

function getChildDocShells() {
  return docShell.getAllDocShellsInSubtree(
    Ci.nsIDocShellTreeItem.typeAll,
    Ci.nsIDocShell.ENUMERATE_FORWARDS
  );
}
