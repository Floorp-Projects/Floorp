function handleRequest(request, response) {
  response.setStatusLine("1.1", 302, "Found");
  response.setHeader("Location", "http://mochi.test:8888/tests/dom/workers/test/serviceworkers/worker.js");
}
