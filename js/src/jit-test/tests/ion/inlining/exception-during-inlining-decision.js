function runNearStackLimit(f) {
  function t() {
    try {
      return t();
    } catch (e) {
      return f();
    }
  }
    return t()
}
var helpers = function helpers() {
  return {
    get isCompatVersion9() {
    }  };
}();
var testRunner = function testRunner() {
  var testRunner = {
    asyncTestHasNoErr: function asyncTestHasNoErr() {
    },
    runTests: function runTests(testsToRun) {
      for (var i in testsToRun) {
          this.runTest(i, testsToRun[i].name, testsToRun[i].body);
      }
    },
    runTest: function runTest(testIndex, testName, testBody) {
      try {
        testBody();
      } catch (ex) {
      }
    },
    asyncTestBegin: function asyncTestBegin() {
        return explicitAsyncTestExit ? `
                    ` : `
                    `;
    }  };
  return testRunner;
}();
var assert = function assert() {
  var validate = function validate() {
  };
  return {
    areEqual: function areEqual() {
      validate( message);
    },
    areNotEqual: function areNotEqual() {
    }  };
}();
class __c_19 {
  constructor() {
      this.foo = 'SimpleParent';
  }
}
var __v_2735 = [{
  body: function () {
    class __c_23 extends __c_19 {
      constructor() {
          super()
      }
    }
    let __v_2738 = new __c_23();
  }
}, {
  body: function () {
    class __c_26 extends __c_19 {
      constructor() {
          super();
      }
    }
    let __v_2739 = new __c_26();
  }
}, {
  body: function () {
    class __c_29 extends __c_19 {
      constructor() {
            super()
      }
    }
    let __v_2743 = new __c_29();
    class __c_30 extends __c_19 {
      constructor() {
            super()
            super();
      }
    }
    let __v_2746 = new __c_30();
  }
}, {
  body: function () {
    class __c_34 extends __c_19 {}
    let __v_2749 = new __c_34();
  }
}, {
  body: function () {
    class __c_87 extends __c_19 {
      constructor() {
        try {
          assert.areEqual();
        } catch (e) {}
            eval('super();')
      }
    }
    function __f_683(__v_2812) {
 __v_2812.foo
    }
      __f_683(new __c_87())
      runNearStackLimit(() => {
        return __f_683();
      })
  }
}, {
  body: function () {
  }
}];
  testRunner.runTests(__v_2735, {
  });