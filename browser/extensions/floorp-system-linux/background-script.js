    function initializePageAction(tab) {
        if (typeof tab.url != 'undefined' && tab.url.length ) {
            browser.pageAction.show(tab.id);
        }
    }
    var gettingAllTabs = browser.tabs.query({});
    gettingAllTabs.then((tabs) => {
        for (let tab of tabs) {
            initializePageAction(tab);
        }
    });