JSON = function () {
    return {
        stringify: function stringify(value, whitelist) {
            switch (typeof(value)) {
              case "object":
                return value.constructor.name;
            }
        }
    };
}();

function missingArgTest2() {
  var testPairs = [
    ["{}", {}],
    ["[]", []],
    ['{"foo":"bar"}', {"foo":"bar"}],
  ]

  var a = [];
  for (var i=0; i < testPairs.length; i++) {
    var s = JSON.stringify(testPairs[i][1])
    a[i] = s;
  }
  return a.join(",");
}
assertEq(missingArgTest2(), "Object,Array,Object");
