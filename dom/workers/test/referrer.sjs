function handleRequest(request, response)
{
  if (request.queryString == "result") {
    response.write(getState("referer"));
    setState("referer", "INVALID");
  } else if (request.queryString == "worker") {
    response.setHeader("Content-Type", "text/javascript", false);
    response.write("onmessage = function() { postMessage(42); }");
    setState("referer", request.getHeader("referer"));
  } else if (request.queryString == 'import') {
    setState("referer", request.getHeader("referer"));
    response.write("'hello world'");
  }
}

