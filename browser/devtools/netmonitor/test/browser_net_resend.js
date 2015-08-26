/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let gPanelWin;
let gPanelDoc;

const ADD_QUERY = "t1=t2";
const ADD_HEADER = "Test-header: true";
const ADD_POSTDATA = "t3=t4";

/**
 * Tests if resending a request works.
 */

function test() {
  initNetMonitor(POST_DATA_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    gPanelWin = aMonitor.panelWin;
    gPanelDoc = gPanelWin.document;

    let { NetMonitorView } = gPanelWin;
    let { RequestsMenu } = NetMonitorView;
    let TAB_UPDATED = aMonitor.panelWin.EVENTS.TAB_UPDATED;
    let CUSTOMREQUESTVIEW_POPULATED = aMonitor.panelWin.EVENTS.CUSTOMREQUESTVIEW_POPULATED;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 0, 2).then(() => {
      let origItem = RequestsMenu.getItemAtIndex(0);
      RequestsMenu.selectedItem = origItem;

      waitFor(aMonitor.panelWin, TAB_UPDATED).then(() => {
        // add a new custom request cloned from selected request
        RequestsMenu.cloneSelectedRequest();
        return waitFor(aMonitor.panelWin, CUSTOMREQUESTVIEW_POPULATED);
      }).then(() => {
        testCustomForm(origItem.attachment);

        let customItem = RequestsMenu.selectedItem;
        testCustomItem(customItem, origItem);

        // edit the custom request
        editCustomForm(() => {
          testCustomItemChanged(customItem, origItem);

          waitForNetworkEvents(aMonitor, 0, 1).then(() => {
            let sentItem = RequestsMenu.selectedItem;
            testSentRequest(sentItem.attachment, origItem.attachment);
            finishUp(aMonitor);
          });
          // send the new request
          RequestsMenu.sendCustomRequest();
        });
      });
    });

    aDebuggee.performRequests();
  });
}

function testCustomItem(aItem, aOrigItem) {
  let method = aItem.target.querySelector(".requests-menu-method").value;
  let origMethod = aOrigItem.target.querySelector(".requests-menu-method").value;
  is(method, origMethod, "menu item is showing the same method as original request");

  let file = aItem.target.querySelector(".requests-menu-file").value;
  let origFile = aOrigItem.target.querySelector(".requests-menu-file").value;
  is(file, origFile, "menu item is showing the same file name as original request");

  let domain = aItem.target.querySelector(".requests-menu-domain").value;
  let origDomain = aOrigItem.target.querySelector(".requests-menu-domain").value;
  is(domain, origDomain, "menu item is showing the same domain as original request");
}

function testCustomItemChanged(aItem, aOrigItem) {
  let file = aItem.target.querySelector(".requests-menu-file").value;
  let expectedFile = aOrigItem.target.querySelector(".requests-menu-file").value + "&" + ADD_QUERY;

  is(file, expectedFile, "menu item is updated to reflect url entered in form");
}

/*
 * Test that the New Request form was populated correctly
 */
function testCustomForm(aData) {
  is(gPanelDoc.getElementById("custom-method-value").value, aData.method,
     "new request form showing correct method");

  is(gPanelDoc.getElementById("custom-url-value").value, aData.url,
     "new request form showing correct url");

  let query = gPanelDoc.getElementById("custom-query-value");
  is(query.value, "foo=bar\nbaz=42\ntype=urlencoded",
     "new request form showing correct query string");

  let headers = gPanelDoc.getElementById("custom-headers-value").value.split("\n");
  for (let {name, value} of aData.requestHeaders.headers) {
    ok(headers.indexOf(name + ": " + value) >= 0, "form contains header from request");
  }

  let postData = gPanelDoc.getElementById("custom-postdata-value");
  is(postData.value, aData.requestPostData.postData.text,
     "new request form showing correct post data");
}

/*
 * Add some params and headers to the request form
 */
function editCustomForm(callback) {
  gPanelWin.focus();

  let query = gPanelDoc.getElementById("custom-query-value");
  query.addEventListener("focus", function onFocus() {
    query.removeEventListener("focus", onFocus, false);

    // add params to url query string field
    type(["VK_RETURN"]);
    type(ADD_QUERY);

    let headers = gPanelDoc.getElementById("custom-headers-value");
    headers.addEventListener("focus", function onFocus() {
      headers.removeEventListener("focus", onFocus, false);

      // add a header
      type(["VK_RETURN"]);
      type(ADD_HEADER);

      let postData = gPanelDoc.getElementById("custom-postdata-value");
      postData.addEventListener("focus", function onFocus() {
        postData.removeEventListener("focus", onFocus, false);

        // add to POST data
        type(ADD_POSTDATA);
        callback();
      }, false);
      postData.focus();
    }, false);
    headers.focus();
  }, false);

  // Bug 1195825: Due to some unexplained dark-matter with promise,
  // focus only works if delayed by one tick.
  executeSoon(() => {
    query.focus();
  });
}

/*
 * Make sure newly created event matches expected request
 */
function testSentRequest(aData, aOrigData) {
  is(aData.method, aOrigData.method, "correct method in sent request");
  is(aData.url, aOrigData.url + "&" + ADD_QUERY, "correct url in sent request");

  let hasHeader = aData.requestHeaders.headers.some((header) => {
    return (header.name + ": " + header.value) == ADD_HEADER;
  })
  ok(hasHeader, "new header added to sent request");

  is(aData.requestPostData.postData.text,
     aOrigData.requestPostData.postData.text + ADD_POSTDATA,
     "post data added to sent request");
}


function type(aString) {
  for (let ch of aString) {
    EventUtils.synthesizeKey(ch, {}, gPanelWin);
  }
}

function finishUp(aMonitor) {
  gPanelWin = null;
  gPanelDoc = null;

  teardown(aMonitor).then(finish);
}
