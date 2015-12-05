// SJS file for CSP redirect mochitests
// This file serves pages which can optionally specify a Content Security Policy
function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  var resource = "/tests/dom/security/test/csp/file_redirects_resource.sjs";

  // CSP header value
  var additional = ""
  if (query['testid'] == "worker") {
    additional = "; script-src 'self' 'unsafe-inline'";
  }
  response.setHeader("Content-Security-Policy",
      "default-src 'self' blob: ; style-src 'self' 'unsafe-inline'" + additional,
      false);

  // downloadable font that redirects to another site
  if (query["testid"] == "font-src") {
    var resp = '<style type="text/css"> @font-face { font-family:' +
               '"Redirecting Font"; src: url("' + resource +
               '?res=font&redir=other&id=font-src-redir")} #test{font-family:' +
               '"Redirecting Font"}</style></head><body>' +
               '<div id="test">test</div></body>';
    response.write(resp);
    return;
  }

  // iframe that redirects to another site
  if (query["testid"] == "frame-src") {
    response.write('<iframe src="'+resource+'?res=iframe&redir=other&id=frame-src-redir"></iframe>');
    return;
  }

  // image that redirects to another site
  if (query["testid"] == "img-src") {
    response.write('<img src="'+resource+'?res=image&redir=other&id=img-src-redir" />');
    return;
  }

  // video content that redirects to another site
  if (query["testid"] == "media-src") {
    response.write('<video src="'+resource+'?res=media&redir=other&id=media-src-redir"></video>');
    return;
  }

  // object content that redirects to another site
  if (query["testid"] == "object-src") {
    response.write('<object type="text/html" data="'+resource+'?res=object&redir=other&id=object-src-redir"></object>');
    return;
  }

  // external script that redirects to another site
  if (query["testid"] == "script-src") {
    response.write('<script src="'+resource+'?res=script&redir=other&id=script-src-redir"></script>');
    return;
  }

  // external stylesheet that redirects to another site
  if (query["testid"] == "style-src") {
    response.write('<link rel="stylesheet" type="text/css" href="'+resource+'?res=style&redir=other&id=style-src-redir"></link>');
    return;
  }

  // worker script resource that redirects to another site
  if (query["testid"] == "worker") {
    response.write('<script>var worker = new Worker("'+resource+'?res=worker&redir=other&id=worker-redir");</script>');
    return;
  }

  // script that XHR's to a resource that redirects to another site
  if (query["testid"] == "xhr-src") {
    response.write('<script src="'+resource+'?res=xhr"></script>');
    return;
  }

  // for bug949706
  if (query["testid"] == "img-src-from-css") {
    // loads a stylesheet, which in turn loads an image that redirects.
    response.write('<link rel="stylesheet" type="text/css" href="'+resource+'?res=cssLoader&id=img-src-redir-from-css">');
    return;
  }

  if (query["testid"] == "from-worker") {
    // loads a script; launches a worker; that worker uses importscript; which then gets redirected
    // So it's:
    // <script src="res=loadWorkerThatMakesRequests">
    //   .. loads Worker("res=makeRequestsWorker")
    //         .. calls importScript("res=script")
    //         .. calls xhr("res=xhr-resp")
    //         .. calls fetch("res=xhr-resp")
    response.write('<script src="'+resource+'?res=loadWorkerThatMakesRequests&id=from-worker"></script>');
    return;
  }

  if (query["testid"] == "from-blob-worker") {
    // loads a script; launches a worker; that worker uses importscript; which then gets redirected
    // So it's:
    // <script src="res=loadBlobWorkerThatMakesRequests">
    //   .. loads Worker("res=makeRequestsWorker")
    //         .. calls importScript("res=script")
    //         .. calls xhr("res=xhr-resp")
    //         .. calls fetch("res=xhr-resp")
    response.write('<script src="'+resource+'?res=loadBlobWorkerThatMakesRequests&id=from-blob-worker"></script>');
    return;
  }
}
