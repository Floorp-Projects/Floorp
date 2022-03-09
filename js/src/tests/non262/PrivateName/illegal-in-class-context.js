// |reftest| skip-if(!xulRuntime.shell)

assertThrowsInstanceOf(() => eval(`class A { #x; #x; }`), SyntaxError);

// No computed private fields
assertThrowsInstanceOf(
    () => eval(`var x = "foo"; class A { #[x] = 20; }`), SyntaxError);

function assertThrowsWithMessage(f, msg) {
    var fullmsg;
    try {
        f();
    } catch (exc) {
        if (exc.message.normalize() === msg.normalize())
            return;

        fullmsg = `Assertion failed: expected message '${msg}', got '${exc.message}'`;
    }

    if (fullmsg === undefined)
        fullmsg = `Assertion failed: expected exception, no exception thrown`;

    throw new Error(fullmsg);
}

assertThrowsWithMessage(() => eval(`class A { #x; h(o) { return !#x; }}`),
    "invalid use of private name in unary expression without object reference");
assertThrowsWithMessage(() => eval(`class A { #x; h(o) { return 1 + #x in o; }}`),
    "invalid use of private name due to operator precedence");


if (typeof reportCompare === 'function') reportCompare(0, 0);
