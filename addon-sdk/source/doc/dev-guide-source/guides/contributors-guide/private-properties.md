<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

#Private Properties

A private property is a property that is only accessible to member
functions of instances of the same class. Unlike other languages, JavaScript
does not have native support for private properties. However, people have come
up with several ways to emulate private properties using existing language
features. We will take a look at two different techniques, using prefixes, and
closures, respectively.

Prefixes and closures both have drawbacks in that they are either not
restrictive enough or too restrictive, respectively. We will therefore introduce
a third technique, based on the use of WeakMaps, that solves both these
problems. Note, however, that WeakMaps might not be supported by all
implementations yet. Next, we generalize the idea of using WeakMaps from
associating one or more private properties with an object to associating one or
more namespaces with each object. A namespace is simply an object on which one
or more private properties are defined.

The SDK uses namespaces internally to implement private properties. The last
section explains how to work with the particular namespace implementation used
by the SDK. It is possible to read this section on its own, but to fully
appreciate how namespaces work, and the problem they are supposed to solve, it
is recommended that you read the entire article.

##Using Prefixes

A common technique to implement private properties is to prefix each private
property name with an underscore. Consider the following example:

    function Point(x, y) {
        this._x = x;
        this._y = y;
    }

The properties `_x` and `_y` are private, and should only be accessed by member
functions.

To make a private property readable/writable from any function, it is common to
define a getter/setter function for the property, respectively:

    Point.prototype.getX = function () {
        return this._x;
    };

    Point.prototype.setX = function (x) {
        this._x = x;
    };

    Point.prototype.getY = function () {
        return this._y;
    };

    Point.prototype.setY = function (y) {
        this._y = y;
    };

The above technique is simple, and clearly expresses our intent. However, the
use of an underscore prefix is just a coding convention, and is not enforced by
the language: there is nothing to prevent a user from directly accessing a
property that is supposed to be private.

##Using Closures
Another common technique is to define private properties as variables, and their
getter and/or setter function as a closure over these variables:

    function Point(_x, _y) {
        this.getX = function () {
            return _x;
        };

        this.setX = function (x) {
            _x = x;
        };

        this.getY = function () {
            return _y;
        };

        this.setY = function (y) {
            _y = y;
        };
    }

Note that this technique requires member functions that need access to private
properties to be defined on the object itself, instead of its prototype. This is
slightly less efficient, but this is probably acceptable.

The advantage of this technique is that it offers more protection: there is no
way for the user to access a private property except by using its getter and/or
setter function. However, the use of closures makes private properties too
restrictive: since there is no way to access variables in one closure from
within another closure, there is no way for objects of the same class to access
each other's private properties.

##Using WeakMaps

The techniques we've seen so far ar either not restrictive enough (prefixes) or
too restrictive (closures). Until recently, a technique that solves both these
problems didn't exist. That changed with the introduction of WeakMaps. WeakMaps
were introduced to JavaScript in ES6, and have recently been implemented in
SpiderMonkey. Before we explain how WeakMaps work, let's take a look at how
ordinary objects can be used as hash maps, by creating a simple image cache:

    let images = {};

    function getImage(name) {
        let image = images[name];
        if (!image) {
            image = loadImage(name);
            images[name] = image;
        }
        return image;
    }

Now suppose we want to associate a thumbnail with each image. Moreover, we want
to create each thumbnail lazily, when it is first required:

    function getThumbnail(image) {
        let thumbnail = image._thumbnail;
        if (!thumbnail) {
            thumbnail = createThumbnail(image);
            image._thumbnail = thumbnail;
        }
        return thumbnail;
    }

This approach is straightforward, but relies on the use of prefixes. A better
approach would be to store thumbnails in their own, separate hash map:

    let thumbnails = {};

    function getThumbnail(image) {
        let thumbnail = thumbnails[image];
        if (!thumbnail) {
            thumbnail = createThumbnail(image);
            thumbnails[image] = thumbnail;
        }
        return thumbnail;
    }

There are two problems with the above approach. First, it's not possible to use
objects as keys. When an object is used as a key, it is converted to a string
using its toString method. To make the above code work, we'd have to associate a
unique identifier with each image, and override its `toString` method. The
second problem is more severe: the thumbnail cache maintains a strong reference
to each thumbnail object, so they will never be freed, even when their
corresponding image has gone out of scope. This is a memory leak waiting to
happen.

The above two problems are exactly what WeakMaps were designed to solve. A
WeakMap is very similar to an ordinary hash map, but differs from it in two
crucial ways:

1. It can use ordinary objects as keys
2. It does not maintain a strong reference to its values

To understand how WeakMaps are used in practice, let's rewrite the thumbnail
cache using WeakMaps:

    let thumbnails = new WeakMap();

    function getThumbnail(image) {
        let thumbnail = thumbnails.get(image);
        if (!thumbnail) {
            thumbnail = createThumbnail(image);
            thumbnails.set(image, thumbnail);
        }
        return thumbnail;
    }

This version suffers of none of the problems we mentioned earlier. When a
thumbnail's image goes out of scope, the WeakMap ensures that its entry in the
thumbnail cache will eventually be garbage collected. As a final caveat: the
image cache we created earlier suffers from the same problem, so for the above
code to work properly, we'd have to rewrite the image cache using WeakMaps, too.

#From WeakMaps to Namespaces
In the previous section we used a WeakMap to associate a private property with
each object. Note that we need a separate WeakMap for each private property.
This is cumbersome if the number of private properties becomes large. A better
solution would be to store all private properties on a single object, called a
namespace, and then store the namespace as a private property on the original
object. Using namespaces, our earlier example can be rewritten as follows:

    let map = new WeakMap();

    let internal = function (object) {
        if (!map.has(object))
            map.set(object, {});
        return map.get(object);
    }

    function Point(x, y) {
        internal(this).x = x;
        internal(this).y = y;
    }

    Point.prototype.getX = function () {
        return internal(shape).x;
    };

    Point.prototype.setX = function (x) {
        internal(shape).x = x;
    };

    Point.prototype.getY = function () {
        return internal(shape).y;
    };

    Point.prototype.setY = function () {
        internal(shape).y = y;
    };

The only way for a function to access the properties `x` and `y` is if it has a
reference to an instance of `Point` and its `internal` namespace. By keeping the
namespace hidden from all functions except members of `Point`, we have
effectively implemented private properties. Moreover, because members of `Point`
have a reference to the `internal` namespace, they can access private properties
on other instances of `Point`.

##Namespaces in the Add-on SDK
The Add-on SDK is built on top of XPCOM, the interface between JavaScript and
C++ code. Since XPCOM allows the user to do virtually anything, security is very
important. Among other things, we don't want add-ons to be able to access
variables that are supposed to be private. The SDK uses namespaces internally to
ensure this. As always with code that is heavily reused, the SDK defines a
helper function to create namespaces. It is defined in the module
"core/namespace", and it's usage is straightforward. To illustrate this, let's
reimplement the class `Point` using namespaces:

    const { ns } = require("./core/namespace");

    var internal = ns();

    function Point(x, y) {
        internal(this).x = x;
        internal(this).y = y;
    }

    Point.prototype.getX = function () {
        return internal(shape).x;
    };

    Point.prototype.setX = function (x) {
        internal(shape).x = x;
    };

    Point.prototype.getY = function () {
        return internal(shape).y;
    };

    Point.prototype.setY = function () {
        internal(shape).y = y;
    };

As a final note, the function `ns` returns a namespace that uses the namespace
associated with the prototype of the object as its prototype.
