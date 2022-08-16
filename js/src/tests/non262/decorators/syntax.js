// |reftest| skip-if(!getBuildConfiguration()['decorators'])

Reflect.parse("class c {@dec1 field = false;}");
Reflect.parse("class c {@dec1 @dec2 @dec3 field = false;}");
Reflect.parse("class c {@dec1 @dec2 @dec3('a') static field = false;}");
Reflect.parse("class c {@dec1 method() {};}");
Reflect.parse("class c {@dec1 @dec2 @dec3 method() {};}");
Reflect.parse("class c {@dec1 @dec2 @dec3 static method() {};}");
Reflect.parse("class c {@dec1 @dec2('a') @dec3 method(...args) {};}");
Reflect.parse("class c {@((a, b, c) => {}) method(a, b) {};}");
Reflect.parse("class c {@((a, b, c) => {}) @dec2('a', 'b') @dec3 method(a, b) {};}");

assertThrowsInstanceOf(() => Reflect.parse("class c {@ method() {};}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("class c {@((a, b, c => {}) method(a, b) {};}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("class c {@dec1 static @dec2 method(a, b) {};}"), SyntaxError);

if (typeof reportCompare === "function") reportCompare(0, 0);
