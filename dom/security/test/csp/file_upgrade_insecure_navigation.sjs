// Custom *.sjs file specifically for the needs of
// https://bugzilla.mozilla.org/show_bug.cgi?id=1271173

"use strict";

const TEST_NAVIGATIONAL_UPGRADE = `
  <!DOCTYPE html>
  <html>
  <head><meta charset="utf-8"></head>
  <body>
  <a href="http://example.com/tests/dom/security/test/csp/file_upgrade_insecure_navigation.sjs?action=framenav" id="testlink">clickme</a>
  <script type="text/javascript">
    // before navigating the current frame we open the window and check that uir applies
    var myWin = window.open("http://example.com/tests/dom/security/test/csp/file_upgrade_insecure_navigation.sjs?action=docnav");

    window.addEventListener("message", receiveMessage, false);
    function receiveMessage(event) {
      myWin.close();
      var link = document.getElementById('testlink');
      link.click();
    }
  </script>
  </body>
  </html>`;

const FRAME_NAV = `
  <!DOCTYPE html>
  <html>
  <head><meta charset="utf-8"></head>
  <body>
  <script type="text/javascript">
    parent.postMessage({result: document.documentURI}, "*");
  </script>
  </body>
  </html>`;

const DOC_NAV = `
  <!DOCTYPE html>
  <html>
  <head><meta charset="utf-8"></head>
  <body>
  <script type="text/javascript">
    // call back to the main testpage signaling whether the upgraded succeeded
    window.opener.parent.postMessage({result: document.documentURI}, "*");
    // let the opener (iframe) now that we can now close the window and move on with the test.
    window.opener.postMessage({result: "readyToMoveOn"}, "*");
  </script>
  </body>
  </html>`;

function handleRequest(request, response) {
  const query = new URLSearchParams(request.queryString);

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);
  if (query.get("csp")) {
    response.setHeader("Content-Security-Policy", query.get("csp"), false);
  }

  if (query.get("action") === "perform_navigation") {
    response.write(TEST_NAVIGATIONAL_UPGRADE);
    return;
  }

  if (query.get("action") === "framenav") {
    response.write(FRAME_NAV);
    return;
  }

  if (query.get("action") === "docnav") {
    response.write(DOC_NAV);
    return;
  }

  // we should never get here, but just in case
  // return something unexpected
  response.write("do'h");
}
