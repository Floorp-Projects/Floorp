// SJS file for getAllResponseRequests vs getResponseRequest
function handleRequest(request, response) {
  // Header strings are interpreted by truncating the characters in them to
  // bytes, so U+2026 HORIZONTAL ELLIPSIS here must be encoded manually: using
  // "…" as the string would write a \x26 byte.
  response.setHeader("X-Custom-Header-Bytes", "\xE2\x80\xA6", false);
  response.write("42");
}
