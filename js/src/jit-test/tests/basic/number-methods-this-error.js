load(libdir + "asserts.js");

let methods = Object.getOwnPropertyNames(Number.prototype)
                    .filter(n => n != "constructor");

for (let method of methods) {
  assertTypeErrorMessage(() => Number.prototype[method].call(new Map),
    `Number.prototype.${method} called on incompatible Map`);

  assertTypeErrorMessage(() => Number.prototype[method].call(false),
    `Number.prototype.${method} called on incompatible boolean`);
}
