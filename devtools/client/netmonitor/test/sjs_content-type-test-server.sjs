/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function gzipCompressString(string, obs) {
  const scs = Cc["@mozilla.org/streamConverters;1"].getService(
    Ci.nsIStreamConverterService
  );
  const listener = Cc["@mozilla.org/network/stream-loader;1"].createInstance(
    Ci.nsIStreamLoader
  );
  listener.init(obs);
  const converter = scs.asyncConvertData(
    "uncompressed",
    "gzip",
    listener,
    null
  );
  const stringStream = Cc[
    "@mozilla.org/io/string-input-stream;1"
  ].createInstance(Ci.nsIStringInputStream);
  stringStream.data = string;
  converter.onStartRequest(null);
  converter.onDataAvailable(null, stringStream, 0, string.length);
  converter.onStopRequest(null, null);
}

function doubleGzipCompressString(string, observer) {
  const observer2 = {
    onStreamComplete(loader, context, status, length, result) {
      const buffer = String.fromCharCode.apply(this, result);
      gzipCompressString(buffer, observer);
    },
  };
  gzipCompressString(string, observer2);
}

function handleRequest(request, response) {
  response.processAsync();

  const params = request.queryString.split("&");
  const format = (params.filter(s => s.includes("fmt="))[0] || "").split(
    "="
  )[1];
  const status =
    (params.filter(s => s.includes("sts="))[0] || "").split("=")[1] || 200;
  const cookies =
    (params.filter(s => s.includes("cookies="))[0] || "").split("=")[1] || 0;
  const cors = request.queryString.includes("cors=1");

  let cachedCount = 0;
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
    // Spice things up a little!
    if (cachedCount % 2) {
      response.setHeader("Cache-Control", "max-age=" + cacheExpire, false);
    } else {
      response.setHeader(
        "Expires",
        Date(Date.now() + cacheExpire * 1000),
        false
      );
    }
    cachedCount++;
  }

  function setCookieHeaders() {
    if (cookies) {
      response.setHeader(
        "Set-Cookie",
        "name1=value1; Domain=.foo.example.com",
        true
      );
      response.setHeader(
        "Set-Cookie",
        "name2=value2; Domain=.example.com",
        true
      );
    }
  }

  function setAllowOriginHeaders() {
    if (cors) {
      response.setHeader("Access-Control-Allow-Origin", "*", false);
    }
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
          setAllowOriginHeaders();
          setCacheHeaders();
          setCookieHeaders();
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
          setAllowOriginHeaders();
          setCacheHeaders();
          setCookieHeaders();
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
        case "jsonp": {
          const fun = (params.filter(s => s.includes("jsonp="))[0] || "").split(
            "="
          )[1];
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "text/json; charset=utf-8", false);
          setCacheHeaders();
          response.write(fun + '({ "greeting": "Hello JSONP!" })');
          response.finish();
          break;
        }
        case "jsonp2": {
          const fun = (params.filter(s => s.includes("jsonp="))[0] || "").split(
            "="
          )[1];
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "text/json; charset=utf-8", false);
          setCacheHeaders();
          response.write(
            " " + fun + ' ( { "greeting": "Hello weird JSONP!" } ) ; '
          );
          response.finish();
          break;
        }
        case "json-b64": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "text/json; charset=utf-8", false);
          setCacheHeaders();
          response.write(btoa('{ "greeting": "This is a base 64 string." }'));
          response.finish();
          break;
        }
        case "json-long": {
          const str = '{ "greeting": "Hello long string JSON!" },';
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "text/json; charset=utf-8", false);
          setCacheHeaders();
          response.write("[" + new Array(2048).join(str).slice(0, -1) + "]");
          response.finish();
          break;
        }
        case "json-malformed": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "text/json; charset=utf-8", false);
          setCacheHeaders();
          response.write('{ "greeting": "Hello malformed JSON!" },');
          response.finish();
          break;
        }
        case "json-text-mime": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader(
            "Content-Type",
            "text/plain; charset=utf-8",
            false
          );
          setCacheHeaders();
          response.write('{ "greeting": "Hello third-party JSON!" }');
          response.finish();
          break;
        }
        case "json-custom-mime": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader(
            "Content-Type",
            "text/x-bigcorp-json; charset=utf-8",
            false
          );
          setCacheHeaders();
          response.write('{ "greeting": "Hello oddly-named JSON!" }');
          response.finish();
          break;
        }
        case "json-valid-xssi-protection": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "text/json; charset=utf-8", false);
          setCacheHeaders();
          response.write(')]}\'\n{"greeting": "Hello good XSSI protection"}');
          response.finish();
          break;
        }

        case "font": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "font/woff", false);
          setAllowOriginHeaders();
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
        case "flash": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader(
            "Content-Type",
            "application/x-shockwave-flash",
            false
          );
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
        case "gzip": {
          // Note: we're doing a double gzip encoding to test multiple
          // converters in network monitor.
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "text/plain", false);
          response.setHeader("Content-Encoding", "gzip\t ,gzip", false);
          setCacheHeaders();

          const observer = {
            onStreamComplete(loader, context, statusl, length, result) {
              const buffer = String.fromCharCode.apply(this, result);
              response.setHeader("Content-Length", "" + buffer.length, false);
              response.write(buffer);
              response.finish();
            },
          };
          const data = new Array(1000).join("Hello gzip!");
          doubleGzipCompressString(data, observer);
          break;
        }
        case "br": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "text/json", false);
          response.setHeader("Content-Encoding", "br", false);
          setCacheHeaders();
          response.setHeader("Content-Length", "10", false);
          // Use static data since we cannot encode brotli.
          response.write("\x1b\x3f\x00\x00\x24\xb0\xe2\x99\x80\x12");
          response.finish();
          break;
        }
        case "hls-m3u8": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "application/x-mpegurl", false);
          setCacheHeaders();
          response.write("#EXTM3U\n");
          response.finish();
          break;
        }
        case "hls-m3u8-alt-mime-type": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader(
            "Content-Type",
            "application/vnd.apple.mpegurl",
            false
          );
          setCacheHeaders();
          response.write("#EXTM3U\n");
          response.finish();
          break;
        }
        case "mpeg-dash": {
          response.setStatusLine(request.httpVersion, status, "OK");
          response.setHeader("Content-Type", "video/vnd.mpeg.dash.mpd", false);
          setCacheHeaders();
          response.write('<?xml version="1.0" encoding="UTF-8"?>\n');
          response.write(
            '<MPD xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"></MPD>\n'
          );
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
