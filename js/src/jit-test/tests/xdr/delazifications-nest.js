load(libdir + 'bytecode-cache.js');

let test = `
// Put some unused atoms in the initial stencil, to verify that atoms are
// correctly mapped while merging stencils.
if (false) {
  "unused text1";
  "unused text2";
  "unused text3";
  "unused text4";
  "unused text5";
}

var result = 0;

function func() {
  var anonFunc = function() {
    // Method/accessor as inner-inner function.
    var obj = {
      method(name) {
        // Test object literal.
        var object = {
          propA: 9,
          propB: 10,
          propC: 11,
        };

        // Test object property access.
        return object["prop" + name];
      },
      get prop1() {
        return 200;
      },
      set prop2(value) {
        result += value;
      }
    };
    result += obj.prop1;
    obj.prop2 = 3000;
    result += obj.method("B");
  };

  function anotherFunc() {
    return 40000;
  }

  function unused() {
  }

  class MyClass {
    constructor() {
      result += 500000;
    }
  }

  // Functions inside arrow parameters, that are parsed multiple times.
  const arrow = (a = (b = c => { result += 6000000 }) => b) => a()();

  // Delazify in the different order as definition.
  new MyClass();
  anonFunc();
  result += anotherFunc();
  arrow();
}
func();

result;
`;

evalWithCache(test, { incremental: true, oassertEqResult : true });
