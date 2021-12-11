function run_test() {
  var sb1 = Cu.Sandbox('http://example.com');
  var sb2 = Cu.Sandbox('http://example.org');
  var chromeObj = {foo: 2};
  var sb1obj = Cu.evalInSandbox('new Object()', sb1);
  chromeObj.__proto__ = sb1obj;
  sb2.wrapMe = chromeObj;
  Assert.ok(true, "Didn't crash");
  Assert.equal(sb2.wrapMe.__proto__, sb1obj, 'proto set correctly');
}
