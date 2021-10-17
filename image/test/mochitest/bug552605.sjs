function handleRequest(request, response)
{
  var redirectstate = "/image/test/mochitest/bug89419.sjs";
  response.setStatusLine("1.1", 302, "Found");
  if (getState(redirectstate) == "") {
    response.setHeader("Location", "red.png", false);
    setState(redirectstate, "red");
  } else {
    response.setHeader("Location", "blue.png", false);
    setState(redirectstate, "");
  }
  response.setHeader("Cache-Control", "no-cache", false);
}
