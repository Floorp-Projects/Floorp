const BUGNUMBER = 1405943;
const summary = "Implement pipeline operator (Reflect.parse)";

print(BUGNUMBER + ": " + summary);

if (hasPipeline()) {
    if (typeof Reflect !== "undefined" && Reflect.parse) {
        const parseTree1 = Reflect.parse("a |> b");
        assertEq(parseTree1.body[0].type, "ExpressionStatement");
        assertEq(parseTree1.body[0].expression.type, "BinaryExpression");
        assertEq(parseTree1.body[0].expression.operator, "|>");
        assertEq(parseTree1.body[0].expression.left.name, "a");
        assertEq(parseTree1.body[0].expression.right.name, "b");

        const parseTree2 = Reflect.parse("a |> b |> c");
        assertEq(parseTree2.body[0].type, "ExpressionStatement");
        assertEq(parseTree2.body[0].expression.type, "BinaryExpression");
        assertEq(parseTree2.body[0].expression.operator, "|>");
        assertEq(parseTree2.body[0].expression.left.type, "BinaryExpression");
        assertEq(parseTree2.body[0].expression.left.operator, "|>");
        assertEq(parseTree2.body[0].expression.left.left.name, "a");
        assertEq(parseTree2.body[0].expression.left.right.name, "b");
        assertEq(parseTree2.body[0].expression.right.name, "c");
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
