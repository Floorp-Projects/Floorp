/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var BUGNUMBER = 1069063;
var summary = "Implement Array.prototype.contains";

print(BUGNUMBER + ": " + summary);

if ('contains' in Array.prototype) {
    assertEq(typeof [].contains, "function");
    assertEq([].contains.length, 1);
    
    assertTrue([1, 2, 3].contains(2));
    assertTrue([1,,2].contains(2));
    assertTrue([1, 2, 3].contains(2, 1));
    assertTrue([1, 2, 3].contains(2, -2));
    assertTrue([1, 2, 3].contains(2, -100));
    assertTrue([Object, Function, Array].contains(Function));
    assertTrue([-0].contains(0));
    assertTrue([NaN].contains(NaN));
    assertTrue([,].contains());
    assertTrue(staticContains("123", "2"));
    assertTrue(staticContains({length: 3, 1: 2}, 2));
    assertTrue(staticContains({length: 3, 1: 2, get 3(){throw ""}}, 2));
    assertTrue(staticContains({length: 3, get 1() {return 2}}, 2));
    assertTrue(staticContains({__proto__: {1: 2}, length: 3}, 2));
    assertTrue(staticContains(new Proxy([1], {get(){return 2}}), 2));
    
    assertFalse([1, 2, 3].contains("2"));
    assertFalse([1, 2, 3].contains(2, 2));
    assertFalse([1, 2, 3].contains(2, -1));
    assertFalse([undefined].contains(NaN));
    assertFalse([{}].contains({}));
    assertFalse(staticContains({length: 3, 1: 2}, 2, 2));
    assertFalse(staticContains({length: 3, get 0(){delete this[1]}, 1: 2}, 2));
    assertFalse(staticContains({length: -100, 0: 1}, 1));
    
    assertThrowsInstanceOf(() => staticContains(), TypeError);
    assertThrowsInstanceOf(() => staticContains(null), TypeError);
    assertThrowsInstanceOf(() => staticContains({get length(){throw TypeError()}}), TypeError);
    assertThrowsInstanceOf(() => staticContains({length: 3, get 1() {throw TypeError()}}, 2), TypeError);
    assertThrowsInstanceOf(() => staticContains({__proto__: {get 1() {throw TypeError()}}, length: 3}, 2), TypeError);
    assertThrowsInstanceOf(() => staticContains(new Proxy([1], {get(){throw TypeError()}})), TypeError);
}

function assertTrue(v){
    assertEq(v, true)
}

function assertFalse(v){
    assertEq(v, false)
}

function staticContains(o, v, f){
    return [].contains.call(o, v, f)
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
