/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

//////////////////////////////////////////////////////////////////////////
// Test helpers

let TopSitesTestHelper = {
  get grid() {
    return Browser.selectedBrowser.contentDocument.getElementById("start-topsites-grid");
  },
  get document() {
    return Browser.selectedBrowser.contentDocument;
  },
  setup: function() {
    return Task.spawn(function(){
      if (BrowserUI.isStartTabVisible)
        return;

      yield addTab("about:start");

      yield waitForCondition(() => BrowserUI.isStartTabVisible);
    });
  },
  mockLinks: function th_mockLinks(aLinks) {
    // create link objects where index. corresponds to grid position
    // falsy values are set to null
    let links = (typeof aLinks == "string") ?
                aLinks.split(/\s*,\s*/) : aLinks;

    links = links.map(function (id) {
      return (id) ? {url: "http://example.com/#" + id, title: id} : null;
    });
    return links;
  },
  siteFromNode: function th_siteFromNode(aNode) {
    return {
      url: aNode.getAttribute("value"),
      title: aNode.getAttribute("label")
    }
  },
  clearHistory: function th_clearHistory() {
    PlacesUtils.history.removeAllPages();
  },
  fillHistory: function th_fillHistory(aLinks) {
    return Task.spawn(function(){
      let numLinks = aLinks.length;
      let transitionLink = Ci.nsINavHistoryService.TRANSITION_LINK;

      let updateDeferred = Promise.defer();

      for (let link of aLinks.reverse()) {
        let place = {
          uri: Util.makeURI(link.url),
          title: link.title,
          visits: [{visitDate: Date.now() * 1000, transitionType: transitionLink}]
        };
        try {
          PlacesUtils.asyncHistory.updatePlaces(place, {
            handleError: function (aError) {
              ok(false, "couldn't add visit to history");
              throw new Task.Result(aError);
            },
            handleResult: function () {},
            handleCompletion: function () {
              if(--numLinks <= 0) {
                updateDeferred.resolve(true);
              }
            }
          });
        } catch(e) {
          ok(false, "because: " + e);
        }
      }
      return updateDeferred.promise;
    });
  },

  /**
   * Allows to specify the list of pinned links (that have a fixed position in
   * the grid.
   * @param aLinksPattern the pattern (see below)
   *
   * Example: setPinnedLinks("foo,,bar")
   * Result: 'http://example.com/#foo' is pinned in the first cell. 'http://example.com/#bar' is
   *         pinned in the third cell.
   */
  setPinnedLinks: function th_setPinnedLinks(aLinks) {
    let links = TopSitesTestHelper.mockLinks(aLinks);

    // (we trust that NewTabUtils works, and test our consumption of it)
    // clear all existing pins
    Array.forEach(NewTabUtils.pinnedLinks.links, function(aLink){
      if(aLink)
        NewTabUtils.pinnedLinks.unpin(aLink);
    });

    links.forEach(function(aLink, aIndex){
      if(aLink) {
        NewTabUtils.pinnedLinks.pin(aLink, aIndex);
      }
    });
    NewTabUtils.pinnedLinks.save();
  },

  /**
   * Allows to provide a list of links that is used to construct the grid.
   * @param aLinksPattern the pattern (see below)
   * @param aPinnedLinksPattern the pattern (see below)
   *
   * Example: setLinks("dougal,florence,zebedee")
   * Result: [{url: "http://example.com/#dougal", title: "dougal"},
   *          {url: "http://example.com/#florence", title: "florence"}
   *          {url: "http://example.com/#zebedee", title: "zebedee"}]
   * Example: setLinks("dougal,florence,zebedee","dougal,,zebedee")
   * Result: http://example.com/#dougal is pinned at index 0, http://example.com/#florence at index 2
   */

  setLinks: function th_setLinks(aLinks, aPinnedLinks) {
    let links = TopSitesTestHelper.mockLinks(aLinks);
    if (links.filter(function(aLink){
      return !aLink;
    }).length) {
      throw new Error("null link objects in setLinks");
    }

    return Task.spawn(function() {
      TopSitesTestHelper.clearHistory();

      yield Task.spawn(TopSitesTestHelper.fillHistory(links));

      if(aPinnedLinks) {
        TopSitesTestHelper.setPinnedLinks(aPinnedLinks);
      }

      // reset the TopSites state, have it update its cache with the new data fillHistory put there
      yield TopSites.prepareCache(true);
    });
  },

  updatePagesAndWait: function th_updatePagesAndWait() {
    let deferredUpdate = Promise.defer();
    let updater = {
      update: function() {
        NewTabUtils.allPages.unregister(updater);
        deferredUpdate.resolve(true);
      }
    };
    NewTabUtils.allPages.register(updater);
    setTimeout(function() {
      NewTabUtils.allPages.update();
    }, 0);
    return deferredUpdate.promise;
  },

  tearDown: function th_tearDown() {
    TopSitesTestHelper.clearHistory();
  }
};


