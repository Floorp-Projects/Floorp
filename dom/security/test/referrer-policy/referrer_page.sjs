function handleRequest(request, response) {
  let params = new URLSearchParams(request.queryString);
  let referrerPolicyHeader = params.get("header") || "";
  let metaReferrerPolicy = params.get("meta") || "";
  let showReferrer = params.has("show");

  if (referrerPolicyHeader) {
    response.setHeader("Referrer-Policy", referrerPolicyHeader, false);
  }

  let metaString = "";
  let resultString = "";

  if (metaReferrerPolicy) {
    metaString = `<meta name="referrer" content="${metaReferrerPolicy}">`;
  }

  if (showReferrer) {
    if (request.hasHeader("Referer")) {
      resultString = `Referer Header: <a id="result">${request.getHeader(
        "Referer"
      )}</a>`;
    } else {
      resultString = `Referer Header: <a id="result"></a>`;
    }
  }

  response.write(
    `<!DOCTYPE HTML>
     <html>
     <head>
       ${metaString}
     </head>
     <body>
       ${resultString}
     </body>
     </html>`
  );
}
