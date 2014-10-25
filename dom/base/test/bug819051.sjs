function handleRequest(request, response)
{
    response.setStatusLine(request.httpVersion, 200, "Ok");
    response.setHeader("X-appended-result", request.getHeader("X-appended-to-this"));
    response.setHeader("X-Accept-Result", request.getHeader("Accept"));
    response.write("");
}
