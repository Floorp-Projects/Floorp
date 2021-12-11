"use strict";

const TEST_FRAME = `<!DOCTYPE HTML>
   <html>
   <body>
   <script id='myScript' nonce='123456789' type='application/javascript'></script>
   <script nonce='123456789'>
     let myScript = document.getElementById('myScript');
     // 1) start loading the script using the nonce 123456789
     myScript.src='file_nonce_snapshot.sjs?redir-script';
     // 2) dynamically change the nonce, load should use initial nonce
     myScript.setAttribute('nonce','987654321');
   </script>
   </body>
   </html>`;

const SCRIPT = "window.parent.postMessage('script-loaded', '*');";

function handleRequest(request, response) {
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  let queryString = request.queryString;

  if (queryString === "load-frame") {
    response.setHeader(
      "Content-Security-Policy",
      "script-src 'nonce-123456789'",
      false
    );
    response.setHeader("Content-Type", "text/html", false);
    response.write(TEST_FRAME);
    return;
  }

  if (queryString === "redir-script") {
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader(
      "Location",
      "file_nonce_snapshot.sjs?load-script",
      false
    );
    return;
  }

  if (queryString === "load-script") {
    response.setHeader("Content-Type", "application/javascript", false);
    response.write(SCRIPT);
    return;
  }

  // we should never get here but just in case return something unexpected
  response.write("do'h");
}
