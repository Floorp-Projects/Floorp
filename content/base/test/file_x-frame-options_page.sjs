// SJS file for X-Frame-Options mochitests
function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  var testHeaders = {
    "deny": "DENY",
    "sameorigin": "SAMEORIGIN",
    "sameorigin2": "SAMEORIGIN, SAMEORIGIN",
    "sameorigin3": "SAMEORIGIN,SAMEORIGIN , SAMEORIGIN",
    "mixedpolicy": "DENY,SAMEORIGIN",

    /* added for bug 836132 */
    "afa": "ALLOW-FROM http://mochi.test:8888/",
    "afd": "ALLOW-FROM http://example.com/",
    "afa1": "ALLOW-FROM http://mochi.test:8888",
    "afd1": "ALLOW-FROM:example.com",
    "afd2": "ALLOW-FROM: example.com",
    "afd3": "ALLOW-FROM example.com",
    "afd4": "ALLOW-FROM:http://example.com",
    "afd5": "ALLOW-FROM: http://example.com",
    "afd6": "ALLOW-FROM http://example.com",
    "afd7": "ALLOW-FROM:mochi.test:8888",
    "afd8": "ALLOW-FROM: mochi.test:8888",
    "afd9": "ALLOW-FROM:http://mochi.test:8888",
    "afd10": "ALLOW-FROM: http://mochi.test:8888",
    "afd11": "ALLOW-FROM mochi.test:8888",
    "afd12": "ALLOW-FROM",
    "afd13": "ALLOW-FROM ",
    "afd14": "ALLOW-FROM:"
  };

  if (testHeaders.hasOwnProperty(query['xfo'])) {
    response.setHeader("X-Frame-Options", testHeaders[query['xfo']], false);
  }

  // from the test harness we'll be checking for the presence of this element
  // to test if the page loaded
  response.write("<h1 id=\"test\">" + query["testid"] + "</h1>");
}
