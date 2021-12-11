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
  response.setHeader("Cache-Control", "max-age=999999"); // Force caching
  response.setHeader("Content-Type", "text/css");
  receivedRequests = receivedRequests + 1;
  setState("requests", "" + receivedRequests);
  response.write(`
        .test{
            color:red;
        }
        .test h1{
            font-size:200px;
        }
    `);
}
