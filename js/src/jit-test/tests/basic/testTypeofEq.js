var testcases = [
  [1, "NUMBER"],
  [1.1, "NUMBER"],
  [true, "BOOLEAN"],
  [false, "BOOLEAN"],
  [null, "OBJECT"],
  [undefined, "UNDEFINED"],
  ["foo", "STRING"],
  [Symbol.iterator, "SYMBOL"],
  [function f() {}, "FUNCTION"],
  [{}, "OBJECT"],
];

function runTest(f) {
  for (let i = 0; i < 2000; i++) {
    let testcase = testcases[i % testcases.length];
    assertEq(f(testcase[0]), testcase[1]);
  }
}

function loose_left(x) {
  if (typeof x == "boolean") {
    return "BOOLEAN";
  }
  if (typeof x == "function") {
    return "FUNCTION";
  }
  if (typeof x == "number") {
    return "NUMBER";
  }
  if (typeof x == "object") {
    return "OBJECT";
  }
  if (typeof x == "string") {
    return "STRING";
  }
  if (typeof x == "symbol") {
    return "SYMBOL";
  }
  if (typeof x == "undefined") {
    return "UNDEFINED";
  }
  return "???";
}

function loose_right(x) {
  if ("boolean" == typeof x) {
    return "BOOLEAN";
  }
  if ("function" == typeof x) {
    return "FUNCTION";
  }
  if ("number" == typeof x) {
    return "NUMBER";
  }
  if ("object" == typeof x) {
    return "OBJECT";
  }
  if ("string" == typeof x) {
    return "STRING";
  }
  if ("symbol" == typeof x) {
    return "SYMBOL";
  }
  if ("undefined" == typeof x) {
    return "UNDEFINED";
  }
  return "???";
}

function strict_left(x) {
  if (typeof x === "boolean") {
    return "BOOLEAN";
  }
  if (typeof x === "function") {
    return "FUNCTION";
  }
  if (typeof x === "number") {
    return "NUMBER";
  }
  if (typeof x === "object") {
    return "OBJECT";
  }
  if (typeof x === "string") {
    return "STRING";
  }
  if (typeof x === "symbol") {
    return "SYMBOL";
  }
  if (typeof x === "undefined") {
    return "UNDEFINED";
  }
  return "???";
}

function strict_right(x) {
  if ("boolean" === typeof x) {
    return "BOOLEAN";
  }
  if ("function" === typeof x) {
    return "FUNCTION";
  }
  if ("number" === typeof x) {
    return "NUMBER";
  }
  if ("object" === typeof x) {
    return "OBJECT";
  }
  if ("string" === typeof x) {
    return "STRING";
  }
  if ("symbol" === typeof x) {
    return "SYMBOL";
  }
  if ("undefined" === typeof x) {
    return "UNDEFINED";
  }
  return "???";
}

function loose_left_ne(x) {
  if (typeof x != "boolean") {
  } else {
    return "BOOLEAN";
  }
  if (typeof x != "function") {
  } else {
    return "FUNCTION";
  }
  if (typeof x != "number") {
  } else {
    return "NUMBER";
  }
  if (typeof x != "object") {
  } else {
    return "OBJECT";
  }
  if (typeof x != "string") {
  } else {
    return "STRING";
  }
  if (typeof x != "symbol") {
  } else {
    return "SYMBOL";
  }
  if (typeof x != "undefined") {
  } else {
    return "UNDEFINED";
  }
  return "???";
}

function loose_right_ne(x) {
  if ("boolean" != typeof x) {
  } else {
    return "BOOLEAN";
  }
  if ("function" != typeof x) {
  } else {
    return "FUNCTION";
  }
  if ("number" != typeof x) {
  } else {
    return "NUMBER";
  }
  if ("object" != typeof x) {
  } else {
    return "OBJECT";
  }
  if ("string" != typeof x) {
  } else {
    return "STRING";
  }
  if ("symbol" != typeof x) {
  } else {
    return "SYMBOL";
  }
  if ("undefined" != typeof x) {
  } else {
    return "UNDEFINED";
  }
  return "???";
}

function strict_left_ne(x) {
  if (typeof x !== "boolean") {
  } else {
    return "BOOLEAN";
  }
  if (typeof x !== "function") {
  } else {
    return "FUNCTION";
  }
  if (typeof x !== "number") {
  } else {
    return "NUMBER";
  }
  if (typeof x !== "object") {
  } else {
    return "OBJECT";
  }
  if (typeof x !== "string") {
  } else {
    return "STRING";
  }
  if (typeof x !== "symbol") {
  } else {
    return "SYMBOL";
  }
  if (typeof x !== "undefined") {
  } else {
    return "UNDEFINED";
  }
  return "???";
}

function strict_right_ne(x) {
  if ("boolean" !== typeof x) {
  } else {
    return "BOOLEAN";
  }
  if ("function" !== typeof x) {
  } else {
    return "FUNCTION";
  }
  if ("number" !== typeof x) {
  } else {
    return "NUMBER";
  }
  if ("object" !== typeof x) {
  } else {
    return "OBJECT";
  }
  if ("string" !== typeof x) {
  } else {
    return "STRING";
  }
  if ("symbol" !== typeof x) {
  } else {
    return "SYMBOL";
  }
  if ("undefined" !== typeof x) {
  } else {
    return "UNDEFINED";
  }
  return "???";
}