//////////////////////////////////////////////////////////////////////////


function test() {
  registerCleanupFunction(TopSitesTestHelper.tearDown);
  runTests();
}

gTests.push({
  desc: "TopSites dependencies",
  run: function() {
    ok(NewTabUtils, "NewTabUtils is truthy");
    ok(TopSites, "TopSites is truthy");
  }
});

gTests.push({
  desc: "load and display top sites",
  setUp: function() {
    yield TopSitesTestHelper.setup();
    let grid = TopSitesTestHelper.grid;

    // setup - set history to known state
    yield TopSitesTestHelper.setLinks("brian,dougal,dylan,ermintrude,florence,moose,sgtsam,train,zebedee,zeebad");

    let arrangedPromise = waitForEvent(grid, "arranged");
    yield TopSitesTestHelper.updatePagesAndWait();
    // pause until the update has fired and the view is finishd updating
    yield arrangedPromise;
  },
  run: function() {
    let grid = TopSitesTestHelper.grid;
    let items = grid.items;
    is(items.length, 8, "should be 8 topsites"); // i.e. not 10
    if(items.length) {
      let firstitem = items[0];
      is(
        firstitem.getAttribute("label"),
        "brian",
        "first item label should be 'brian': " + firstitem.getAttribute("label")
      );
      is(
        firstitem.getAttribute("value"),
        "http://example.com/#brian",
        "first item url should be 'http://example.com/#brian': " + firstitem.getAttribute("url")
      );
    }
  }
});

gTests.push({
  desc: "pinned sites",
  pins: "dangermouse,zebedee,,,dougal",
  setUp: function() {
    yield TopSitesTestHelper.setup();
    // setup - set history to known state
    yield TopSitesTestHelper.setLinks(
      "brian,dougal,dylan,ermintrude,florence,moose,sgtsam,train,zebedee,zeebad",
      this.pins
    );
    // pause until the update has fired and the view is finishd updating
    let arrangedPromise = waitForEvent(TopSitesTestHelper.grid, "arranged");
    yield TopSitesTestHelper.updatePagesAndWait();
    yield arrangedPromise;
  },
  run: function() {
    // test that pinned state of each site as rendered matches our expectations
    let pins = this.pins.split(",");
    let items = TopSitesTestHelper.grid.items;
    is(items.length, 8, "should be 8 topsites in the grid");

    is(TopSitesTestHelper.document.querySelectorAll("#start-topsites-grid > [pinned]").length, 3, "should be 3 children with 'pinned' attribute");
    try {
      Array.forEach(items, function(aItem, aIndex){
        // pinned state should agree with the pins array
        is(
            aItem.hasAttribute("pinned"), !!pins[aIndex],
            "site at index " + aIndex + " was " +aItem.hasAttribute("pinned")
            +", should agree with " + !!pins[aIndex]
        );
        if (pins[aIndex]) {
          is(
            aItem.getAttribute("label"),
            pins[aIndex],
            "pinned site has correct label: " + pins[aIndex] +"=="+ aItem.getAttribute("label")
          );
        }
      }, this);
    } catch(e) {
      ok(false, this.desc + ": Test of pinned state on items failed");
      info("because: " + e.message + "\n" + e.stack);
    }
  }
});

