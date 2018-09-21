// tests calling script functions via Debugger.Object.prototype.setProperty
"use strict";

var global = newGlobal();
var dbg = new Debugger(global);
dbg.onDebuggerStatement = onDebuggerStatement;

global.eval(`
const sym = Symbol("a symbol key");

const arr = [];
const obj = {
    set stringNormal(value) {
        this._stringNormal = value;
    },
    set stringAbrupt(value) {
        throw value;
    },

    set objectNormal(value) {
        this._objectNormal = value;
    },
    set objectAbrupt(value) {
        throw value;
    },

    set context(value) {
        this._context = this;
    },

    set 1234(value) {
        this._1234 = value;
    },

    set [sym](value) {
        this._sym = value;
    },

    get getterOnly() {},
    readOnly: "",

    stringProp: "",
    objProp: {},
};
Object.defineProperty(obj, "readOnly", { writable: false });

const objChild = Object.create(obj);
const proxyChild = new Proxy(obj, {});

const testObj = { };
const testChildObj = { };
const testProxyObj = { };

debugger;
`);

function onDebuggerStatement(frame) {
    const { environment } = frame;
    const sym = environment.getVariable("sym");
    const arr = environment.getVariable("arr");
    const obj = environment.getVariable("obj");
    const objChild = environment.getVariable("objChild");
    const proxyChild = environment.getVariable("proxyChild");

    assertEq(arr.setProperty(1, "index 1").return, true);
    assertEq(arr.getProperty(1).return, "index 1");
    assertEq(arr.getProperty("1").return, "index 1");
    assertEq(arr.getProperty(0).return, undefined);
    assertEq(arr.getProperty("length").return, 2);

    assertEq(arr.setProperty("2", "index 2").return, true);
    assertEq(arr.getProperty(2).return, "index 2");
    assertEq(arr.getProperty("2").return, "index 2");
    assertEq(arr.getProperty(0).return, undefined);
    assertEq(arr.getProperty("length").return, 3);

    const testObj = environment.getVariable("testObj");
    assertEq(obj.setProperty("undefined", 123).return, true);
    assertEq(obj.getProperty("undefined").return, 123);
    assertEq(obj.setProperty().return, true);
    assertEq(obj.getProperty("undefined").return, undefined);

    assertEq(obj.setProperty("missing", 42).return, true);
    assertEq(obj.getProperty("missing").return, 42);

    assertEq(obj.setProperty("stringNormal", "a normal value").return, true);
    assertEq(obj.getProperty("_stringNormal").return, "a normal value");
    assertEq(obj.setProperty("stringAbrupt", "an abrupt value").throw, "an abrupt value");

    assertEq(obj.setProperty("objectNormal", testObj).return, true);
    assertEq(obj.getProperty("_objectNormal").return, testObj);
    assertEq(obj.setProperty("objectAbrupt", testObj).throw, testObj);

    assertEq(obj.setProperty("context", "a value").return, true);
    assertEq(obj.getProperty("_context").return, obj);

    assertEq(obj.setProperty(1234, "number value").return, true);
    assertEq(obj.getProperty("_1234").return, "number value");

    assertEq(obj.setProperty(sym, "symbol value").return, true);
    assertEq(obj.getProperty("_sym").return, "symbol value");

    assertEq(obj.setProperty("getterOnly", "a value").return, false);
    assertEq(obj.setProperty("readOnly", "a value").return, false);

    assertEq(obj.setProperty("stringProp", "a normal value").return, true);
    assertEq(obj.getProperty("stringProp").return, "a normal value");

    assertEq(obj.setProperty("objectProp", testObj).return, true);
    assertEq(obj.getProperty("objectProp").return, testObj);

    const testChildObj = environment.getVariable("testChildObj");
    assertEq(objChild.setProperty("undefined", 123).return, true);
    assertEq(objChild.getProperty("undefined").return, 123);
    assertEq(objChild.setProperty().return, true);
    assertEq(objChild.getProperty("undefined").return, undefined);

    assertEq(objChild.setProperty("missing", 42).return, true);
    assertEq(objChild.getProperty("missing").return, 42);

    assertEq(objChild.setProperty("stringNormal", "a normal child value").return, true);
    assertEq(objChild.getProperty("_stringNormal").return, "a normal child value");

    assertEq(objChild.setProperty("stringAbrupt", "an abrupt child value").throw, "an abrupt child value");

    assertEq(objChild.setProperty("objectNormal", testChildObj).return, true);
    assertEq(objChild.getProperty("_objectNormal").return, testChildObj);

    assertEq(objChild.setProperty("objectAbrupt", testChildObj).throw, testChildObj);

    assertEq(objChild.setProperty("context", "a value").return, true);
    assertEq(objChild.getProperty("_context").return, objChild);

    assertEq(objChild.setProperty(1234, "number value").return, true);
    assertEq(objChild.getProperty("_1234").return, "number value");

    assertEq(objChild.setProperty(sym, "symbol value").return, true);
    assertEq(objChild.getProperty("_sym").return, "symbol value");

    assertEq(objChild.setProperty("getterOnly", "a value").return, false);
    assertEq(objChild.setProperty("readOnly", "a value").return, false);

    assertEq(objChild.setProperty("stringProp", "a normal child value").return, true);
    assertEq(objChild.getProperty("stringProp").return, "a normal child value");

    assertEq(objChild.setProperty("objectProp", testChildObj).return, true);
    assertEq(objChild.getProperty("objectProp").return, testChildObj);

    const testProxyObj = environment.getVariable("testProxyObj");
    assertEq(proxyChild.setProperty("undefined", 123).return, true);
    assertEq(proxyChild.getProperty("undefined").return, 123);
    assertEq(proxyChild.setProperty().return, true);
    assertEq(proxyChild.getProperty("undefined").return, undefined);

    assertEq(proxyChild.setProperty("missing", 42).return, true);
    assertEq(proxyChild.getProperty("missing").return, 42);

    assertEq(proxyChild.setProperty("stringNormal", "a normal child value").return, true);
    assertEq(proxyChild.getProperty("_stringNormal").return, "a normal child value");

    assertEq(proxyChild.setProperty("stringAbrupt", "an abrupt child value").throw, "an abrupt child value");

    assertEq(proxyChild.setProperty("objectNormal", testProxyObj).return, true);
    assertEq(proxyChild.getProperty("_objectNormal").return, testProxyObj);

    assertEq(proxyChild.setProperty("objectAbrupt", testProxyObj).throw, testProxyObj);

    assertEq(proxyChild.setProperty("context", "a value").return, true);
    assertEq(proxyChild.getProperty("_context").return, proxyChild);

    assertEq(proxyChild.setProperty(1234, "number value").return, true);
    assertEq(proxyChild.getProperty("_1234").return, "number value");

    assertEq(proxyChild.setProperty(sym, "symbol value").return, true);
    assertEq(proxyChild.getProperty("_sym").return, "symbol value");

    assertEq(proxyChild.setProperty("getterOnly", "a value").return, false);
    assertEq(proxyChild.setProperty("readOnly", "a value").return, false);

    assertEq(proxyChild.setProperty("stringProp", "a normal child value").return, true);
    assertEq(proxyChild.getProperty("stringProp").return, "a normal child value");

    assertEq(proxyChild.setProperty("objectProp", testProxyObj).return, true);
    assertEq(proxyChild.getProperty("objectProp").return, testProxyObj);
};
