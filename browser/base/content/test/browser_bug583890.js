/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Call the aCallback when aTab's label is equal to the aExpectedLabel.
 * Either happens immediately, or after successive checks.
 * In the case of failure, this will cause timeout of the test.
 *
 * @param aTab           the tab whose label is being tested
 * @param aExpectedLabel the value the tab's label must match
 * @param aCallback      the callback for use upon success
 */
function waitForTabLabel(aTab, aExpectedLabel, aCallback) {
  if (aTab.visibleLabel == aExpectedLabel) {
    executeSoon(aCallback);
  } else {
    executeSoon(function () { waitForTabLabel(aTab, aExpectedLabel, aCallback); });
  }
}

/**
 * Call the aCallback after adding aCount tabs.
 *
 * @param aCount         the number of tabs to add
 * @param aCallback      the callback for use upon success
 */
function addTabs(aCount, aCallback) {
  let addedTabs = [];
  for (let i = aCount; i > 0; i--) {
    addedTabs.push(gBrowser.addTab(null, {skipAnimation: true}));
  }
  executeSoon(function () { aCallback(addedTabs); });
}

/**
 * Call the aCallback after updating aTab's title and waiting for a label update
 * In the case of failure, this will cause timeout of the test.
 *
 * @param aTab           the tab whose title is set
 * @param aTitle         the value to give the tab title
 * @param aExpectedLabel the value the tab's label must match
 * @param aCallback      the callback for use upon success
 */
function setTitleForTab(aTab, aTitle, aExpectedLabel, aCallback) {
  gBrowser.tabContainer.addEventListener("TabLabelModified",
                                         afterTabLabelModified, false);
  // Only start polling for the label after the event has hit
  function afterTabLabelModified() {
    gBrowser.tabContainer.removeEventListener("TabLabelModified",
                                              afterTabLabelModified, false);
    waitForTabLabel(aTab, aExpectedLabel, aCallback);
  }
  // On the off chance we're trying to set the title on a too-young tab,
  // we wait until it's mature
  function doSetTitle() {
    if (aTab.linkedBrowser && aTab.linkedBrowser.contentDocument) {
      aTab.linkedBrowser.contentDocument.title = aTitle;
    } else {
      executeSoon(doSetTitle);
    }
  }
  executeSoon(doSetTitle);
}

/**
 * Call the aCallback after updating aTab's label and waiting for a label update
 * In the case of failure, this will cause timeout of the test.
 *
 * @param aTab           the tab whose title is set
 * @param aTitle         the value to give the tab title
 * @param aExpectedLabel the value the tab's label must match
 * @param aCallback      the callback for use upon success
 */
function setLabelForTab(aTab, aTitle, aExpectedLabel, aCallback) {
  executeSoon(function () {
    aTab.label = aTitle;
    waitForTabLabel(aTab, aExpectedLabel, aCallback);
  });
}

function GroupTest() {
  this.groupNumber = 0;
  this.tabs = [];
}

GroupTest.prototype = {
  groups: [
    [
      /*
       * Test proxying and suffix protection
       */
      [
        "Foo - Bar - Baz",
        "Foo - Baz - Baz",
        "Foo - Baz - Baz",
        "Foo - Baz - Qux"
      ],
      [
        [
          "Bar - Baz",
          "Baz - Baz"
        ],
        [
          "Bar - Baz",
          "Baz - Baz",
          "Baz - Baz"
        ],
        [
          "Bar - Baz",
          "Baz",
          "Baz",
          "Qux"
        ]
      ]
    ],
    [
      /*
       * Test pathmode
       */
      [
        "http://example.com/foo.html",
        "http://example.com/foo/bar.html",
        "Browse - ftp://example.com/pub/",
        "Browse - ftp://example.com/pub/src/"
      ],
      [
        [
          "foo.html",
          "foo/bar.html"
        ],
        [
          "foo.html",
          "foo/bar.html",
          "Browse - ftp://example.com/pub/"
        ],
        [
          "foo.html",
          "foo/bar.html",
          "pub/",
          "src/"
        ]
      ]
    ],
    [
      /*
       * Test that we don't leave a lone suffix
       */
      [
        "'Zilla and the Foxes - Singles - Musical Monkey",
        "'Zilla and the Foxes - Biography - Musical Monkey",
        "'Zilla and the Foxes - Musical Monkey",
        "'Zilla and the Foxes - Interviews - Musical Monkey"
      ],
      [
        [
          "Singles - Musical Monkey",
          "Biography - Musical Monkey"
        ],
        [
          "Singles - Musical Monkey",
          "Biography - Musical Monkey",
          "'Zilla and the Foxes - Musical Monkey"
        ],
        [
          "Singles - Musical Monkey",
          "Biography - Musical Monkey",
          "'Zilla and the Foxes - Musical Monkey",
          "Interviews - Musical Monkey"
        ]
      ]
    ],
    /*
     * Test short endings for MIN_CHOP
     */
    [
      [
        "Foo - Bar - 0",
        "Foo - Bar - 0 - extra - 0",
        "Foo - Bar - 1",
        "Foo - Bar - 2 - extra",
        "Foo - Bar - 3"
      ],
      [
        [
          "Bar - 0",
          "0 - extra - 0"
        ],
        [
          "Bar - 0",
          "0 - extra - 0",
          "Bar - 1"
        ],
        [
          "Bar - 0",
          "0 - extra - 0",
          "Bar - 1",
          "2 - extra"
        ],
        [
          "Bar - 0",
          "0 - extra - 0",
          "Bar - 1",
          "2 - extra",
          "Bar - 3"
        ]
      ]
    ],
    [
      /*
       * Test multiple whitespace
       */
      [
        "Foo  - Bar -  Baz",
        "Foo -  Bar -  Baz",
        "Foo  -  Bar -  Baz",
        "Foo  - Baz -  Baz"
      ],
      [
        [
          "Foo - Bar - Baz",
          "Foo - Bar - Baz"
        ],
        [
          "Foo - Bar - Baz",
          "Foo - Bar - Baz",
          "Foo - Bar - Baz"
        ],
        [
          "Bar - Baz",
          "Bar - Baz",
          "Bar - Baz",
          "Baz - Baz"
        ]
      ]
    ]
  ],

  /**
   * Either proceed with the next group, or finish group tests
   */
  nextGroup: function GroupTest_nextGroup() {
    while (this.tabs.length) {
      gBrowser.removeTab(this.tabs.pop());
    }
    if (this.groups.length) {
      this.groupNumber++;
      [this.labels, this.expectedLabels] = this.groups.shift();
      this.nextTab();
    } else {
      runNextTest();
    }
  },

  /**
   * Runs tests for existing tabs, and adds the next tab (if group isn't empty)
   * If the group is empty, starts the next group
   */
  nextTab: function GroupTest_nextTab() {
    if (this.tabs.length > 1) {
      let ourExpected = this.expectedLabels.shift();
      for (let i = 0; i < this.tabs.length; i++) {
        is(this.tabs[i].visibleLabel, ourExpected[i],
          "Tab " + this.groupNumber + "." + (i + 1) + " has correct visibleLabel");
      }
    }
    if (this.labels.length) {
      this.tabs.push(gBrowser.addTab(
        "data:text/html,<title>" + this.labels.shift() + "</title>",
        {skipAnimation: true}));
      if (this.tabs.length > 1) {
        waitForTabLabel(this.tabs[this.tabs.length - 1],
                        this.expectedLabels[0][this.expectedLabels[0].length - 1],
                        this.nextTab.bind(this));
      } else {
        this.nextTab();
      }
    } else {
      this.nextGroup();
    }
  }
};

