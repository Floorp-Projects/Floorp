// SJS file to serve resources for CSP redirect tests
// This file mimics serving resources, e.g. fonts, images, etc., which a CSP
// can include. The resource may redirect to a different resource, if specified.
function handleRequest(request, response) {
  var query = {};
  request.queryString.split("&").forEach(function (val) {
    var [name, value] = val.split("=");
    query[name] = unescape(value);
  });

  var thisSite = "http://mochi.test:8888";
  var otherSite = "http://example.com";
  var resource = "/tests/dom/security/test/csp/file_redirects_resource.sjs";

  response.setHeader("Cache-Control", "no-cache", false);

  // redirect to a resource on this site
  if (query.redir == "same") {
    var loc = thisSite + resource + "?res=" + query.res + "&testid=" + query.id;
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", loc, false);
    return;
  }

  // redirect to a resource on a different site
  else if (query.redir == "other") {
    var loc =
      otherSite + resource + "?res=" + query.res + "&testid=" + query.id;
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", loc, false);
    return;
  }

  // not a redirect.  serve some content.
  // the content doesn't have to be valid, since we're only checking whether
  // the request for the content was sent or not.

  // downloadable font
  if (query.res == "font") {
    response.setHeader("Access-Control-Allow-Origin", "*", false);
    response.setHeader("Content-Type", "text/plain", false);
    response.write("font data...");
    return;
  }

  // iframe with arbitrary content
  if (query.res == "iframe") {
    response.setHeader("Content-Type", "text/html", false);
    response.write("iframe content...");
    return;
  }

  // image
  if (query.res == "image") {
    response.setHeader("Content-Type", "image/gif", false);
    response.write("image data...");
    return;
  }

  // media content, e.g. Ogg video
  if (query.res == "media") {
    response.setHeader("Content-Type", "video/ogg", false);
    response.write("video data...");
    return;
  }

  // plugin content, e.g. <object>
  if (query.res == "object") {
    response.setHeader("Content-Type", "text/html", false);
    response.write("object data...");
    return;
  }

  // script
  if (query.res == "script") {
    response.setHeader("Content-Type", "application/javascript", false);
    response.write("some script...");
    return;
  }

  // external stylesheet
  if (query.res == "style") {
    response.setHeader("Content-Type", "text/css", false);
    response.write("css data...");
    return;
  }

  // internal stylesheet that loads an image from an external site
  if (query.res == "cssLoader") {
    let bgURL = thisSite + resource + "?redir=other&res=image&id=" + query.id;
    response.setHeader("Content-Type", "text/css", false);
    response.write("body { background:url('" + bgURL + "'); }");
    return;
  }

  // script that loads an internal worker that uses importScripts on a redirect
  // to an external script.
  if (query.res == "loadWorkerThatMakesRequests") {
    // this creates a worker (same origin) that imports a redirecting script.
    let workerURL =
      thisSite + resource + "?res=makeRequestsWorker&id=" + query.id;
    response.setHeader("Content-Type", "application/javascript", false);
    response.write("new Worker('" + workerURL + "');");
    return;
  }

  // script that loads an internal worker that uses importScripts on a redirect
  // to an external script.
  if (query.res == "loadBlobWorkerThatMakesRequests") {
    // this creates a worker (same origin) that imports a redirecting script.
    let workerURL =
      thisSite + resource + "?res=makeRequestsWorker&id=" + query.id;
    response.setHeader("Content-Type", "application/javascript", false);
    response.write(
      "var x = new XMLHttpRequest(); x.open('GET', '" + workerURL + "'); "
    );
    response.write("x.responseType = 'blob'; x.send(); ");
    response.write(
      "x.onload = () => { new Worker(URL.createObjectURL(x.response)); };"
    );
    return;
  }

  // source for a worker that simply calls importScripts on a script that
  // redirects.
  if (query.res == "makeRequestsWorker") {
    // this is code for a worker that imports a redirected script.
    let scriptURL =
      thisSite +
      resource +
      "?redir=other&res=script&id=script-src-redir-" +
      query.id;
    let xhrURL =
      thisSite +
      resource +
      "?redir=other&res=xhr-resp&id=xhr-src-redir-" +
      query.id;
    let fetchURL =
      thisSite +
      resource +
      "?redir=other&res=xhr-resp&id=fetch-src-redir-" +
      query.id;
    response.setHeader("Content-Type", "application/javascript", false);
    response.write("try { importScripts('" + scriptURL + "'); } catch(ex) {} ");
    response.write(
      "var x = new XMLHttpRequest(); x.open('GET', '" + xhrURL + "'); x.send();"
    );
    response.write("fetch('" + fetchURL + "');");
    return;
  }

  // script that invokes XHR
  if (query.res == "xhr") {
    response.setHeader("Content-Type", "application/javascript", false);
    var resp =
      'var x = new XMLHttpRequest();x.open("GET", "' +
      thisSite +
      resource +
      '?redir=other&res=xhr-resp&id=xhr-src-redir", false);\n' +
      "x.send(null);";
    response.write(resp);
    return;
  }

  // response to XHR
  if (query.res == "xhr-resp") {
    response.setHeader("Access-Control-Allow-Origin", "*", false);
    response.setHeader("Content-Type", "text/html", false);
    response.write("XHR response...");
  }
}
