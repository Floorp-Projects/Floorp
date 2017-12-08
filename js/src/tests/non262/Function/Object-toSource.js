var BUGNUMBER = 1317400;
var summary = "Function string representation in Object.prototype.toSource";

print(BUGNUMBER + ": " + summary);

// Methods.

assertEq(({ foo(){} }).toSource(),
         "({foo(){}})");
assertEq(({ *foo(){} }).toSource(),
         "({*foo(){}})");
assertEq(({ async foo(){} }).toSource(),
         "({async foo(){}})");

assertEq(({ 1(){} }).toSource(),
         "({1(){}})");

// Methods with more spacing.
// Spacing is kept.

assertEq(({ foo (){} }).toSource(),
         "({foo (){}})");
assertEq(({ foo () {} }).toSource(),
         "({foo () {}})");

// Methods with computed name.
// Method syntax is composed.

let name = "foo";
assertEq(({ [name](){} }).toSource(),
         "({foo(){}})");
assertEq(({ *[name](){} }).toSource(),
         "({*foo(){}})");
assertEq(({ async [name](){} }).toSource(),
         "({async foo(){}})");

assertEq(({ [ Symbol.iterator ](){} }).toSource(),
         "({[Symbol.iterator](){}})");

// Accessors.

assertEq(({ get foo(){} }).toSource(),
         "({get foo(){}})");
assertEq(({ set foo(v){} }).toSource(),
         "({set foo(v){}})");

// Accessors with computed name.
// Method syntax is composed.

assertEq(({ get [name](){} }).toSource(),
         "({get foo(){}})");
assertEq(({ set [name](v){} }).toSource(),
         "({set foo(v){}})");

assertEq(({ get [ Symbol.iterator ](){} }).toSource(),
         "({get [Symbol.iterator](){}})");
assertEq(({ set [ Symbol.iterator ](v){} }).toSource(),
         "({set [Symbol.iterator](v){}})");

// Getter and setter with same name.
// Getter always comes before setter.

assertEq(({ get foo(){}, set foo(v){} }).toSource(),
         "({get foo(){}, set foo(v){}})");
assertEq(({ set foo(v){}, get foo(){} }).toSource(),
         "({get foo(){}, set foo(v){}})");

// Normal properties.

assertEq(({ foo: function(){} }).toSource(),
         "({foo:(function(){})})");
assertEq(({ foo: function bar(){} }).toSource(),
         "({foo:(function bar(){})})");
assertEq(({ foo: function*(){} }).toSource(),
         "({foo:(function*(){})})");
assertEq(({ foo: async function(){} }).toSource(),
         "({foo:(async function(){})})");

// Normal properties with computed name.

assertEq(({ [ Symbol.iterator ]: function(){} }).toSource(),
         "({[Symbol.iterator]:(function(){})})");

// Dynamically defined properties with function expression.
// Never become a method syntax.

let obj = {};
obj.foo = function() {};
assertEq(obj.toSource(),
         "({foo:(function() {})})");

obj = {};
Object.defineProperty(obj, "foo", {value: function() {}});
assertEq(obj.toSource(),
         "({})");

obj = {};
Object.defineProperty(obj, "foo", {value: function() {}, enumerable: true});
assertEq(obj.toSource(),
         "({foo:(function() {})})");

obj = {};
Object.defineProperty(obj, "foo", {value: function bar() {}, enumerable: true});
assertEq(obj.toSource(),
         "({foo:(function bar() {})})");

obj = {};
Object.defineProperty(obj, Symbol.iterator, {value: function() {}, enumerable: true});
assertEq(obj.toSource(),
         "({[Symbol.iterator]:(function() {})})");

// Dynamically defined property with other object's method.
// Method syntax is composed.

let method = ({foo() {}}).foo;

obj = {};
Object.defineProperty(obj, "foo", {value: method, enumerable: true});
assertEq(obj.toSource(),
         "({foo() {}})");

obj = {};
Object.defineProperty(obj, "bar", {value: method, enumerable: true});
assertEq(obj.toSource(),
         "({bar() {}})");

method = ({*foo() {}}).foo;

