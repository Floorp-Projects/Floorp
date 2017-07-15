// SJS file for test_bug795418-2.html
"use strict";

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "application/xhtml+xml", false);
  response.write("<html contenteditable='' xmlns='http://www.w3.org/1999/xhtml'><span>AB</span></html>");
}
