"use strict";

/**
 * help.pandora.com - Hide unsupported message in Firefox for Android
 * WebCompat issue #38433 - https://webcompat.com/issues/38433
 *
 * SalesForce Communities are showing unsupported message
 * for help.pandora.com and some more sites. See the full list here:
 * https://github.com/webcompat/web-bugs/issues?utf8=%E2%9C%93&q=doNotShowUnsupportedBrowserModal
 */

console.info(
  "Unsupported message has been hidden for compatibility reasons. See https://webcompat.com/issues/38433 for details."
);

const NOTIFICATIONS_LIMIT = 20;

const createObserver = callback => {
  return new MutationObserver(callback).observe(document, {
    childList: true,
    subtree: true,
  });
};

const removeElementWhenReady = elementId => {
  const element = document.getElementById(elementId);
  if (element) {
    element.remove();
    return;
  }

  let n = 0;
  createObserver(function(records, observer) {
    const _element = document.getElementById(elementId);
    if (_element) {
      _element.remove();
      observer.disconnect();
      return;
    }

    if (n > NOTIFICATIONS_LIMIT) {
      observer.disconnect();
    }

    n++;
  });
};

removeElementWhenReady("community-browser-not-support-message");
