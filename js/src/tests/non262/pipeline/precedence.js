const BUGNUMBER = 1405943;
const summary = "Implement pipeline operator (operator precedence)";

print(BUGNUMBER + ": " + summary);

if (hasPipeline()) {
    // Operator precedence
    eval(`
    const double = (n) => n * 2;
    const increment = (n) => n + 1;
    const getType = (v) => typeof v;
    const toString = (v) => v.toString();
    let a = 8;
    assertEq(10 |> double |> increment |> double, 42);
    assertEq(++ a |> double, 18);
    assertEq(typeof 42 |> toString, "number");
    assertEq(! true |> toString, "false");
    assertEq(delete 42 |> toString, "true");
    assertEq(10 ** 2 |> double, 200);
    assertEq(10 * 2 |> increment, 21);
    assertEq(5 + 5 |> double, 20);
    assertEq(1 << 4 |> double, 32);
    assertEq(0 < 1 |> toString, "true");
    assertEq("a" in {} |> toString, "false");
    assertEq(10 instanceof String |> toString, "false");
    assertEq(10 == 10 |> toString, "true");
    assertEq(0b0101 & 0b1101 |> increment, 0b0110);
    assertEq(false && true |> toString, "false");
    assertEq(0 |> double ? toString : null, null);
    assertEq(true ? 20 |> toString : 10, '20');
    assertEq(true ? 10 : 20 |> toString, 10);
    a = 10 |> toString;
    assertEq(a, "10");
    a = 10;
    a += 2 |> increment;
    assertEq(a, 13);

    // Right-side association
    a = toString;
    assertThrowsInstanceOf(() => "42" |> parseInt ** 2, TypeError);
    assertThrowsInstanceOf(() => "42" |> parseInt * 1, TypeError);
    assertThrowsInstanceOf(() => "42" |> parseInt + 0, TypeError);
    assertThrowsInstanceOf(() => "42" |> parseInt << 1, TypeError);
    assertThrowsInstanceOf(() => "42" |> parseInt < 0, TypeError);
    assertThrowsInstanceOf(() => "42" |> parseInt in {}, TypeError);
    assertThrowsInstanceOf(() => "42" |> parseInt instanceof String, TypeError);
    assertThrowsInstanceOf(() => "42" |> parseInt == 255, TypeError);
    assertThrowsInstanceOf(() => "42" |> parseInt & 0b1111, TypeError);
    assertThrowsInstanceOf(() => "42" |> parseInt && true, TypeError);
    assertThrowsInstanceOf(() => Function('"42" |> a = parseInt'), ReferenceError);
    assertThrowsInstanceOf(() => Function('"42" |> a += parseInt'), ReferenceError);
    `);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
