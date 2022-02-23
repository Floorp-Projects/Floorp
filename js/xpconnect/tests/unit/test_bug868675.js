function run_test() {

  // Make sure we don't throw for primitive values.
  var result = "threw";
  try { result = XPCNativeWrapper.unwrap(2); } catch (e) {}
  Assert.equal(result, 2);
  result = "threw";
  try { result = XPCNativeWrapper(2); } catch (e) {}
  Assert.equal(result, 2);

  // Make sure that we can waive on a non-Xrayable object, and that we preserve
  // transitive waiving behavior.
  var sb = new Cu.Sandbox('http://www.example.com', { wantGlobalProperties: ["XMLHttpRequest"] });
  Cu.evalInSandbox('this.xhr = new XMLHttpRequest();', sb);
  Cu.evalInSandbox('this.jsobj = {mynative: xhr};', sb);
  Assert.ok(!Cu.isXrayWrapper(XPCNativeWrapper.unwrap(sb.xhr)));
  Assert.ok(Cu.isXrayWrapper(sb.jsobj.mynative));
  Assert.ok(!Cu.isXrayWrapper(XPCNativeWrapper.unwrap(sb.jsobj).mynative));

  // Test the new Cu API.
  var waived = Cu.waiveXrays(sb.xhr);
  Assert.ok(!Cu.isXrayWrapper(waived));
  Assert.ok(Cu.isXrayWrapper(Cu.unwaiveXrays(waived)));
}
