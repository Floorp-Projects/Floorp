// Test case from bug 785989 comment 3.

version(170);
Reflect.parse("for (let [a, b] of c) ;");
