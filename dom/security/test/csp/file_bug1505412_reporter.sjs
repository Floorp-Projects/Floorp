function handleRequest(request, response) {
  var receivedRequests = parseInt(getState("requests"));
  if (isNaN(receivedRequests)) {
    receivedRequests = 0;
  }
  if (request.queryString.includes("state")) {
    response.write(receivedRequests);
    return;
  }
  if (request.queryString.includes("flush")) {
    setState("requests", "0");
    response.write("OK");
    return;
  }
  receivedRequests = receivedRequests + 1;
  setState("requests", "" + receivedRequests);
  response.write("OK");
}
