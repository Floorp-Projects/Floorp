function handleRequest(request, response)
{
  if (request.queryString.indexOf("report") != -1) {
    response.setHeader("Content-Type", "text/javascript", false);
    if (getState("loaded") == "loaded") {
      response.write("ok(false, 'There was an attempt to preload the image.');");      
    } else {
      response.write("ok(true, 'There was no attempt to preload the image.');");      
    }
    response.write("SimpleTest.finish();");
  } else {
    setState("loaded", "loaded");
    response.setHeader("Content-Type", "image/svg", false);
    response.write("<svg xmlns='http://www.w3.org/2000/svg'>Not supposed to load this</svg>");
  }
}
