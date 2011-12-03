function handleRequest(request, response)
{
  if ((request.queryString == "test=user1_xhr" &&
       request.hasHeader("Authorization") &&
       request.getHeader("Authorization") == "Basic dXNlciAxOnBhc3N3b3JkIDE=") ||
      (request.queryString == "test=user1_evtsrc" &&
       request.hasHeader("Authorization") &&
       request.getHeader("Authorization") == "Basic dXNlciAxOnBhc3N3b3JkIDE=" &&
       request.hasHeader("Cookie") &&
       request.getHeader("Cookie") == "test=5c")) {
    response.setStatusLine(null, 200, "OK");
    response.setHeader("Content-Type", "text/event-stream", false);
    response.setHeader("Access-Control-Allow-Origin", "http://mochi.test:8888", false);
    response.setHeader("Access-Control-Allow-Credentials", "true", false);
    response.setHeader("Cache-Control", "no-cache, must-revalidate", false);
    if (request.queryString == "test=user1_xhr") {
      response.setHeader("Set-Cookie", "test=5c", false);
    }
    response.write("event: message\ndata: 1\n\n");
  } else if ((request.queryString == "test=user2_xhr" &&
              request.hasHeader("Authorization") &&
              request.getHeader("Authorization") == "Basic dXNlciAyOnBhc3N3b3JkIDI=") ||
             (request.queryString == "test=user2_evtsrc" &&
              request.hasHeader("Authorization") &&
              request.getHeader("Authorization") == "Basic dXNlciAyOnBhc3N3b3JkIDI=" &&
              request.hasHeader("Cookie") &&
              request.getHeader("Cookie") == "test=5d")) {
    response.setStatusLine(null, 200, "OK");
    response.setHeader("Content-Type", "text/event-stream", false);
    response.setHeader("Access-Control-Allow-Origin", "http://mochi.test:8888", false);
    response.setHeader("Access-Control-Allow-Credentials", "true", false);
    response.setHeader("Cache-Control", "no-cache, must-revalidate", false);
    if (request.queryString == "test=user2_xhr") {
      response.setHeader("Set-Cookie", "test=5d", false);
    }
    response.write("event: message\ndata: 1\n\n");
  } else if (request.queryString == "test=user1_xhr" ||
             request.queryString == "test=user2_xhr") {
    response.setStatusLine(null, 401, "Unauthorized");
    response.setHeader("WWW-Authenticate", "basic realm=\"restricted\"", false);
    response.setHeader("Access-Control-Allow-Origin", "http://mochi.test:8888", false);
    response.setHeader("Access-Control-Allow-Credentials", "true", false);
    response.write("Unauthorized");
  } else {
    response.setStatusLine(null, 403, "Forbidden");
    response.setHeader("Access-Control-Allow-Origin", "http://mochi.test:8888", false);
    response.setHeader("Access-Control-Allow-Credentials", "true", false);
    response.write("Forbidden");
  }
}