Components.utils.importGlobalProperties(["URLSearchParams"]);
const SJS = "referrer_test_server.sjs?";
const SHARED_KEY = SJS;

var SAME_ORIGIN = "https://example.com/tests/dom/workers/test/" + SJS;
var CROSS_ORIGIN = "https://test2.example.com/tests/dom/workers/test/" + SJS;
var DOWNGRADE = "http://example.com/tests/dom/workers/test/" + SJS;

function createUrl(aRequestType, aPolicy) {
  var searchParams = new URLSearchParams();
  searchParams.append("ACTION", "request-worker");
  searchParams.append("Referrer-Policy", aPolicy);
  searchParams.append("TYPE", aRequestType);

  var url = SAME_ORIGIN;

  if (aRequestType === "cross-origin") {
    url = CROSS_ORIGIN;
  } else if (aRequestType === "downgrade") {
    url = DOWNGRADE;
  }

  return url + searchParams.toString();
}
function createWorker (aRequestType, aPolicy) {
  return `
    onmessage = function() {
      fetch("${createUrl(aRequestType, aPolicy)}").then(function () {
        postMessage(42);
        close();
      });
    }
  `;
}

function handleRequest(request, response) {
  var params = new URLSearchParams(request.queryString);
  var policy = params.get("Referrer-Policy");
  var type = params.get("TYPE");
  var action = params.get("ACTION");
  response.setHeader("Content-Security-Policy", "default-src *", false);
  response.setHeader("Access-Control-Allow-Origin", "*", false);

  if (policy) {
    response.setHeader("Referrer-Policy", policy, false);
  }

  if (action === "test") {
    response.setHeader("Content-Type", "text/javascript", false);
    response.write(createWorker(type, policy));
    return;
  }

  if (action === "resetState") {
    setSharedState(SHARED_KEY, "{}");
    response.write("");
    return;
  }

  if (action === "get-test-results") {
    response.setHeader("Cache-Control", "no-cache", false);
    response.setHeader("Content-Type", "text/plain", false);
    response.write(getSharedState(SHARED_KEY));
    return;
  }

  if (action === "request-worker") {
    var result = getSharedState(SHARED_KEY);
    result = result ? JSON.parse(result) : {};
    var referrerLevel = "none";
    var test = {};

    if (request.hasHeader("Referer")) {
      var referrer = request.getHeader("Referer");
      if (referrer.indexOf("referrer_test_server") > 0) {
        referrerLevel = "full";
      } else if (referrer.indexOf("https://example.com") == 0) {
        referrerLevel = "origin";
      } else {
        // this is never supposed to happen
        referrerLevel = "other-origin";
      }
      test.referrer = referrer;
    } else {
      test.referrer = "";
    }

    test.policy = referrerLevel;
    test.expected = policy;

    // test id equals type + "-" + policy
    // Ex: same-origin-default
    result[type + "-" + policy] = test;
    setSharedState(SHARED_KEY, JSON.stringify(result));

    response.write("'hello world'");
    return;
  }
}


