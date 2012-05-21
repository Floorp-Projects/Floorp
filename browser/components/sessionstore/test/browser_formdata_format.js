/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  /** Tests formdata format **/
  waitForExplicitFinish();

  let formData = [
    { },
    // old format
    { "#input1" : "value0" },
    { "#input1" : "value1", "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value2" },
    { "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value3" },
    // new format
    { id: { "input1" : "value4" } },
    { id: { "input1" : "value5" }, xpath: {} },
    { id: { "input1" : "value6" }, xpath: { "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value7" } },
    { id: {}, xpath: { "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value8" } },
    { xpath: { "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value9" } },
    // combinations
    { "#input1" : "value10", id: { "input1" : "value11" } },
    { "#input1" : "value12", id: { "input1" : "value13" }, xpath: { "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value14" } },
    { "#input1" : "value15", xpath: { "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value16" } },
    { "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value17", id: { "input1" : "value18" } },
    { "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value19", id: { "input1" : "value20" }, xpath: { "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value21" } },
    { "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value22", xpath: { "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value23" } },
    { "#input1" : "value24", "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value25", id: { "input1" : "value26" } },
    { "#input1" : "value27", "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value28", id: { "input1" : "value29" }, xpath: { "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value30" } },
    { "#input1" : "value31", "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value32", xpath: { "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value33" } }
  ]
  let expectedValues = [
    [ "" , "" ],
    // old format
    [ "value0", "" ],
    [ "value1", "value2" ],
    [ "", "value3" ],
    // new format
    [ "value4", "" ],
    [ "value5", "" ],
    [ "value6", "value7" ],
    [ "", "value8" ],
    [ "", "value9" ],
    // combinations
    [ "value11", "" ],
    [ "value13", "value14" ],
    [ "", "value16" ],
    [ "value18", "" ],
    [ "value20", "value21" ],
    [ "", "value23" ],
    [ "value26", "" ],
    [ "value29", "value30" ],
    [ "", "value33" ]
  ];
  let testTabCount = 0;
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

function testTabRestoreData(aFormData, aExpectedValue, aCallback) {
  let testURL =
    getRootDirectory(gTestPath) + "browser_formdata_format_sample.html";
  let tab = gBrowser.addTab(testURL);
  let tabState = { entries: [{ url: testURL, formdata: aFormData}] };

  let browserLoadedCallback = function(aEvent) {
    let tabStateCallback = function(aEvent) {
      let doc = tab.linkedBrowser.contentDocument;
      let input1 = doc.getElementById("input1");
      let input2 = doc.querySelector("input[name=input2]");
      let saveStateCallback = function(aEvent) {
        let restoredTabState = JSON.parse(ss.getTabState(tab));
        let restoredFormData = restoredTabState.entries[0].formdata;

        if (restoredFormData) {
          // test format
          ok("id" in restoredFormData && "xpath" in restoredFormData,
            "FormData format is valid: " + restoredFormData);
          // validate that there are no old keys
          is(Object.keys(restoredFormData).length, 2,
            "FormData key length is valid");
          // test id
          is(input1.value, aExpectedValue[0],
            "FormData by 'id' has been restored correctly");
          // test xpath
          is(input2.value, aExpectedValue[1],
            "FormData by 'xpath' has been restored correctly");
        }

        // clean up
        gBrowser.removeTab(tab);
        aCallback();
      };

      waitForSaveState(saveStateCallback);

      // force a change event to recollect the formdata
      let changeEvent = document.createEvent("Events");
      changeEvent.initEvent("change", true, true);
      input.dispatchEvent(changeEvent);
    };

    waitForTabState(tab, tabState, tabStateCallback);
  };

  whenBrowserLoaded(tab.linkedBrowser, browserLoadedCallback);
}
