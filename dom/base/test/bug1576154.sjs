function handleRequest(request, response)
{
  response.setStatusLine("1.1", 500, "Internal Server Error");
  response.setHeader("Content-Type", "image/svg+xml", false);

  let body = "<svg xmlns='http://www.w3.org/2000/svg' width='70' height='0'></svg>";
  response.bodyOutputStream.write(body, body.length);
}
