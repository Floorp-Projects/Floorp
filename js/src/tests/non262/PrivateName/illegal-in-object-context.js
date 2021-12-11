// |reftest| skip-if(!xulRuntime.shell)

// Private names aren't valid in object literals.

assertThrowsInstanceOf(() => eval(`var o = {#a: 0};`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var o = {#a};`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var o = {#a(){}};`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var o = {get #a(){}};`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var o = {set #a(v){}};`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var o = {*#a(v){}};`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var o = {async #a(v){}};`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var o = {async *#a(v){}};`), SyntaxError);

if (typeof reportCompare === 'function') reportCompare(0, 0);
