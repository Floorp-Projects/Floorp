const Cu = Components.utils;

function setupChromeSandbox() {
  this.chromeObj = {a: 2, __exposedProps__: {a: "rw", b: "rw"} };
  this.chromeArr = [4, 2, 1];
}

function checkDefineThrows(sb, obj, prop, desc) {
  var result = Cu.evalInSandbox('(function() { try { Object.defineProperty(' + obj + ', "' + prop + '", ' + desc.toSource() + '); return "nothrow"; } catch (e) { return e.toString(); }})();', sb);
  Assert.notEqual(result, 'nothrow');
  Assert.ok(!!/denied|prohibited/.exec(result));
  Assert.ok(result.indexOf(prop) != -1); // Make sure the prop name is in the error message.
}

function run_test() {
  var chromeSB = new Cu.Sandbox(this);
  var contentSB = new Cu.Sandbox('http://www.example.org');
  Cu.evalInSandbox('(' + setupChromeSandbox.toSource() + ')()', chromeSB);
  contentSB.chromeObj = chromeSB.chromeObj;
  contentSB.chromeArr = chromeSB.chromeArr;

  Assert.equal(Cu.evalInSandbox('chromeObj.a', contentSB), undefined);
  try {
    Cu.evalInSandbox('chromeArr[1]', contentSB);
    Assert.ok(false);
  } catch (e) { Assert.ok(/denied|insecure/.test(e)); }

  checkDefineThrows(contentSB, 'chromeObj', 'a', {get: function() { return 2; }});
  checkDefineThrows(contentSB, 'chromeObj', 'a', {configurable: true, get: function() { return 2; }});
  checkDefineThrows(contentSB, 'chromeObj', 'b', {configurable: true, get: function() { return 2; }, set: function() {}});
  checkDefineThrows(contentSB, 'chromeArr', '1', {configurable: true, get: function() { return 2; }});
}
