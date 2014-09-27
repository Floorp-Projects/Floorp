/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let gOriginalEngine;
let gEngine;
let gUnifiedCompletePref = "browser.urlbar.unifiedcomplete";

/**
 * Asynchronously adds visits to a page.
 *
 * @param aPlaceInfo
 *        Can be an nsIURI, in such a case a single LINK visit will be added.
 *        Otherwise can be an object describing the visit to add, or an array
 *        of these objects:
 *          { uri: nsIURI of the page,
 *            transition: one of the TRANSITION_* from nsINavHistoryService,
 *            [optional] title: title of the page,
 *            [optional] visitDate: visit date in microseconds from the epoch
 *            [optional] referrer: nsIURI of the referrer for this visit
 *          }
 *
 * @return {Promise}
 * @resolves When all visits have been added successfully.
 * @rejects JavaScript exception.
 */
function promiseAddVisits(aPlaceInfo) {
  return new Promise((resolve, reject) => {
    let places = [];
    if (aPlaceInfo instanceof Ci.nsIURI) {
      places.push({ uri: aPlaceInfo });
    }
    else if (Array.isArray(aPlaceInfo)) {
      places = places.concat(aPlaceInfo);
    } else {
      places.push(aPlaceInfo)
    }

    // Create mozIVisitInfo for each entry.
    let now = Date.now();
    for (let i = 0, len = places.length; i < len; ++i) {
      if (!places[i].title) {
        places[i].title = "test visit for " + places[i].uri.spec;
      }
      places[i].visits = [{
        transitionType: places[i].transition === undefined ? Ci.nsINavHistoryService.TRANSITION_LINK
                                                           : places[i].transition,
        visitDate: places[i].visitDate || (now++) * 1000,
        referrerURI: places[i].referrer
      }];
    }

    PlacesUtils.asyncHistory.updatePlaces(
      places,
      {
        handleError: function AAV_handleError(aResultCode, aPlaceInfo) {
          let ex = new Components.Exception("Unexpected error in adding visits.",
                                            aResultCode);
          reject(ex);
        },
        handleResult: function () {},
        handleCompletion: function UP_handleCompletion() {
          resolve();
        }
      }
    );
  });
}

function* promiseAutocompleteResultPopup(inputText) {
  gURLBar.focus();
  gURLBar.value = inputText.slice(0, -1);
  EventUtils.synthesizeKey(inputText.slice(-1) , {});
  yield promiseSearchComplete();

  return gURLBar.popup.richlistbox.children;
}

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(gUnifiedCompletePref);
  Services.search.currentEngine = gOriginalEngine;
  Services.search.removeEngine(gEngine);
  return promiseClearHistory();
});

add_task(function*() {
  Services.prefs.setBoolPref(gUnifiedCompletePref, true);
});

add_task(function*() {

  Services.search.addEngineWithDetails("SearchEngine", "", "", "",
                                       "GET", "http://s.example.com/search");
  gEngine = Services.search.getEngineByName("SearchEngine");
  gEngine.addParam("q", "{searchTerms}", null);
  gOriginalEngine = Services.search.currentEngine;
  Services.search.currentEngine = gEngine;

  let uri = NetUtil.newURI("http://s.example.com/search?q=foo&client=1");
  yield promiseAddVisits({ uri: uri, title: "Foo - SearchEngine Search" });

  // The first autocomplete result has the action searchengine, while
  // the second result is the "search favicon" element.
  let result = yield promiseAutocompleteResultPopup("foo");
  result = result[1];

  isnot(result, null, "Expect a search result");
  is(result.getAttribute("type"), "search favicon", "Expect correct `type` attribute");

  is_element_visible(result._title, "Title element should be visible");
  is_element_visible(result._extraBox, "Extra box element should be visible");

  is(result._extraBox.pack, "start", "Extra box element should start after the title");
  let iconElem = result._extraBox.nextSibling;
  is_element_visible(iconElem,
                     "The element containing the magnifying glass icon should be visible");
  ok(iconElem.classList.contains("ac-result-type-keyword"),
     "That icon should have the same class use for `keyword` results");

  is_element_visible(result._url, "URL element should be visible");
  is(result._url.textContent, "Search with SearchEngine");
});
