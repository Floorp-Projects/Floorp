// SJS file for test_bug386386.html
"use strict";

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "application/vnd.mozilla.xul+xml;charset=utf-8", false);
  response.write("%3C%3Fxml%20version%3D%221.0%22%3F%3E%0A%3Cwindow%3E%3C/window%3E");
}