obj = {};
Object.defineProperty(obj, "bar", {value: method, enumerable: true});
assertEq(obj.toSource(),
         "({*bar() {}})");

method = ({async foo() {}}).foo;

obj = {};
Object.defineProperty(obj, "bar", {value: method, enumerable: true});
assertEq(obj.toSource(),
         "({async bar() {}})");

// Dynamically defined accessors.
// Accessor syntax is composed.

obj = {};
Object.defineProperty(obj, "foo", {get: function() {}, enumerable: true});
assertEq(obj.toSource(),
         "({get foo() {}})");

obj = {};
Object.defineProperty(obj, "foo", {set: function() {}, enumerable: true});
assertEq(obj.toSource(),
         "({set foo() {}})");

obj = {};
Object.defineProperty(obj, Symbol.iterator, {get: function() {}, enumerable: true});
assertEq(obj.toSource(),
         "({get [Symbol.iterator]() {}})");

obj = {};
Object.defineProperty(obj, Symbol.iterator, {set: function() {}, enumerable: true});
assertEq(obj.toSource(),
         "({set [Symbol.iterator]() {}})");

// Dynamically defined accessors with other object's accessors.
// Accessor syntax is composed.

let accessor = Object.getOwnPropertyDescriptor({ get foo() {} }, "foo").get;
obj = {};
Object.defineProperty(obj, "foo", {get: accessor, enumerable: true});
assertEq(obj.toSource(),
         "({get foo() {}})");

accessor = Object.getOwnPropertyDescriptor({ get bar() {} }, "bar").get;
obj = {};
Object.defineProperty(obj, "foo", {get: accessor, enumerable: true});
assertEq(obj.toSource(),
         "({get foo() {}})");

accessor = Object.getOwnPropertyDescriptor({ set foo(v) {} }, "foo").set;
obj = {};
Object.defineProperty(obj, "foo", {get: accessor, enumerable: true});
assertEq(obj.toSource(),
         "({get foo(v) {}})");

accessor = Object.getOwnPropertyDescriptor({ set bar(v) {} }, "bar").set;
obj = {};
Object.defineProperty(obj, "foo", {get: accessor, enumerable: true});
assertEq(obj.toSource(),
         "({get foo(v) {}})");

accessor = Object.getOwnPropertyDescriptor({ get foo() {} }, "foo").get;
obj = {};
Object.defineProperty(obj, "foo", {set: accessor, enumerable: true});
assertEq(obj.toSource(),
         "({set foo() {}})");

accessor = Object.getOwnPropertyDescriptor({ get bar() {} }, "bar").get;
obj = {};
Object.defineProperty(obj, "foo", {set: accessor, enumerable: true});
assertEq(obj.toSource(),
         "({set foo() {}})");

accessor = Object.getOwnPropertyDescriptor({ set foo(v) {} }, "foo").set;
obj = {};
Object.defineProperty(obj, "foo", {set: accessor, enumerable: true});
assertEq(obj.toSource(),
         "({set foo(v) {}})");

accessor = Object.getOwnPropertyDescriptor({ set bar(v) {} }, "bar").set;
obj = {};
Object.defineProperty(obj, "foo", {set: accessor, enumerable: true});
assertEq(obj.toSource(),
         "({set foo(v) {}})");

// Methods with proxy.
// Treated as normal property.

method = ({foo() {}}).foo;
let handler = {
  get(that, name) {
    if (name == "toSource") {
      return function() {
        return that.toSource();
      };
    }
    return that[name];
  }
};
let proxy = new Proxy(method, handler);

obj = {};
Object.defineProperty(obj, "foo", {value: proxy, enumerable: true});
assertEq(obj.toSource(),
         "({foo:foo() {}})");

// Accessors with proxy.
// Accessor syntax is composed.

accessor = Object.getOwnPropertyDescriptor({ get foo() {} }, "foo").get;
proxy = new Proxy(accessor, handler);

obj = {};
Object.defineProperty(obj, "foo", {get: proxy, enumerable: true});
assertEq(obj.toSource(),
         "({get foo() {}})");