gTests.push({
  desc: "pin site",
  setUp: function() {
    yield TopSitesTestHelper.setup();
    // setup - set history to known state
    yield TopSitesTestHelper.setLinks("sgtsam,train,zebedee,zeebad", []); // nothing initially pinned
    // pause until the update has fired and the view is finishd updating
    let arrangedPromise = waitForEvent(TopSitesTestHelper.grid, "arranged");
    yield TopSitesTestHelper.updatePagesAndWait();
    yield arrangedPromise;
  },
  run: function() {
    // pin a site
    // test that site is pinned as expected
    // and that sites fill positions around it
    let grid = TopSitesTestHelper.grid,
        items = grid.items;
    is(items.length, 4, this.desc + ": should be 4 topsites");

    let tile = grid.items[2],
        url = tile.getAttribute("value"),
        title = tile.getAttribute("label");

    info(this.desc + ": pinning site at index 2");
    TopSites.pinSites([{
          url: url,
          title: title
        }], [2]);

    // pinning shouldn't require re-arranging - just wait for isUpdating flag to flip
    yield waitForCondition(function(){
      return !grid.controller.isUpdating;
    });

    let thirdTile = grid.items[2];
    ok( thirdTile.hasAttribute("pinned"), thirdTile.getAttribute("value")+ " should look pinned" );

    // visit some more sites
    yield TopSitesTestHelper.fillHistory( TopSitesTestHelper.mockLinks("brian,dougal,dylan,ermintrude,florence,moose") );

    // force flush and repopulation of links cache
    yield TopSites.prepareCache(true);
    // pause until the update has fired and the view is finishd updating
    let arrangedPromise = waitForEvent(grid, "arranged");
    yield TopSitesTestHelper.updatePagesAndWait();
    yield arrangedPromise;

    // check zebedee is still pinned at index 2
    is( items[2].getAttribute("label"), "zebedee", "Pinned site remained at its index" );
    ok( items[2].hasAttribute("pinned"), "3rd site should still look pinned" );
  }
});

gTests.push({
  desc: "unpin site",
  pins: ",zebedee",
  setUp: function() {
    yield TopSitesTestHelper.setup();
    // setup - set history to known state
    yield TopSitesTestHelper.setLinks(
      "brian,dougal,dylan,ermintrude,florence,moose,sgtsam,train,zebedee,zeebad",
      this.pins
    );
    // pause until the update has fired and the view is finishd updating
    let arrangedPromise = waitForEvent(TopSitesTestHelper.grid, "arranged");
    yield TopSitesTestHelper.updatePagesAndWait();
    yield arrangedPromise;
  },
  run: function() {
    // unpin a pinned site
    // test that sites are unpinned as expected
    let grid = TopSitesTestHelper.grid,
        items = grid.items;
    is(items.length, 8, this.desc + ": should be 8 topsites");
    let site = {
      url: items[1].getAttribute("value"),
      title: items[1].getAttribute("label")
    };
    // verify assumptions before unpinning this site
    ok( NewTabUtils.pinnedLinks.isPinned(site), "2nd item is pinned" );
    ok( items[1].hasAttribute("pinned"), "2nd item has pinned attribute" );

    // unpinning shouldn't require re-arranging - just wait for isUpdating flag to flip
    TopSites.unpinSites([site]);

    yield waitForCondition(function(){
      return !grid.controller.isUpdating;
    });

    let secondTile = grid.items[1];
    ok( !secondTile.hasAttribute("pinned"), "2nd item should no longer be marked as pinned" );
    ok( !NewTabUtils.pinnedLinks.isPinned(site), "2nd item should no longer be pinned" );
  }
});

