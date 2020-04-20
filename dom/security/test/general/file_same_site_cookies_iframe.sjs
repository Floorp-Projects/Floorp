// Custom *.sjs file specifically for the needs of Bug 1454027

// small red image
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
  "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==");

const NESTED_IFRAME_NAVIGATION = `
  <html>
  <body>
    <a id="testlink" href="http://mochi.test:8888/tests/dom/security/test/general/file_same_site_cookies_iframe.sjs"></a>
    <script type="application/javascript">
      let link = document.getElementById("testlink");
      link.click();
    <\/script>
  </body>
  </html>`;

const NESTED_IFRAME_INCLUSION = `
  <html>
  <body>
    <script type="application/javascript">
    // simply passing on the message from the child to parent
    window.addEventListener("message", receiveMessage);
    function receiveMessage(event) {
      window.removeEventListener("message", receiveMessage);
      window.parent.postMessage({result: event.data.result}, '*');
    }
    <\/script>
    <iframe src="http://mochi.test:8888/tests/dom/security/test/general/file_same_site_cookies_iframe.sjs"></iframe>
  </body>
  </html>`;

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  // using startsWith and discard the math random
  if (request.queryString.startsWith("setSameSiteCookie")) {
    response.setHeader("Set-Cookie", "myKey=mySameSiteIframeTestCookie; samesite=strict", true);
    response.setHeader("Content-Type", "image/png");
    response.write(IMG_BYTES);
    return;
  }

  // navigation tests
  if (request.queryString === "nestedIframeNavigation") {
    response.write(NESTED_IFRAME_NAVIGATION);
    return;
  }

  if (request.queryString === "nestedSandboxIframeNavigation") {
    response.setHeader("Content-Security-Policy", "sandbox allow-scripts", false);
    response.write(NESTED_IFRAME_NAVIGATION);
    return;
  }

  // inclusion tests
  if (request.queryString === "nestedIframeInclusion") {
    response.write(NESTED_IFRAME_INCLUSION);
    return;
  }

  if (request.queryString === "nestedSandboxIframeInclusion") {
    response.setHeader("Content-Security-Policy", "sandbox allow-scripts", false);
    response.write(NESTED_IFRAME_INCLUSION);
    return;
  }

  const cookies = request.hasHeader("Cookie") ? request.getHeader("Cookie") : "";
  response.write(`
    <!DOCTYPE html>
    <html>
    <head>
      <title>Bug 1454027 - Update SameSite cookie handling inside iframes</title>
    </head>
    <body>
      <script type="application/javascript">
        window.parent.postMessage({result: "${cookies}" }, '*');
      </script>
    </body>
    </html>
  `);
}
