/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

addEventListener("message", function (event) {
  let data;
  try {
    data = JSON.parse(event.data);
  } catch {}

  switch (data.type) {
    case "request-sourceMapURL":
      const dbg = new Debugger(global);
      const sourceMapURLs = dbg
        .findSources()
        .filter(source => source.url === data.url)
        .map(source => source.sourceMapURL);

      postMessage(
        JSON.stringify({
          type: "response-sourceMapURL",
          value: sourceMapURLs,
        })
      );
      break;
  }
});
