/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(req, resp) {
  resp.setHeader("Cache-Control", "no-cache", false);
  resp.setHeader("Content-Type", "text/plain", false);

  let stateKey = "allowMediaState";
  let state = getState(stateKey);
  setState(stateKey, req.queryString ? "FAIL" : "");
  resp.write(state || "PASS");
}
