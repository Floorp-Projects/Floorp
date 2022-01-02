"use strict";

function handleRequest(request, response) {
  try {
    reallyHandleRequest(request, response);
  } catch (e) {
    response.setStatusLine("1.0", 200, "AlmostOK");
    response.write("Error handling request: " + e);
  }
}

function reallyHandleRequest(request, response) {
  let match;
  let requestAuth = true,
    requestProxyAuth = true;

  // Allow the caller to drive how authentication is processed via the query.
  // Eg, http://localhost:8888/authenticate.sjs?user=foo&realm=bar
  // The extra ? allows the user/pass/realm checks to succeed if the name is
  // at the beginning of the query string.
  let query = "?" + request.queryString;

  let expected_user = "",
    expected_pass = "",
    realm = "mochitest";
  let proxy_expected_user = "",
    proxy_expected_pass = "",
    proxy_realm = "mochi-proxy";
  let huge = false,
    plugin = false,
    anonymous = false;
  let authHeaderCount = 1;
  // user=xxx
  match = /[^_]user=([^&]*)/.exec(query);
  if (match) {
    expected_user = match[1];
  }

  // pass=xxx
  match = /[^_]pass=([^&]*)/.exec(query);
  if (match) {
    expected_pass = match[1];
  }

  // realm=xxx
  match = /[^_]realm=([^&]*)/.exec(query);
  if (match) {
    realm = match[1];
  }

  // proxy_user=xxx
  match = /proxy_user=([^&]*)/.exec(query);
  if (match) {
    proxy_expected_user = match[1];
  }

  // proxy_pass=xxx
  match = /proxy_pass=([^&]*)/.exec(query);
  if (match) {
    proxy_expected_pass = match[1];
  }

  // proxy_realm=xxx
  match = /proxy_realm=([^&]*)/.exec(query);
  if (match) {
    proxy_realm = match[1];
  }

  // huge=1
  match = /huge=1/.exec(query);
  if (match) {
    huge = true;
  }

  // plugin=1
  match = /plugin=1/.exec(query);
  if (match) {
    plugin = true;
  }

  // multiple=1
  match = /multiple=([^&]*)/.exec(query);
  if (match) {
    authHeaderCount = match[1] + 0;
  }

  // anonymous=1
  match = /anonymous=1/.exec(query);
  if (match) {
    anonymous = true;
  }

  // Look for an authentication header, if any, in the request.
  //
  // EG: Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==
  //
  // This test only supports Basic auth. The value sent by the client is
  // "username:password", obscured with base64 encoding.

  let actual_user = "",
    actual_pass = "",
    authHeader,
    authPresent = false;
  if (request.hasHeader("Authorization")) {
    authPresent = true;
    authHeader = request.getHeader("Authorization");
    match = /Basic (.+)/.exec(authHeader);
    if (match.length != 2) {
      throw "Couldn't parse auth header: " + authHeader;
    }

    let userpass = base64ToString(match[1]); // no atob() :-(
    match = /(.*):(.*)/.exec(userpass);
    if (match.length != 3) {
      throw "Couldn't decode auth header: " + userpass;
    }
    actual_user = match[1];
    actual_pass = match[2];
  }

  let proxy_actual_user = "",
    proxy_actual_pass = "";
  if (request.hasHeader("Proxy-Authorization")) {
    authHeader = request.getHeader("Proxy-Authorization");
    match = /Basic (.+)/.exec(authHeader);
    if (match.length != 2) {
      throw "Couldn't parse auth header: " + authHeader;
    }

    let userpass = base64ToString(match[1]); // no atob() :-(
    match = /(.*):(.*)/.exec(userpass);
    if (match.length != 3) {
      throw "Couldn't decode auth header: " + userpass;
    }
    proxy_actual_user = match[1];
    proxy_actual_pass = match[2];
  }

  // Don't request authentication if the credentials we got were what we
  // expected.
  if (expected_user == actual_user && expected_pass == actual_pass) {
    requestAuth = false;
  }
  if (
    proxy_expected_user == proxy_actual_user &&
    proxy_expected_pass == proxy_actual_pass
  ) {
    requestProxyAuth = false;
  }

  if (anonymous) {
    if (authPresent) {
      response.setStatusLine(
        "1.0",
        400,
        "Unexpected authorization header found"
      );
    } else {
      response.setStatusLine("1.0", 200, "Authorization header not found");
    }
  } else if (requestProxyAuth) {
    response.setStatusLine("1.0", 407, "Proxy authentication required");
    for (i = 0; i < authHeaderCount; ++i) {
      response.setHeader(
        "Proxy-Authenticate",
        'basic realm="' + proxy_realm + '"',
        true
      );
    }
  } else if (requestAuth) {
    response.setStatusLine("1.0", 401, "Authentication required");
    for (i = 0; i < authHeaderCount; ++i) {
      response.setHeader(
        "WWW-Authenticate",
        'basic realm="' + realm + '"',
        true
      );
    }
  } else {
    response.setStatusLine("1.0", 200, "OK");
  }

  response.setHeader("Content-Type", "application/xhtml+xml", false);
  response.write("<html xmlns='http://www.w3.org/1999/xhtml'>");
  response.write(
    "<p>Login: <span id='ok'>" +
      (requestAuth ? "FAIL" : "PASS") +
      "</span></p>\n"
  );
  response.write(
    "<p>Proxy: <span id='proxy'>" +
      (requestProxyAuth ? "FAIL" : "PASS") +
      "</span></p>\n"
  );
  response.write("<p>Auth: <span id='auth'>" + authHeader + "</span></p>\n");
  response.write("<p>User: <span id='user'>" + actual_user + "</span></p>\n");
  response.write("<p>Pass: <span id='pass'>" + actual_pass + "</span></p>\n");

  if (huge) {
    response.write("<div style='display: none'>");
    for (i = 0; i < 100000; i++) {
      response.write("123456789\n");
    }
    response.write("</div>");
    response.write(
      "<span id='footnote'>This is a footnote after the huge content fill</span>"
    );
  }

  if (plugin) {
    response.write(
      "<embed id='embedtest' style='width: 400px; height: 100px;' " +
        "type='application/x-test'></embed>\n"
    );
  }

  response.write("</html>");
}

// base64 decoder
//
// Yoinked from extensions/xml-rpc/src/nsXmlRpcClient.js because btoa()
// doesn't seem to exist. :-(
/* Convert Base64 data to a string */
/* eslint-disable prettier/prettier */
const toBinaryTable = [
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1, 0,-1,-1,
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
];
/* eslint-enable prettier/prettier */
const base64Pad = "=";

function base64ToString(data) {
  let result = "";
  let leftbits = 0; // number of bits decoded, but yet to be appended
  let leftdata = 0; // bits decoded, but yet to be appended

  // Convert one by one.
  for (let i = 0; i < data.length; i++) {
    let c = toBinaryTable[data.charCodeAt(i) & 0x7f];
    let padding = data[i] == base64Pad;
    // Skip illegal characters and whitespace
    if (c == -1) {
      continue;
    }

    // Collect data into leftdata, update bitcount
    leftdata = (leftdata << 6) | c;
    leftbits += 6;

    // If we have 8 or more bits, append 8 bits to the result
    if (leftbits >= 8) {
      leftbits -= 8;
      // Append if not padding.
      if (!padding) {
        result += String.fromCharCode((leftdata >> leftbits) & 0xff);
      }
      leftdata &= (1 << leftbits) - 1;
    }
  }

  // If there are any bits left, the base64 string was corrupted
  if (leftbits) {
    throw Components.Exception("Corrupted base64 string");
  }

  return result;
}
