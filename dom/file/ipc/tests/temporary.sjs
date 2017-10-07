function handleRequest(request, response)
{
  response.setHeader("Content-Type", "text/plain", false);

  var data = new Array(1024*64).join("1234567890ABCDEF");
  response.bodyOutputStream.write(data, data.length);
}
