// Custom *.sjs file specifically for the needs of Bug:
// Bug 663570 - Implement Content Security Policy via meta tag

const HTML_HEAD =
  "<!DOCTYPE HTML>" +
  "<html>" +
  "<head>" +
  "<meta charset='utf-8'>" +
  "<title>Bug 663570 - Implement Content Security Policy via <meta> tag</title>";

const HTML_BODY =
  "</head>" +
  "<body>" +
  "<img id='testimage' src='http://mochi.test:8888/tests/image/test/mochitest/blue.png'></img>" +
  "<script type='application/javascript'>" +
  "  var myImg = document.getElementById('testimage');" +
  "  myImg.onload = function(e) {" +
  "    window.parent.postMessage({result: 'img-loaded'}, '*');" +
  "  };" +
  "  myImg.onerror = function(e) { " +
  "    window.parent.postMessage({result: 'img-blocked'}, '*');" +
  "  };" +
  "</script>" +
  "</body>" +
  "</html>";

const META_CSP_BLOCK_IMG =
  "<meta http-equiv=\"Content-Security-Policy\" content=\"img-src 'none'\">";

const META_CSP_ALLOW_IMG =
  "<meta http-equiv=\"Content-Security-Policy\" content=\"img-src http://mochi.test:8888;\">";

const HEADER_CSP_BLOCK_IMG = "img-src 'none';";

const HEADER_CSP_ALLOW_IMG = "img-src http://mochi.test:8888";

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);
  var queryString = request.queryString;

  if (queryString === "test1") {
    /* load image without any CSP */
    response.write(HTML_HEAD + HTML_BODY);
    return;
  }

  if (queryString === "test2") {
    /* load image where meta denies load */
    response.write(HTML_HEAD + META_CSP_BLOCK_IMG + HTML_BODY);
    return;
  }

  if (queryString === "test3") {
    /* load image where meta allows load */
    response.write(HTML_HEAD + META_CSP_ALLOW_IMG + HTML_BODY);
    return;
  }

  if (queryString === "test4") {
    /* load image where meta allows but header blocks */
    response.setHeader("Content-Security-Policy", HEADER_CSP_BLOCK_IMG, false);
    response.write(HTML_HEAD + META_CSP_ALLOW_IMG + HTML_BODY);
    return;
  }

  if (queryString === "test5") {
    /* load image where meta blocks but header allows */
    response.setHeader("Content-Security-Policy", HEADER_CSP_ALLOW_IMG, false);
    response.write(HTML_HEAD + META_CSP_BLOCK_IMG + HTML_BODY);
    return;
  }

  if (queryString === "test6") {
    /* load image where meta allows and header allows */
    response.setHeader("Content-Security-Policy", HEADER_CSP_ALLOW_IMG, false);
    response.write(HTML_HEAD + META_CSP_ALLOW_IMG + HTML_BODY);
    return;
  }

  if (queryString === "test7") {
    /* load image where meta1 allows but meta2 blocks */
    response.write(HTML_HEAD + META_CSP_ALLOW_IMG + META_CSP_BLOCK_IMG + HTML_BODY);
    return;
  }

  if (queryString === "test8") {
    /* load image where meta1 allows and meta2 allows */
    response.write(HTML_HEAD + META_CSP_ALLOW_IMG + META_CSP_ALLOW_IMG + HTML_BODY);
    return;
  }

  // we should never get here, but just in case, return
  // something unexpected
  response.write("do'h");
}
