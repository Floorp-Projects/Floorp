/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { classes: Cc, interfaces: Ci } = Components;

function handleRequest(request, response) {
  response.processAsync();

  let params = request.queryString.split("&");
  let status = params.filter(s => s.includes("sts="))[0].split("=")[1];
  let cached = params.filter(s => s === 'cached').length !== 0;

  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(() => {
    // to avoid garbage collection
    timer = null;
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
      case "ok":
        response.setStatusLine(request.httpVersion, 200, "OK");
        break;
      case "redirect":
        response.setStatusLine(request.httpVersion, 301, "Moved Permanently");
        response.setHeader("Location", "http://example.com/redirected");
        break;
    }

    if(!cached) {
      response.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      response.setHeader("Pragma", "no-cache");
      response.setHeader("Expires", "0");
    }
    else {
      response.setHeader("Cache-Control", "no-transform,public,max-age=300,s-maxage=900");
      response.setHeader("Expires", "Thu, 01 Dec 2100 20:00:00 GMT");
    }

    response.setHeader("Content-Type", "text/plain; charset=utf-8", false);
    response.write("Hello status code " + status + "!");
    response.finish();
  }, 10, Ci.nsITimer.TYPE_ONE_SHOT); // Make sure this request takes a few ms.
}
