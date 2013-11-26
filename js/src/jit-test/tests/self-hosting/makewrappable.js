
var testObject = {prop:'value'};

var testWrappersAllowUseAsKey = getSelfHostedValue('testWrappersAllowUseAsKey');
assertEq(typeof testWrappersAllowUseAsKey, 'function');
assertEq(testWrappersAllowUseAsKey(testObject), testObject);

var testWrappersForbidAccess = getSelfHostedValue('testWrappersForbidAccess');
assertEq(typeof testWrappersForbidAccess, 'function');
assertEq(testWrappersForbidAccess(testObject, 'get'), true);
assertEq(testWrappersForbidAccess(testObject, 'set'), true);
assertEq(testWrappersForbidAccess(function(){}, 'call'), true);
assertEq(testWrappersForbidAccess(testObject, '__proto__'), true);

var wrappedFunction = testWrappersForbidAccess;
function testWrappersAllowCallsOnly(fun, operation) {
  try {
    switch (operation) {
      case 'get': var result = fun.prop; break;
      case 'set': fun.prop2 = 'value'; break;
      case 'call': o(); break;
      case '__proto__':
        Object.getOwnPropertyDescriptor(Object.prototype, '__proto__').set.call(fun, new Object());
        break;
    }
  } catch (e) {
    // Got the expected exception.
    return /denied/.test(e);
  }
  return false;
}

assertEq(testWrappersAllowCallsOnly(wrappedFunction, 'get'), true);
assertEq(testWrappersAllowCallsOnly(wrappedFunction, 'set'), true);
assertEq(testWrappersAllowCallsOnly(wrappedFunction, '__proto__'), true);
assertEq(testWrappersAllowCallsOnly(wrappedFunction, 'call'), false);
