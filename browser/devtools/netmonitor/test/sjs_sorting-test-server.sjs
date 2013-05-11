/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { classes: Cc, interfaces: Ci } = Components;

function handleRequest(request, response) {
  response.processAsync();

  let params = request.queryString.split("&");
  let index = params.filter((s) => s.contains("index="))[0].split("=")[1];

  Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer).initWithCallback(() => {
    response.setStatusLine(request.httpVersion, index == 1 ? 101 : index * 100, "Meh");
    response.setHeader("Content-Type", "text/" + index, false);
    response.write(new Array(index * 10).join(index)); // + 0.01 KB
    response.finish();
  }, 50, Ci.nsITimer.TYPE_ONE_SHOT); // Make sure this request takes a few ms.
}
