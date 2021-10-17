
function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.scheme === "https") {
    // Simulating a timeout by processing the https request
    // async and *never* return anything!
    response.processAsync();
    return;
  }
  // we should never get here; just in case, return something unexpected
  response.write("do'h");
}
