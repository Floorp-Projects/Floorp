if (!("oomTest" in this))
    quit();

var source = "{";
for (var i = 0; i < 120; ++i)
    source += `function f${i}(){}`
source += "}";

oomTest(function() {
    Function(source);
});
