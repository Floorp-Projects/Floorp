const BUGNUMBER = 1405943;
const summary = "Implement pipeline operator (parse error)";

print(BUGNUMBER + ": " + summary);

if (hasPipeline()) {
    // Invalid Token
    assertThrows(() => Function("2 | > parseInt"), SyntaxError);
    assertThrows(() => Function("2 ||> parseInt"), SyntaxError);
    assertThrows(() => Function("2 |>> parseInt"), SyntaxError);
    assertThrows(() => Function("2 <| parseInt"), SyntaxError);
    // Invalid Syntax
    assertThrows(() => Function("2 |>"), SyntaxError);
    assertThrows(() => Function("|> parseInt"), SyntaxError);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