gTests.push({
  desc: "block/unblock sites",
  setUp: function() {
    yield TopSitesTestHelper.setup();
    // setup - set topsites to known state
    yield TopSitesTestHelper.setLinks(
      "brian,dougal,dylan,ermintrude,florence,moose,sgtsam,train,zebedee,zeebad,basic,coral",
      ",dougal"
    );
    // pause until the update has fired and the view is finishd updating
    let arrangedPromise = waitForEvent(TopSitesTestHelper.grid, "arranged");
    yield TopSitesTestHelper.updatePagesAndWait();
    yield arrangedPromise;
  },
  run: function() {
    try {
      // block a site
      // test that sites are removed from the grid as expected
      let grid = TopSitesTestHelper.grid,
          items = grid.items;
      is(items.length, 8, this.desc + ": should be 8 topsites");

      let brianSite = TopSitesTestHelper.siteFromNode(items[0]);
      let dougalSite = TopSitesTestHelper.siteFromNode(items[1]);
      let dylanSite = TopSitesTestHelper.siteFromNode(items[2]);

      let arrangedPromise = waitForEvent(grid, "arranged");
      // we'll block brian (he's not pinned)
      TopSites.hideSites([brianSite]);
      // pause until the update has fired and the view is finished updating
      yield arrangedPromise;

      // verify brian is blocked and removed from the grid
      ok( (new Site(brianSite)).blocked, "Site has truthy blocked property" );
      ok( NewTabUtils.blockedLinks.isBlocked(brianSite), "Site was blocked" );
      is( grid.querySelectorAll("[value='"+brianSite.url+"']").length, 0, "Blocked site was removed from grid");

      // make sure the empty slot was backfilled
      is(items.length, 8, this.desc + ": should be 8 topsites");

      arrangedPromise = waitForEvent(grid, "arranged");
      // block dougal,dylan. dougal is currently pinned at index 1
      TopSites.hideSites([dougalSite, dylanSite]);
      // pause until the update has fired and the view is finished updating
      yield arrangedPromise;

      // verify dougal is blocked and removed from the grid
      ok( (new Site(dougalSite)).blocked, "Site has truthy blocked property" );
      ok( NewTabUtils.blockedLinks.isBlocked(dougalSite), "Site was blocked" );
      ok( !NewTabUtils.pinnedLinks.isPinned(dougalSite), "Blocked Site is no longer pinned" );
      is( grid.querySelectorAll("[value='"+dougalSite.url+"']").length, 0, "Blocked site was removed from grid");

      // verify dylan is blocked and removed from the grid
      ok( (new Site(dylanSite)).blocked, "Site has truthy blocked property" );
      ok( NewTabUtils.blockedLinks.isBlocked(dylanSite), "Site was blocked" );
      ok( !NewTabUtils.pinnedLinks.isPinned(dylanSite), "Blocked Site is no longer pinned" );
      is( grid.querySelectorAll("[value='"+dylanSite.url+"']").length, 0, "Blocked site was removed from grid");

      // make sure the empty slots were backfilled
      is(items.length, 8, this.desc + ": should be 8 topsites");

      arrangedPromise = waitForEvent(grid, "arranged");
      TopSites.restoreSites([brianSite, dougalSite, dylanSite]);
      // pause until the update has fired and the view is finished updating
      yield arrangedPromise;

      // verify brian, dougal and dyland are unblocked and back in the grid
      ok( !NewTabUtils.blockedLinks.isBlocked(brianSite), "site was unblocked" );
      is( grid.querySelectorAll("[value='"+brianSite.url+"']").length, 1, "Unblocked site is back in the grid");

      ok( !NewTabUtils.blockedLinks.isBlocked(dougalSite), "site was unblocked" );
      is( grid.querySelectorAll("[value='"+dougalSite.url+"']").length, 1, "Unblocked site is back in the grid");
      // ..and that a previously pinned site is re-pinned after being blocked, then restored
      ok( NewTabUtils.pinnedLinks.isPinned(dougalSite), "Restoring previously pinned site makes it pinned again" );
      is( grid.items[1].getAttribute("value"), dougalSite.url, "Blocked Site restored to pinned index" );

      ok( !NewTabUtils.blockedLinks.isBlocked(dylanSite), "site was unblocked" );
      is( grid.querySelectorAll("[value='"+dylanSite.url+"']").length, 1, "Unblocked site is back in the grid");

    } catch(ex) {

      ok(false, this.desc+": Caught exception in test: " + ex);
      info("trace: " + ex.stack);
    }
  }
});

