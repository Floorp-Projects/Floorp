// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

Reflect.parse("#[]");
Reflect.parse("#[ foo ]");
Reflect.parse("#[ foo, ]");
Reflect.parse("#[ foo, bar ]");
Reflect.parse("#[ foo, ...bar ]");
Reflect.parse("#[ foo, ...bar, ]");
Reflect.parse("#[ foo, ...bar, baz ]");
Reflect.parse("#[ foo() ]");

assertThrowsInstanceOf(() => Reflect.parse("#[,]"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("#[ foo, , bar ]"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("#[ foo ] = bar"), SyntaxError);

if (typeof reportCompare === "function") reportCompare(0, 0);
