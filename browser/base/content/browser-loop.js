// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// the "exported" symbols
let LoopUI;

XPCOMUtils.defineLazyModuleGetter(this, "injectLoopAPI", "resource:///modules/loop/MozLoopAPI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MozLoopService", "resource:///modules/loop/MozLoopService.jsm");


(function() {

  LoopUI = {
    /**
     * Opens the panel for Loop and sizes it appropriately.
     *
     * @param {event} event The event opening the panel, used to anchor
     *                      the panel to the button which triggers it.
     */
    openCallPanel: function(event) {
      let panel = document.getElementById("loop-panel");
      let anchor = event.target;
      let iframe = document.getElementById("loop-panel-frame");

      if (!iframe) {
        // XXX This should be using SharedFrame (bug 1011392 may do this).
        iframe = document.createElement("iframe");
        iframe.setAttribute("id", "loop-panel-frame");
        iframe.setAttribute("type", "content");
        iframe.setAttribute("class", "loop-frame social-panel-frame");
        iframe.setAttribute("flex", "1");
        panel.appendChild(iframe);
      }

      // We inject in DOMContentLoaded as that is before any scripts have tun.
      iframe.addEventListener("DOMContentLoaded", function documentDOMLoaded() {
        iframe.removeEventListener("DOMContentLoaded", documentDOMLoaded, true);
        injectLoopAPI(iframe.contentWindow);

        // We use loopPanelInitialized so that we know we've finished localising before
        // sizing the panel.
        iframe.contentWindow.addEventListener("loopPanelInitialized",
          function documentLoaded() {
            iframe.contentWindow.removeEventListener("loopPanelInitialized",
                                                     documentLoaded, true);
            // XXX We end up with the wrong size here, so this
            // needs further investigation (bug 1011394).
            sizeSocialPanelToContent(panel, iframe);
          }, true);

      }, true);

      iframe.setAttribute("src", "about:looppanel");
      panel.hidden = false;
      panel.openPopup(anchor, "bottomcenter topright", 0, 0, false, false);
    },

    /**
     * Triggers the initialization of the loop service.  Called by
     * delayedStartup.
     */
    initialize: function() {
      MozLoopService.initialize();
    },

  };
})();
