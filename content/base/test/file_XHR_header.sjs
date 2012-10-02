// SJS file for getAllResponseRequests vs getResponseRequest
function handleRequest(request, response)
{
  response.setHeader("X-Custom-Header-Bytes", "…", false);
  response.write("42");
}
