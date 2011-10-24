/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let pb = Cc["@mozilla.org/privatebrowsing;1"].
         getService(Ci.nsIPrivateBrowsingService);

function test() {
  let thumbnailsSaved = false;

  waitForExplicitFinish();

  registerCleanupFunction(function () {
    ok(thumbnailsSaved, "thumbs have been saved before entering pb mode");
    pb.privateBrowsingEnabled = false;
  });

  afterAllTabsLoaded(function () {
    showTabView(function () {
      hideTabView(function () {
        let numConditions = 2;

        function check() {
          if (--numConditions)
            return;

          togglePrivateBrowsing(finish);
        }

        let tabItem = gBrowser.tabs[0]._tabViewTabItem;

        // save all thumbnails synchronously to cancel all delayed thumbnail
        // saves that might be active
        tabItem.saveThumbnail({synchronously: true});

        // force a tabCanvas paint to flag the thumbnail as dirty
        tabItem.tabCanvas.paint();

        tabItem.addSubscriber("savedCachedImageData", function onSaved() {
          tabItem.removeSubscriber("savedCachedImageData", onSaved);
          thumbnailsSaved = true;
          check();
        });

        togglePrivateBrowsing(check);
      });
    });
  });
}
