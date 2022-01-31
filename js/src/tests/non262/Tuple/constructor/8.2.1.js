// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
8.2.1 The Tuple Constructor
The Tuple constructor:

is the intrinsic object %Tuple%.
*/

assertEq(typeof Tuple, "function");
assertEq(typeof Tuple.prototype, "object");

/*
is the initial value of the "Tuple" property of the global object.
*/
assertEq(this.Tuple, Tuple);

/*
creates and initializes a new Tuple object when called as a function.
*/
assertEq(Tuple(), #[]);
assertEq(Tuple(1), #[1]);
assertEq(Tuple("a", 1, true), #["a", 1, true]);
/* 8.2.1.1
3. For each element e of items,
a. If Type(e) is Object, throw a TypeError exception.
*/
assertThrowsInstanceOf(() => Tuple("a", new Object()), TypeError,
                       "Object in Tuple");

/*
is not intended to be used with the new operator or to be subclassed.
*/
/* 8.2.1.1
1. If NewTarget is not undefined, throw a TypeError exception.
*/
assertThrowsInstanceOf(() => new Tuple(1, 2, 3), TypeError, "Tuple is not intended to be used with the new operator");


/* It may be used as the value of an extends clause of a class definition but a super call to the Tuple constructor will cause an exception.
*/
class C extends Tuple{}; // class declaration is allowed
// super() is called implicitly
assertThrowsInstanceOf (() => new C(), TypeError, "super call to Tuple constructor");
class D extends Tuple {
    constructor() {
        super();
    }
};
// Explicit call to super() will also throw
assertThrowsInstanceOf(() => new D(), TypeError, "super call to Tuple constructor");

reportCompare(0, 0);



