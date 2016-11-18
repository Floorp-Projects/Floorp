function pushPrefs(...aPrefs) {
  return SpecialPowers.pushPrefEnv({"set": aPrefs});
}

function promiseWaitForEvent(object, eventName, capturing = false, chrome = false) {
  return new Promise((resolve) => {
    function listener(event) {
      info("Saw " + eventName);
      object.removeEventListener(eventName, listener, capturing, chrome);
      resolve(event);
    }

    info("Waiting for " + eventName);
    object.addEventListener(eventName, listener, capturing, chrome);
  });
}

/**
 * Waits for the next load to complete in any browser or the given browser.
 * If a <tabbrowser> is given it waits for a load in any of its browsers.
 *
 * @return promise
 */
function waitForDocLoadComplete(aBrowser=gBrowser) {
  return new Promise(resolve => {
    let listener = {
      onStateChange: function (webProgress, req, flags, status) {
        let docStop = Ci.nsIWebProgressListener.STATE_IS_NETWORK |
                      Ci.nsIWebProgressListener.STATE_STOP;
        info("Saw state " + flags.toString(16) + " and status " + status.toString(16));
        // When a load needs to be retargetted to a new process it is cancelled
        // with NS_BINDING_ABORTED so ignore that case
        if ((flags & docStop) == docStop && status != Cr.NS_BINDING_ABORTED) {
          aBrowser.removeProgressListener(this);
          waitForDocLoadComplete.listeners.delete(this);
          let chan = req.QueryInterface(Ci.nsIChannel);
          info("Browser loaded " + chan.originalURI.spec);
          resolve();
        }
      },
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                             Ci.nsISupportsWeakReference])
    };
    aBrowser.addProgressListener(listener);
    waitForDocLoadComplete.listeners.add(listener);
    info("Waiting for browser load");
  });
}
// Keep a set of progress listeners for waitForDocLoadComplete() to make sure
// they're not GC'ed before we saw the page load.
waitForDocLoadComplete.listeners = new Set();
registerCleanupFunction(() => waitForDocLoadComplete.listeners.clear());
