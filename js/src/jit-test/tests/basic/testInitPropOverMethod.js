for (var i = 0; i < 10; i++)
    x = {a: function () { return 33; }, a: 4};

try {
    result = x.a();
} catch (exc) {
    result = "threw " + exc.name;
}    
assertEq(result, "threw TypeError");
