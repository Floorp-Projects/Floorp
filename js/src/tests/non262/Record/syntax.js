// |reftest| skip-if(!this.hasOwnProperty("Record"))

Reflect.parse("#{}");
Reflect.parse("#{ foo: 1, bar: 2 }");
Reflect.parse("#{ foo: 1, bar: 2, }");
Reflect.parse("#{ foo, ...bar }");
Reflect.parse("#{ foo, ...bar, }");
Reflect.parse("#{ foo: 1, ...bar, baz: 2 }");
Reflect.parse("#{ foo }");
Reflect.parse("#{ foo, }");
Reflect.parse("#{ foo, bar }");
Reflect.parse("#{ foo: 1, bar }");
Reflect.parse("#{ foo, bar: 2 }");
Reflect.parse("#{ foo: bar() }");

Reflect.parse("#{ __proto__ }");
Reflect.parse("#{ ['__proto__']: 2 }");

assertThrowsInstanceOf(() => Reflect.parse("#{,}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("#{ foo() {} }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("#{ get foo() {} }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("#{ __proto__: 2 }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("#{ '__proto__': 2 }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("#{ foo = 2 }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("#{ foo } = bar"), SyntaxError);

if (typeof reportCompare === "function") reportCompare(0, 0);
