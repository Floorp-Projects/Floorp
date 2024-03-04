// |reftest| skip-if(!xulRuntime.shell)

// Test reflect.parse on a function with arguments.length
let ast = Reflect.parse(`function f10() {
    return arguments.length;
}`);

assertEq(ast.body[0].body.body[0].argument.object.type, "Identifier");
assertEq(ast.body[0].body.body[0].argument.object.name, "arguments");
assertEq(ast.body[0].body.body[0].argument.property.type, "Identifier");
assertEq(ast.body[0].body.body[0].argument.property.name, "length");

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
