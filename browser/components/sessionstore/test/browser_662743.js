/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This tests that session restore component does restore the right <select> option.
// Session store should not rely only on previous user's selectedIndex, it should
// check its value as well.

function test() {
  /** Tests selected options **/
  waitForExplicitFinish();

  let testTabCount = 0;
  let formData = [
  // default case
    { },
  // old format
    { "#select_id" : 0 },
    { "#select_id" : 2 },
    // invalid index
    { "#select_id" : 8 },
    { "/xhtml:html/xhtml:body/xhtml:select" : 5},
    { "/xhtml:html/xhtml:body/xhtml:select[@name='select_name']" : 6},

  // new format
    // index doesn't match value (testing an option in between (two))
    { id:{ "select_id": {"selectedIndex":0,"value":"val2"} } },
    // index doesn't match value (testing an invalid value)
    { id:{ "select_id": {"selectedIndex":4,"value":"val8"} } },
    // index doesn't match value (testing an invalid index)
    { id:{ "select_id": {"selectedIndex":8,"value":"val5"} } },
    // index and value match position zero
    { id:{ "select_id": {"selectedIndex":0,"value":"val0"} }, xpath: {} },
    // index doesn't match value (testing the last option (seven))
    { id:{},"xpath":{ "/xhtml:html/xhtml:body/xhtml:select[@name='select_name']": {"selectedIndex":1,"value":"val7"} } },
    // index and value match the default option "selectedIndex":3,"value":"val3"
    { xpath: { "/xhtml:html/xhtml:body/xhtml:select[@name='select_name']" : {"selectedIndex":3,"value":"val3"} } },
    // index matches default option however it doesn't match value
    { id:{ "select_id": {"selectedIndex":3,"value":"val4"} } },

  // combinations
    { "#select_id" : 3, id:{ "select_id": {"selectedIndex":1,"value":"val1"} } },
    { "#select_id" : 5, xpath: { "/xhtml:html/xhtml:body/xhtml:select[@name='select_name']" : {"selectedIndex":4,"value":"val4"} } },
    { "/xhtml:html/xhtml:body/xhtml:select" : 5, id:{ "select_id": {"selectedIndex":1,"value":"val1"} }},
    { "/xhtml:html/xhtml:body/xhtml:select[@name='select_name']" : 2, xpath: { "/xhtml:html/xhtml:body/xhtml:select[@name='select_name']" : {"selectedIndex":7,"value":"val7"} } }
  ];

  let expectedValues = [
    [ "val3"], // default value
    [ "val0"],
    [ "val2"],
    [ "val3"], // default value (invalid index)
    [ "val5"],
    [ "val6"],
    [ "val2"],
    [ "val3"], // default value (invalid value)
    [ "val5"], // value is still valid (even it has an invalid index)
    [ "val0"],
    [ "val7"],
    [ "val3"],
    [ "val4"],
    [ "val1"],
    [ "val4"],
    [ "val1"],
    [ "val7"]
  ];
  let callback = function() {
    testTabCount--;
    if (testTabCount == 0) {
      finish();
    }
  };

  for (let i = 0; i < formData.length; i++) {
    testTabCount++;
    testTabRestoreData(formData[i], expectedValues[i], callback);
  }
}

function testTabRestoreData(aFormData, aExpectedValues, aCallback) {
  let testURL =
    getRootDirectory(gTestPath) + "browser_662743_sample.html";
  let tab = gBrowser.addTab(testURL);
  let tabState = { entries: [{ url: testURL, formdata: aFormData}] };

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    ss.setTabState(tab, JSON.stringify(tabState));

    tab.addEventListener("SSTabRestored", function(aEvent) {
      tab.removeEventListener("SSTabRestored", arguments.callee);
      let doc = tab.linkedBrowser.contentDocument;
      let select = doc.getElementById("select_id");
      let value = select.options[select.selectedIndex].value;

      // test select options values
      is(value, aExpectedValues[0],
        "Select Option by selectedIndex &/or value has been restored correctly");

      // clean up
      gBrowser.removeTab(tab);
      // Call stopPropagation on the event so we won't fire the
      // tabbrowser's SSTabRestored listeners.
      aEvent.stopPropagation();
      aCallback();
    });

    tab.addEventListener("TabClose", function(aEvent) {
      tab.removeEventListener("TabClose", arguments.callee);
      let restoredTabState = JSON.parse(ss.getTabState(tab));
      let restoredFormData = restoredTabState.entries[0].formdata;
      let selectIdFormData = restoredFormData.id.select_id;
      let value = restoredFormData.id.select_id.value;

      // test format
      ok("id" in restoredFormData && "xpath" in restoredFormData,
        "FormData format is valid");
      // validate that there are no old keys
      is(Object.keys(restoredFormData).length, 2,
        "FormData key length is valid");
      // test format
      ok("selectedIndex" in selectIdFormData && "value" in selectIdFormData,
        "select format is valid");
      // validate that there are no old keys
      is(Object.keys(selectIdFormData).length, 2,
        "select_id length is valid");
       // test set collection values
      is(value, aExpectedValues[0],
        "Collection has been saved correctly");
    });
  }, true);
}
