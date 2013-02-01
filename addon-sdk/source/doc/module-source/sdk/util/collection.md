<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Drew Willcoxon [adw@mozilla.com]  -->
<!-- edited by Noelle Murata [fiveinchpixie@gmail.com]  -->

The `collection` module provides a simple list-like class and utilities for
using it.  A collection is ordered, like an array, but its items are unique,
like a set.

<api name="Collection">
@class
A collection object provides for...in-loop iteration.  Items are yielded in the
order they were added.  For example, the following code...

    var collection = require("sdk/util/collection");
    var c = new collection.Collection();
    c.add(1);
    c.add(2);
    c.add(3);
    for (item in c)
      console.log(item);

... would print this to the console:

<pre>
  1
  2
  3
</pre>

Iteration proceeds over a copy of the collection made before iteration begins,
so it is safe to mutate the collection during iteration; doing so does not
affect the results of the iteration.

<api name="Collection">
@constructor
Creates a new collection.  The collection is backed by an array.
@param [array] {array}
If *array* is given, it will be used as the backing array.  This way the caller
can fully control the collection.  Otherwise a new empty array will be used, and
no one but the collection will have access to it.
</api>
<api name="length">
@property {number}
The number of items in the collection array.
</api>
<api name="add">
@method
Adds a single item or an array of items to the collection.  Any items already
contained in the collection are ignored.
@param itemOrItems {object} An item or array of items.
@returns {Collection} The Collection.
</api>
<api name="remove">
@method
Removes a single item or an array of items from the collection.  Any items not
contained in the collection are ignored.
@param itemOrItems {object} An item or array of items.
@returns {Collection} The Collection.
</api>
</api>

<api name="addCollectionProperty">
@function
Adds a collection property to the given object.  Setting the property to a
scalar value empties the collection and adds the value.  Setting it to an array
empties the collection and adds all the items in the array.
@param object {object}
The property will be defined on this object.
@param propName {string}
The name of the property.
@param [backingArray] {array}
If given, this will be used as the collection's backing array.
</api>

