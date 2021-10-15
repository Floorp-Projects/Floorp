function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);
  switch (request.queryString) {
    case "reset":
      response.setHeader("Content-Type", "application/ecmascript", false);
      setState("imageloaded", "");
      break;
    case "image":
      setState("imageloaded", "imageloaded");
      response.setStatusLine("1.1", 302, "Found");
      // redirect to a solid blue image
      response.setHeader("Location", "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVQI12NgYPgPAAEDAQDZqt2zAAAAAElFTkSuQmCC");
      response.setHeader("Content-Type", "text/plain", false);
      break;
    case "result":
      response.setHeader("Content-Type", "application/ecmascript", false);
      var state = getState("imageloaded");
      response.write("is('" + state +
                     "', '', 'image should not have been loaded')\n");
      response.write("SimpleTest.finish()");
      break;
  }
}
