// GENERATED, DO NOT EDIT
// file: arrayContains.js
// Copyright (C) 2017 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |
    Verify that a subArray is contained within an array.
defines: [arrayContains]
---*/

/**
 * @param {Array} array
 * @param {Array} subArray
 */

function arrayContains(array, subArray) {
  var found;
  for (var i = 0; i < subArray.length; i++) {
    found = false;
    for (var j = 0; j < array.length; j++) {
      if (subArray[i] === array[j]) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

// file: isConstructor.js
// Copyright (C) 2017 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: |
    Test if a given function is a constructor function.
defines: [isConstructor]
---*/

function isConstructor(f) {
    try {
        Reflect.construct(function(){}, [], f);
    } catch (e) {
        return false;
    }
    return true;
}
