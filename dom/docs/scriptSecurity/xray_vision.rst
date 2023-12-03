Xray Vision
===========

.. container:: summary

   Xray vision helps JavaScript running in a privileged security context
   safely access objects created by less privileged code, by showing the
   caller only the native version of the objects.

Gecko runs JavaScript from a variety of different sources and at a
variety of different privilege levels.

-  The JavaScript code that along with the C++ core, implements the
   browser itself is called *chrome code* and runs using system
   privileges. If chrome-privileged code is compromised, the attacker
   can take over the user's computer.
-  JavaScript loaded from normal web pages is called *content code*.
   Because this code is being loaded from arbitrary web pages, it is
   regarded as untrusted and potentially hostile, both to other websites
   and to the user.
-  As well as these two levels of privilege, chrome code can create
   sandboxes. The security principal  defined for the sandbox determines
   its privilege level. If an
   Expanded Principal is used, the sandbox is granted certain privileges
   over content code and is protected from direct access by content
   code.

| The security machinery in Gecko ensures that there's asymmetric access
  between code at different privilege levels: so for example, content
  code can't access objects created by chrome code, but chrome code can
  access objects created by content.
| However, even the ability to access content objects can be a security
  risk for chrome code. JavaScript's a highly malleable language.
  Scripts running in web pages can add extra properties to DOM objects
  (also known as expando properties)
  and even redefine standard DOM objects to do something unexpected. If
  chrome code relies on such modified objects, it can be tricked into
  doing things it shouldn't.
| For example: ``window.confirm()`` is a DOM
  API that's supposed to ask the user to confirm an action, and return a
  boolean depending on whether they clicked "OK" or "Cancel". A web page
  could redefine it to return ``true``:

.. code:: JavaScript

   window.confirm = function() {
     return true;
   }

Any privileged code calling this function and expecting its result to
represent user confirmation would be deceived. This would be very naive,
of course, but there are more subtle ways in which accessing content
objects from chrome can cause security problems.

| This is the problem that Xray vision is designed to solve. When a
  script accesses an object using Xray vision it sees only the native
  version of the object. Any expandos are invisible, and if any
  properties of the object have been redefined, it sees the original
  implementation, not the redefined version.
| So in the example above, chrome code calling the content's
  ``window.confirm()`` would get the original version of ``confirm()``,
  not the redefined version.

.. note::

   It's worth emphasizing that even if content tricks chrome into
   running some unexpected code, that code does not run with chrome
   privileges. So this is not a straightforward privilege escalation
   attack, although it might lead to one if the chrome code is
   sufficiently confused.

.. _How_you_get_Xray_vision:

How you get Xray vision
-----------------------

Privileged code automatically gets Xray vision whenever it accesses
objects belonging to less-privileged code. So when chrome code accesses
content objects, it sees them with Xray vision:

.. code:: JavaScript

   // chrome code
   var transfer = gBrowser.contentWindow.confirm("Transfer all my money?");
   // calls the native implementation

.. note::

   Note that using window.confirm() would be a terrible way to implement
   a security policy, and is only shown here to illustrate how Xray
   vision works.

.. _Waiving_Xray_vision:

Waiving Xray vision
-------------------

| Xray vision is a kind of security heuristic, designed to make most
  common operations on untrusted objects simple and safe. However, there
  are some operations for which they are too restrictive: for example,
  if you need to see expandos on DOM objects. In cases like this you can
  waive Xray protection, but then you can no longer rely on any
  properties or functions being, or doing, what you expect. Any of them,
  even setters and getters, could have been redefined by untrusted code.
| To waive Xray vision for an object you can use
  Components.utils.waiveXrays(object),
  or use the object's ``wrappedJSObject`` property:

.. code:: JavaScript

   // chrome code
   var waivedWindow = Components.utils.waiveXrays(gBrowser.contentWindow);
   var transfer = waivedWindow.confirm("Transfer all my money?");
   // calls the redefined implementation

.. code:: JavaScript

   // chrome code
   var waivedWindow = gBrowser.contentWindow.wrappedJSObject;
   var transfer = waivedWindow.confirm("Transfer all my money?");
   // calls the redefined implementation

Waivers are transitive: so if you waive Xray vision for an object, then
you automatically waive it for all the object's properties. For example,
``window.wrappedJSObject.document`` gets you the waived version of
``document``.

To undo the waiver again, call Components.utils.unwaiveXrays(waivedObject):

.. code:: JavaScript

   var unwaived = Components.utils.unwaiveXrays(waivedWindow);
   unwaived.confirm("Transfer all my money?");
   // calls the native implementation

.. _Xrays_for_DOM_objects:

