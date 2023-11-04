/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Simple server which can handle several response types and states.
// Trimmed down from devtools/client/netmonitor/test/sjs_content-type-test-server.sjs
// Additional features can be ported if needed.
function handleRequest(request, response) {
  response.processAsync();

  const params = request.queryString.split("&");
  const format = (params.filter(s => s.includes("fmt="))[0] || "").split(
    "="
  )[1];
  const status =
    (params.filter(s => s.includes("sts="))[0] || "").split("=")[1] || 200;

  const cacheExpire = 60; // seconds

  function setCacheHeaders() {
    if (status != 304) {
      response.setHeader(
        "Cache-Control",
        "no-cache, no-store, must-revalidate"
      );
      response.setHeader("Pragma", "no-cache");
      response.setHeader("Expires", "0");
      return;
    }

    response.setHeader("Expires", Date(Date.now() + cacheExpire * 1000), false);
  }

  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

  timer.initWithCallback(
    // eslint-disable-next-line complexity
    () => {
      // to avoid garbage collection
      timer = null;
      switch (format) {
        case "txt": {
          response.setStatusLine(request.httpVersion, status, "DA DA DA");
          response.setHeader("Content-Type", "text/plain", false);
          setCacheHeaders();

          function convertToUtf8(str) {
            return String.fromCharCode(...new TextEncoder().encode(str));
          }

          // This script must be evaluated as UTF-8 for this to write out the
          // bytes of the string in UTF-8.  If it's evaluated as Latin-1, the
          // written bytes will be the result of UTF-8-encoding this string
          // *twice*.
          const data = "Братан, ты вообще качаешься?";
          const stringOfUtf8Bytes = convertToUtf8(data);
          response.write(stringOfUtf8Bytes);

          response.finish();
          break;
        }
        case "xml": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "text/xml; charset=utf-8", false);
          setCacheHeaders();
          response.write("<label value='greeting'>Hello XML!</label>");
          response.finish();
          break;
        }
        case "html": {
          const content = (
            params.filter(s => s.includes("res="))[0] || ""
          ).split("=")[1];
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "text/html; charset=utf-8", false);
          setCacheHeaders();
          response.write(content || "<p>Hello HTML!</p>");
          response.finish();
          break;
        }
        case "xhtml": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader(
            "Content-Type",
            "application/xhtml+xml; charset=utf-8",
            false
          );
          setCacheHeaders();
          response.write("<p>Hello XHTML!</p>");
          response.finish();
          break;
        }
        case "html-long": {
          const str = new Array(102400 /* 100 KB in bytes */).join(".");
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "text/html; charset=utf-8", false);
          setCacheHeaders();
          response.write("<p>" + str + "</p>");
          response.finish();
          break;
        }
        case "css": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "text/css; charset=utf-8", false);
          setCacheHeaders();
          response.write("body:pre { content: 'Hello CSS!' }");
          response.finish();
          break;
        }
        case "js": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader(
            "Content-Type",
            "application/javascript; charset=utf-8",
            false
          );
          setCacheHeaders();
          response.write("function() { return 'Hello JS!'; }");
          response.finish();
          break;
        }
        case "json": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader(
            "Content-Type",
            "application/json; charset=utf-8",
            false
          );
          setCacheHeaders();
          response.write('{ "greeting": "Hello JSON!" }');
          response.finish();
          break;
        }

        case "font": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "font/woff", false);
          setCacheHeaders();
          response.finish();
          break;
        }
        case "image": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "image/png", false);
          setCacheHeaders();
          response.finish();
          break;
        }
        case "application-ogg": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "application/ogg", false);
          setCacheHeaders();
          response.finish();
          break;
        }
        case "audio": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "audio/ogg", false);
          setCacheHeaders();
          response.finish();
          break;
        }
        case "video": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "video/webm", false);
          setCacheHeaders();
          response.finish();
          break;
        }
        case "ws": {
          response.setStatusLine(
            request.httpVersion,
            101,
            "Switching Protocols"
          );
          response.setHeader("Connection", "upgrade", false);
          response.setHeader("Upgrade", "websocket", false);
          setCacheHeaders();
          response.finish();
          break;
        }
        default: {
          response.setStatusLine(request.httpVersion, 404, "Not Found");
          response.setHeader("Content-Type", "text/html; charset=utf-8", false);
          setCacheHeaders();
          response.write("<blink>Not Found</blink>");
          response.finish();
          break;
        }
      }
    },
    10,
    Ci.nsITimer.TYPE_ONE_SHOT
  ); // Make sure this request takes a few ms.
}
