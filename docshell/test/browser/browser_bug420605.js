/* Test for Bug 420605
 * https://bugzilla.mozilla.org/show_bug.cgi?id=420605
 */

function test() {
    waitForExplicitFinish();

    var pageurl = "http://mochi.test:8888/browser/docshell/test/browser/file_bug420605.html";
    var fragmenturl = "http://mochi.test:8888/browser/docshell/test/browser/file_bug420605.html#firefox";

    var historyService = Cc["@mozilla.org/browser/nav-history-service;1"]
                         .getService(Ci.nsINavHistoryService);

    /* Queries nsINavHistoryService and returns a single history entry
     * for a given URI */
    function getNavHistoryEntry(aURI) {
        var options = historyService.getNewQueryOptions();
        options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY;
        options.maxResults = 1;

        var query = historyService.getNewQuery();
        query.uri = aURI;

        var result = historyService.executeQuery(query, options);
        result.root.containerOpen = true;

        if (!result.root.childCount) {
            return null;
        }
        return result.root.getChild(0);
    }

    // We'll save the favicon URL of the orignal page here and check that the
    // page with a hash has the same favicon.
    var originalFavicon;

    // Control flow in this test is a bit complicated.
    //
    // When the page loads, onPageLoad (the DOMContentLoaded handler) and
    // historyObserver::onPageChanged are both called, in some order.  Once
    // they've both run, we click a fragment link in the content page
    // (clickLinkIfReady), which should trigger another onPageChanged event,
    // this time for the fragment's URL.

    var _clickLinkTimes = 0;
    function clickLinkIfReady() {
      _clickLinkTimes++;
      if (_clickLinkTimes == 2) {
        BrowserTestUtils.synthesizeMouseAtCenter('#firefox-link', {},
                                                 gBrowser.selectedBrowser);
      }
    }

    /* Global history observer that triggers for the two test URLs above. */
    var historyObserver = {
        onBeginUpdateBatch: function() {},
        onEndUpdateBatch: function() {},
        onVisit: function(aURI, aVisitID, aTime, aSessionId, aReferringId,
                          aTransitionType, _added) {},
        onTitleChanged: function(aURI, aPageTitle) {},
        onDeleteURI: function(aURI) {},
        onClearHistory: function() {},
        onPageChanged: function(aURI, aWhat, aValue) {
            if (aWhat != Ci.nsINavHistoryObserver.ATTRIBUTE_FAVICON) {
                return;
            }
            aURI = aURI.spec;
            switch (aURI) {
            case pageurl:
                ok(aValue, "Favicon value is not null for page without fragment.");
                originalFavicon = aValue;

                // Now that the favicon has loaded, click on fragment link.
                // This should trigger the |case fragmenturl| below.
                clickLinkIfReady();

                return;
            case fragmenturl:
                // If the fragment URL's favicon isn't set, this branch won't
                // be called and the test will time out.

                is(aValue, originalFavicon, "New favicon should be same as original favicon.");

                // Let's explicitly check that we can get the favicon
                // from nsINavHistoryService now.
                let info = getNavHistoryEntry(makeURI(aURI));
                ok(info, "There must be a history entry for the fragment.");
                ok(info.icon, "The history entry must have an associated favicon.");
                historyService.removeObserver(historyObserver, false);
                gBrowser.removeCurrentTab();
                finish();
            }
        },
        onPageExpired: function(aURI, aVisitTime, aWholeEntry) {},
        QueryInterface: function(iid) {
            if (iid.equals(Ci.nsINavHistoryObserver) ||
                iid.equals(Ci.nsISupports)) {
                return this;
            }
            throw Cr.NS_ERROR_NO_INTERFACE;
        }
    };
    historyService.addObserver(historyObserver);

    function onPageLoad() {
      clickLinkIfReady();
    }

    // Make sure neither of the test pages haven't been loaded before.
    var info = getNavHistoryEntry(makeURI(pageurl));
    ok(!info, "The test page must not have been visited already.");
    info = getNavHistoryEntry(makeURI(fragmenturl));
    ok(!info, "The fragment test page must not have been visited already.");

    // Now open the test page in a new tab.
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
    BrowserTestUtils.waitForContentEvent(gBrowser.selectedBrowser, "DOMContentLoaded", true).then(onPageLoad);
    gBrowser.selectedBrowser.loadURI(pageurl);
}
