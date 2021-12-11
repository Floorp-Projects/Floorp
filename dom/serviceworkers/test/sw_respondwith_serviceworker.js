const SERVICEWORKER_DOC = `<!DOCTYPE HTML>
<html>
<head>
  <meta charset="utf-8">
  <script src="utils.js" type="text/javascript"></script>
</head>
<body>
SERVICEWORKER
</body>
</html>
`;

const SERVICEWORKER_RESPONSE = new Response(SERVICEWORKER_DOC, {
  headers: { "content-type": "text/html" },
});

addEventListener("fetch", event => {
  // Allow utils.js which we explicitly include to be loaded by resetting
  // interception.
  if (event.request.url.endsWith("/utils.js")) {
    return;
  }
  event.respondWith(SERVICEWORKER_RESPONSE.clone());
});
