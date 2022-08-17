// |jit-test| skip-if: !getBuildConfiguration()['decorators']

load(libdir + "asserts.js");

Reflect.parse("class c {@dec1 field = false;}");
Reflect.parse("class c {@dec1 @dec2 @dec3 field = false;}");
Reflect.parse("class c {@dec1 @dec2 @dec3('a') static field = false;}");
Reflect.parse("class c {@dec1 method() {};}");
Reflect.parse("class c {@dec1 @dec2 @dec3 method() {};}");
Reflect.parse("class c {@dec1 @dec2 @dec3 static method() {};}");
Reflect.parse("class c {@dec1 @dec2('a') @dec3 method(...args) {};}");
Reflect.parse("class c {@((a, b, c) => {}) method(a, b) {};}");
Reflect.parse("class c {@((a, b, c) => {}) @dec2('a', 'b') @dec3 method(a, b) {};}");
Reflect.parse("@dec1 class c {}");
Reflect.parse("@dec1 @dec2 @dec3 class c {}");
Reflect.parse("@dec1('a') @(() => {}) @dec3 class c {}");
Reflect.parse("@dec1('a') @(() => {}) @dec3 class c {@((a, b, c) => {}) @dec2('a', 'b') @dec3 method(a, b) {};}");
Reflect.parse("x = @dec class { #x }");
Reflect.parse("x = (class A { }, @dec class { })");
Reflect.parse("@dec1 class A extends @dec2 class B extends @dec3 class {} {} {}");
Reflect.parse("class c {@dec1.dec2.dec3 method() {};}");
Reflect.parse("class c {@dec1 @dec2.dec3.dec4 @dec5 method() {};}");
Reflect.parse("class c {@dec1('a') @dec2.dec3.dec4 @dec5 method() {};}");
Reflect.parse("class c {@dec1('a') @dec2.dec3.dec4('b', 'c') @dec5 method() {};}");
Reflect.parse("class c {@dec1('a') @dec2.dec3.dec4('b', 'c') @dec5.dec6 method() {};}");
Reflect.parse("@dec1.dec2 class c {}");
Reflect.parse("@dec1.dec2 @dec3.dec4 @dec5 class c {}");
Reflect.parse("@dec1.dec2('a') @(() => {}) @dec4 class c {}");
Reflect.parse("@dec1.dec2('a') @(() => {}) @dec4 class c {@((a, b, c) => {}) @dec5.dec6('a', 'b') @dec7 method(a, b) {};}");

assertThrowsInstanceOf(() => Reflect.parse("class c {@ method() {};}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("class c {@((a, b, c => {}) method(a, b) {};}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("class c {@dec1 static @dec2 method(a, b) {};}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("@dec1 let x = 1"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("@dec1 f(a) {}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("@dec1 () => {}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("@class class { x; }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("@for class { x; }"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("class c {@dec1. method() {};}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("@dec1. class c {method() {};}"), SyntaxError);
assertThrowsInstanceOf(() => Reflect.parse("@dec1.(a) class c {method() {};}"), SyntaxError);
