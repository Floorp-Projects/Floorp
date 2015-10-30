// the worker won't shut down between events because we increased
// the timeout values.
var client;

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

onnotificationclick = function(e) {
  var results = [];
  var promises = [];

  promises.push(testForUrl("about:blank", "TypeError", null, results));
  promises.push(testForUrl("http://example.com", null, null, results));
  promises.push(testForUrl("http://mochi.test:8888/same_origin.html", null,
                           {url: "http://mochi.test:8888/same_origin.html"}, results));

  // redirect tests
  var redirect = "http://mochi.test:8888/tests/dom/workers/test/serviceworkers/redirect.sjs?"
  var baseURL = "http://mochi.test:8888/tests/dom/workers/test/serviceworkers/"
  promises.push(testForUrl(redirect + "same_origin_redirect.html", null,
			   {url: baseURL + "same_origin_redirect.html"}, results));
  promises.push(testForUrl(redirect + "http://example.com/redirect_to_other_origin.html", null,
			   null, results));

  var redirect_xorigin = "http://example.com/tests/dom/workers/test/serviceworkers/redirect.sjs?"
  promises.push(testForUrl(redirect_xorigin + "xorigin_redirect.html", null,
			   null, results));
  promises.push(testForUrl(redirect_xorigin + "http://mochi.test:8888/xorigin_to_same_origin.html", null,
			   {url: "http://mochi.test:8888/xorigin_to_same_origin.html"}, results));

  Promise.all(promises).then(function(e) {
    client.postMessage(results);
  });
}

onfetch = function(e) {
  if (e.request.url.indexOf(same_origin) >= 0) {
    e.respondWith(new Response("same_origin_window"));
  }
}
