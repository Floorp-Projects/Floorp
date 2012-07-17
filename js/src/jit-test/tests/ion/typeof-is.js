function isObject(x) {
        return "object" === typeof x;
}
function isObject2(x) {
    if ("object" === typeof x)
        return true;
    return false;
}
function isBoolean(x) {
        return "boolean" === typeof x;
}
function isBoolean2(x) {
    if ("boolean" === typeof x)
        return true;
    return false;
}
function isFunction(x) {
        return "function" === typeof x;
}
function isFunction2(x) {
    if ("function" === typeof x)
        return true;
    return false;
}
function isNumber(x) {
        return "number" === typeof x;
}
function isNumber2(x) {
    if ("number" === typeof x)
        return true;
    return false;
}
function isString(x) {
        return "string" === typeof x;
}
function isString2(x) {
    if ("string" === typeof x)
        return true;
    return false;
}
function isUndefined(x) {
        return "undefined" === typeof x;
}
function isUndefined2(x) {
    if ("undefined" === typeof x)
        return true;
    return false;
}
function isBanana(x) {
        return "Banana" === typeof x;
}
function isBanana2(x) {
    if ("Banana" === typeof x)
        return true;
    return false;
}

for (var i = 0; i < 100; i++) {
    assertEq(isObject(null), true);
    assertEq(isObject2(null), true);
    assertEq(isBoolean(null), false);
    assertEq(isBoolean2(null), false);
    assertEq(isFunction(null), false);
    assertEq(isFunction2(null), false);
    assertEq(isNumber(null), false);
    assertEq(isNumber2(null), false);
    assertEq(isString(null), false);
    assertEq(isString2(null), false);
    assertEq(isUndefined(null), false);
    assertEq(isUndefined2(null), false);
    assertEq(isBanana(null), false);
    assertEq(isBanana2(null), false);

    assertEq(isObject({}), true);
    assertEq(isObject2({}), true);
    assertEq(isBoolean({}), false);
    assertEq(isBoolean2({}), false);
    assertEq(isFunction({}), false);
    assertEq(isFunction2({}), false);
    assertEq(isNumber({}), false);
    assertEq(isNumber2({}), false);
    assertEq(isString({}), false);
    assertEq(isString2({}), false);
    assertEq(isUndefined({}), false);
    assertEq(isUndefined2({}), false);
    assertEq(isBanana({}), false);
    assertEq(isBanana2({}), false);

    assertEq(isObject([]), true);
    assertEq(isObject2([]), true);
    assertEq(isBoolean([]), false);
    assertEq(isBoolean2([]), false);
    assertEq(isFunction([]), false);
    assertEq(isFunction2([]), false);
    assertEq(isNumber([]), false);
    assertEq(isNumber2([]), false);
    assertEq(isString([]), false);
    assertEq(isString2([]), false);
    assertEq(isUndefined([]), false);
    assertEq(isUndefined2([]), false);
    assertEq(isBanana([]), false);
    assertEq(isBanana2([]), false);

    assertEq(isObject(new Date()), true);
    assertEq(isObject2(new Date()), true);
    assertEq(isBoolean(new Date()), false);
    assertEq(isBoolean2(new Date()), false);
    assertEq(isFunction(new Date()), false);
    assertEq(isFunction2(new Date()), false);
    assertEq(isNumber(new Date()), false);
    assertEq(isNumber2(new Date()), false);
    assertEq(isString(new Date()), false);
    assertEq(isString2(new Date()), false);
    assertEq(isUndefined(new Date()), false);
    assertEq(isUndefined2(new Date()), false);
    assertEq(isBanana(new Date()), false);
    assertEq(isBanana2(new Date()), false);

    assertEq(isObject(new String()), true);
    assertEq(isObject2(new String()), true);
    assertEq(isBoolean(new String()), false);
    assertEq(isBoolean2(new String()), false);
    assertEq(isFunction(new String()), false);
    assertEq(isFunction2(new String()), false);
    assertEq(isNumber(new String()), false);
    assertEq(isNumber2(new String()), false);
    assertEq(isString(new String()), false);
    assertEq(isString2(new String()), false);
    assertEq(isUndefined(new String()), false);
    assertEq(isUndefined2(new String()), false);
    assertEq(isBanana(new String()), false);
    assertEq(isBanana2(new String()), false);

    assertEq(isObject(new Boolean()), true);
    assertEq(isObject2(new Boolean()), true);
    assertEq(isBoolean(new Boolean()), false);
    assertEq(isBoolean2(new Boolean()), false);
    assertEq(isFunction(new Boolean()), false);
    assertEq(isFunction2(new Boolean()), false);
    assertEq(isNumber(new Boolean()), false);
    assertEq(isNumber2(new Boolean()), false);
    assertEq(isString(new Boolean()), false);
    assertEq(isString2(new Boolean()), false);
    assertEq(isUndefined(new Boolean()), false);
    assertEq(isUndefined2(new Boolean()), false);
    assertEq(isBanana(new Boolean()), false);
    assertEq(isBanana2(new Boolean()), false);

    assertEq(isObject(Proxy.create({})), true);
    assertEq(isObject2(Proxy.create({})), true);
    assertEq(isBoolean(Proxy.create({})), false);
    assertEq(isBoolean2(Proxy.create({})), false);
    assertEq(isFunction(Proxy.create({})), false);
    assertEq(isFunction2(Proxy.create({})), false);
    assertEq(isNumber(Proxy.create({})), false);
    assertEq(isNumber2(Proxy.create({})), false);
    assertEq(isString(Proxy.create({})), false);
    assertEq(isString2(Proxy.create({})), false);
    assertEq(isUndefined(Proxy.create({})), false);
    assertEq(isUndefined2(Proxy.create({})), false);
    assertEq(isBanana(Proxy.create({})), false);
    assertEq(isBanana2(Proxy.create({})), false);

    assertEq(isObject(true), false);
    assertEq(isObject2(true), false);
    assertEq(isBoolean(true), true);
    assertEq(isBoolean2(true), true);
    assertEq(isFunction(true), false);
    assertEq(isFunction2(true), false);
    assertEq(isNumber(true), false);
    assertEq(isNumber2(true), false);
    assertEq(isString(true), false);
    assertEq(isString2(true), false);
    assertEq(isUndefined(true), false);
    assertEq(isUndefined2(true), false);
    assertEq(isBanana(true), false);
    assertEq(isBanana2(true), false);

    assertEq(isObject(false), false);
    assertEq(isObject2(false), false);
    assertEq(isBoolean(false), true);
    assertEq(isBoolean2(false), true);
    assertEq(isFunction(false), false);
    assertEq(isFunction2(false), false);
    assertEq(isNumber(false), false);
    assertEq(isNumber2(false), false);
    assertEq(isString(false), false);
    assertEq(isString2(false), false);
    assertEq(isUndefined(false), false);
    assertEq(isUndefined2(false), false);
    assertEq(isBanana(false), false);
    assertEq(isBanana2(false), false);


    assertEq(isObject(Function.prototype), false);
    assertEq(isObject2(Function.prototype), false);
    assertEq(isBoolean(Function.prototype), false);
    assertEq(isBoolean2(Function.prototype), false);
    assertEq(isFunction(Function.prototype), true);
    assertEq(isFunction2(Function.prototype), true);
    assertEq(isNumber(Function.prototype), false);
    assertEq(isNumber2(Function.prototype), false);
    assertEq(isString(Function.prototype), false);
    assertEq(isString2(Function.prototype), false);
    assertEq(isUndefined(Function.prototype), false);
    assertEq(isUndefined2(Function.prototype), false);
    assertEq(isBanana(Function.prototype), false);
    assertEq(isBanana2(Function.prototype), false);


    assertEq(isObject(eval), false);
    assertEq(isObject2(eval), false);
    assertEq(isBoolean(eval), false);
    assertEq(isBoolean2(eval), false);
    assertEq(isFunction(eval), true);
    assertEq(isFunction2(eval), true);
    assertEq(isNumber(eval), false);
    assertEq(isNumber2(eval), false);
    assertEq(isString(eval), false);
    assertEq(isString2(eval), false);
    assertEq(isUndefined(eval), false);
    assertEq(isUndefined2(eval), false);
    assertEq(isBanana(eval), false);
    assertEq(isBanana2(eval), false);


    assertEq(isObject((function () {}).bind()), false);
    assertEq(isObject2((function () {}).bind()), false);
    assertEq(isBoolean((function () {}).bind()), false);
    assertEq(isBoolean2((function () {}).bind()), false);
    assertEq(isFunction((function () {}).bind()), true);
    assertEq(isFunction2((function () {}).bind()), true);
    assertEq(isNumber((function () {}).bind()), false);
    assertEq(isNumber2((function () {}).bind()), false);
    assertEq(isString((function () {}).bind()), false);
    assertEq(isString2((function () {}).bind()), false);
    assertEq(isUndefined((function () {}).bind()), false);
    assertEq(isUndefined2((function () {}).bind()), false);
    assertEq(isBanana((function () {}).bind()), false);
    assertEq(isBanana2((function () {}).bind()), false);

    assertEq(isObject(eval), false);
    assertEq(isObject2(eval), false);
    assertEq(isBoolean(eval), false);
    assertEq(isBoolean2(eval), false);
    assertEq(isFunction(eval), true);
    assertEq(isFunction2(eval), true);
    assertEq(isNumber(eval), false);
    assertEq(isNumber2(eval), false);
    assertEq(isString(eval), false);
    assertEq(isString2(eval), false);
    assertEq(isUndefined(eval), false);
    assertEq(isUndefined2(eval), false);
    assertEq(isBanana(eval), false);
    assertEq(isBanana2(eval), false);

    assertEq(isObject(Proxy.createFunction({}, function () {}, function () {})), false);
    assertEq(isObject2(Proxy.createFunction({}, function () {}, function () {})), false);
    assertEq(isBoolean(Proxy.createFunction({}, function () {}, function () {})), false);
    assertEq(isBoolean2(Proxy.createFunction({}, function () {}, function () {})), false);
    assertEq(isFunction(Proxy.createFunction({}, function () {}, function () {})), true);
    assertEq(isFunction2(Proxy.createFunction({}, function () {}, function () {})), true);
    assertEq(isNumber(Proxy.createFunction({}, function () {}, function () {})), false);
    assertEq(isNumber2(Proxy.createFunction({}, function () {}, function () {})), false);
    assertEq(isString(Proxy.createFunction({}, function () {}, function () {})), false);
    assertEq(isString2(Proxy.createFunction({}, function () {}, function () {})), false);
    assertEq(isUndefined(Proxy.createFunction({}, function () {}, function () {})), false);
    assertEq(isUndefined2(Proxy.createFunction({}, function () {}, function () {})), false);
    assertEq(isBanana(Proxy.createFunction({}, function () {}, function () {})), false);
    assertEq(isBanana2(Proxy.createFunction({}, function () {}, function () {})), false);

    assertEq(isObject(1.5), false);
    assertEq(isObject2(1.5), false);
    assertEq(isBoolean(1.5), false);
    assertEq(isBoolean2(1.5), false);
    assertEq(isFunction(1.5), false);
    assertEq(isFunction2(1.5), false);
    assertEq(isNumber(1.5), true);
    assertEq(isNumber2(1.5), true);
    assertEq(isString(1.5), false);
    assertEq(isString2(1.5), false);
    assertEq(isUndefined(1.5), false);
    assertEq(isUndefined2(1.5), false);
    assertEq(isBanana(1.5), false);
    assertEq(isBanana2(1.5), false);

    assertEq(isObject(30), false);
    assertEq(isObject2(30), false);
    assertEq(isBoolean(30), false);
    assertEq(isBoolean2(30), false);
    assertEq(isFunction(30), false);
    assertEq(isFunction2(30), false);
    assertEq(isNumber(30), true);
    assertEq(isNumber2(30), true);
    assertEq(isString(30), false);
    assertEq(isString2(30), false);
    assertEq(isUndefined(30), false);
    assertEq(isUndefined2(30), false);
    assertEq(isBanana(30), false);
    assertEq(isBanana2(30), false);

    assertEq(isObject(NaN), false);
    assertEq(isObject2(NaN), false);
    assertEq(isBoolean(NaN), false);
    assertEq(isBoolean2(NaN), false);
    assertEq(isFunction(NaN), false);
    assertEq(isFunction2(NaN), false);
    assertEq(isNumber(NaN), true);
    assertEq(isNumber2(NaN), true);
    assertEq(isString(NaN), false);
    assertEq(isString2(NaN), false);
    assertEq(isUndefined(NaN), false);
    assertEq(isUndefined2(NaN), false);
    assertEq(isBanana(NaN), false);
    assertEq(isBanana2(NaN), false);

    assertEq(isObject("test"), false);
    assertEq(isObject2("test"), false);
    assertEq(isBoolean("test"), false);
    assertEq(isBoolean2("test"), false);
    assertEq(isFunction("test"), false);
    assertEq(isFunction2("test"), false);
    assertEq(isNumber("test"), false);
    assertEq(isNumber2("test"), false);
    assertEq(isString("test"), true);
    assertEq(isString2("test"), true);
    assertEq(isUndefined("test"), false);
    assertEq(isUndefined2("test"), false);
    assertEq(isBanana("test"), false);
    assertEq(isBanana2("test"), false);

    assertEq(isObject(undefined), false);
    assertEq(isObject2(undefined), false);
    assertEq(isBoolean(undefined), false);
    assertEq(isBoolean2(undefined), false);
    assertEq(isFunction(undefined), false);
    assertEq(isFunction2(undefined), false);
    assertEq(isNumber(undefined), false);
    assertEq(isNumber2(undefined), false);
    assertEq(isString(undefined), false);
    assertEq(isString2(undefined), false);
    assertEq(isUndefined(undefined), true);
    assertEq(isUndefined2(undefined), true);
    assertEq(isBanana(undefined), false);
    assertEq(isBanana2(undefined), false);
}
