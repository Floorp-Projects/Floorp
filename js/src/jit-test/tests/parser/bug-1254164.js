// |jit-test| slow;

var s = '';
for (var i = 0; i < 70000; i++)
    s += 'function x' + i + '() { x' + i + '(); }\n';
eval("(function() { " + s + " })();");
