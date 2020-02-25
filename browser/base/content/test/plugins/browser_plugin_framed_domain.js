/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);

/**
 * Verify that giving permission to a plugin works based on the toplevel
 * page's principal, so that permissions meant for framed plugins persist
 * correctly for the duration of the session.
 */
add_task(async function test_toplevel_frame_permission() {
  await BrowserTestUtils.withNewTab(
    gTestRoot + "empty_file.html",
    async browser => {
      // Add a cross-origin iframe and return when it's loaded.
      await SpecialPowers.spawn(browser.browsingContext, [], async function() {
        let doc = content.document;
        let iframe = doc.createElement("iframe");
        let loadPromise = ContentTaskUtils.waitForEvent(iframe, "load");
        iframe.src = doc.location.href.replace(".com/", ".org/");
        doc.body.appendChild(iframe);
        // Note that we cannot return (rather than await) loadPromise, because
        // it resolves with the event, which isn't structured-clonable.
        await loadPromise;
      });

      // Show a plugin notification from the iframe's actor:
      let { currentWindowGlobal } = browser.browsingContext.getChildren()[0];
      let actor = currentWindowGlobal.getActor("Plugin");
      const kHost = Cc["@mozilla.org/plugin/host;1"].getService(
        Ci.nsIPluginHost
      );
      const { PLUGIN_CLICK_TO_PLAY } = Ci.nsIObjectLoadingContent;
      let plugin = kHost.getPluginTags()[0];
      actor.showClickToPlayNotification(
        browser,
        { id: plugin.id, fallbackType: PLUGIN_CLICK_TO_PLAY },
        false /* showNow */
      );

      // Check that it is associated with the toplevel origin (.com), not
      // the subframe's origin (.org):
      let notification = PopupNotifications.getNotification(
        "click-to-play-plugins",
        browser
      );
      is(
        notification.options.principal.URI.host,
        "example.com",
        "Should use top host for permission prompt!"
      );
    }
  );
});