Xrays for DOM objects
---------------------

The primary use of Xray vision is for DOM objects: that is, the
objects that represent parts of the web page.

In Gecko, DOM objects have a dual representation: the canonical
representation is in C++, and this is reflected into JavaScript for the
benefit of JavaScript code. Any modifications to these objects, such as
adding expandos or redefining standard properties, stays in the
JavaScript reflection and does not affect the C++ representation.

The dual representation enables an elegant implementation of Xrays: the
Xray just directly accesses the C++ representation of the original
object, and doesn't go to the content's JavaScript reflection at all.
Instead of filtering out modifications made by content, the Xray
short-circuits the content completely.

This also makes the semantics of Xrays for DOM objects clear: they are
the same as the DOM specification, since that is defined using the
`WebIDL <http://www.w3.org/TR/WebIDL/>`__, and the WebIDL also defines
the C++ representation.

.. _Xrays_for_JavaScript_objects:

Xrays for JavaScript objects
----------------------------

Until recently, built-in JavaScript objects that are not part of the
DOM, such as
``Date``, ``Error``, and ``Object``, did not get Xray vision when
accessed by more-privileged code.

Most of the time this is not a problem: the main concern Xrays solve is
with untrusted web content manipulating objects, and web content is
usually working with DOM objects. For example, if content code creates a
new ``Date`` object, it will usually be created as a property of a DOM
object, and then it will be filtered out by the DOM Xray:

.. code:: JavaScript

   // content code

   // redefine Date.getFullYear()
   Date.prototype.getFullYear = function() {return 1000};
   var date = new Date();

.. code:: JavaScript

   // chrome code

   // contentWindow is an Xray, and date is an expando on contentWindow
   // so date is filtered out
   gBrowser.contentWindow.date.getFullYear()
   // -> TypeError: gBrowser.contentWindow.date is undefined

The chrome code will only even see ``date`` if it waives Xrays, and
then, because waiving is transitive, it should expect to be vulnerable
to redefinition:

.. code:: JavaScript

   // chrome code

   Components.utils.waiveXrays(gBrowser.contentWindow).date.getFullYear();
   // -> 1000

However, there are some situations in which privileged code will access
JavaScript objects that are not themselves DOM objects and are not
properties of DOM objects. For example:

-  the ``detail`` property of a CustomEvent fired by content could be a JavaScript
   Object or Date as well as a string or a primitive
-  the return value of ``evalInSandbox()`` and any properties attached to the
   ``Sandbox`` object may be pure JavaScript objects

Also, the WebIDL specifications are starting to use JavaScript types
such as ``Date`` and ``Promise``: since WebIDL definition is the basis
of DOM Xrays, not having Xrays for these JavaScript types starts to seem
arbitrary.

So, in Gecko 31 and 32 we've added Xray support for most JavaScript
built-in objects.

Like DOM objects, most JavaScript built-in objects have an underlying
C++ state that is separate from their JavaScript representation, so the
Xray implementation can go straight to the C++ state and guarantee that
the object will behave as its specification defines:

.. code:: JavaScript

   // chrome code

   var sandboxScript = 'Date.prototype.getFullYear = function() {return 1000};' +
                       'var date = new Date(); ';

   var sandbox = Components.utils.Sandbox("https://example.org/");
   Components.utils.evalInSandbox(sandboxScript, sandbox);

   // Date objects are Xrayed
   console.log(sandbox.date.getFullYear());
   // -> 2014

   // But you can waive Xray vision
   console.log(Components.utils.waiveXrays(sandbox.date).getFullYear());
   // -> 1000

.. note::

   To test out examples like this, you can use the Scratchpad in
   browser context
   for the code snippet, and the Browser Console to see the expected
   output.

   Because code running in Scratchpad's browser context has chrome
   privileges, any time you use it to run code, you need to understand
   exactly what the code is doing. That includes the code samples in
   this article.

.. _Xray_semantics_for_Object_and_Array:

Xray semantics for Object and Array
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The exceptions are ``Object``
and ``Array``: their interesting state is in JavaScript, not C++. This
means that the semantics of their Xrays have to be independently
defined: they can't simply be defined as "the C++ representation".

The aim of Xray vision is to make most common operations simple and
safe, avoiding the need to access the underlying object except in more
involved cases. So the semantics defined for ``Object`` and ``Array``
Xrays aim to make it easy for privileged code to treat untrusted objects
like simple dictionaries.

Any value properties
of the object are visible in the Xray. If the object has properties
which are themselves objects, and these objects are same-origin with the
content, then their value properties are visible as well.

There are two main sorts of restrictions:

