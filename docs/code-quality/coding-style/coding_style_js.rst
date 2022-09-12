=======================
JavaScript Coding style
=======================

Coding style
~~~~~~~~~~~~

`prettier <https://prettier.io/>`_ is the tool used to reformat the JavaScript code.


Methods and functions
~~~~~~~~~~~~~~~~~~~~~

In JavaScript, functions should use camelCase, but should not capitalize
the first letter. Methods should not use the named function expression
syntax, because our tools understand method names:

.. code-block:: cpp

   doSomething: function (aFoo, aBar) {
     ...
   }

In-line functions should have spaces around braces, except before commas
or semicolons:

.. code-block:: cpp

   function valueObject(aValue) { return { value: aValue }; }


JavaScript objects
~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   var foo = { prop1: "value1" };

   var bar = {
     prop1: "value1",
     prop2: "value2"
   };

Constructors for objects should be capitalized and use Pascal Case:

.. code-block:: cpp

   function ObjectConstructor() {
     this.foo = "bar";
   }


Operators
~~~~~~~~~

In JavaScript, overlong expressions not joined by ``&&`` and
``||`` should break so the operator starts on the second line and
starting in the same column as the beginning of the expression in the
first line. This applies to ``?:``, binary arithmetic operators
including ``+``, and member-of operators. Rationale: an operator at the
front of the continuation line makes for faster visual scanning, as
there is no need to read to the end of line. Also there exists a
context-sensitive keyword hazard in JavaScript; see {{bug(442099, "bug",
19)}}, which can be avoided by putting . at the start of a continuation
line, in long member expression.

In JavaScript, ``==`` is preferred to ``===``.

Unary keyword operators, such as ``typeof``, should have their operand
parenthesized; e.g. ``typeof("foo") == "string"``.

Literals
~~~~~~~~

Double-quoted strings (e.g. ``"foo"``) are preferred to single-quoted
strings (e.g. ``'foo'``), in JavaScript, except to avoid escaping
embedded double quotes, or when assigning inline event handlers.


Prefixes
~~~~~~~~

-  k=constant (e.g. ``kNC_child``). Not all code uses this style; some
   uses ``ALL_CAPS`` for constants.
-  g=global (e.g. ``gPrefService``)
-  a=argument (e.g. ``aCount``)

-  JavaScript Specific Prefixes

   -  \_=member (variable or function) (e.g. ``_length`` or
      ``_setType(aType)``)
   -  k=enumeration value (e.g. ``const kDisplayModeNormal = 0``)
   -  on=event handler (e.g. ``function onLoad()``)
   -  Convenience constants for interface names should be prefixed with
      ``nsI``:

      .. code-block:: javascript

         const nsISupports = Components.interfaces.nsISupports;
         const nsIWBN = Components.interfaces.nsIWebBrowserNavigation;



Other advices
~~~~~~~~~~~~~

-  Do not compare ``x == true`` or ``x == false``. Use ``(x)`` or
   ``(!x)`` instead. ``x == true``, is certainly different from if
   ``(x)``! Compare objects to ``null``, numbers to ``0`` or strings to
   ``""``, if there is chance for confusion.
-  Make sure that your code doesn't generate any strict JavaScript
   warnings, such as:

   -  Duplicate variable declaration.
   -  Mixing ``return;`` with ``return value;``
   -  Undeclared variables or members. If you are unsure if an array
      value exists, compare the index to the array's length. If you are
      unsure if an object member exists, use ``"name"`` in ``aObject``,
      or if you are expecting a particular type you may use
      ``typeof(aObject.name) == "function"`` (or whichever type you are
      expecting).

-  Use ``['value1, value2']`` to create a JavaScript array in preference
   to using
   ``new {{JSxRef("Array", "Array", "Syntax", 1)}}(value1, value2)``
   which can be confusing, as ``new Array(length)`` will actually create
   a physically empty array with the given logical length, while
   ``[value]`` will always create a 1-element array. You cannot actually
   guarantee to be able to preallocate memory for an array.
-  Use ``{ member: value, ... }`` to create a JavaScript object; a
   useful advantage over ``new {{JSxRef("Object", "Object", "", 1)}}()``
   is the ability to create initial properties and use extended
   JavaScript syntax, to define getters and setters.
-  If having defined a constructor you need to assign default
   properties, it is preferred to assign an object literal to the
   prototype property.
-  Use regular expressions, but use wisely. For instance, to check that
   ``aString`` is not completely whitespace use
   ``/\S/.{{JSxRef("RegExp.test", "test(aString)", "", 1)}}``. Only use
   {{JSxRef("String.search", "aString.search()")}} if you need to know
   the position of the result, or {{JSxRef("String.match",
   "aString.match()")}} if you need to collect matching substrings
   (delimited by parentheses in the regular expression). Regular
   expressions are less useful if the match is unknown in advance, or to
   extract substrings in known positions in the string. For instance,
   {{JSxRef("String.slice", "aString.slice(-1)")}} returns the last
   letter in ``aString``, or the empty string if ``aString`` is empty.
