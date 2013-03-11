const Cu = Components.utils;
function run_test() {
  var sb = new Cu.Sandbox('http://www.example.com');

  sb.obj = { priv: 42, __exposedProps__ : {} };
  Object.defineProperty(sb.obj, 'readable', { get: function() { return this.priv; }, set: function(x) { this.priv = x; }} );
  sb.obj.__exposedProps__.readable = 'r';
  do_check_eq(Cu.evalInSandbox('Object.getOwnPropertyDescriptor(obj, "readable").get.call(obj)', sb), 42);
  sb.obj.readable = 32;
  do_check_eq(Cu.evalInSandbox('Object.getOwnPropertyDescriptor(obj, "readable").get.call(obj);', sb), 32);
  do_check_eq(Cu.evalInSandbox('Object.__lookupGetter__.call(obj, "readable").call(obj);', sb), 32);
  Object.getOwnPropertyDescriptor(sb.obj, "readable").set.call(sb.obj, 22);
  do_check_eq(sb.obj.readable, 22);
  Object.__lookupSetter__.call(sb.obj, "readable").call(sb.obj, 21);
  do_check_eq(sb.obj.readable, 21);
  checkThrows(sb, 'obj.readable = 11;');
  do_check_eq(Cu.evalInSandbox('Object.getOwnPropertyDescriptor(obj, "readable").set', sb), null);
  do_check_eq(Cu.evalInSandbox('Object.__lookupSetter__.call(obj, "readable")', sb), null);
  do_check_eq(sb.obj.readable, 21);
}

function checkThrows(sb, expression) {
  var result = Cu.evalInSandbox('(function() { try { ' + expression + ' return "success"; } catch (e) { return e.toString(); } })();', sb);
  dump("Result: " + result);
  do_check_true(!!/denied/.exec(result));
}
