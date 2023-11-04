"use strict";

function handleRequest(request, response) {
  const params = new URLSearchParams(request.queryString);

  switch (params.get("test")) {
    case "cache":
      handleCacheTestRequest(request, response);
      break;

    case "user-agent":
      handleUserAgentTestRequest(request, response);
      break;

    case "injected-script":
      handleInjectedScriptTestRequest(request, response, params);
      break;
  }
}

function handleCacheTestRequest(request, response) {
  response.setHeader("Content-Type", "text/plain; charset=UTF-8", false);

  if (request.hasHeader("pragma") && request.hasHeader("cache-control")) {
    response.write(
      `${request.getHeader("pragma")}:${request.getHeader("cache-control")}`
    );
  } else {
    response.write("empty cache headers");
  }
}

function handleUserAgentTestRequest(request, response) {
  response.setHeader("Content-Type", "text/plain; charset=UTF-8", false);

  if (request.hasHeader("user-agent")) {
    response.write(request.getHeader("user-agent"));
  } else {
    response.write("no user agent header");
  }
}

function handleInjectedScriptTestRequest(request, response, params) {
  response.setHeader("Content-Type", "text/html; charset=UTF-8", false);

  const frames = parseInt(params.get("frames"), 10);
  let content = "";

  if (frames > 0) {
    // Output an iframe in seamless mode, so that there is an higher chance that in case
    // of test failures we get a screenshot where the nested iframes are all visible.
    content = `<iframe seamless src="?test=injected-script&frames=${
      frames - 1
    }"></iframe>`;
  } else {
    // Output an about:srcdoc frame to be sure that inspectedWindow.eval is able to
    // evaluate js code into it.
    const srcdoc = `
      <pre>injected script NOT executed</pre>
      <script>window.pageScriptExecutedFirst = true</script>
    `;
    content = `<iframe style="height: 30px;" srcdoc="${srcdoc}"></iframe>`;
  }

  if (params.get("stop") == "windowStop") {
    content = "<script>window.stop();</script>" + content;
  }

  response.write(`<!DOCTYPE html>
    <html>
      <head>
       <meta charset="utf-8">
       <style>
         iframe { width: 100%; height: ${frames * 150}px; }
       </style>
      </head>
      <body>
       <h1>IFRAME ${frames}</h1>
       <pre>injected script NOT executed</pre>
       <script>
         window.pageScriptExecutedFirst = true;
       </script>
       ${content}
      </body>
    </html>
  `);
}
