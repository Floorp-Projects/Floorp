// SJS file to serve resources for CSP redirect tests
// This file mimics serving resources, e.g. fonts, images, etc., which a CSP
// can include. The resource may redirect to a different resource, if specified.
function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  var thisSite = "http://mochi.test:8888";
  var otherSite = "http://example.com";
  var resource = "/tests/content/base/test/csp/file_csp_redirects_resource.sjs";

  response.setHeader("Cache-Control", "no-cache", false);

  // redirect to a resource on this site
  if (query["redir"] == "same") {
    var loc = thisSite+resource+"?res="+query["res"]+"&testid="+query["id"];
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", loc, false);
    return;
  }

  // redirect to a resource on a different site
  else if (query["redir"] == "other") {
    var loc = otherSite+resource+"?res="+query["res"]+"&testid="+query["id"];
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", loc, false);
    return;
  }

  // not a redirect.  serve some content.
  // the content doesn't have to be valid, since we're only checking whether
  // the request for the content was sent or not.

  // downloadable font
  if (query["res"] == "font") {
    response.setHeader("Access-Control-Allow-Origin", "*", false);
    response.setHeader("Content-Type", "text/plain", false);
    response.write("font data...");
    return;
  }

  if (query["res"] == "font-spec-compliant") {
    response.setHeader("Access-Control-Allow-Origin", "*", false);
    response.setHeader("Content-Type", "text/plain", false);
    response.write("font data...");
    return;
  }

  // iframe with arbitrary content
  if (query["res"] == "iframe") {
    response.setHeader("Content-Type", "text/html", false);
    response.write("iframe content...");
    return;
  }

  // image
  if (query["res"] == "image") {
    response.setHeader("Content-Type", "image/gif", false);
    response.write("image data...");
    return;
  }

  // media content, e.g. Ogg video
  if (query["res"] == "media") {
    response.setHeader("Content-Type", "video/ogg", false);
    response.write("video data...");
    return;
  }

  // plugin content, e.g. <object>
  if (query["res"] == "object") {
    response.setHeader("Content-Type", "text/html", false);
    response.write("object data...");
    return;
  }

  // script
  if (query["res"] == "script") {
    response.setHeader("Content-Type", "application/javascript", false);
    response.write("some script...");
    return;
  }

  // external stylesheet
  if (query["res"] == "style") {
    response.setHeader("Content-Type", "text/css", false);
    response.write("css data...");
    return;
  }

  // web worker resource
  if (query["res"] == "worker") {
    response.setHeader("Content-Type", "application/javascript", false);
    response.write("worker script data...");
    return;
  }

  // script that invokes XHR
  if (query["res"] == "xhr") {
    response.setHeader("Content-Type", "text/html", false);
    var resp = 'var x = new XMLHttpRequest(); x.open("GET", "' + otherSite +
               resource+'?res=xhr-resp&testid=xhr-src-redir", false); ' +
               'x.send(null);';
    response.write(resp);
    return;
  }

  if (query["res"] == "xhr-spec-compliant") {
    response.setHeader("Content-Type", "text/html", false);
    var resp = 'var x = new XMLHttpRequest(); x.open("GET", "' + otherSite +
               resource+'?res=xhr-resp-spec-compliant&testid=xhr-src-redir-spec-compliant", false); ' +
               'x.send(null);';
    response.write(resp);
    return;
  }

  // response to XHR
  if (query["res"] == "xhr-resp-spec-compliant") {
    response.setHeader("Access-Control-Allow-Origin", "*", false);
    response.setHeader("Content-Type", "text/html", false);
    response.write('XHR response...');
    return;
  }
}
