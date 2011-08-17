(function () {
  let ifaceReq = docShell.QueryInterface(Ci.nsIInterfaceRequestor);
  let webProgress = ifaceReq.getInterface(Ci.nsIWebProgress);

  let WebProgressListener = {
    onStateChange: function WebProgressListener_onStateChange(
      webProgress, request, flag, status) {

      if (flag & Ci.nsIWebProgressListener.STATE_START &&
          flag & Ci.nsIWebProgressListener.STATE_IS_WINDOW) {
        // ensure the dom window is the top one
        return (webProgress.DOMWindow.parent == webProgress.DOMWindow);
      }
    },

    // ----------
    // Implements progress listener interface.
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                           Ci.nsISupportsWeakReference])
  };

  // add web progress listener
  webProgress.addProgressListener(WebProgressListener, Ci.nsIWebProgress.NOTIFY_STATE_ALL);

  addMessageListener("bug-678816-kill-webProgressListener", function () {
    webProgress.removeProgressListener(WebProgressListener);
  });
})();
