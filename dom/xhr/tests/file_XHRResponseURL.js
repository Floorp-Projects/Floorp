"use strict";

// utility functions for worker/window communication

function isInWorker() {
  try {
    return !(self instanceof Window);
  } catch (e) {
    return true;
  }
}

function message(aData) {
  if (isInWorker()) {
    self.postMessage(aData);
  } else {
    self.postMessage(aData, "*");
  }
}
message.ping = 0;
message.pong = 0;

function is(aActual, aExpected, aMessage) {
  var obj = {
    type: "is",
    actual: aActual,
    expected: aExpected,
    message: aMessage
  };
  ++message.ping;
  message(obj);
}

function ok(aBool, aMessage) {
  var obj = {
    type: "ok",
    bool: aBool,
    message: aMessage
  };
  ++message.ping;
  message(obj);
}

function info(aMessage) {
  var obj = {
    type: "info",
    message: aMessage
  };
  ++message.ping;
  message(obj);
}

function request(aURL) {
  return new Promise(function (aResolve, aReject) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", aURL);
    xhr.addEventListener("load", function () {
      xhr.succeeded = true;
      aResolve(xhr);
    });
    xhr.addEventListener("error", function () {
      xhr.succeeded = false;
      aResolve(xhr);
    });
    xhr.send();
  });
}

function createSequentialRequest(aParameters, aTest) {
  var sequence = aParameters.reduce(function (aPromise, aParam) {
    return aPromise.then(function () {
      return request(aParam.requestURL);
    }).then(function (aXHR) {
      return aTest(aXHR, aParam);
    });
  }, Promise.resolve());

  return sequence;
}

function testSuccessResponse() {
  var blob = new Blob(["data"], {type: "text/plain"});
  var blobURL = URL.createObjectURL(blob);

  var parameters = [
    // tests that start with same-origin request
    {
      message: "request to same-origin without redirect",
      requestURL: "http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.text",
      responseURL: "http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.text"
    },
    {
      message: "request to same-origin redirect to same-origin URL",
      requestURL: "http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.text",
      responseURL: "http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.text"
    },
    {
      message: "request to same-origin redirects several times and finally go to same-origin URL",
      requestURL: "http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=" + encodeURIComponent("http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.text"),
      responseURL: "http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.text"
    },
    {
      message: "request to same-origin redirect to cross-origin URL",
      requestURL: "http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.text",
      responseURL: "http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.text",
    },
    {
      message: "request to same-origin redirects several times and finally go to cross-origin URL",
      requestURL: "http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=" + encodeURIComponent("http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.text"),
      responseURL: "http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.text",
    },

    // tests that start with cross-origin request
    {
      message: "request to cross-origin without redirect",
      requestURL: "http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.text",
      responseURL: "http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.text"
    },
    {
      message: "request to cross-origin redirect back to same-origin URL",
      requestURL: "http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.text",
      responseURL: "http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.text"
    },
    {
      message: "request to cross-origin redirect to the same cross-origin URL",
      requestURL: "http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.text",
      responseURL: "http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.text",
    },
    {
      message: "request to cross-origin redirect to another cross-origin URL",
      requestURL: "http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=http://example.org/tests/dom/xhr/tests/file_XHRResponseURL.text",
      responseURL: "http://example.org/tests/dom/xhr/tests/file_XHRResponseURL.text",
    },
    {
      message: "request to cross-origin redirects several times and finally go to same-origin URL",
      requestURL: "http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=" + encodeURIComponent("http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.text"),
      responseURL: "http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.text",
    },
    {
      message: "request to cross-origin redirects several times and finally go to the same cross-origin URL",
      requestURL: "http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=" + encodeURIComponent("http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.text"),
      responseURL: "http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.text",
    },
    {
      message: "request to cross-origin redirects several times and finally go to another cross-origin URL",
      requestURL: "http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=" + encodeURIComponent("http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=http://example.org/tests/dom/xhr/tests/file_XHRResponseURL.text"),
      responseURL: "http://example.org/tests/dom/xhr/tests/file_XHRResponseURL.text",
    },
    {
      message: "request to cross-origin redirects to another cross-origin and finally go to the other cross-origin URL",
      requestURL: "http://example.com/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=" + encodeURIComponent("http://example.org/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=http://test1.example.com/tests/dom/xhr/tests/file_XHRResponseURL.text"),
      responseURL: "http://test1.example.com/tests/dom/xhr/tests/file_XHRResponseURL.text",
    },
    {
      message: "request URL has fragment",
      requestURL: "http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.text#fragment",
      responseURL: "http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.text"
    },

    // tests for non-http(s) URL
    {
      message: "request to data: URL",
      requestURL: "data:text/plain,data",
      responseURL: "data:text/plain,data"
    },
    {
      message: "request to blob: URL",
      requestURL: blobURL,
      responseURL: blobURL
    }
  ];

  var sequence = createSequentialRequest(parameters, function (aXHR, aParam) {
    ok(aXHR.succeeded, "assert request succeeded");
    is(aXHR.responseURL, aParam.responseURL, aParam.message);
  });

  sequence.then(function () {
    URL.revokeObjectURL(blobURL);
  });

  return sequence;
}

