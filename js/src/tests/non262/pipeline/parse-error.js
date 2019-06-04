const BUGNUMBER = 1405943;
const summary = "Implement pipeline operator (parse error)";

print(BUGNUMBER + ": " + summary);

if (hasPipeline()) {
    // Invalid Token
    assertThrowsInstanceOf(() => Function("2 | > parseInt"), SyntaxError);
    assertThrowsInstanceOf(() => Function("2 ||> parseInt"), SyntaxError);
    assertThrowsInstanceOf(() => Function("2 |>> parseInt"), SyntaxError);
    assertThrowsInstanceOf(() => Function("2 <| parseInt"), SyntaxError);
    // Invalid Syntax
    assertThrowsInstanceOf(() => Function("2 |>"), SyntaxError);
    assertThrowsInstanceOf(() => Function("|> parseInt"), SyntaxError);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