function loose_left_expr(x) {
  if (typeof (0, x) == "boolean") {
    return "BOOLEAN";
  }
  if (typeof (0, x) == "function") {
    return "FUNCTION";
  }
  if (typeof (0, x) == "number") {
    return "NUMBER";
  }
  if (typeof (0, x) == "object") {
    return "OBJECT";
  }
  if (typeof (0, x) == "string") {
    return "STRING";
  }
  if (typeof (0, x) == "symbol") {
    return "SYMBOL";
  }
  if (typeof (0, x) == "undefined") {
    return "UNDEFINED";
  }
  return "???";
}

function loose_right_expr(x) {
  if ("boolean" == typeof (0, x)) {
    return "BOOLEAN";
  }
  if ("function" == typeof (0, x)) {
    return "FUNCTION";
  }
  if ("number" == typeof (0, x)) {
    return "NUMBER";
  }
  if ("object" == typeof (0, x)) {
    return "OBJECT";
  }
  if ("string" == typeof (0, x)) {
    return "STRING";
  }
  if ("symbol" == typeof (0, x)) {
    return "SYMBOL";
  }
  if ("undefined" == typeof (0, x)) {
    return "UNDEFINED";
  }
  return "???";
}

function strict_left_expr(x) {
  if (typeof (0, x) === "boolean") {
    return "BOOLEAN";
  }
  if (typeof (0, x) === "function") {
    return "FUNCTION";
  }
  if (typeof (0, x) === "number") {
    return "NUMBER";
  }
  if (typeof (0, x) === "object") {
    return "OBJECT";
  }
  if (typeof (0, x) === "string") {
    return "STRING";
  }
  if (typeof (0, x) === "symbol") {
    return "SYMBOL";
  }
  if (typeof (0, x) === "undefined") {
    return "UNDEFINED";
  }
  return "???";
}

function strict_right_expr(x) {
  if ("boolean" === typeof (0, x)) {
    return "BOOLEAN";
  }
  if ("function" === typeof (0, x)) {
    return "FUNCTION";
  }
  if ("number" === typeof (0, x)) {
    return "NUMBER";
  }
  if ("object" === typeof (0, x)) {
    return "OBJECT";
  }
  if ("string" === typeof (0, x)) {
    return "STRING";
  }
  if ("symbol" === typeof (0, x)) {
    return "SYMBOL";
  }
  if ("undefined" === typeof (0, x)) {
    return "UNDEFINED";
  }
  return "???";
}

function loose_left_ne_expr(x) {
  if (typeof (0, x) != "boolean") {
  } else {
    return "BOOLEAN";
  }
  if (typeof (0, x) != "function") {
  } else {
    return "FUNCTION";
  }
  if (typeof (0, x) != "number") {
  } else {
    return "NUMBER";
  }
  if (typeof (0, x) != "object") {
  } else {
    return "OBJECT";
  }
  if (typeof (0, x) != "string") {
  } else {
    return "STRING";
  }
  if (typeof (0, x) != "symbol") {
  } else {
    return "SYMBOL";
  }
  if (typeof (0, x) != "undefined") {
  } else {
    return "UNDEFINED";
  }
  return "???";
}

function loose_right_ne_expr(x) {
  if ("boolean" != typeof (0, x)) {
  } else {
    return "BOOLEAN";
  }
  if ("function" != typeof (0, x)) {
  } else {
    return "FUNCTION";
  }
  if ("number" != typeof (0, x)) {
  } else {
    return "NUMBER";
  }
  if ("object" != typeof (0, x)) {
  } else {
    return "OBJECT";
  }
  if ("string" != typeof (0, x)) {
  } else {
    return "STRING";
  }
  if ("symbol" != typeof (0, x)) {
  } else {
    return "SYMBOL";
  }
  if ("undefined" != typeof (0, x)) {
  } else {
    return "UNDEFINED";
  }
  return "???";
}

function strict_left_ne_expr(x) {
  if (typeof (0, x) !== "boolean") {
  } else {
    return "BOOLEAN";
  }
  if (typeof (0, x) !== "function") {
  } else {
    return "FUNCTION";
  }
  if (typeof (0, x) !== "number") {
  } else {
    return "NUMBER";
  }
  if (typeof (0, x) !== "object") {
  } else {
    return "OBJECT";
  }
  if (typeof (0, x) !== "string") {
  } else {
    return "STRING";
  }
  if (typeof (0, x) !== "symbol") {
  } else {
    return "SYMBOL";
  }
  if (typeof (0, x) !== "undefined") {
  } else {
    return "UNDEFINED";
  }
  return "???";
}

function strict_right_ne_expr(x) {
  if ("boolean" !== typeof (0, x)) {
  } else {
    return "BOOLEAN";
  }
  if ("function" !== typeof (0, x)) {
  } else {
    return "FUNCTION";
  }
  if ("number" !== typeof (0, x)) {
  } else {
    return "NUMBER";
  }
  if ("object" !== typeof (0, x)) {
  } else {
    return "OBJECT";
  }
  if ("string" !== typeof (0, x)) {
  } else {
    return "STRING";
  }
  if ("symbol" !== typeof (0, x)) {
  } else {
    return "SYMBOL";
  }
  if ("undefined" !== typeof (0, x)) {
  } else {
    return "UNDEFINED";
  }
  return "???";
}

runTest(loose_left);
runTest(loose_right);
runTest(strict_left);
runTest(strict_right);
runTest(loose_left_ne);
runTest(loose_right_ne);
runTest(strict_left_ne);
runTest(strict_right_ne);
runTest(loose_left_expr);
runTest(loose_right_expr);
runTest(strict_left_expr);
runTest(strict_right_expr);
runTest(loose_left_ne_expr);
runTest(loose_right_ne_expr);
runTest(strict_left_ne_expr);
runTest(strict_right_ne_expr);
