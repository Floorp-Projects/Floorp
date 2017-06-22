/* Test for Bug 503832
 * https://bugzilla.mozilla.org/show_bug.cgi?id=503832
 */

add_task(async function() {
    var pagetitle = "Page Title for Bug 503832";
    var pageurl = "http://mochi.test:8888/browser/docshell/test/browser/file_bug503832.html";
    var fragmenturl = "http://mochi.test:8888/browser/docshell/test/browser/file_bug503832.html#firefox";

    var historyService = Cc["@mozilla.org/browser/nav-history-service;1"]
                         .getService(Ci.nsINavHistoryService);

    let fragmentPromise = new Promise(resolve => {
        /* Global history observer that triggers for the two test URLs above. */
        var historyObserver = {
            onBeginUpdateBatch: function() {},
            onEndUpdateBatch: function() {},
            onVisit: function(aURI, aVisitID, aTime, aSessionId, aReferringId,
                              aTransitionType, _added) {},
            onTitleChanged: function(aURI, aPageTitle) {
                aURI = aURI.spec;
                switch (aURI) {
                case pageurl:
                    is(aPageTitle, pagetitle, "Correct page title for " + aURI);
                    return;
                case fragmenturl:
                    is(aPageTitle, pagetitle, "Correct page title for " + aURI);
                    // If titles for fragment URLs aren't set, this code
                    // branch won't be called and the test will timeout,
                    // resulting in a failure
                    historyService.removeObserver(historyObserver, false);
                    resolve();
                }
            },
            onDeleteURI: function(aURI) {},
            onClearHistory: function() {},
            onPageChanged: function(aURI, aWhat, aValue) {},
            onDeleteVisits: function () {},
            QueryInterface: function(iid) {
                if (iid.equals(Ci.nsINavHistoryObserver) ||
                    iid.equals(Ci.nsISupports)) {
                    return this;
                }
                throw Cr.NS_ERROR_NO_INTERFACE;
            }
        };

        historyService.addObserver(historyObserver);
    });

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
        var node = result.root.getChild(0);
        result.root.containerOpen = false;
        return node;
    }

    // Make sure neither of the test pages haven't been loaded before.
    var info = getNavHistoryEntry(makeURI(pageurl));
    ok(!info, "The test page must not have been visited already.");
    info = getNavHistoryEntry(makeURI(fragmenturl));
    ok(!info, "The fragment test page must not have been visited already.");

    // Now open the test page in a new tab
    await BrowserTestUtils.openNewForegroundTab(gBrowser, pageurl);

    // Now that the page is loaded, click on fragment link
    await BrowserTestUtils.synthesizeMouseAtCenter("#firefox-link", {},
                                                   gBrowser.selectedBrowser);
    await fragmentPromise;

    gBrowser.removeCurrentTab();
});
