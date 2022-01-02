function handleRequest(request, response) {
  response.setHeader("Referrer-Policy", "same-origin");
  response.write(
    '<!DOCTYPE HTML><html><body>Loaded</body><script>parent.postMessage(document.referrer, "*");</script></html>'
  );
}
