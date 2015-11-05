if (helperThreadCount() != 0)
   quit();

var code = `
function f() {
    [function() { { function g() { } } },
     function() { with ({}) { } }];
}
`;

offThreadCompileScript(code);
runOffThreadScript();
