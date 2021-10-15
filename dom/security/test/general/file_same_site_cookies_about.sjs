// Custom *.sjs file specifically for the needs of Bug 1454721

// small red image
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
  "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==");

const IFRAME_INC =
  `<iframe src='http://mochi.test:8888/tests/dom/security/test/general/file_same_site_cookies_about.sjs?inclusion'></iframe>`;

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  // using startsWith and discard the math random
  if (request.queryString.startsWith("setSameSiteCookie")) {
    response.setHeader("Set-Cookie", "myKey=mySameSiteAboutCookie; samesite=strict", true);
    response.setHeader("Content-Type", "image/png");
    response.write(IMG_BYTES);
    return;
  }

  // navigation tests
  if (request.queryString.includes("loadsrcdocframeNav")) {
    let FRAME = `
      <iframe srcdoc="foo"
       onload="document.location='http://mochi.test:8888/tests/dom/security/test/general/file_same_site_cookies_about.sjs?navigation'">
      </iframe>`;
    response.write(FRAME);
    return;
  }

  if (request.queryString.includes("loadblankframeNav")) {
    let FRAME = `
      <iframe src="about:blank"
       onload="document.location='http://mochi.test:8888/tests/dom/security/test/general/file_same_site_cookies_about.sjs?navigation'">
      </iframe>`;
    response.write(FRAME);
    return;
  }

  // inclusion tets
  if (request.queryString.includes("loadsrcdocframeInc")) {
    response.write("<iframe srcdoc=\"" + IFRAME_INC + "\"></iframe>");
    return;
  }

  if (request.queryString.includes("loadblankframeInc")) {
    let FRAME = `
      <iframe id="blankframe" src="about:blank"></iframe>
      <script>
        document.getElementById("blankframe").contentDocument.write(\"` + IFRAME_INC +`\");
      <\/script>`;
    response.write(FRAME);
    return;
  }

  if (request.queryString.includes("navigation")) {
    const cookies = request.hasHeader("Cookie") ? request.getHeader("Cookie") : "";
    response.write(`
      <!DOCTYPE html>
      <html>
      <body>
        <script type="application/javascript">
          window.parent.postMessage({result: "${cookies}" }, '*');
        </script>
      </body>
      </html>
    `);
  }

  if (request.queryString.includes("inclusion")) {
    const cookies = request.hasHeader("Cookie") ? request.getHeader("Cookie") : "";
    response.write(`
      <!DOCTYPE html>
      <html>
      <body>
        <script type="application/javascript">
          window.parent.parent.parent.postMessage({result: "${cookies}" }, '*');
        </script>
      </body>
      </html>
    `);
  }

  // we should never get here, but just in case return something unexpected
  response.write("D'oh");
}
