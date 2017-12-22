// |reftest| skip-if(!xulRuntime.shell)
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function funArgs(params) {
    return Reflect.parse(`function f(${params}) {}`).body[0].rest;
}

var arrayRest = funArgs("...[]");
assertEq(arrayRest.type, "ArrayPattern");
assertEq(arrayRest.elements.length, 0);

arrayRest = funArgs("...[a]");
assertEq(arrayRest.type, "ArrayPattern");
assertEq(arrayRest.elements.length, 1);

var objectRest = funArgs("...{}");
assertEq(objectRest.type, "ObjectPattern");
assertEq(objectRest.properties.length, 0);

objectRest = funArgs("...{p: a}");
assertEq(objectRest.type, "ObjectPattern");
assertEq(objectRest.properties.length, 1);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
