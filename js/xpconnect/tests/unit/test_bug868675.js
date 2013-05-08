const Cu = Components.utils;
function run_test() {

  // Make sure we don't throw for primitive values.
  var result = "threw";
  try { result = XPCNativeWrapper.unwrap(2); } catch (e) {}
  do_check_eq(result, 2);
  result = "threw";
  try { result = XPCNativeWrapper(2); } catch (e) {}
  do_check_eq(result, 2);

  // Make sure that we can waive on a non-Xrayable object, and that we preserve
  // transitive waiving behavior.
  var sb = new Cu.Sandbox('http://www.example.com', {wantXHRConstructor: true});
  Cu.evalInSandbox('this.xhr = new XMLHttpRequest();', sb);
  Cu.evalInSandbox('this.jsobj = {mynative: xhr};', sb);
  do_check_true(!Cu.isXrayWrapper(XPCNativeWrapper.unwrap(sb.xhr)));
  do_check_true(Cu.isXrayWrapper(sb.jsobj.mynative));
  do_check_true(!Cu.isXrayWrapper(XPCNativeWrapper.unwrap(sb.jsobj).mynative));

  // Test the new Cu API.
  var waived = Cu.waiveXrays(sb.xhr);
  do_check_true(!Cu.isXrayWrapper(waived));
  do_check_true(Cu.isXrayWrapper(Cu.unwaiveXrays(waived)));
}
