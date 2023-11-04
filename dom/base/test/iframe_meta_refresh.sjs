/*
 * Test server for iframe refresh from meta http-equiv
 */

const SHARED_KEY = "iframe_meta_refresh";
const DEFAULT_STATE = { count: 0, referrers: [] };
const REFRESH_PAGE =
  "http://example.com/tests/dom/base/test/iframe_meta_refresh.sjs?action=test";

function createContent(refresh) {
  let metaRefresh = "";
  let scriptMessage = "";

  if (refresh) {
    metaRefresh = `<meta http-equiv="refresh" content="0;URL=${REFRESH_PAGE}">`;
  } else {
    scriptMessage = `
      <script>
          window.addEventListener("load", function() {
            parent.postMessage("childLoadComplete", "http://mochi.test:8888");
          }, false);
      </script>`;
  }

  return `<!DOCTYPE HTML>
         <html>
         <head>
         <meta charset="utf-8">
         ${metaRefresh}
         <title> Test referrer of meta http-equiv refresh</title>
         </head>
         <body>
           ${scriptMessage}
         </body>
         </html>`;
}

function handleRequest(request, response) {
  let query = new URLSearchParams(request.queryString);

  let action = query.get("action");

  var referrerLevel = "none";
  if (request.hasHeader("Referer")) {
    let referrer = request.getHeader("Referer");
    if (referrer.indexOf("test_meta_refresh_referrer") > 0) {
      referrerLevel = "full";
    } else if (referrer == "http://mochi.test:8888/") {
      referrerLevel = "origin";
    }
  }

  var state = getSharedState(SHARED_KEY);
  if (state === "") {
    state = DEFAULT_STATE;
  } else {
    state = JSON.parse(state);
  }

  response.setStatusLine(request.httpVersion, 200, "OK");

  //avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if (action === "results") {
    response.setHeader("Content-Type", "text/plain", false);
    response.write(JSON.stringify(state));
    return;
  }

  if (action === "reset") {
    //reset server state
    setSharedState(SHARED_KEY, JSON.stringify(DEFAULT_STATE));
    response.write("");
    return;
  }

  if (action === "test") {
    let load = query.get("load");
    state.count++;
    if (state.referrers.indexOf(referrerLevel) < 0) {
      state.referrers.push(referrerLevel);
    }

    // Write frame content
    response.write(createContent(load));
  }

  setSharedState(SHARED_KEY, JSON.stringify(state));
}
