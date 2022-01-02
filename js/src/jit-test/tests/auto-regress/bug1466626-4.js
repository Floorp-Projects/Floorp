// |jit-test| skip-if: !('oomTest' in this)

var source = "{";
for (var i = 0; i < 120; ++i)
    source += `function f${i}(){}`
source += "}";

oomTest(function() {
    Function(source);
});
