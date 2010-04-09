function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);
  var query = request.queryString;
  switch (query) {
    case "reset":
      response.setHeader("Content-Type", "application/ecmascript", false);
      setState("1l", "");
      setState("1v", "");
      setState("2l", "");
      setState("2v", "");
      break;
    case "1l":
    case "1v":
    case "2l":
    case "2v":
      setState(query, getState(query) + "load");
      response.setStatusLine("1.1", 302, "Found");
      // redirect to a solid blue image
      response.setHeader("Location", "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVQI12NgYPgPAAEDAQDZqt2zAAAAAElFTkSuQmCC");
      response.setHeader("Content-Type", "text/plain", false);
      break;
    case "result":
      response.setHeader("Content-Type", "application/ecmascript", false);
      response.write("is('" + getState("1l") +
                     "', 'load', 'image 1l should have been loaded once')\n");
      response.write("is('" + getState("1v") +
                     "', '', 'image 1v should not have been loaded')\n");
      response.write("is('" + getState("2l") +
                     "', 'load', 'image 2l should have been loaded once')\n");
      response.write("is('" + getState("2v") +
                     "', '', 'image 2v should not have been loaded')\n");
      response.write("SimpleTest.finish()");
      break;
  }
}
