function handleRequest(request, response)
{
  if (request.queryString == "result") {
    response.write(getState("referer"));
  } else {
    response.setHeader("Content-Type", "text/javascript", false);
    response.write("onmessage = function() { postMessage(42); }");
    setState("referer", request.getHeader("referer"));
  }
}

