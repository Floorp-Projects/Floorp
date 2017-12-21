function run_test() {
  var Cu = Components.utils;

  var handler = {
      get: function(target, name){
          return name in target?
              target[name] :
              37;
      }
  };

  var p = new Proxy({}, handler);
  Assert.ok(Cu.isProxy(p));
  Assert.ok(!Cu.isProxy({}));
  Assert.ok(!Cu.isProxy(42));

  sb = new Cu.Sandbox(this,
                      { wantExportHelpers: true });

  Assert.ok(!Cu.isProxy(sb));

  sb.do_check_true = do_check_true;
  sb.do_check_false = do_check_false;
  sb.p = p;
  Cu.evalInSandbox('do_check_true(isProxy(p));' +
                   'do_check_false(isProxy({}));' +
                   'do_check_false(isProxy(42));',
                   sb);
}