gTests.push({
  desc: "delete and restore site tiles",
  pins: "brian",
  setUp: function() {
    yield TopSitesTestHelper.setup();
    // setup - set history to known state
    yield TopSitesTestHelper.setLinks(
      "brian,dougal,dylan,ermintrude",
      this.pins
    );
    // pause until the update has fired and the view is finishd updating
    let arrangedPromise = waitForEvent(TopSitesTestHelper.grid, "arranged");
    yield TopSitesTestHelper.updatePagesAndWait();
    yield arrangedPromise;
  },
  run: function() {
    // delete a both pinned and unpinned sites
    // test that sites are removed from the grid
    let grid = TopSitesTestHelper.grid,
        items = grid.items;
    is(items.length, 4, this.desc + ": should be 4 topsites");

    let brianTile = grid.querySelector('richgriditem[value$="brian"]');
    let dougalTile = grid.querySelector('richgriditem[value$="dougal"]')

    // verify assumptions before deleting sites
    ok( brianTile, "Tile for Brian was created");
    ok( dougalTile, "Tile for Dougal was created");

    let brianSite = TopSitesTestHelper.siteFromNode(brianTile);
    let dougalSite = TopSitesTestHelper.siteFromNode(dougalTile);
    ok( NewTabUtils.pinnedLinks.isPinned( brianSite ), "Brian tile is pinned" );

    // select the 2 tiles
    grid.toggleItemSelection(brianTile);
    grid.toggleItemSelection(dougalTile);
    is(grid.selectedItems.length, 2, "2 tiles were selected");

    // pause until the update has fired and the view is finishd updating
    let arrangedPromise = waitForEvent(grid, "arranged");

    // raise a mock context-action event to trigger deletion of the selection
    let event = document.createEvent("Events");
    event.action = "delete";
    event.initEvent("context-action", true, true); // is cancelable
    grid.dispatchEvent(event);

    yield arrangedPromise;

    // those sites are blocked and their tiles have been removed from the grid?
    ok( !grid.querySelector('richgriditem[value="'+brianSite.value+'"]'));
    ok( !grid.querySelector('richgriditem[value="'+dougalSite.value+'"]'));
    ok( NewTabUtils.blockedLinks.isBlocked(brianSite), "Brian site was blocked" );
    ok( NewTabUtils.blockedLinks.isBlocked(dougalSite), "Dougal site was blocked" );
    // with the tiles deleted, selection should be empty
    is( grid.selectedItems.length, 0, "Gris selection is empty after deletion" );

    // raise a mock context-action event to trigger restore
    arrangedPromise = waitForEvent(grid, "arranged");
    event = document.createEvent("Events");
    event.action = "restore";
    event.initEvent("context-action", true, true); // is cancelable
    grid.dispatchEvent(event);

    yield arrangedPromise;
    brianTile = grid.querySelector('richgriditem[value$="brian"]');
    dougalTile = grid.querySelector('richgriditem[value$="dougal"]');

    // those tiles have been restored to the grid?
    ok( brianTile, "First tile was restored to the grid" );
    ok( dougalTile, "2nd tile was restored to the grid" );

    is(grid.selectedItems.length, 2, "2 tiles are still selected");
    is( grid.selectedItems[0], brianTile, "Brian is still selected" );
    is( grid.selectedItems[1], dougalTile, "Dougal is still selected" );
    ok( NewTabUtils.pinnedLinks.isPinned( brianSite ), "Brian tile is still pinned" );
    ok( !NewTabUtils.blockedLinks.isBlocked(brianSite), "Brian site was unblocked" );
    ok( !NewTabUtils.blockedLinks.isBlocked(dougalSite), "Dougal site was unblocked" );

  }
});
