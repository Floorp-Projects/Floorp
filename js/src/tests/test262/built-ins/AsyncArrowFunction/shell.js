// GENERATED, DO NOT EDIT
// file: hidden-constructors.js
// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: |
  Provides uniform access to built-in constructors that are not exposed to the global object.
defines:
  - AsyncArrowFunction
  - AsyncFunction
  - AsyncGeneratorFunction
  - GeneratorFunction
---*/

var AsyncArrowFunction = Object.getPrototypeOf(async () => {}).constructor;
var AsyncFunction = Object.getPrototypeOf(async function () {}).constructor;
var AsyncGeneratorFunction = Object.getPrototypeOf(async function* () {}).constructor;
var GeneratorFunction = Object.getPrototypeOf(function* () {}).constructor;

// file: isConstructor.js
// Copyright (C) 2017 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: |
    Test if a given function is a constructor function.
defines: [isConstructor]
features: [Reflect.construct]
---*/

function isConstructor(f) {
    if (typeof f !== "function") {
      throw new Test262Error("isConstructor invoked with a non-function value");
    }

    try {
        Reflect.construct(function(){}, [], f);
    } catch (e) {
        return false;
    }
    return true;
}