obj = {};
Object.defineProperty(obj, "foo", {set: proxy, enumerable: true});
assertEq(obj.toSource(),
         "({set foo() {}})");

// Methods from other global.
// Treated as normal property.

let g = newGlobal();

method = g.eval("({ foo() {} }).foo");

obj = {};
Object.defineProperty(obj, "foo", {value: method, enumerable: true});
assertEq(obj.toSource(),
         "({foo:foo() {}})");

// Accessors from other global.
// Accessor syntax is composed.

accessor = g.eval("Object.getOwnPropertyDescriptor({ get foo() {} }, 'foo').get");

obj = {};
Object.defineProperty(obj, "foo", {get: accessor, enumerable: true});
assertEq(obj.toSource(),
         "({get foo() {}})");

accessor = g.eval("Object.getOwnPropertyDescriptor({ get bar() {} }, 'bar').get");

obj = {};
Object.defineProperty(obj, "foo", {get: accessor, enumerable: true});
assertEq(obj.toSource(),
         "({get foo() {}})");

accessor = g.eval("Object.getOwnPropertyDescriptor({ set foo(v) {} }, 'foo').set");

obj = {};
Object.defineProperty(obj, "foo", {get: accessor, enumerable: true});
assertEq(obj.toSource(),
         "({get foo(v) {}})");

accessor = g.eval("Object.getOwnPropertyDescriptor({ set bar(v) {} }, 'bar').set");

obj = {};
Object.defineProperty(obj, "foo", {get: accessor, enumerable: true});
assertEq(obj.toSource(),
         "({get foo(v) {}})");

accessor = g.eval("Object.getOwnPropertyDescriptor({ get foo() {} }, 'foo').get");

obj = {};
Object.defineProperty(obj, "foo", {set: accessor, enumerable: true});
assertEq(obj.toSource(),
         "({set foo() {}})");

accessor = g.eval("Object.getOwnPropertyDescriptor({ get bar() {} }, 'bar').get");

obj = {};
Object.defineProperty(obj, "foo", {set: accessor, enumerable: true});
assertEq(obj.toSource(),
         "({set foo() {}})");

accessor = g.eval("Object.getOwnPropertyDescriptor({ set foo(v) {} }, 'foo').set");

obj = {};
Object.defineProperty(obj, "foo", {set: accessor, enumerable: true});
assertEq(obj.toSource(),
         "({set foo(v) {}})");

accessor = g.eval("Object.getOwnPropertyDescriptor({ set bar(v) {} }, 'bar').set");

obj = {};
Object.defineProperty(obj, "foo", {set: accessor, enumerable: true});
assertEq(obj.toSource(),
         "({set foo(v) {}})");

// **** Some weird cases ****

// Accessors with generator or async.

obj = {};
Object.defineProperty(obj, "foo", {get: function*() {}, enumerable: true});
assertEq(obj.toSource(),
         "({get foo() {}})");

obj = {};
Object.defineProperty(obj, "foo", {set: async function() {}, enumerable: true});
assertEq(obj.toSource(),
         "({set foo() {}})");

// Modified toSource.

obj = { foo() {} };
obj.foo.toSource = () => "hello";
assertEq(obj.toSource(),
         "({hello})");

obj = { foo() {} };
obj.foo.toSource = () => "bar() {}";
assertEq(obj.toSource(),
         "({bar() {}})");

// Modified toSource with different method name.

obj = {};
Object.defineProperty(obj, "foo", {value: function bar() {}, enumerable: true});
obj.foo.toSource = () => "hello";
assertEq(obj.toSource(),
         "({foo:hello})");

obj = {};
Object.defineProperty(obj, "foo", {value: function* bar() {}, enumerable: true});
obj.foo.toSource = () => "hello";
assertEq(obj.toSource(),
         "({foo:hello})");

obj = {};
Object.defineProperty(obj, "foo", {value: async function bar() {}, enumerable: true});
obj.foo.toSource = () => "hello";
assertEq(obj.toSource(),
         "({foo:hello})");

if (typeof reportCompare === "function")
    reportCompare(true, true);
