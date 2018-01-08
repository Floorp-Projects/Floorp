XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

/**
 * Asynchronously check a url is visited.

 * @param aURI The URI.
 * @param aExpectedValue The expected value.
 * @return {Promise}
 * @resolves When the check has been added successfully.
 * @rejects JavaScript exception.
 */
function promiseIsURIVisited(aURI, aExpectedValue) {
  return new Promise(resolve => {
    PlacesUtils.asyncHistory.isURIVisited(aURI, function(unused, aIsVisited) {
      resolve(aIsVisited);
    });

  });
}

/**
 * Ensures that the specified URIs are either cleared or not.
 *
 * @param aURIs
 *        Array of page URIs
 * @param aShouldBeCleared
 *        True if each visit to the URI should be cleared, false otherwise
 */
function promiseHistoryClearedState(aURIs, aShouldBeCleared) {
  return new Promise(resolve => {
    let callbackCount = 0;
    let niceStr = aShouldBeCleared ? "no longer" : "still";
    function callbackDone() {
      if (++callbackCount == aURIs.length)
        resolve();
    }
    aURIs.forEach(function(aURI) {
      PlacesUtils.asyncHistory.isURIVisited(aURI, function(uri, isVisited) {
        is(isVisited, !aShouldBeCleared,
           "history visit " + uri.spec + " should " + niceStr + " exist");
        callbackDone();
      });
    });

  });
}
