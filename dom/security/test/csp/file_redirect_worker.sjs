// SJS file to serve resources for CSP redirect tests
// This file redirects to a specified resource.
const THIS_SITE = "http://mochi.test:8888";
const OTHER_SITE = "http://example.com";

function handleRequest(request, response) {
  var query = {};
  request.queryString.split("&").forEach(function (val) {
    var [name, value] = val.split("=");
    query[name] = unescape(value);
  });

  var resource = query.path;

  response.setHeader("Cache-Control", "no-cache", false);
  var loc = "";

  // redirect to a resource on this site
  if (query.redir == "same") {
    loc = THIS_SITE + resource + "#" + query.page_id;
  }

  // redirect to a resource on a different site
  else if (query.redir == "other") {
    loc = OTHER_SITE + resource + "#" + query.page_id;
  }

  response.setStatusLine("1.1", 302, "Found");
  response.setHeader("Location", loc, false);

  response.write(
    '<html><head><meta http-equiv="refresh" content="0; url=' + loc + '">'
  );
}
