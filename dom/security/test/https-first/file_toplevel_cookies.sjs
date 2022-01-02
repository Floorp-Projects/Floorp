// Custom *.sjs file specifically for the needs of Bug 1711453
"use strict";

// small red image
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
    "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg=="
);

const IFRAME_INC = `<iframe id="testframeinc"></iframe>`;

// Sets an image sends cookie and location after loading
const SET_COOKIE_IMG = `
<html>
<body>
<img id="cookieImage">
<script class="testbody" type="text/javascript">
  var cookieImage = document.getElementById("cookieImage");
  cookieImage.onload = function() {
    let myLocation = window.location.href;
    let myCookie = document.cookie;
    window.opener.postMessage({result: 'upgraded', loc: myLocation, cookie: myCookie}, '*');
  }
  cookieImage.onerror = function() {
    window.opener.postMessage({result: 'error'}, '*');
  }
  // Add the last number of the old query to the new query to set cookie properly
  cookieImage.src = window.location.origin + "/tests/dom/security/test/https-first/file_toplevel_cookies.sjs?setSameSiteCookie"
   + window.location.href.charAt(window.location.href.length -1);
</script>
</body>
</html>
`;

// Load blank frame navigation sends cookie and location after loading
const LOAD_BLANK_FRAME_NAV = `
<html>
<body>
<iframe id="testframe"></iframe>
<script>
  let testframe = document.getElementById("testframe");
  testframe.onload = function() {
    let myLocation = window.location.href;
    let myCookie = document.cookie;
    window.opener.postMessage({result: 'upgraded', loc: myLocation, cookie: myCookie}, '*');
  }
  testframe.onerror = function() {
    window.opener.postMessage({result: 'error', loc: 'error', cookie: ''}, '*');
  }
  testframe.src = window.location.origin + "/tests/dom/security/test/https-first/file_toplevel_cookies.sjs?loadblankframeNav";
</script>
</body>
</html>
`;

// Load frame navigation sends cookie and location after loading
const LOAD_FRAME_NAV = `
<html>
<body>
<iframe id="testframe"></iframe>
<script>
  let testframe = document.getElementById("testframe");
  testframe.onload = function() {
    let myLocation = window.location.href;
    let myCookie = document.cookie;
    window.opener.postMessage({result: 'upgraded', loc: myLocation, cookie: myCookie}, '*');
  }
  testframe.onerror = function() {
    window.opener.postMessage({result: 'error', loc: 'error', cookie: ''}, '*');
  }
  testframe.src = window.location.origin + "/tests/dom/security/test/https-first/file_toplevel_cookies.sjs?loadsrcdocframeNav";
</script>
</body>
</html>

`;
// blank frame sends cookie and location after loading
const LOAD_BLANK_FRAME = `
<html>
<body>
<iframe id="testframe"></iframe>
<script>
  let testframe = document.getElementById("testframe");
  testframe.onload = function() {
    let myLocation = window.location.href;
    let myCookie = document.cookie;
    window.opener.postMessage({result: 'upgraded', loc: myLocation, cookie: myCookie}, '*');
  }
  testframe.onerror = function() {
    window.opener.postMessage({result: 'error', loc: 'error', cookie: ''}, '*');
  }
  testframe.src = window.location.origin + "/tests/dom/security/test/https-first/file_toplevel_cookies.sjs?loadblankframeInc";
</script>
</body>
</html>
`;
// frame sends cookie and location after loading
const LOAD_FRAME = `
<html>
<body>
<iframe id="testframe"></iframe>
<script>
  let testframe = document.getElementById("testframe");
  testframe.onload = function() {
    let myLocation = window.location.href;
    let myCookie = document.cookie;
    window.opener.postMessage({result: 'upgraded', loc: myLocation, cookie: myCookie}, '*');
  }
  testframe.onerror = function() {
    window.opener.postMessage({result: 'error', loc: 'error', cookie: ''}, '*');
  }
  testframe.src = window.location.origin + "/tests/dom/security/test/https-first/file_toplevel_cookies.sjs?loadsrcdocframeInc";
</script>
</body>
</html>
`;

const RESPONSE_UNEXPECTED = `
  <html>
    <body>
      send message, error
    <script type="application/javascript">
      let myLocation = document.location.href;
      window.opener.postMessage({result: 'error', loc: myLocation}, '*');
    </script>
    </body>
  </html>`;

function setCookie(name, query) {
  let cookie = name + "=";
  if (query.includes("0")) {
    cookie += "0;Domain=.example.com;sameSite=none";
    return cookie;
  }
  if (query.includes("1")) {
    cookie += "1;Domain=.example.com;sameSite=strict";
    return cookie;
  }
  if (query.includes("2")) {
    cookie += "2;Domain=.example.com;sameSite=none;secure";
    return cookie;
  }
  if (query.includes("3")) {
    cookie += "3;Domain=.example.com;sameSite=strict;secure";
    return cookie;
  }
  return cookie + "error";
}

function handleRequest(request, response) {
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  let query = request.queryString;
  if (query.includes("setImage")) {
    response.write(SET_COOKIE_IMG);
    return;
  }
  // using startsWith and discard the math random
  if (query.includes("setSameSiteCookie")) {
    response.setHeader("Set-Cookie", setCookie("setImage", query), true);
    response.setHeader("Content-Type", "image/png");
    response.write(IMG_BYTES);
    return;
  }

  // navigation tests
  if (query.includes("loadNavBlank")) {
    response.setHeader("Set-Cookie", setCookie("loadNavBlank", query), true);
    response.write(LOAD_BLANK_FRAME_NAV);
    return;
  }

  if (request.queryString === "loadblankframeNav") {
    let FRAME = `
      <iframe src="about:blank"
        // nothing happens here
      </iframe>`;
    response.write(FRAME);
    return;
  }

  if (query.includes("loadNav")) {
    response.setHeader("Set-Cookie", setCookie("loadNav", query), true);
    response.write(LOAD_FRAME_NAV);
    return;
  }

  if (query === "loadsrcdocframeNav") {
    let FRAME = `
      <iframe srcdoc="foo"
       // nothing happens here
      </iframe>`;
    response.write(FRAME);
    return;
  }

  // inclusion tests
  if (query.includes("loadframeIncBlank")) {
    response.setHeader(
      "Set-Cookie",
      setCookie("loadframeIncBlank", query),
      true
    );
    response.write(LOAD_BLANK_FRAME);
    return;
  }

  if (request.queryString === "loadblankframeInc") {
    let FRAME =
      ` <iframe id="blankframe" src="about:blank"></iframe>
      <script>
        document.getElementById("blankframe").contentDocument.write("` +
      IFRAME_INC +
      `");
      <\script>`;
    response.write(FRAME);
    return;
  }

  if (query.includes("loadframeInc")) {
    response.setHeader("Set-Cookie", setCookie("loadframeInc", query), true);
    response.write(LOAD_FRAME);
    return;
  }

  if (request.queryString === "loadsrcdocframeInc") {
    response.write('<iframe srcdoc="' + IFRAME_INC + '"></iframe>');
    return;
  }

  // We should never arrive here, just in case send 'error'
  response.write(RESPONSE_UNEXPECTED);
}
