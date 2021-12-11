function f(N)
{
        for (var i = 0; i != N; ++i) {
                var obj1 = {}, obj2 = {};
                obj1['a'+i] = 0;
                obj2['b'+i] = 0;
                for (var repeat = 0;repeat != 2; ++repeat) {
                        for (var j in obj1) {
                                for (var k in obj2) {
                                        gc();
                                }
                        }
                }
        }
}
var array = [function() { f(10); },
    function(array) { f(50); },
    function() { propertyIsEnumerable.call(undefined, {}); },
    ];
try {
  for (var i = 0; i != array.length; ++i)
    array[i]();
} catch (e) {}