-  First, the chrome code might expect to rely on the prototype's
   integrity, so the object's prototype is protected:

   -  the Xray has the standard ``Object`` or ``Array`` prototype,
      without any modifications that content may have done to that
      prototype. The Xray always inherits from this standard prototype,
      even if the underlying instance has a different prototype.
   -  if a script has created a property on an object instance that
      shadows a property on the prototype, the shadowing property is not
      visible in the Xray

-  Second, we want to prevent the chrome code from running content code,
   so functions and accessor properties
   of the object are not visible in the Xray.

These rules are demonstrated in the script below, which evaluates a
script in a sandbox, then examines the object attached to the sandbox.

.. note::

   To test out examples like this, you can use the Scratchpad in
   browser context  for the code snippet, and the Browser Console
   to see the expected output.

   Because code running in Scratchpad's browser context has chrome
   privileges, any time you use it to run code, you need to understand
   exactly what the code is doing. That includes the code samples in
   this article.

.. code:: JavaScript

   /*
   The sandbox script:
   * redefines Object.prototype.toSource()
   * creates a Person() constructor that:
     * defines a value property "firstName" using assignment
     * defines a value property which shadows "constructor"
     * defines a value property "address" which is a simple object
     * defines a function fullName()
   * using defineProperty, defines a value property on Person "lastName"
   * using defineProperty, defines an accessor property on Person "middleName",
   which has some unexpected accessor behavior
   */

   var sandboxScript = 'Object.prototype.toSource = function() {'+
                       '  return "not what you expected?";' +
                       '};' +
                       'function Person() {' +
                       '  this.constructor = "not a constructor";' +
                       '  this.firstName = "Joe";' +
                       '  this.address = {"street" : "Main Street"};' +
                       '  this.fullName = function() {' +
                       '    return this.firstName + " " + this.lastName;'+
                       '  };' +
                       '};' +
                       'var me = new Person();' +
                       'Object.defineProperty(me, "lastName", {' +
                       '  enumerable: true,' +
                       '  configurable: true,' +
                       '  writable: true,' +
                       '  value: "Smith"' +
                       '});' +
                       'Object.defineProperty(me, "middleName", {' +
                       '  enumerable: true,' +
                       '  configurable: true,' +
                       '  get: function() { return "wait, is this really a getter?"; }' +
                       '});';

   var sandbox = Components.utils.Sandbox("https://example.org/");
   Components.utils.evalInSandbox(sandboxScript, sandbox);

   // 1) trying to access properties in the prototype that have been redefined
   // (non-own properties) will show the original 'native' version
   // note that functions are not included in the output
   console.log("1) Property redefined in the prototype:");
   console.log(sandbox.me.toSource());
   // -> "({firstName:"Joe", address:{street:"Main Street"}, lastName:"Smith"})"

   // 2) trying to access properties on the object that shadow properties
   // on the prototype will show the original 'native' version
   console.log("2) Property that shadows the prototype:");
   console.log(sandbox.me.constructor);
   // -> function()

   // 3) value properties defined by assignment to this are visible:
   console.log("3) Value property defined by assignment to this:");
   console.log(sandbox.me.firstName);
   // -> "Joe"

   // 4) value properties defined using defineProperty are visible:
   console.log("4) Value property defined by defineProperty");
   console.log(sandbox.me.lastName);
   // -> "Smith"

   // 5) accessor properties are not visible
   console.log("5) Accessor property");
   console.log(sandbox.me.middleName);
   // -> undefined

   // 6) accessing a value property of a value-property object is fine
   console.log("6) Value property of a value-property object");
   console.log(sandbox.me.address.street);
   // -> "Main Street"

   // 7) functions defined on the sandbox-defined object are not visible in the Xray
   console.log("7) Call a function defined on the object");
   try {
     console.log(sandbox.me.fullName());
   }
   catch (e) {
     console.error(e);
   }
   // -> TypeError: sandbox.me.fullName is not a function

   // now with waived Xrays
   console.log("Now with waived Xrays");

   console.log("1) Property redefined in the prototype:");
   console.log(Components.utils.waiveXrays(sandbox.me).toSource());
   // -> "not what you expected?"

   console.log("2) Property that shadows the prototype:");
   console.log(Components.utils.waiveXrays(sandbox.me).constructor);
   // -> "not a constructor"

   console.log("3) Accessor property");
   console.log(Components.utils.waiveXrays(sandbox.me).middleName);
   // -> "wait, is this really a getter?"

   console.log("4) Call a function defined on the object");
   console.log(Components.utils.waiveXrays(sandbox.me).fullName());
   // -> "Joe Smith"