let TESTS = [
function test_about_blank() {
  let tab1 = gBrowser.selectedTab;
  let tab2;
  let tab3;
  addTabs(2, setup1);
  function setup1(aTabs) {
    [tab2, tab3] = aTabs
    waitForTabLabel(tab3, "New Tab", setupComplete);
  }
  function setupComplete() {
    is(tab1.visibleLabel, "New Tab", "First tab has original label");
    is(tab2.visibleLabel, "New Tab", "Second tab has original label");
    is(tab3.visibleLabel, "New Tab", "Third tab has original label");
    runNextTest();
  }
},

function test_two_tabs() {
  let tab1 = gBrowser.selectedTab;
  addTabs(1, setup1);
  let tab2;
  function setup1(aTabs) {
    tab2 = aTabs[0];
    setTitleForTab(tab1, "Foo - Bar - Baz", "Foo - Bar - Baz", setup2);
  }
  function setup2() {
    setTitleForTab(tab2, "Foo - Baz - Baz", "Baz - Baz", setupComplete);
  }
  function setupComplete() {
    is(tab1.visibleLabel, "Bar - Baz", "Removed exactly two tokens");
    is(tab2.visibleLabel, "Baz - Baz", "Removed exactly two tokens");
    gBrowser.removeTab(tab2);
    waitForTabLabel(tab1, "Foo - Bar - Baz", afterRemoval);
  }
  function afterRemoval() {
    is (tab1.visibleLabel, "Foo - Bar - Baz", "Single tab has full title");
    runNextTest();
  }
},

function test_direct_label() {
  let tab1 = gBrowser.selectedTab;
  addTabs(2, setup1);
  let tab2;
  let tab3;
  function setup1(aTabs) {
    [tab2, tab3] = aTabs;
    setLabelForTab(tab1, "Foo  - Bar -  Baz", "Foo  - Bar -  Baz", setup2);
  }
  function setup2() {
    setLabelForTab(tab2, "Foo -  Baz  - Baz", "Foo -  Baz  - Baz", setup3);
  }
  function setup3() {
    setLabelForTab(tab3, "Foo  - Baz -  Baz", "Baz -  Baz", setupComplete);
  }
  function setupComplete() {
    is(tab1.visibleLabel, "Bar -  Baz", "Removed exactly two tokens");
    is(tab2.visibleLabel, "Foo -  Baz  - Baz", "Irregular spaces mean no match");
    is(tab3.visibleLabel, "Baz -  Baz", "Removed exactly two tokens");
    gBrowser.removeTab(tab3);
    waitForTabLabel(tab1, "Foo  - Bar -  Baz", afterRemoval);
  }
  function afterRemoval() {
    is (tab1.visibleLabel, "Foo  - Bar -  Baz", "Single tab has full title");
    gBrowser.removeTab(tab2);
    runNextTest();
  }
},

function test_groups() {
  let g = new GroupTest();
  g.nextGroup();
}
];

function runNextTest() {
  if (TESTS.length == 0) {
    finish();
    return;
  }

  while (gBrowser.tabs.length > 1) {
    gBrowser.removeTab(gBrowser.tabs[1]);
  }

  info("Running " + TESTS[0].name);
  TESTS.shift()();
};

function test() {
  waitForExplicitFinish();
  runNextTest();
}

