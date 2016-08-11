const Cu = Components.utils;
function run_test() {
  var sb = Cu.Sandbox('http://www.example.com', { wantGlobalProperties: ['XMLHttpRequest']});
  var xhr = Cu.evalInSandbox('new XMLHttpRequest()', sb);
  do_check_eq(xhr.toString(), '[object XMLHttpRequest]');
  do_check_eq((new sb.Object()).toString(), '[object Object]');
  do_check_eq(sb.Object.prototype.toString.call(new sb.Uint16Array()), '[object Uint16Array]');
}
