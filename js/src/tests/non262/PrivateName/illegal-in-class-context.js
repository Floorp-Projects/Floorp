// |reftest| skip-if(!xulRuntime.shell)

// Private names can't appear in classes (yet)

assertThrowsInstanceOf(() => eval(`class A { #x }`), SyntaxError);
assertThrowsInstanceOf(() => eval(`class A { #x=10 }`), SyntaxError);

if (typeof reportCompare === 'function') reportCompare(0, 0);
