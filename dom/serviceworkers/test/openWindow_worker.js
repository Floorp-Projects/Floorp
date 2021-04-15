// the worker won't shut down between events because we increased
// the timeout values.
var client;
var window_count = 0;
var expected_window_count = 9;
var isolated_window_count = 0;
var expected_isolated_window_count = 2;
var resolve_got_all_windows = null;
var got_all_windows = new Promise(function(res, rej) {
  resolve_got_all_windows = res;
});

// |expected_window_count| needs to be updated for every new call that's
// expected to actually open a new window regardless of what |clients.openWindow|
// returns.
function testForUrl(url, throwType, clientProperties, resultsArray) {
  return clients
    .openWindow(url)
    .then(function(e) {
      if (throwType != null) {
        resultsArray.push({
          result: false,
          message: "openWindow should throw " + throwType,
        });
      } else if (clientProperties) {
        resultsArray.push({
          result: e instanceof WindowClient,
          message: `openWindow should resolve to a WindowClient for url ${url}, got ${e}`,
        });
        resultsArray.push({
          result: e.url == clientProperties.url,
          message: "Client url should be " + clientProperties.url,
        });
        // Add more properties
      } else {
        resultsArray.push({
          result: e == null,
          message: "Open window should resolve to null. Got: " + e,
        });
      }
    })
    .catch(function(err) {
      if (throwType == null) {
        resultsArray.push({
          result: false,
          message: "Unexpected throw: " + err,
        });
      } else {
        resultsArray.push({
          result: err.toString().includes(throwType),
          message: "openWindow should throw: " + err,
        });
      }
    });
}

onmessage = function(event) {
  if (event.data == "testNoPopup") {
    client = event.source;

    var results = [];
    var promises = [];
    promises.push(testForUrl("about:blank", "TypeError", null, results));
    promises.push(
      testForUrl("http://example.com", "InvalidAccessError", null, results)
    );
    promises.push(
      testForUrl("_._*`InvalidURL", "InvalidAccessError", null, results)
    );
    event.waitUntil(
      Promise.all(promises).then(function(e) {
        client.postMessage(results);
      })
    );
  }

  if (event.data == "NEW_WINDOW" || event.data == "NEW_ISOLATED_WINDOW") {
    window_count += 1;
    if (event.data == "NEW_ISOLATED_WINDOW") {
      isolated_window_count += 1;
    }
    if (window_count == expected_window_count) {
      resolve_got_all_windows();
    }
  }

  if (event.data == "CHECK_NUMBER_OF_WINDOWS") {
    event.waitUntil(
      got_all_windows
        .then(function() {
          return clients.matchAll();
        })
        .then(function(cl) {
          event.source.postMessage([
            {
              result: cl.length == expected_window_count,
              message: `The number of windows is correct. ${cl.length} == ${expected_window_count}`,
            },
            {
              result: isolated_window_count == expected_isolated_window_count,
              message: `The number of isolated windows is correct. ${isolated_window_count} == ${expected_isolated_window_count}`,
            },
          ]);
          for (i = 0; i < cl.length; i++) {
            cl[i].postMessage("CLOSE");
          }
        })
    );
  }
};

onnotificationclick = function(e) {
  var results = [];
  var promises = [];

  var redirect =
    "http://mochi.test:8888/tests/dom/serviceworkers/test/redirect.sjs?";
  var redirect_xorigin =
    "http://example.com/tests/dom/serviceworkers/test/redirect.sjs?";
  var same_origin =
    "http://mochi.test:8888/tests/dom/serviceworkers/test/open_window/client.sjs";
  var different_origin =
    "http://example.com/tests/dom/serviceworkers/test/open_window/client.sjs";

  promises.push(testForUrl("about:blank", "TypeError", null, results));
  promises.push(testForUrl(different_origin, null, null, results));
  promises.push(testForUrl(same_origin, null, { url: same_origin }, results));
  promises.push(
    testForUrl("open_window/client.sjs", null, { url: same_origin }, results)
  );

  // redirect tests
  promises.push(
    testForUrl(
      redirect + "open_window/client.sjs",
      null,
      { url: same_origin },
      results
    )
  );
  promises.push(testForUrl(redirect + different_origin, null, null, results));

  promises.push(
    testForUrl(redirect_xorigin + "open_window/client.sjs", null, null, results)
  );
  promises.push(
    testForUrl(
      redirect_xorigin + same_origin,
      null,
      { url: same_origin },
      results
    )
  );

  // coop+coep tests
  promises.push(
    testForUrl(
      same_origin + "?crossOriginIsolated=true",
      null,
      { url: same_origin + "?crossOriginIsolated=true" },
      results
    )
  );
  promises.push(
    testForUrl(
      different_origin + "?crossOriginIsolated=true",
      null,
      null,
      results
    )
  );

  e.waitUntil(
    Promise.all(promises).then(function() {
      client.postMessage(results);
    })
  );
};
