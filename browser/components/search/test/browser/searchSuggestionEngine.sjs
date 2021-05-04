/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable-next-line mozilla/reject-importGlobalProperties */
Cu.importGlobalProperties(["TextEncoder"]);

let gTimer;

function handleRequest(req, resp) {
  // Parse the query params.  If the params aren't in the form "foo=bar", then
  // treat the entire query string as a search string.
  let params = req.queryString.split("&").reduce((memo, pair) => {
    let [key, val] = pair.split("=");
    if (!val) {
      // This part isn't in the form "foo=bar".  Treat it as the search string
      // (the "query").
      val = key;
      key = "query";
    }
    memo[decode(key)] = decode(val);
    return memo;
  }, {});

  let timeout = parseInt(params.timeout);
  if (timeout) {
    // Write the response after a timeout.
    resp.processAsync();
    gTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    gTimer.init(
      () => {
        writeResponse(params, resp);
        resp.finish();
      },
      timeout,
      Ci.nsITimer.TYPE_ONE_SHOT
    );
    return;
  }

  writeResponse(params, resp);
}

function writeResponse(params, resp) {
  // Echo back the search string with "foo" and "bar" appended.
  let suffixes = ["foo", "bar"];
  let data = [params.query, suffixes.map(s => params.query + s)];
  resp.setHeader("Content-Type", "application/json", false);

  let json = JSON.stringify(data);
  let utf8 = String.fromCharCode(...new TextEncoder().encode(json));
  resp.write(utf8);
}

function decode(str) {
  return decodeURIComponent(str.replace(/\+/g, encodeURIComponent(" ")));
}
