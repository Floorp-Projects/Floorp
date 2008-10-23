function handleRequest(request, response)
{
  response.setHeader("Content-Type", "text/plain", false);
  var rawdata = request.body.purge();
  var data = String.fromCharCode.apply(null, rawdata);
  for (var i = 0; i < 10; ++i) {
    response.write(data);
  }
}

