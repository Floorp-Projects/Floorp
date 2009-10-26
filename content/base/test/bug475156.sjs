function handleRequest(request, response)
{
  if (request.queryString == "")
  {
    var etag = request.hasHeader("If-Match") ? request.getHeader("If-Match") : null;
    if (!etag || etag == getState("etag"))
    {
      response.setStatusLine(request.httpVersion, 200, "Ok");
      response.setHeader("Content-Type", "text/html");
      response.setHeader("ETag", getState("etag"));
      response.setHeader("Cache-control", "max-age=36000");
      response.write(getState("etag"));
    }
    else if (etag)
    {
      response.setStatusLine(request.httpVersion, 412, "Precondition Failed");        
    }
  }
  else
  {
    var etag = request.queryString.match(/^etag=(.*)$/);
    if (etag)
      setState("etag", etag[1]);
      
    response.setStatusLine(request.httpVersion, 204, "No content");
  }
}
