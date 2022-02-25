/**
 * An observer that detects event listener removal triggered by the document.open function.
 * https://stackoverflow.com/questions/37176536/document-open-removes-all-window-listeners/68896987
 */
export default {
  onDetach: {
    addListener: c => void callbacks.add(c),
    hasListener: c =>  callbacks.has(c),
    removeListener: c => callbacks.delete(c)
  }
}

const callbacks = new Set();
const eventName = "listenerStillAttached";

window.addEventListener(eventName, _handleListenerStillAttached);


/**
 * The method document.open removes/replaces the current document.
 * Therefore a MutationObserver is used to get notified when this happens.
 * After that we trigger both the custom event and the timeout.
 * Depending on who wins the "onDetach" callbacks will be called or not.
 */
new MutationObserver((entries) => {
  // check if a new document got attached
  const documentReplaced = entries.some(entry =>
    Array.from(entry.addedNodes).includes(document.documentElement)
  );
  if (documentReplaced) {
    const timeoutId = setTimeout(_handleListenerDetached);
    // test if previously registered event listener will still fire
    window.dispatchEvent(new CustomEvent(eventName, {detail: timeoutId}));
  }
}).observe(document, { childList: true });


/**
 * This function will only be called by the setTimeout method.
 * This happens after the event loop handled all queued event listeners.
 * If the timeout wasn't prevented/canceled from the event listener, the event listener must have been removed.
 * Therefore we call the "onDetach" callbacks and re-register the event listener.
 */
function _handleListenerDetached() {
  // reattach event listener
  window.addEventListener(eventName, _handleListenerStillAttached);
  // run registered callbacks
  callbacks.forEach((callback) => callback());
}


/**
 * This function will only be called by the event listener of the custom event.
 * If this is called the event listener is still attached which means apparently no event listener got removed.
 * Therefore we need to prevent the timeout from calling the "onDetach" callbacks.
 */
function _handleListenerStillAttached(event) {
  clearTimeout(event.detail);
}