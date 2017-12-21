const Cu = Components.utils;
function run_test() {

  var sb = new Cu.Sandbox('http://www.example.com');
  function checkThrows(str, rgxp) {
    try {
      sb.eval(str);
      Assert.ok(false, "eval should have thrown");
    } catch (e) {
      Assert.ok(rgxp.test(e), "error message should match");
    }
  }

  sb.exposed = {
    get getterProp() { return 42; },
    set setterProp(x) { },
    get getterSetterProp() { return 42; },
    set getterSetterProp(x) { },
    simpleValueProp: 42,
    objectValueProp: { val: 42, __exposedProps__: { val: 'r' } },
    contentCallableValueProp: new sb.Function('return 42'),
    chromeCallableValueProp: function() {},
    __exposedProps__: { getterProp : 'r',
                        setterProp : 'w',
                        getterSetterProp: 'rw',
                        simpleValueProp: 'r',
                        objectValueProp: 'r',
                        contentCallableValueProp: 'r',
                        chromeCallableValueProp: 'r' }
  };

  Assert.equal(sb.eval('exposed.simpleValueProp'), undefined);
  Assert.equal(sb.eval('exposed.objectValueProp'), undefined);
  Assert.equal(sb.eval('exposed.getterProp;'), undefined);
  Assert.equal(sb.eval('exposed.getterSetterProp;'), undefined);
  checkThrows('exposed.setterProp = 42;', /Permission denied/i);
  checkThrows('exposed.getterSetterProp = 42;', /Permission denied/i);
  Assert.equal(sb.eval('exposed.contentCallableValueProp'), undefined);
  checkThrows('exposed.chromeCallableValueProp();', /is not a function/i);
}
