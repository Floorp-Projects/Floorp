// SJS file specifically for the needs of bug
// Bug 1223743 - CSP: Check baseChannel for CSP when loading multipart channel

var CSP = "script-src 'unsafe-inline', img-src 'none'";
var BOUNDARY = "fooboundary"

// small red image
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
  "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==");

var RESPONSE = `
  <script>
  var myImg = new Image;
  myImg.src = "file_multipart_testserver.sjs?img";
  myImg.onerror = function(e) {
    window.parent.postMessage("img-blocked", "*");
  };
  myImg.onload = function() {
    window.parent.postMessage("img-loaded", "*");
  };
  document.body.appendChild(myImg);
  </script>
`;

var myTimer;

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.queryString == "doc") {
    response.setHeader("Content-Security-Policy", CSP, false);
    response.setHeader("Content-Type", "multipart/x-mixed-replace; boundary=" + BOUNDARY, false);
    response.write(BOUNDARY + "\r\n");
    response.write(RESPONSE);
    response.write(BOUNDARY + "\r\n");
    return;
  }

  if (request.queryString == "img") {
    response.setHeader("Content-Type", "image/png");
    response.write(IMG_BYTES);
    return;
  }

  // we should never get here - return something unexpected
  response.write("d'oh");
}
