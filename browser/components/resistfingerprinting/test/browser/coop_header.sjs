const HTML_DATA = `
  <!DOCTYPE HTML>
  <html>
  <head>
  <title>Dummy test page</title>
  <meta http-equiv="Content-Type" content="text/html;charset=utf-8"></meta>
  </head>
  <body>
  <p>Dummy test page</p>
  <div id="testDiv">test</div>
  </body>
  </html>
  `;

const WORKER = `
  self.onmessage = function(e) {
    if (e.data.type == "runCmdAndGetResult") {
      let result = eval(e.data.cmd);

      postMessage({ type: "result", resultOf: e.data.cmd, result: result });
      return;
    } else if (e.data.type == "runCmds") {
       for (let cmd of e.data.cmds) {
         eval(cmd);
       }

       postMessage({ type: "result", resultOf: "entriesLength", result: performance.getEntries().length });
       return;
    } else if (e.data.type == "getResult") {
      if (e.data.resultOf == "startTimeAndDuration") {
         postMessage({ type: "result",
                       resultOf: "startTimeAndDuration",
                       result: {
                         index: e.data.num,
                         startTime: performance.getEntries()[e.data.num].startTime,
                         duration: performance.getEntries()[e.data.num].duration,
                       }
         });
         return;
     } else if (e.data.resultOf == "getEntriesByTypeLength") {
        postMessage({
                      type: "result",
                      resultOf: "entriesByTypeLength",
                      result: {
                      markLength: performance.getEntriesByType("mark").length,
                      resourceLength: performance.getEntriesByType("resource").length,
                      testAndMarkLength: performance.getEntriesByName("Test", "mark").length,
                    }
        });
        return;
      }
    }

    postMessage({ type: "unexpectedMessageType", data: e.data.type });
  };
  `;

function handleRequest(request, response) {
  let query = new URLSearchParams(request.queryString);

  if (query.get("crossOriginIsolated") === "true") {
    response.setHeader("Cross-Origin-Opener-Policy", "same-origin", false);
    response.setHeader("Cross-Origin-Embedder-Policy", "require-corp", false);
  }

  if (query.get("worker")) {
    response.setHeader("Cache-Control", "no-cache", false);
    response.setHeader("Content-Type", "application/javascript");
    response.write(WORKER);
    return;
  }

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);
  response.write(HTML_DATA);
}
