// tests calling script functions via Debugger.Object.prototype.getProperty
"use strict";

var global = newGlobal();
var dbg = new Debugger(global);
dbg.onDebuggerStatement = onDebuggerStatement;

global.eval(`
const normalObj = { };
const abruptObj = { };
const sym = Symbol("a symbol key");

const arr = [1, 2, 3];
const obj = {
    get stringNormal(){
        return "a value";
    },
    get stringAbrupt() {
        throw "a value";
    },
    get objectNormal() {
        return normalObj;
    },
    get objectAbrupt() {
        throw abruptObj;
    },
    get context() {
        return this;
    },

    1234: "number key",
    [sym]: "symbol key",
    stringProp: "a value",
    objectProp: {},
    method() {
        return "a value";
    },
    undefined: "undefined value",
};
const propObj = obj.objectProp;
const methodObj = obj.method;

const objChild = Object.create(obj);

const proxyChild = new Proxy(obj, {});

debugger;
`);

function onDebuggerStatement(frame) {
    const { environment } = frame;
    const arr = environment.getVariable("arr");
    const obj = environment.getVariable("obj");
    const objChild = environment.getVariable("objChild");
    const proxyChild = environment.getVariable("proxyChild");

    const sym = environment.getVariable("sym");
    const normalObj = environment.getVariable("normalObj");
    const abruptObj = environment.getVariable("abruptObj");
    const propObj = environment.getVariable("propObj");
    const methodObj = environment.getVariable("methodObj");

    assertEq(arr.getProperty(1).return, 2);
    assertEq(arr.getProperty("1").return, 2);

    assertEq(obj.getProperty().return, "undefined value");

    assertEq(obj.getProperty("missing").return, undefined);

    assertEq(obj.getProperty("stringNormal").return, "a value");
    assertEq(obj.getProperty("stringAbrupt").throw, "a value");

    assertEq(obj.getProperty("objectNormal").return, normalObj);
    assertEq(obj.getProperty("objectAbrupt").throw, abruptObj);

    assertEq(obj.getProperty("context").return, obj);

    assertEq(obj.getProperty(1234).return, "number key");
    assertEq(obj.getProperty(sym).return, "symbol key");
    assertEq(obj.getProperty("stringProp").return, "a value");
    assertEq(obj.getProperty("objectProp").return, propObj);

    assertEq(obj.getProperty("method").return, methodObj);

    assertEq(objChild.getProperty().return, "undefined value");

    assertEq(objChild.getProperty("missing").return, undefined);

    assertEq(objChild.getProperty("stringNormal").return, "a value");
    assertEq(objChild.getProperty("stringAbrupt").throw, "a value");

    assertEq(objChild.getProperty("objectNormal").return, normalObj);
    assertEq(objChild.getProperty("objectAbrupt").throw, abruptObj);

    assertEq(objChild.getProperty("context").return, objChild);

    assertEq(objChild.getProperty(1234).return, "number key");
    assertEq(objChild.getProperty(sym).return, "symbol key");
    assertEq(objChild.getProperty("stringProp").return, "a value");
    assertEq(objChild.getProperty("objectProp").return, propObj);

    assertEq(objChild.getProperty("method").return, methodObj);

    assertEq(proxyChild.getProperty().return, "undefined value");

    assertEq(proxyChild.getProperty("missing").return, undefined);

    assertEq(proxyChild.getProperty("stringNormal").return, "a value");
    assertEq(proxyChild.getProperty("stringAbrupt").throw, "a value");

    assertEq(proxyChild.getProperty("objectNormal").return, normalObj);
    assertEq(proxyChild.getProperty("objectAbrupt").throw, abruptObj);

    assertEq(proxyChild.getProperty("context").return, proxyChild);

    assertEq(proxyChild.getProperty(1234).return, "number key");
    assertEq(proxyChild.getProperty(sym).return, "symbol key");
    assertEq(proxyChild.getProperty("stringProp").return, "a value");
    assertEq(proxyChild.getProperty("objectProp").return, propObj);

    assertEq(proxyChild.getProperty("method").return, methodObj);
};