function testFailedResponse() {
  info("test not to leak responseURL for denied cross-origin request");
  var parameters = [
    {
      message: "should be empty for denied cross-origin request without redirect",
      requestURL: "http://example.com/tests/dom/xhr/tests/file_XHRResponseURL_nocors.text"
    },
    {
      message: "should be empty for denied cross-origin request with redirect",
      requestURL: "http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=http://example.com/tests/dom/xhr/tests/file_XHRResponseURL_nocors.text"
    }
  ];

  var sequence = createSequentialRequest(parameters, function (aXHR, aParam) {
    ok(!aXHR.succeeded, "assert request failed");
    is(aXHR.responseURL, "" , aParam.message);
  });

  return sequence;
}

function testNotToLeakResponseURLWhileDoingRedirects() {
  info("test not to leak responeseURL while doing redirects");

  if (isInWorker()) {
    return testNotToLeakResponseURLWhileDoingRedirectsInWorker();
  } else {
    return testNotToLeakResponseURLWhileDoingRedirectsInWindow();
  }
}

function testNotToLeakResponseURLWhileDoingRedirectsInWindow() {
  var xhr = new XMLHttpRequest();
  var requestObserver = {
    observe: function (aSubject, aTopic, aData) {
      is(xhr.readyState, XMLHttpRequest.OPENED, "assert for XHR state");
      is(xhr.responseURL, "",
         "responseURL should return empty string before HEADERS_RECEIVED");
    }
  };
  SpecialPowers.addObserver(requestObserver, "specialpowers-http-notify-request");

  return new Promise(function (aResolve, aReject) {
    xhr.open("GET", "http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.text");
    xhr.addEventListener("load", function () {
      SpecialPowers.removeObserver(requestObserver, "specialpowers-http-notify-request");
      aResolve();
    });
    xhr.addEventListener("error", function () {
      ok(false, "unexpected request falilure");
      SpecialPowers.removeObserver(requestObserver, "specialpowers-http-notify-request");
      aResolve();
    });
    xhr.send();
  });
}

function testNotToLeakResponseURLWhileDoingRedirectsInWorker() {
  var xhr = new XMLHttpRequest();
  var testRedirect = function (e) {
    if (e.data === "request" && xhr.readyState === XMLHttpRequest.OPENED) {
      is(xhr.responseURL, "",
         "responseURL should return empty string before HEADERS_RECEIVED");
    }
  };

  return new Promise(function (aResolve, aReject) {
    self.addEventListener("message", testRedirect);
    message({type: "redirect_test", status: "start"});
    xhr.open("GET", "http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.sjs?url=http://mochi.test:8888/tests/dom/xhr/tests/file_XHRResponseURL.text");
    xhr.addEventListener("load", function () {
      self.removeEventListener("message", testRedirect);
      message({type: "redirect_test", status: "end"});
      aResolve();
    });
    xhr.addEventListener("error", function (e) {
      ok(false, "unexpected request falilure");
      self.removeEventListener("message", testRedirect);
      message({type: "redirect_test", status: "end"});
      aResolve();
    });
    xhr.send();
  });
}

function waitForAllMessagesProcessed() {
  return new Promise(function (aResolve, aReject) {
    var id = setInterval(function () {
      if (message.ping === message.pong) {
        clearInterval(id);
        aResolve();
      }
    }, 100);
  });
}


self.addEventListener("message", function (aEvent) {
  if (aEvent.data === "start") {
    ok("responseURL" in (new XMLHttpRequest()),
       "XMLHttpRequest should have responseURL attribute");
    is((new XMLHttpRequest()).responseURL, "",
       "responseURL should be empty string if response's url is null");

    var promise = testSuccessResponse();
    promise.then(function () {
      return testFailedResponse();
    }).then(function () {
      return testNotToLeakResponseURLWhileDoingRedirects();
    }).then(function () {
      return waitForAllMessagesProcessed();
    }).then(function () {
      message("done");
    });
  }
  if (aEvent.data === "pong") {
    ++message.pong;
  }
});
