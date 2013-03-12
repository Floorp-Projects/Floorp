/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { classes: Cc, interfaces: Ci } = Components;

function handleRequest(request, response) {
  response.processAsync();

  let params = request.queryString.split("&");
  let format = params.filter((s) => s.contains("fmt="))[0].split("=")[1];

  Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer).initWithCallback(() => {
    switch (format) {
      case "xml":
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.setHeader("Content-Type", "text/xml; charset=utf-8", false);
        response.write("<label value='greeting'>Hello XML!</label>");
        response.finish();
        break;
      case "css":
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.setHeader("Content-Type", "text/css; charset=utf-8", false);
        response.write("body:pre { content: 'Hello CSS!' }");
        response.finish();
        break;
      case "js":
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.setHeader("Content-Type", "application/javascript; charset=utf-8", false);
        response.write("function() { return 'Hello JS!'; }");
        response.finish();
        break;
      case "json":
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.setHeader("Content-Type", "application/json; charset=utf-8", false);
        response.write("{ \"greeting\": \"Hello JSON!\" }");
        response.finish();
        break;
      default:
        response.setStatusLine(request.httpVersion, 404, "Not Found");
        response.setHeader("Content-Type", "text/html; charset=utf-8", false);
        response.write("<blink>Not Found</blink>");
        response.finish();
        break;
    }
  }, 10, Ci.nsITimer.TYPE_ONE_SHOT); // Make sure this request takes a few ms.
}
