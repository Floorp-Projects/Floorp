// |reftest| skip-if(!xulRuntime.shell) shell-option(--enable-private-fields)

// Private methods aren't yet supported.

assertThrowsInstanceOf(() => eval(`var A = class { #a(){} };`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var A = class { get #a(){} };`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var A = class { set #a(v){} };`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var A = class { *#a(v){} };`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var A = class { async #a(v){} };`), SyntaxError);
assertThrowsInstanceOf(() => eval(`var A = class { async *#a(v){} };`), SyntaxError);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
