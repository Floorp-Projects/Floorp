const Cu = Components.utils;

function checkThrows(fn) {
  try {
    fn();
    ok(false, "Should have thrown");
  } catch (e) {
    do_check_true(/denied|insecure/.test(e));
  }
}

function run_test() {
  var xosb = new Cu.Sandbox('http://www.example.org');
  var sb = new Cu.Sandbox('http://www.example.com');
  sb.do_check_true = do_check_true;
  sb.fun = function() { ok(false, "Shouldn't ever reach me"); };
  sb.cow = { foopy: 2, __exposedProps__: { foopy: 'rw' } };
  sb.payload = Cu.evalInSandbox('new Object()', xosb);
  Cu.evalInSandbox(checkThrows.toSource(), sb);
  Cu.evalInSandbox('checkThrows(fun.bind(null, payload));', sb);
  Cu.evalInSandbox('checkThrows(fun.bind(payload));', sb);
  Cu.evalInSandbox('checkThrows(function() { new fun(payload); });', sb);
  Cu.evalInSandbox('checkThrows(function() { cow.foopy = payload; });', sb);
  Cu.evalInSandbox('checkThrows(function() { Object.defineProperty(cow, "foopy", { value: payload }); });', sb);
}
