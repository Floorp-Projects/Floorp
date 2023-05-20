const MESSAGE_PAGE = function (msg) {
  return `
<!DOCTYPE html>
<html>
  <body>
    <p id="msg">${msg}</p>
  <body>
</html>
`;
};

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-store");
  response.setHeader("Content-Type", "text/html");

  if (request.queryString.includes("setcookies")) {
    response.setHeader(
      "Set-Cookie",
      "auth_secure=foo; SameSite=None; HttpOnly; Secure",
      true
    );
    response.setHeader("Set-Cookie", "auth=foo; HttpOnly;", true);
    response.write(MESSAGE_PAGE(request.queryString));
    return;
  }

  const cookies = request.hasHeader("Cookie")
    ? request.getHeader("Cookie")
    : "";
  response.write(MESSAGE_PAGE(cookies));
}
