// |jit-test| module

// import.meta is an object.
assertEq(typeof import.meta, "object");
assertEq(import.meta !== null, true);

// import.meta always returns the same object.
let obj = import.meta;
assertEq(import.meta, obj);

// Test calling from lazily compile function.
function get() {
    return import.meta;
}
assertEq(get(), import.meta);

// import.meta.url: This property will contain the module script's base URL,
// serialized.

assertEq("url" in import.meta, true);
assertEq(import.meta.url.endsWith("import-meta.js"), true);

import getOtherMetaObject from "exportImportMeta.js";

let otherImportMeta = getOtherMetaObject();
assertEq(otherImportMeta.url.endsWith("exportImportMeta.js"), true);

// By default the import.meta object will be extensible, and its properties will
// be writable, configurable, and enumerable.

assertEq(Object.isExtensible(import.meta), true);

var desc = Object.getOwnPropertyDescriptor(import.meta, "url");
assertEq(desc.writable, true);
assertEq(desc.enumerable, true);
assertEq(desc.configurable, true);
assertEq(desc.value, import.meta.url);

// The import.meta object's prototype is null.
assertEq(Object.getPrototypeOf(import.meta), null);

import.meta.url = 0;
assertEq(import.meta.url, 0);

import.meta.newProp = 42;
assertEq(import.meta.newProp, 42);

let found = new Set(Reflect.ownKeys(import.meta));

assertEq(found.size, 2);
assertEq(found.has("url"), true);
assertEq(found.has("newProp"), true);

delete import.meta.url;
delete import.meta.newProp;

found = new Set(Reflect.ownKeys(import.meta));
assertEq(found.size, 0);
