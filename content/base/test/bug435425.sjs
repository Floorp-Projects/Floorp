function handleRequest(request, response)
{
  response.setHeader("Content-Type", "text/plain", false);
  if (request.method == "GET") {
    response.write(request.queryString);
  } else {
    var rawdata = request.body.purge();
    var data = String.fromCharCode.apply(null, rawdata);
    response.write(data);
  }
}

