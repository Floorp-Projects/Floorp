function testImport(path, name, value) {
    let result = null;
    let error = null;
    let promise = import(path);
    promise.then((ns) => {
        result = ns;
    }).catch((e) => {
        error = e;
    });

    drainJobQueue();
    assertEq(error, null);
    assertEq(result[name], value);
}

// Resolved via module load path.
testImport("module1.js", "a", 1);

// Relative path resolved relative to this script.
testImport("../../modules/module1a.js", "a", 2);

// Import inside function.
function f() {
    testImport("../../modules/module2.js", "b", 2);
}
f();

// Import inside eval.
eval(`testImport("../../modules/module3.js", "c", 3)`);

// Import inside indirect eval.
const indirect = eval;
const defineTestFunc = testImport.toString();
indirect(defineTestFunc + `testImport("../../modules/module3.js");`);

// Import inside dynamic function.
Function(defineTestFunc + `testImport("../../modules/module3.js");`)();

// Import in eval in promise handler.
let ran = false;
Promise
    .resolve(`import("../../modules/module3.js").then(() => { ran = true; })`)
    .then(eval)
drainJobQueue();
assertEq(ran, true);
