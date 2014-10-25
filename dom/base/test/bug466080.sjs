
function handleRequest(request, response)
{
    var body = "loaded";
    var origin = "localhost";
    try {
        var origin = request.getHeader("Origin");
     } catch(e) {}
    
    response.setHeader("Access-Control-Allow-Origin",
                        origin,
                        false);
    response.setHeader("Access-Control-Allow-Credentials", "true", false);
    response.setHeader("Connection", "Keep-alive", false);

    response.bodyOutputStream.write(body, body.length);
}
