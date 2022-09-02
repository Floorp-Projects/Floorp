/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var _tests = [];
function addTest(test) {
  _tests.push(test);
}

function addAsyncTest(fn) {
  _tests.push(() => fn().catch(ok.bind(null, false)));
}

function runNextTest() {
  if (!_tests.length) {
    SimpleTest.finish();
    return;
  }
  const fn = _tests.shift();
  try {
    fn();
  } catch (ex) {
    info(
      "Test function " +
        (fn.name ? "'" + fn.name + "' " : "") +
        "threw an exception: " +
        ex
    );
  }
}

/**
 * Helper to perform an XHR then blob response to create worker
 */
function doXHRGetBlob(uri) {
  return new Promise(resolve => {
    const xhr = new XMLHttpRequest();
    xhr.open("GET", uri);
    xhr.responseType = "blob";
    xhr.addEventListener("load", function() {
      is(
        xhr.status,
        200,
        "doXHRGetBlob load uri='" + uri + "' status=" + xhr.status
      );
      resolve(xhr.response);
    });
    xhr.send();
  });
}

function removeObserver(observer) {
  SpecialPowers.removeObserver(observer, "specialpowers-http-notify-request");
  SpecialPowers.removeObserver(observer, "csp-on-violate-policy");
}

/**
 * Helper to perform an assert to check if the request should be blocked or
 * allowed by CSP
 */
function assertCSPBlock(url, shouldBlock) {
  return new Promise((resolve, reject) => {
    let observer = {
      observe(subject, topic, data) {
        if (topic === "specialpowers-http-notify-request") {
          if (data == url) {
            is(shouldBlock, false, "Should allow request uri='" + url);
            removeObserver(observer);
            resolve();
          }
        }

        if (topic === "csp-on-violate-policy") {
          let asciiSpec = SpecialPowers.getPrivilegedProps(
            SpecialPowers.do_QueryInterface(subject, "nsIURI"),
            "asciiSpec"
          );
          if (asciiSpec == url) {
            is(shouldBlock, true, "Should block request uri='" + url);
            removeObserver(observer);
            resolve();
          }
        }
      },
    };

    SpecialPowers.addObserver(observer, "csp-on-violate-policy");
    SpecialPowers.addObserver(observer, "specialpowers-http-notify-request");
  });
}
