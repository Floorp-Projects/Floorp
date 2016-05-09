add_task(function*() {
  registerCleanupFunction(() => {
    PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);
  });

  function* addTagItem(tagName) {
    let uri = NetUtil.newURI(`http://example.com/this/is/tagged/${tagName}`);
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         uri,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         `test ${tagName}`);
    PlacesUtils.tagging.tagURI(uri, [tagName]);
    yield PlacesTestUtils.addVisits([{uri: uri, title: `Test page with tag ${tagName}`}]);
  }

  // We use different tags for each part of the test, as otherwise the
  // autocomplete code tries to be smart by using the previously cached element
  // without updating it (since all parameters it knows about are the same).

  let testcases = [{
    description: "Test with suggest.bookmark=true",
    tagName: "tagtest1",
    prefs: {
      "suggest.bookmark": true,
    },
    input: "tagtest1",
    expected: {
      type: "bookmark",
      typeImageVisible: true,
    },
  }, {
    description: "Test with suggest.bookmark=false",
    tagName: "tagtest2",
    prefs: {
      "suggest.bookmark": false,
    },
    input: "tagtest2",
    expected: {
      type: "tag",
      typeImageVisible: false,
    },
  }, {
    description: "Test with suggest.bookmark=true (again)",
    tagName: "tagtest3",
    prefs: {
      "suggest.bookmark": true,
    },
    input: "tagtest3",
    expected: {
      type: "bookmark",
      typeImageVisible: true,
    },
  }, {
    description: "Test with bookmark restriction token",
    tagName: "tagtest4",
    prefs: {
      "suggest.bookmark": true,
    },
    input: "* tagtest4",
    expected: {
      type: "bookmark",
      typeImageVisible: true,
    },
  }, {
    description: "Test with history restriction token",
    tagName: "tagtest5",
    prefs: {
      "suggest.bookmark": true,
    },
    input: "^ tagtest5",
    expected: {
      type: "tag",
      typeImageVisible: false,
    },
  }];

  for (let testcase of testcases) {
    info(`Test case: ${testcase.description}`);

    yield addTagItem(testcase.tagName);
    for (let prefName of Object.keys(testcase.prefs)) {
      Services.prefs.setBoolPref(`browser.urlbar.${prefName}`, testcase.prefs[prefName]);
    }

    yield promiseAutocompleteResultPopup(testcase.input);
    let result = gURLBar.popup.richlistbox.children[1];
    ok(result && !result.collasped, "Should have result");

    is(result.getAttribute("type"), testcase.expected.type, "Result should have expected type");

    let typeIconStyle = window.getComputedStyle(result._typeIcon);
    let imageURL = typeIconStyle.listStyleImage;
    if (testcase.expected.typeImageVisible) {
      ok(/^url\(.+\)$/.test(imageURL), "Type image should be visible");
    } else {
      is(imageURL, "none", "Type image should be hidden");
    }

    gURLBar.popup.hidePopup();
    yield promisePopupHidden(gURLBar.popup);
  }
});
