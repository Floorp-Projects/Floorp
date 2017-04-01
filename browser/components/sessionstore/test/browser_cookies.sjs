/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Components.utils.importGlobalProperties(["URLSearchParams"]);

function handleRequest(req, resp) {
  resp.setStatusLine(req.httpVersion, 200);

  let params = new URLSearchParams(req.queryString);
  let value = params.get("value");

  let domain = "";
  if (params.has("domain")) {
    domain = `; Domain=${params.get("domain")}`;
  }

  let secure = "";
  if (params.has("secure")) {
    secure = "; Secure";
  }

  resp.setHeader("Set-Cookie", `foobar=${value}${domain}${secure}`);
  resp.write("<meta charset=utf-8>hi");
}
