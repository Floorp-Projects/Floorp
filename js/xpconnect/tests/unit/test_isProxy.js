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

  sb.ok = ok;
  sb.p = p;
  Cu.evalInSandbox('ok(isProxy(p));' +
                   'ok(!isProxy({}));' +
                   'ok(!isProxy(42));',
                   sb);
}
