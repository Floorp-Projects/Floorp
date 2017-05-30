function handleRequest(aRequest, aResponse) {
 aResponse.setStatusLine(aRequest.httpVersion, 302, "Moved");
 aResponse.setHeader("Location", "http://mochi.test:8888/tests/caps/tests/mochitest/file_bug1367586-target.html");
 aResponse.write("To be redirected to target");
}
