const Cu = Components.utils;
function run_test() {
  var sb = Cu.Sandbox('http://www.example.com', { wantGlobalProperties: ['XMLHttpRequest']});
  var xhr = Cu.evalInSandbox('new XMLHttpRequest()', sb);
  Assert.equal(xhr.toString(), '[object XMLHttpRequest]');
  Assert.equal((new sb.Object()).toString(), '[object Object]');
  Assert.equal(sb.Object.prototype.toString.call(new sb.Uint16Array()), '[object Uint16Array]');
}
