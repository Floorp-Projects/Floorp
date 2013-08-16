<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

#Classes and Inheritance
A class is a blueprint from which individual objects are created. These
individual objects are the instances of the class. Each class defines one or
more members, which are initialized to a given value when the class is
instantiated. Data members are properties that allow each instance to have
their own state, whereas member functions are properties that allow instances to
have behavior. Inheritance allows classes to inherit state and behavior from an
existing classes, known as the base class. Unlike languages like C++ and Java,
JavaScript does not have native support for classical inheritance. Instead, it
uses something called prototypal inheritance. As it turns out, it is possible to
emulate classical inheritance using prototypal inheritance, but not without
writing a significant amount of boilerplate code.

Classes in JavaScript are defined using constructor functions. Each constructor
function has an associated object, known as its prototype, which is shared
between all instances of that class. We will show how to define classes using
constructors, and how to use prototypes to efficiently define member functions
on each instance. Classical inheritance can be implemented in JavaScript using
constructors and prototypes. We will show how to make inheritance work correctly
with respect to constructors, prototypes, and the instanceof operator, and how
to override methods in subclasses. The SDK uses a special constructor internally,
known as `Class`, to create constructors that behave properly with respect to
inheritance. The last section shows how to work with the `Class` constructor. It
is possible to read this section on its own. However, to fully appreciate how
`Class` works, and the problem it is supposed to solve, it is recommended that
you read the entire article.

##Constructors
In JavaScript, a class is defined by defining a constructor function for that
class. To illustrate this, let's define a simple constructor for a class
`Shape`:

    function Shape(x, y) {
        this.x = x;
        this.y = y;
    }

We can now use this constructor to create instances of `Shape`:

    let shape = new Shape(2, 3);
    shape instanceof Shape; // => true
    shape.x; // => 2
    shape.y; // => 3

The keyword new tells JavaScript that we are performing a constructor call.
Constructor calls differ from ordinary function calls in that JavaScript
automatically creates a new object and binds it to the keyword this for the
duration of the call. Moreover, if the constructor does not return a value, the
result of the call defaults to the value of this. Constructors are just ordinary
functions, however, so it is perfectly legal to perform ordinary function calls
on them. In fact, some people (including the Add-on SDK team) prefer to use
constructors this way. However, since the value of this is undefined for
ordinary function calls, we need to add some boilerplate code to convert them to
constructor calls:

    function Shape(x, y) {
        if (!this)
            return new Shape(x, y);
        this.x = x;
        this.y = y;
    }

##Prototypes
Every object has an implicit property, known as its prototype. When JavaScript
looks for a property, it first looks for it in the object itself. If it cannot
find the property there, it looks for it in the object's prototype. If the
property is found on the prototype, the lookup succeeds, and JavaScript pretends
that it found the property on the original object. Every function has an
explicit property, known as `prototype`. When a function is used in a
constructor call, JavaScript makes the value of this property the prototype of
the newly created object:

    let shape = Shape(2, 3);
    Object.getPrototypeOf(shape) == Shape.prototype; // => true

All instances of a class have the same prototype. This makes the prototype the
perfect place to define properties that are shared between instances of the
class. To illustrate this, let's add a member function to the class `Shape`:

    Shape.prototype.draw = function () {
        throw Error("not yet implemented");
    }
    let shape = Shape(2, 3);
    Shape.draw(); // => Error: not yet implemented

##Inheritance and Constructors
Suppose we want to create a new class, `Circle`, and inherit it from `Shape`.
Since every `Circle` is also a `Shape`, the constructor for `Circle` must be
called every time we call the constructor for `Shape`. Since JavaScript does
not have native support for inheritance, it doesn't do this automatically.
Instead, we need to call the constructor for `Shape` explicitly. The resulting
constructor looks as follows:

    function Circle(x, y, radius) {
       if (!this)
           return new Circle(x, y, radius);
       Shape.call(this, x, y);
       this.radius = radius;
    }

Note that the constructor for `Shape` is called as an ordinary function, and
reuses the object created for the constructor call to `Circle`. Had we used a
constructor call instead, the constructor for `Shape` would have been applied to
a different object than the constructor for `Circle`. We can now use the above
constructor to create instances of the class `Circle`:

    let circle = Circle(2, 3, 5);
    circle instanceof Circle; // => true
    circle.x; // => 2
    circle.y; // => 3
    circle.radius; // => 5

##Inheritance and Prototypes
There is a problem with the definition of `Circle` in the previous section that
we have glossed over thus far. Consider the following:

    let circle = Circle(2, 3, 5);
    circle.draw(); // => TypeError: circle.draw is not a function

This is not quite right. The method `draw` is defined on instances of `Shape`,
so we definitely want it to be defined on instances of `Circle`. The problem is
that `draw` is defined on the prototype of `Shape`, but not on the prototype of
`Circle`. We could of course copy every property from the prototype of `Shape`
over to the prototype of `Circle`, but this is needlessly inefficient. Instead,
we use a clever trick, based on the observation that prototypes are ordinary
objects. Since prototypes are objects, they have a prototype as well. We can
thus override the prototype of `Circle` with an object which prototype is the
prototype of `Shape`.

    Circle.prototype = Object.create(Shape.prototype);

