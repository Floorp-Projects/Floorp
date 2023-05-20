// SJS file specifically for the needs of bug
// Bug 1416045/Bug 1223743 - CSP: Check baseChannel for CSP when loading multipart channel

var CSP = "script-src 'unsafe-inline', img-src 'none'";
var rootCSP = "script-src 'unsafe-inline'";
var part1CSP = "img-src *";
var part2CSP = "img-src 'none'";
var BOUNDARY = "fooboundary";

// small red image
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
    "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg=="
);

var RESPONSE = `
  <script>
  var myImg = new Image;
  myImg.src = "file_multipart_testserver.sjs?img";
  myImg.onerror = function(e) {
    window.parent.postMessage({"test": "rootCSP_test",
                               "msg": "img-blocked"}, "*");
  };
  myImg.onload = function() {
    window.parent.postMessage({"test": "rootCSP_test",
                               "msg": "img-loaded"}, "*");
  };
  document.body.appendChild(myImg);
  </script>
`;

var RESPONSE1 = `
  <body>
  <script>
  var triggerNextPartFrame = document.createElement('iframe');
  var myImg = new Image;
  myImg.src = "file_multipart_testserver.sjs?img";
  myImg.onerror = function(e) {
    window.parent.postMessage({"test": "part1CSP_test",
                               "msg": "part1-img-blocked"}, "*");
    triggerNextPartFrame.src = 'file_multipart_testserver.sjs?sendnextpart';
  };
  myImg.onload = function() {
    window.parent.postMessage({"test": "part1CSP_test",
                               "msg": "part1-img-loaded"}, "*");
    triggerNextPartFrame.src = 'file_multipart_testserver.sjs?sendnextpart';
  };
  document.body.appendChild(myImg);
  document.body.appendChild(triggerNextPartFrame);
  </script>
  </body>
`;

var RESPONSE2 = `
  <body>
  <script>
  var myImg = new Image;
  myImg.src = "file_multipart_testserver.sjs?img";
  myImg.onerror = function(e) {
    window.parent.postMessage({"test": "part2CSP_test",
                               "msg": "part2-img-blocked"}, "*");
  };
  myImg.onload = function() {
    window.parent.postMessage({"test": "part2CSP_test",
                               "msg": "part2-img-loaded"}, "*");
  };
  document.body.appendChild(myImg);
  </script>
  </body>
`;

function setGlobalState(data, key) {
  x = {
    data,
    QueryInterface(iid) {
      return this;
    },
  };
  x.wrappedJSObject = x;
  setObjectState(key, x);
}

function getGlobalState(key) {
  var data;
  getObjectState(key, function (x) {
    data = x && x.wrappedJSObject.data;
  });
  return data;
}

function handleRequest(request, response) {
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.queryString == "doc") {
    response.setHeader("Content-Security-Policy", CSP, false);
    response.setHeader(
      "Content-Type",
      "multipart/x-mixed-replace; boundary=" + BOUNDARY,
      false
    );
    response.write(BOUNDARY + "\r\n");
    response.write(RESPONSE);
    response.write(BOUNDARY + "\r\n");
    return;
  }

  if (request.queryString == "partcspdoc") {
    response.setHeader("Content-Security-Policy", rootCSP, false);
    response.setHeader(
      "Content-Type",
      "multipart/x-mixed-replace; boundary=" + BOUNDARY,
      false
    );
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.processAsync();
    response.write("--" + BOUNDARY + "\r\n");
    sendNextPart(response, 1);
    return;
  }

  if (request.queryString == "sendnextpart") {
    response.setStatusLine(request.httpVersion, 204, "No content");
    var blockedResponse = getGlobalState("root-document-response");
    if (typeof blockedResponse == "object") {
      sendNextPart(blockedResponse, 2);
      sendClose(blockedResponse);
    } else {
      dump("Couldn't find the stored response object.");
    }
    return;
  }

  if (request.queryString == "img") {
    response.setHeader("Content-Type", "image/png");
    response.write(IMG_BYTES);
    return;
  }

  // we should never get here - return something unexpected
  response.write("d'oh");
}

function sendClose(response) {
  response.write("--" + BOUNDARY + "--\r\n");
  response.finish();
}

function sendNextPart(response, partNumber) {
  response.write("Content-type: text/html" + "\r\n");
  if (partNumber == 1) {
    response.write("Content-Security-Policy:" + part1CSP + "\r\n");
    response.write(RESPONSE1);
    setGlobalState(response, "root-document-response");
  } else {
    response.write("Content-Security-Policy:" + part2CSP + "\r\n");
    response.write(RESPONSE2);
  }
  response.write("--" + BOUNDARY + "\r\n");
}
