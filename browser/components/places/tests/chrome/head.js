/**
 * Asynchronously adds visits to a page, invoking a callback function when done.
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
 * @param [optional] aCallback
 *        Function to be invoked on completion.
 */
function addVisits(aPlaceInfo, aCallback) {
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
  for (let i = 0; i < places.length; i++) {
    if (!places[i].title) {
      places[i].title = "test visit for " + places[i].uri.spec;
    }
    places[i].visits = [{
      transitionType: places[i].transition === undefined ? PlacesUtils.history.TRANSITION_LINK
                                                         : places[i].transition,
      visitDate: places[i].visitDate || (now++) * 1000,
      referrerURI: places[i].referrer
    }];
  }

  PlacesUtils.asyncHistory.updatePlaces(
    places,
    {
      handleError: function AAV_handleError() {
        throw("Unexpected error in adding visit.");
      },
      handleResult: function () {},
      handleCompletion: function UP_handleCompletion() {
        if (aCallback)
          aCallback();
      }
    }
  );
}

/**
 * Clears history invoking callback when done.
 */
function waitForClearHistory(aCallback) {
  const TOPIC_EXPIRATION_FINISHED = "places-expiration-finished";
  Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
    Services.obs.removeObserver(observer, TOPIC_EXPIRATION_FINISHED);
    aCallback();
  }, TOPIC_EXPIRATION_FINISHED, false);
  Cc["@mozilla.org/browser/nav-history-service;1"]
    .getService(Ci.nsINavHistoryService)
    .QueryInterface(Ci.nsIBrowserHistory).removeAllPages();
}
