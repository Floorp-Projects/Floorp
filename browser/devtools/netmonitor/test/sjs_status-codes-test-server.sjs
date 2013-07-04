/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { classes: Cc, interfaces: Ci } = Components;

function handleRequest(request, response) {
  response.processAsync();

  let params = request.queryString.split("&");
  let status = params.filter((s) => s.contains("sts="))[0].split("=")[1];

  Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer).initWithCallback(() => {
    switch (status) {
      case "100":
        response.setStatusLine(request.httpVersion, 101, "Switching Protocols");
        break;
      case "200":
        response.setStatusLine(request.httpVersion, 202, "Created");
        break;
      case "300":
        response.setStatusLine(request.httpVersion, 303, "See Other");
        break;
      case "400":
        response.setStatusLine(request.httpVersion, 404, "Not Found");
        break;
      case "500":
        response.setStatusLine(request.httpVersion, 501, "Not Implemented");
        break;
    }
    response.setHeader("Content-Type", "text/plain; charset=utf-8", false);
    response.write("Hello status code " + status + "!");
    response.finish();
  }, 10, Ci.nsITimer.TYPE_ONE_SHOT); // Make sure this request takes a few ms.
}
