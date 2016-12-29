/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * If the user has opted into blocking refresh and redirect attempts by
 * default, this handles showing the notification to the user which
 * gives them the option to let the refresh or redirect proceed.
 */
var RefreshBlocker = {
  init() {
    gBrowser.addEventListener("RefreshBlocked", this);
  },

  uninit() {
    gBrowser.removeEventListener("RefreshBlocked", this);
  },

  handleEvent(event) {
    if (event.type == "RefreshBlocked") {
      this.block(event.originalTarget, event.detail);
    }
  },

  /**
   * Shows the blocked refresh / redirect notification for some browser.
   *
   * @param browser (<xul:browser>)
   *        The browser that had the refresh blocked. This will be the browser
   *        for which we'll show the notification on.
   * @param data (object)
   *        An object with the following properties:
   *
   *        URI (string)
   *          The URI that a page is attempting to refresh or redirect to.
   *
   *        delay (int)
   *          The delay (in milliseconds) before the page was going to reload
   *          or redirect.
   *
   *        sameURI (bool)
   *          true if we're refreshing the page. false if we're redirecting.
   *
   *        outerWindowID (int)
   *          The outerWindowID of the frame that requested the refresh or
   *          redirect.
   */
  block(browser, data) {
    let brandBundle = document.getElementById("bundle_brand");
    let brandShortName = brandBundle.getString("brandShortName");
    let message =
      gNavigatorBundle.getFormattedString(data.sameURI ? "refreshBlocked.refreshLabel"
                                                       : "refreshBlocked.redirectLabel",
                                          [brandShortName]);

    let notificationBox = gBrowser.getNotificationBox(browser);
    let notification = notificationBox.getNotificationWithValue("refresh-blocked");

    if (notification) {
      notification.label = message;
    } else {
      let refreshButtonText =
        gNavigatorBundle.getString("refreshBlocked.goButton");
      let refreshButtonAccesskey =
        gNavigatorBundle.getString("refreshBlocked.goButton.accesskey");

      let buttons = [{
        label: refreshButtonText,
        accessKey: refreshButtonAccesskey,
        callback() {
          if (browser.messageManager) {
            browser.messageManager.sendAsyncMessage("RefreshBlocker:Refresh", data);
          }
        }
      }];

      notificationBox.appendNotification(message, "refresh-blocked",
                                         "chrome://browser/skin/Info.png",
                                         notificationBox.PRIORITY_INFO_MEDIUM,
                                         buttons);
    }
  }
};
