let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://testing-common/httpd.js");

let server = new HttpServer();
server.start(-1);

let body = "<!DOCTYPE HTML><html><head><meta charset='utf-8'></head><body></body></html>";

function handler(request, response) {
  response.setStatusLine(request.httpVersion, 200, "Ok");
  response.setHeader("Content-Type", "text/html", false);

  if (!request.hasHeader("Cookie")) {
    response.setHeader("Set-Cookie", "test", false);
    ok(true);
  } else {
    ok(false);
  }

  response.bodyOutputStream.write(body, body.length);
}

function run_test() {
  do_test_pending();
  server.registerPathHandler("/foo", handler);

  let xhr = Cc['@mozilla.org/xmlextras/xmlhttprequest;1'].createInstance(Ci.nsIXMLHttpRequest);
  xhr.open("GET", "http://localhost:" + server.identity.primaryPort + "/foo", true);
  xhr.send(null);

  xhr.onload = function() {
    // We create another XHR to connect to the same site, but this time we
    // specify with different origin attributes, which will make the XHR use a
    // different cookie-jar than the previous one.
    let xhr2 = Cc['@mozilla.org/xmlextras/xmlhttprequest;1'].createInstance(Ci.nsIXMLHttpRequest);
    xhr2.open("GET", "http://localhost:" + server.identity.primaryPort + "/foo", true);
    xhr2.setOriginAttributes({userContextId: 1});
    xhr2.send(null);

    let loadInfo = xhr2.channel.loadInfo;
    Assert.equal(loadInfo.originAttributes.userContextId, 1);

    xhr2.onload = function() {
      server.stop(do_test_finished);
    }
  };
}
