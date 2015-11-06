// the worker won't shut down between events because we increased
// the timeout values.
var client;
var window_count = 0;
var expected_window_count = 7;
var resolve_got_all_windows = null;
var got_all_windows = new Promise(function(res, rej) {
  resolve_got_all_windows = res;
});

// |expected_window_count| needs to be updated for every new call that's
// expected to actually open a new window regardless of what |clients.openWindow|
// returns.
function testForUrl(url, throwType, clientProperties, resultsArray) {
  return clients.openWindow(url)
    .then(function(e) {
      if (throwType != null) {
        resultsArray.push({
          result: false,
          message: "openWindow should throw " + throwType
        });
      } else if (clientProperties) {
        resultsArray.push({
          result: (e instanceof WindowClient),
          message: "openWindow should resolve to a WindowClient"
        });
        resultsArray.push({
          result: e.url == clientProperties.url,
          message: "Client url should be " + clientProperties.url
        });
        // Add more properties
      } else {
        resultsArray.push({
          result: e == null,
          message: "Open window should resolve to null. Got: " + e
        });
      }
    })
    .catch(function(err) {
      if (throwType == null) {
        resultsArray.push({
          result: false,
          message: "Unexpected throw: " + err
        });
      } else {
        resultsArray.push({
          result: err.toString().indexOf(throwType) >= 0,
          message: "openWindow should throw: " + err
        });
      }
    })
}

onmessage = function(event) {
  if (event.data == "testNoPopup") {
    client = event.source;

    var results = [];
    var promises = [];
    promises.push(testForUrl("about:blank", "TypeError", null, results));
    promises.push(testForUrl("http://example.com", "InvalidAccessError", null, results));
    promises.push(testForUrl("_._*`InvalidURL", "InvalidAccessError", null, results));
    Promise.all(promises).then(function(e) {
      client.postMessage(results);
    });
  }
  if (event.data == "NEW_WINDOW") {
    window_count += 1;
    if (window_count == expected_window_count) {
      resolve_got_all_windows();
    }
  }

  if (event.data == "CHECK_NUMBER_OF_WINDOWS") {
    got_all_windows.then(function() {
      return clients.matchAll();
    }).then(function(cl) {
      event.source.postMessage({result: cl.length == expected_window_count,
                                message: "The number of windows is correct."});
      for (i = 0; i < cl.length; i++) {
        cl[i].postMessage("CLOSE");
      }
    });
  }
}

onnotificationclick = function(e) {
  var results = [];
  var promises = [];

  var redirect = "http://mochi.test:8888/tests/dom/workers/test/serviceworkers/redirect.sjs?"
  var redirect_xorigin = "http://example.com/tests/dom/workers/test/serviceworkers/redirect.sjs?"
  var same_origin = "http://mochi.test:8888/tests/dom/workers/test/serviceworkers/open_window/client.html"
  var different_origin = "http://example.com/tests/dom/workers/test/serviceworkers/open_window/client.html"


  promises.push(testForUrl("about:blank", "TypeError", null, results));
  promises.push(testForUrl(different_origin, null, null, results));
  promises.push(testForUrl(same_origin, null, {url: same_origin}, results));
  promises.push(testForUrl("open_window/client.html", null, {url: same_origin}, results));

  // redirect tests
  promises.push(testForUrl(redirect + "open_window/client.html", null,
			   {url: same_origin}, results));
  promises.push(testForUrl(redirect + different_origin, null, null, results));

  promises.push(testForUrl(redirect_xorigin + "open_window/client.html", null,
			   null, results));
  promises.push(testForUrl(redirect_xorigin + same_origin, null,
			   {url: same_origin}, results));

  Promise.all(promises).then(function(e) {
    client.postMessage(results);
  });
}

