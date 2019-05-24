const BODY = `
  <script>
  opener.postMessage("ok!", "*");
  close();
  </script>`;

function handleRequest(request, response) {
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.queryString.includes("unset")) {
    response.setHeader("Set-Cookie", "test=wow", true);
  }

  if (request.queryString.includes("none")) {
    response.setHeader("Set-Cookie", "test2=wow2; samesite=none", true);
  }

  if (request.queryString.includes("lax")) {
    response.setHeader("Set-Cookie", "test3=wow3; samesite=lax", true);
  }

  response.write(BODY);
}
