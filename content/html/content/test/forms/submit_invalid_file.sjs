function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 200, "Ok");
  response.setHeader("Content-Type", "text/html");
  response.setHeader("Cache-Control", "no-cache");

  var result = {};
  request.bodyInputStream.search("testfile", true, result, {});
  if (result.value) {
    response.write("SUCCESS");
  } else {
    response.write("FAIL");
  }
}
