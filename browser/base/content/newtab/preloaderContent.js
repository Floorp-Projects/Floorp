/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

(function () { // bug 673569 workaround :(

const ALLOW_BG_CAPTURES_MSG = "BrowserNewTabPreloader:allowBackgroundCaptures";

addMessageListener(ALLOW_BG_CAPTURES_MSG, function onMsg(msg) {
  removeMessageListener(ALLOW_BG_CAPTURES_MSG, onMsg);

  if (content.document.readyState == "complete") {
    setAllowBackgroundCaptures();
    return;
  }

  // This is the case when preloading is disabled.
  addEventListener("load", function onLoad(event) {
    if (event.target == content.document) {
      removeEventListener("load", onLoad, true);
      setAllowBackgroundCaptures();
    }
  }, true);
});

function setAllowBackgroundCaptures() {
  content.document.documentElement.setAttribute("allow-background-captures",
                                                "true");
}

})();
