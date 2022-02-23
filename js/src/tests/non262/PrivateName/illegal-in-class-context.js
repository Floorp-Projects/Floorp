// |reftest| skip-if(!xulRuntime.shell)

assertThrowsInstanceOf(() => eval(`class A { #x; #x; }`), SyntaxError);

// No computed private fields
assertThrowsInstanceOf(
    () => eval(`var x = "foo"; class A { #[x] = 20; }`), SyntaxError);


if (typeof reportCompare === 'function') reportCompare(0, 0);
