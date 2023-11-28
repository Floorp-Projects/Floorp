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

  writeResponse(params, resp);
}

function writeResponse(params, resp) {
  // Echoes back 15 results, query, query0, query1, query2 etc.
  let query = params.query || "";
  let suffixes = [...Array(15).keys()].map(s => query + s);
  // If we have a query, echo it back (to help test deduplication)
  if (query) {
    suffixes.unshift(query);
  }
  let data = [query, suffixes];

  if (params?.richsuggestions) {
    data.push([]);
    data.push({
      "google:suggestdetail": data[1].map(() => ({
        a: "Extended title",
        dc: "#FFFFFF",
        i: "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==",
        t: "Title",
      })),
    });
  }
  resp.setHeader("Content-Type", "application/json", false);

  let json = JSON.stringify(data);
  let utf8 = String.fromCharCode(...new TextEncoder().encode(json));
  resp.write(utf8);
}

function decode(str) {
  return decodeURIComponent(str.replace(/\+/g, encodeURIComponent(" ")));
}
