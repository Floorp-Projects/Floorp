const MESSAGE_PAGE = function (msg) {
  return `
<html>
<script type="text/javascript">
window.parent.postMessage({test : "${msg}"},"*");
</script>
<script>
  addEventListener("back", () => {
	  history.back();
  });
  addEventListener("forward", () => {
	  history.forward();
  });
</script>
<body>
  <a id="test2_button" href="https://example.com/browser/dom/security/test/sec-fetch/file_no_cache.sjs?test2">Click me</a>
  <a id="test3_button" href="https://example.com/browser/dom/security/test/sec-fetch/file_no_cache.sjs?test3">Click me</a>
<body>
</html>
`;
};

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-store");
  response.setHeader("Content-Type", "text/html");

  response.write(MESSAGE_PAGE(request.queryString));
}