Now when JavaScript looks for the method draw on an instance of Circle, it first
looks for it on the object itself. When it cannot find the property there, it
looks for it on the prototype of `Circle`. When it cannot find the property
there either, it looks for it on `Shape`, at which point the lookup succeeds.
The resulting behavior is what we were aiming for.

##Inheritance and Instanceof
The single line of code we added in the previous section solved the problem with
prototypes, but introduced a new problem with the **instanceof** operator.
Consider the following:

    let circle = Circle(2, 3, 5);
    circle instanceof Shape; // => false

Since instances of `Circle` inherit from `Shape`, we definitely want the result
of this expression to be true. To understand why it is not, we need to
understand how **instanceof** works. Every prototype has a `constructor`
property, which is a reference to the constructor for objects with this
prototype. In other words:

    Circle.prototype.constructor == Circle // => true

The **instanceof** operator compares the `constructor` property of the prototype
of the left hand side with that of the right hand side, and returns true if they
are equal. Otherwise, it repeats the comparison for the prototype of the right
hand side, and so on, until either it returns **true**, or the prototype becomes
**null**, in which case it returns **false**. The problem is that when we
overrode the prototype of `Circle` with an object whose prototype is the
prototype of `Shape`, we didn't correctly set its `constructor` property. This
property is set automatically for the `prototype` property of a constructor, but
not for objects created with `Object.create`. The `constructor` property is
supposed to be non-configurable, non-enumberable, and non-writable, so the
correct way to define it is as follows:

    Circle.prototype = Object.create(Shape.prototype, {
        constructor: {
            value: Circle
        }
    });

##Overriding Methods
As a final example, we show how to override the stub implementation of the
method `draw` in `Shape` with a more specialized one in `Circle`. Recall that
JavaScript returns the first property it finds when walking the prototype chain
of an object from the bottom up. Consequently, overriding a method is as simple
as providing a new definition on the prototype of the subclass:

    Circle.prototype.draw = function (ctx) {
        ctx.beginPath();
        ctx.arc(this.x, this.y, this.radius,
                0, 2 * Math.PI, false);
        ctx.fill();
    };

With this definition in place, we get:

    let shape = Shape(2, 3);
    shape.draw(); // Error: not yet implemented 
    let circle = Circle(2, 3, 5);
    circle.draw(); // TypeError: ctx is not defined

which is the behavior we were aiming for.

##Classes in the Add-on SDK
We have shown how to emulate classical inheritance in JavaScript using
constructors and prototypes. However, as we have seen, this takes a significant
amount of boilerplate code. The Add-on SDK team consists of highly trained
professionals, but they are also lazy: that is why the SDK contains a helper
function that handles this boilerplate code for us. It is defined in the module
“core/heritage”:

    const { Class } = require('sdk/core/heritage');

The function `Class` is a meta-constructor: it creates constructors that behave
properly with respect to inheritance. It takes a single argument, which is an
object which properties will be defined on the prototype of the resulting
constructor. The semantics of `Class` are based on what we've learned earlier.
For instance, to define a constructor for a class `Shape` in terms of `Class`,
we can write:

    let Shape = Class({
        initialize: function (x, y) {
            this.x = x;
            this.y = y;
        },
        draw: function () {
            throw new Error("not yet implemented");
        }
    }); 

The property `initialize` is special. When it is present, the call to the
constructor is forwarded to it, as are any arguments passed to it (including the
this object). In effect, initialize specifies the body of the constructor. Note
that the constructors created with `Class` automatically check whether they are
called as constructors, so an explicit check is no longer necessary.

Another special property is `extends`. It specifies the base class from which
this class inherits, if any. `Class` uses this information to automatically set
up the prototype chain of the constructor. If the extends property is omitted,
`Class` itself is used as the base class:

    var shape = new Shape(2, 3);
    shape instanceof Shape; // => true
    shape instanceof Class; // => true

To illustrate the use of the `extends` property, let's redefine the constructor
for the class `Circle` in terms of `Class`:

    var Circle = Class({
        extends: Shape,
        initialize: function(x, y, radius) {
            Shape.prototype.initialize.call(this, x, y);
            this.radius = radius;
        },
        draw: function () {
            context.beginPath();
            context.arc(this.x, this.y, this.radius,
                        0, 2 * Math.PI, false);
            context.fill();
        }
    });

Unlike the definition of `Circle` in the previous section, we no longer have to
override its prototype, or set its `constructor` property. This is all handled
automatically. On the other hand, the call to the constructor for `Shape` still
has to be made explicitly. This is done by forwarding to the initialize method
of the prototype of the base class. Note that this is always safe, even if there
is no `initialize` method defined on the base class: in that case the call is
forwarded to a stub implementation defined on `Class` itself.

The last special property we will look into is `implements`. It specifies a list
of objects, which properties are to be copied to the prototype of the
constructor. Note that only properties defined on the object itself are copied:
properties defined on one of its prototypes are not. This allows objects to
inherit from more than one class. It is not true multiple inheritance, however:
no constructors are called for objects inherited via `implements`, and
**instanceof** only works correctly for classes inherited via `extends`.
