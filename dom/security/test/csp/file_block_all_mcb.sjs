// custom *.sjs for Bug 1122236
// CSP: 'block-all-mixed-content'

const HEAD =
  "<!DOCTYPE HTML>" +
  "<html><head><meta charset=\"utf-8\">" +
  "<title>Bug 1122236 - CSP: Implement block-all-mixed-content</title>" +
  "</head>";

const CSP_ALLOW =
  "<meta http-equiv=\"Content-Security-Policy\" content=\"img-src *\">";

const CSP_BLOCK =
  "<meta http-equiv=\"Content-Security-Policy\" content=\"block-all-mixed-content\">";

const BODY =
  "<body>" +
  "<img id=\"testimage\" src=\"http://mochi.test:8888/tests/image/test/mochitest/blue.png\"></img>" +
  "<script type=\"application/javascript\">" +
  "  var myImg = document.getElementById(\"testimage\");" +
  "  myImg.onload = function(e) {" +
  "    window.parent.postMessage({result: \"img-loaded\"}, \"*\");" +
  "  };" +
  "  myImg.onerror = function(e) {" +
  "    window.parent.postMessage({result: \"img-blocked\"}, \"*\");" +
  "  };" +
  "</script>" +
  "</body>" +
  "</html>";

// We have to use this special code fragment, in particular '?nocache' to trigger an
// actual network load rather than loading the image from the cache.
const BODY_CSPRO =
  "<body>" +
  "<img id=\"testimage\" src=\"http://mochi.test:8888/tests/image/test/mochitest/blue.png?nocache\"></img>" +
  "<script type=\"application/javascript\">" +
  "  var myImg = document.getElementById(\"testimage\");" +
  "  myImg.onload = function(e) {" +
  "    window.parent.postMessage({result: \"img-loaded\"}, \"*\");" +
  "  };" +
  "  myImg.onerror = function(e) {" +
  "    window.parent.postMessage({result: \"img-blocked\"}, \"*\");" +
  "  };" +
  "</script>" +
  "</body>" +
  "</html>";

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
 
  var queryString = request.queryString;

  if (queryString === "csp-block") {
    response.write(HEAD + CSP_BLOCK + BODY);
    return;
  }
  if (queryString === "csp-allow") {
    response.write(HEAD + CSP_ALLOW + BODY);
    return;
  }
  if (queryString === "no-csp") {
    response.write(HEAD + BODY);
    return;
  }
  if (queryString === "cspro-block") {
    // CSP RO is not supported in meta tag, let's use the header
    response.setHeader("Content-Security-Policy-Report-Only", "block-all-mixed-content", false);
    response.write(HEAD + BODY_CSPRO);
    return;
  }
  // we should never get here but just in case return something unexpected
  response.write("do'h");

}
