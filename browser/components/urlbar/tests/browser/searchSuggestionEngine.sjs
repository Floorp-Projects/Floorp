/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

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
  if (params.count) {
    // Add more suffixes.
    let serial = 0;
    while (suffixes.length < params.count) {
      suffixes.push(++serial);
    }
  }
  let data = [params.query, suffixes.map(s => params.query + s)];
  resp.setHeader("Content-Type", "application/json", false);
  resp.write(JSON.stringify(data));
}

function decode(str) {
  return decodeURIComponent(str.replace(/\+/g, encodeURIComponent(" ")));
}
