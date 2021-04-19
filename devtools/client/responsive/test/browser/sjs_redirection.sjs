/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function handleRequest(request, response) {
  Components.utils.importGlobalProperties(["URLSearchParams"]);
  const query = new URLSearchParams(request.queryString);

  const requestUserAgent = request.getHeader("user-agent");
  const redirectRequestUserAgent = getState(
    "redirect-request-user-agent-header"
  );

  const shouldRedirect = query.has("redirect");
  if (shouldRedirect) {
    response.setStatusLine(request.httpVersion, 302, "Found");
    setState("redirect-request-user-agent-header", requestUserAgent);
    response.setHeader(
      "Location",
      `http://${request.host}${request.path}?redirected`
    );
  } else {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(`
      <script>
        globalThis.requestUserAgent = ${JSON.stringify(requestUserAgent)};
        globalThis.redirectRequestUserAgent = ${JSON.stringify(
          redirectRequestUserAgent
        )};
      </script>
      ${requestUserAgent}
    `);
  }
}
