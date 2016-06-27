function ok(what, msg) {
  postMessage({ event: msg, test: 'ok', a: what });
}

function is(a, b, msg) {
  postMessage({ event: msg, test: 'is', a: a, b: b });
}

// This is a copy of dom/xhr/tests/test_XHR_parameters.js
var validParameters = [
  undefined,
  null,
  {},
  {mozSystem: ""},
  {mozSystem: 0},
  {mozAnon: 1},
  {mozAnon: []},
  {get mozAnon() { return true; }},
  0,
  7,
  Math.PI,
  "string",
  true,
  false,
];

var invalidParameters = [
  {get mozSystem() { throw "Bla"; } },
];


function testParameters(havePrivileges) {

  function testValidParameter(value) {
    var xhr;
    try {
      xhr = new XMLHttpRequest(value);
    } catch (ex) {
      ok(false, "Got unexpected exception: " + ex);
      return;
    }
    ok(!!xhr, "passed " + JSON.stringify(value));

    // If the page doesnt have privileges to create a system or anon XHR,
    // these flags will always be false no matter what is passed.
    var expectedAnon = false;
    var expectedSystem = false;
    if (havePrivileges) {
      expectedAnon = Boolean(value && value.mozAnon);
      expectedSystem = Boolean(value && value.mozSystem);
    }
    is(xhr.mozAnon, expectedAnon, "testing mozAnon");
    is(xhr.mozSystem, expectedSystem, "testing mozSystem");
  }


  function testInvalidParameter(value) {
    try {
      new XMLHttpRequest(value);
      ok(false, "invalid parameter did not cause exception: " +
         JSON.stringify(value));
    } catch (ex) {
      ok(true, "invalid parameter raised exception as expected: " +
       JSON.stringify(ex));
    }
  }

  validParameters.forEach(testValidParameter);
  invalidParameters.forEach(testInvalidParameter);
}

self.onmessage = function onmessage(event) {
  testParameters(event.data);
  postMessage({test: "finish"});
};
