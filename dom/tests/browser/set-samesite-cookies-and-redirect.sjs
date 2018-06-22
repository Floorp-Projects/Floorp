/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function handleRequest(request, response) {
  // Set cookies and redirect for .org:
  if (request.host.endsWith(".org")) {
    response.setHeader("Set-Cookie", "normalCookie=true; path=/;", true);
    response.setHeader("Set-Cookie", "laxHeader=true; path=/; SameSite=Lax", true);
    response.setHeader("Set-Cookie", "strictHeader=true; path=/; SameSite=Strict", true);
    response.write(`
      <head>
        <meta http-equiv='set-cookie' content='laxMeta=true; path=/; SameSite=Lax'>
        <meta http-equiv='set-cookie' content='strictMeta=true; path=/; SameSite=Strict'>
      </head>
      <body>
        <script>
        document.cookie = 'laxScript=true; path=/; SameSite=Lax';
        document.cookie = 'strictScript=true; path=/; SameSite=Strict';
        location.href = location.href.replace(/\.org/, ".com");
        </script>
      </body>`);
  } else {
    let baseURI = "https://example.org/" + request.path.replace(/[a-z-]*\.sjs/, "mimeme.sjs?type=");
    response.write(`
      <link rel="stylesheet" type="text/css" href="${baseURI}css">
      <iframe src="${baseURI}html"></iframe>
      <script src="${baseURI}js"></script>
      <img src="${baseURI}png">
    `);
  }
}
