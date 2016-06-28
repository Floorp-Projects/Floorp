/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

function handleRequest(request, response) {
  switch (request.method) {
    case "POST":
      try {
        var optionsHost = request.getHeader("options-host");
      } catch(e) { }

      var headerFound = false;
      if (optionsHost) {
        setState("postHost", request.host);
        setState("optionsHost", optionsHost);
        headerFound = true;
      }

      try {
        var emptyHeader = "nada" + request.getHeader("empty");
      } catch(e) { }

      if (emptyHeader && emptyHeader == "nada") {
        setState("emptyHeader", "nada");
        headerFound = true;
      }
      if (headerFound) {
        return;
      } else {
        break;
      }

    case "OPTIONS":
      if (getState("optionsHost") == request.host) {
        try {
          var optionsHeader =
            request.getHeader("Access-Control-Request-Headers");
        } catch(e) { }
        setState("optionsHeader", "'" + optionsHeader + "'");
      }
      break;

    case "GET":
      response.setHeader("Cache-Control", "no-cache", false);
      response.setHeader("Content-Type", "text/plain", false);

      if (getState("postHost") == request.host &&
          getState("emptyHeader") == "nada") {
        var result = getState("optionsHeader");
        if (result) {
          response.write("Success: expected OPTIONS request with " + result +
                         " header");
        } else if (getState("badGet") == 1) {
          response.write("Error: unexpected GET request");
        }
      } else {
        setState("badGet", "1");
        response.write("Error: this response should never be seen");
      }
      return;
  }

  response.setStatusLine(request.httpVersion, 501, "Not Implemented");
}
