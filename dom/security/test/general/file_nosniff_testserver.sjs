"use strict";
Components.utils.importGlobalProperties(["URLSearchParams"]);

const SCRIPT = "var foo = 24;";
const CSS = "body { background-color: green; }";

// small red image
const IMG = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
  "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==");

function handleRequest(request, response) {
  const query = new URLSearchParams(request.queryString);

  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  // set the nosniff header
  response.setHeader("X-Content-Type-Options", "  NoSniFF  , foo  ", false);

  if (query.has("cssCorrectType")) {
    response.setHeader("Content-Type", "teXt/cSs", false);
    response.write(CSS);
    return;
  }

  if (query.has("cssWrongType")) {
    response.setHeader("Content-Type", "text/html", false);
    response.write(CSS);
    return;
  }

  if (query.has("scriptCorrectType")) {
    response.setHeader("Content-Type", "appLIcation/jAvaScriPt;blah", false);
    response.write(SCRIPT);
    return;
  }

  if (query.has("scriptWrongType")) {
    response.setHeader("Content-Type", "text/html", false);
    response.write(SCRIPT);
    return;
  }

  if (query.has("imgCorrectType")) {
    response.setHeader("Content-Type", "iMaGe/pnG;blah", false);
    response.write(IMG);
    return;
  }

  if (query.has("imgtWrongType")) {
    response.setHeader("Content-Type", "text/html", false);
    response.write(IMG);
    return;
  }

  // we should never get here, but just in case
  response.setHeader("Content-Type", "text/html", false);
  response.write("do'h");
}
