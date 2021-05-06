WebIDL bindings
===============

.. note::

   Need to document the setup for indexed and named
   setters/creators/deleters.

The `WebIDL <https://heycam.github.io/webidl/>`__ bindings are generated
at build time based on two things: the actual WebIDL file and a
configuration file that lists some metadata about how the WebIDL should
be reflected into Gecko-internal code.

All WebIDL files should be placed in
```dom/webidl`` <https://searchfox.org/mozilla-central/source/dom/webidl/>`__
and added to the list in the
`moz.build <https://searchfox.org/mozilla-central/source/dom/webidl/moz.build>`__
file in that directory.

Note that if you're adding new interfaces, then the test at
``dom/tests/mochitest/general/test_interfaces.html`` will most likely
fail. This is a signal that you need to get a review from a `DOM
peer <https://wiki.mozilla.org/Modules/All#Document_Object_Model>`__.
Resist the urge to just add your interfaces to the
`moz.build <https://searchfox.org/mozilla-central/source/dom/webidl/moz.build>`__ list
without the review; it will just annoy the DOM peers and they'll make
you get the review anyway.

The configuration file, ``dom/bindings/Bindings.conf``, is basically a
Python dict that maps interface names to information about the
interface, called a *descriptor*. There are all sorts of possible
options here that handle various edge cases, but most descriptors can be
very simple.

All the generated code is placed in the ``mozilla::dom`` namespace. For
each interface, a namespace whose name is the name of the interface with
``Binding`` appended is created, and all the things pertaining to that
interface's binding go in that namespace.

There are various helper objects and utility methods in
```dom/bindings`` <https://searchfox.org/mozilla-central/source/dom/bindings>`__
that are also all in the ``mozilla::dom`` namespace and whose headers
are all exported into ``mozilla/dom`` (placed in
``$OBJDIR/dist/include`` by the build process).

.. _Adding_WebIDL_bindings_to_a_class:

Adding WebIDL bindings to a class
---------------------------------

To add a WebIDL binding for interface ``MyInterface`` to a class
``mozilla::dom::MyInterface`` that's supposed to implement that
interface, you need to do the following:

#. If your interface doesn't inherit from any other interfaces, inherit
   from ``nsWrapperCache`` and hook up the class to the cycle collector
   so it will trace the wrapper cache properly. Note that you may not
   need to do this if your objects can only be created, never gotten
   from other objects. If you also inherit from ``nsISupports``, make
   sure the ``nsISupports`` comes before the ``nsWrapperCache`` in your
   list of parent classes. If your interface *does* inherit from another
   interface, just inherit from the C++ type that the other interface
   corresponds to.

   If you do need to hook up cycle collection, it will look like this in
   the common case of also inheriting from nsISupports:

   .. code:: brush:

      // Add strong pointers your class holds here. If you do, change to using
      // NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE.
      NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(MyClass)
      NS_IMPL_CYCLE_COLLECTING_ADDREF(MyClass)
      NS_IMPL_CYCLE_COLLECTING_RELEASE(MyClass)
      NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MyClass)
        NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
        NS_INTERFACE_MAP_ENTRY(nsISupports)
      NS_INTERFACE_MAP_END

#. If your class doesn't inherit from a class that implements
   ``GetParentObject``, then add a function of that name that, for a
   given instance of your class, returns the same object every time
   (unless you write explicit code that handles your parent object
   changing by reparenting JS wrappers, as nodes do). The idea is that
   walking the ``GetParentObject`` chain will eventually get you to a
   Window, so that every WebIDL object is associated with a particular
   Window.
   For example, ``nsINode::GetParentObject`` returns the node's owner
   document. The return type of ``GetParentObject`` doesn't matter other
   than it must either singly-inherit from ``nsISupports`` or have a
   corresponding
   ```ToSupports`` <https://searchfox.org/mozilla-central/search?q=ToSupports&path=&case=true&regexp=false>`__
   method that can produce an ``nsISupports`` from it. (This allows the
   return value to be implicitly converted to a
   `ParentObject <https://searchfox.org/mozilla-central/search?q=ParentObject&path=&case=true&regexp=false>`__
   instance by the compiler via one of that class's non-explicit
   constructors.)
   If many instances of ``MyInterface`` are expected to be created
   quicky, the return value of ``GetParentObject`` should itself inherit
   from ``nsWrapperCache`` for optimal performance. Returning null from
   ``GetParentObject`` is allowed in situations in which it's OK to
   associate the resulting object with a random global object for
   security purposes; this is not usually ok for things that are exposed
   to web content. Again, if you do not need wrapper caching you don't
   need to do this.  The actual type returned from ``GetParentObject``
   must be defined in a header included from your implementation header,
   so that this type's definition is visible to the binding code.

#. Add the WebIDL for ``MyInterface`` in ``dom/webidl`` and to the list
   in ``dom/webidl/moz.build``.

#. Add an entry to ``dom/bindings/Bindings.conf`` that sets some basic
   information about the implementation of the interface. If the C++
   type is not ``mozilla::dom::MyInterface``, you need to set the
   ``'nativeType'`` to the right type. If the type is not in the header
   file one gets by replacing '::' with '/' and appending '``.h``', then
   add a corresponding ``'headerFile'`` annotation (or
   ```HeaderFile`` <#HeaderFile>`__ annotation to the .webidl file). If
   you don't have to set any annotations, then you don't need to add an
   entry either and the code generator will simply assume the defaults
   here.  Note that using a ``'headerFile'`` annotation is generally not
   recommended.  If you do use it, you will need to make sure your
   header includes all the headers needed for your ```Func`` <#Func>`__
   annotations.

#. Add external interface entries to ``Bindings.conf`` for whatever
   non-WebIDL interfaces your new interface has as arguments or return
   values.

#. Implement a ``WrapObject`` override on ``mozilla::dom::MyInterface``
   that just calls through to
   ``mozilla::dom::MyInterface_Binding::Wrap``. Note that if your C++
   type is implementing multiple distinct Web IDL interfaces, you need
   to choose which ``mozilla::dom::MyInterface_Binding::Wrap`` to call
   here. See ``AudioContext::WrapObject``, for example.

#. Expose whatever methods the interface needs on
   ``mozilla::dom::MyInterface``. These can be inline, virtual, have any
   calling convention, and so forth, as long as they have the right
   argument types and return types. You can see an example of what the
   function declarations should look like by running
   ``mach webidl-example MyInterface``. This will produce two files in
   ``dom/bindings`` in your objdir: ``MyInterface-example.h`` and
   ``MyInterface-example.cpp``, which show a basic implementation of the
   interface using a class that inherits from ``nsISupports`` and has a
   wrapper cache.

See this `sample patch that migrates window.performance.\* to WebIDL
bindings <https://hg.mozilla.org/mozilla-central/rev/dd08c10193c6>`__.

.. note::

   If your object can only be reflected into JS by creating it, not by
   retrieving it from somewhere, you can skip steps 1 and 2 above and
   instead add ``'wrapperCache': False`` to your descriptor. You will
   need to flag the functions that return your object as
   ```[NewObject]`` <https://heycam.github.io/webidl/#NewObject>`__ in
   the WebIDL. If your object is not refcounted then the return value of
   functions that return it should return an nsAutoPtr.

.. _C_reflections_of_WebIDL_constructs:

C++ reflections of WebIDL constructs
------------------------------------

.. _C_reflections_of_WebIDL_operations_methods:

C++ reflections of WebIDL operations (methods)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A WebIDL operation is turned into a method call on the underlying C++
object. The return type and argument types are determined `as described
below <#typemapping>`__. In addition to those, all `methods that are
allowed to throw <#Throws>`__ will get an ``ErrorResult&`` argument
appended to their argument list. Non-static methods that use certain
WebIDL types like ``any`` or ``object`` will get a ``JSContext*``
argument prepended to the argument list. Static methods will be passed a
```const GlobalObject&`` <#GlobalObject>`__ for the relevant global and
can get a ``JSContext*`` by calling ``Context()`` on it.

The name of the C++ method is simply the name of the WebIDL operation
with the first letter converted to uppercase.

WebIDL overloads are turned into C++ overloads: they simply call C++
methods with the same name and different signatures.

For example, this webidl:

.. code:: notranslate

   interface MyInterface
   {
     void doSomething(long number);
     double doSomething(MyInterface? otherInstance);

     [Throws]
     MyInterface doSomethingElse(optional long maybeNumber);
     [Throws]
     void doSomethingElse(MyInterface otherInstance);

     void doTheOther(any something);

     void doYetAnotherThing(optional boolean actuallyDoIt = false);

     static void staticOperation(any arg);
   };

will require these method declarations:

.. code:: brush:

   class MyClass
   {
     void DoSomething(int32_t aNumber);
     double DoSomething(MyClass* aOtherInstance);

     already_AddRefed<MyInterface> DoSomethingElse(Optional<int32_t> aMaybeNumber,
                                                   ErrorResult& rv);
     void DoSomethingElse(MyClass& aOtherInstance, ErrorResult& rv);

     void DoTheOther(JSContext* cx, JS::Value aSomething);

     void DoYetAnotherThing(bool aActuallyDoIt);

     static void StaticOperation(const GlobalObject& aGlobal, JS::Value aSomething);
   }

.. _C_reflections_of_WebIDL_attributes:

C++ reflections of WebIDL attributes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A WebIDL attribute is turned into a pair of method calls for the getter
and setter on the underlying C++ object. A readonly attribute only has a
getter and no setter.

The getter's name is the name of the attribute with the first letter
converted to uppercase. This has ``Get`` prepended to it if any of these
conditions hold:

#. The type of the attribute is nullable.
#. The getter can throw.
#. The return value of the attribute is returned via an out parameter in
   the C++.

The method signature for the getter looks just like an operation with no
arguments and the attribute's type as the return type.

The setter's name is ``Set`` followed by the name of the attribute with
the first letter converted to uppercase. The method signature looks just
like an operation with a void return value and a single argument whose
type is the attribute's type.

.. _C_reflections_of_WebIDL_constructors:

C++ reflections of WebIDL constructors
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A WebIDL constructor is turned into a static class method named
``Constructor``. The arguments of this method will be the arguments of
the WebIDL constructor, with a
```const GlobalObject&`` <#GlobalObject>`__ for the relevant global
prepended. For the non-worker case, the global is typically the inner
window for the DOM Window the constructor function is attached to. If a
``JSContext*`` is also needed due to some of the argument types, it will
come after the global. The return value of the constructor for
``MyInterface`` is exactly the same as that of a method returning an
instance of ``MyInterface``. Constructors are always allowed to throw.

For example, this IDL:

.. code:: notranslate

   [Constructor,
    Constructor(unsigned long someNumber)]
   interface MyInterface
   {
   };

will require the following declarations in ``MyClass``:

.. code:: brush:

   class MyClass {
     // Various nsISupports stuff or whatnot
     static
     already_AddRefed<MyClass> Constructor(const GlobalObject& aGlobal,
                                           ErrorResult& rv);
     static
     already_AddRefed<MyClass> Constructor(const GlobalObject& aGlobal,
                                           uint32_t aSomeNumber,
                                           ErrorResult& rv);
   };

.. _typemapping:

C++ reflections of WebIDL types
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The exact C++ representation for WebIDL types can depend on the precise
way that they're being used (e.g., return values, arguments, and
sequence or dictionary members might all have different
representations).

Unless stated otherwise, a type only has one representation. Also,
unless stated otherwise, nullable types are represented by wrapping
```Nullable<>`` <#Nullable>`__ around the base type.

In all cases, optional arguments which do not have a default value are
represented by wrapping ```const Optional<>&`` <#Optional>`__ around the
representation of the argument type. If the argument type is a C++
reference, it will also become a ```NonNull<>`` <#NonNull>`__ around the
actual type of the object in the process. Optional arguments which do
have a default value are just represented by the argument type itself,
set to the default value if the argument was not in fact passed in.

Variadic WebIDL arguments are treated as a
```const Sequence<>&`` <#Sequence>`__ around the actual argument type.

Here's a table, see the specific sections below for more details and
explanations.

.. list-table:: Type reflection
   :header-rows: 1
   :stub-columns: 1

   * - WebIDL Type
     - Argument Type
     - Return Type
     - Dictionary/Member Type
   * - any
     - ``JS::Handle<JS::Value>``
     - ``JS::MutableHandle<JS::Value>``
     - ``JS::Value``
   * - boolean
     - ``bool``
     - ``bool``
     - ``bool``
   * - byte
     - ``int8_t``
     - ``int8_t``
     - ``int8_t``
   * - ByteString
     - ``const nsACString&``
     - | ``nsCString&`` *(outparam)*
       | ``nsACString&`` *(outparam)*
     - ``nsCString``
   * - Date
     -
     - 
     - ``mozilla::dom::Date``
   * - DOMString
     - ``const nsAString&``
     - | ```mozilla::dom::DOMString&`` <#DOMString-helper>`__ *(outparam)*
       | ``nsAString&`` *(outparam)*
       | ``nsString&`` *(outparam)*
     - ``nsString``
   * - UTF8String
     - ``const nsACString&``
     - ``nsACString&`` *(outparam)*
     - ``nsCString``
   * - double
     - ``double``
     - ``double``
     - ``double``
   * - float
     - ``float``
     - ``float``
     - ``float``
   * - interface: non-nullable
     - ``Foo&``
     - ``already_addRefed<Foo>``
     - ```OwningNonNull<Foo>`` <#OwningNonNull>`__
   * - interface: nullable
     - ``Foo*``
     - | ``already_addRefed<Foo>``
       | ``Foo*``
     - ``RefPtr<Foo>``
   * - long
     - ``int32_t``
     - ``int32_t``
     - ``int32_t``
   * - long long
     - ``int64_t``
     - ``int64_t``
     - ``int64_t``
   * - object
     - ``JS::Handle<JSObject*>``
     - ``JS::MutableHandle<JSObject*>``
     - ``JSObject*``
   * - octet
     - ``uint8_t``
     - ``uint8_t``
     - ``uint8_t``
   * - sequence
     - ```const Sequence<T>&`` <#Sequence>`__
     - ``nsTArray<T>&`` *(outparam)*
     - 
   * - short
     - ``int16_t``
     - ``int16_t``
     - ``int16_t``
   * - unrestricted double
     - ``double``
     - ``double``
     - ``double``
   * - unrestricted float
     - ``float``
     - ``float``
     - ``float``
   * - unsigned long
     - ``uint32_t``
     - ``uint32_t``
     - ``uint32_t``
   * - unsigned long long
     - ``uint64_t``
     - ``uint64_t``
     - ``uint64_t``
   * - unsigned short
     - ``uint16_t``
     - ``uint16_t``
     - ``uint16_t``
   * - USVString
     - ``const nsAString&``
     - | ```mozilla::dom::DOMString`` <#DOMString-helper>`__ *(outparam)*
       | ``nsAString&`` *(outparam)*
       | ``nsString&`` *(outparam)*
     - ``nsString``

``any``
^^^^^^^

``any`` is represented in three different ways, depending on use:

-  ``any`` arguments become ``JS::Handle<JS::Value>``.  They will be in
   the compartment of the passed-in JSContext.
-  ``any`` return values become a ``JS::MutableHandle<JS::Value>`` out
   param appended to the argument list. This comes after all IDL
   arguments, but before the ``ErrorResult&``, if any, for the method.
   The return value is allowed to be in any compartment; bindings will
   wrap it into the context compartment as needed.
-  ``any`` dictionary members and sequence elements become
   ``JS::Value``. The dictionary members and sequence elements are
   guaranteed to be marked by whomever puts the sequence or dictionary
   on the stack, using ``SequenceRooter`` and ``DictionaryRooter``.

Methods using ``any`` always get a ``JSContext*`` argument.

For example, this WebIDL:

.. code:: notranslate

   interface Test {
     attribute any myAttr;
     any myMethod(any arg1, sequence<any> arg2, optional any arg3);
   };

will correspond to these C++ function declarations:

.. code:: brush:

   void MyAttr(JSContext* cx, JS::MutableHandle<JS::Value> retval);
   void SetMyAttr(JSContext* cx, JS::Handle<JS::Value> value);
   void MyMethod(JSContext* cx, JS::Handle<JS::Value> arg1,
                 const Sequence<JS::Value>& arg2,
                 const Optional<JS::Handle<JS::Value> >& arg3,
                 JS::MutableHandle<JS::Value> retval);

``boolean``
^^^^^^^^^^^

The ``boolean`` WebIDL type is represented as a C++ ``bool``.

For example, this WebIDL:

.. code:: notranslate

   interface Test {
     attribute boolean myAttr;
     boolean myMethod(optional boolean arg);
   };

will correspond to these C++ function declarations:

.. code:: brush:

   bool MyAttr();
   void SetMyAttr(bool value);
   JS::Value MyMethod(const Optional<bool>& arg);

.. _Integer_types:

Integer types
^^^^^^^^^^^^^

Integer WebIDL types are mapped to the corresponding C99 stdint types.

For example, this WebIDL:

.. code:: notranslate

   interface Test {
     attribute short myAttr;
     long long myMethod(unsigned long? arg);
   };

will correspond to these C++ function declarations:

.. code:: brush:

   int16_t MyAttr();
   void SetMyAttr(int16_t value);
   int64_t MyMethod(const Nullable<uint32_t>& arg);

.. _Floating_point_types:

Floating point types
^^^^^^^^^^^^^^^^^^^^

Floating point WebIDL types are mapped to the C++ type of the same
name.  So ``float`` and ``unrestricted float`` become a C++ ``float``,
while ``double`` and ``unrestricted double`` become a C++ ``double``.

For example, this WebIDL:

.. code:: notranslate

   interface Test {
     float myAttr;
     double myMethod(unrestricted double? arg);
   };

will correspond to these C++ function declarations:

.. code:: brush:

   float MyAttr();
   void SetMyAttr(float value);
   double MyMethod(const Nullable<double>& arg);

.. _DOMString:

``DOMString``
^^^^^^^^^^^^^

Strings are reflected in three different ways, depending on use:

-  String arguments become ``const nsAString&``.
-  String return values become a
   ```mozilla::dom::DOMString&`` <#DOMString-helper>`__ out param
   appended to the argument list. This comes after all IDL arguments,
   but before the ``ErrorResult&``, if any, for the method. Note that
   this allows callees to declare their methods as taking an
   ``nsAString&`` or ``nsString&`` if desired.
-  Strings in sequences, dictionaries, owning unions, and variadic
   arguments become ``nsString``.

Nullable strings are represented by the same types as non-nullable ones,
but the string will return true for ``DOMStringIsNull()``. Returning
null as a string value can be done using ``SetDOMStringToNull`` on the
out param if it's an ``nsAString`` or calling ``SetNull()`` on a
``DOMString``.

For example, this WebIDL:

.. code:: notranslate

   interface Test {
     DOMString myAttr;
     [Throws]
     DOMString myMethod(sequence<DOMString> arg1, DOMString? arg2, optional DOMString arg3);
   };

will correspond to these C++ function declarations:

.. code:: brush:

   void GetMyAttr(nsString& retval);
   void SetMyAttr(const nsAString& value);
   void MyMethod(const Sequence<nsString>& arg1, const nsAString& arg2,
                 const Optional<nsAString>& arg3, nsString& retval, ErrorResult& rv);

.. _USVString:

``USVString``
^^^^^^^^^^^^^

``USVString`` is reflected just like ``DOMString``.

.. _UTF8String:

``UTF8String``
^^^^^^^^^^^^^^

``UTF8String`` is a string with guaranteed-valid UTF-8 contents. It is
not a standard in the WebIDL spec, but its observables are the same as
those of ``USVString``.

It is a good fit for when the specification allows a ``USVString``, but
you want to process the string as UTF-8 rather than UTF-16.

It is reflected in three different ways, depending on use:

-  Arguments become ``const nsACString&``.
-  Return values become an ``nsACString&`` out param appended to the
   argument list. This comes after all IDL arguments, but before the
   ``ErrorResult&``, if any, for the method.
-  In sequences, dictionaries owning unions, and variadic arguments it
   becomes ``nsCString``.

Nullable ``UTF8String``\s are represented by the same types as
non-nullable ones, but the string will return true for ``IsVoid()``.
Returning null as a string value can be done using ``SetIsVoid()`` on
the out param.

.. _ByteString:

``ByteString``
^^^^^^^^^^^^^^

``ByteString`` is reflected in three different ways, depending on use:

-  ``ByteString`` arguments become ``const nsACString&``.
-  ``ByteString`` return values become an ``nsCString&`` out param
   appended to the argument list. This comes after all IDL arguments,
   but before the ``ErrorResult&``, if any, for the method.
-  ``ByteString`` in sequences, dictionaries, owning unions, and
   variadic arguments becomes ``nsCString``.

Nullable ``ByteString`` are represented by the same types as
non-nullable ones, but the string will return true for ``IsVoid()``.
Returning null as a string value can be done using ``SetIsVoid()`` on
the out param.

``object``
^^^^^^^^^^

``object`` is represented in three different ways, depending on use:

-  ``object`` arguments become ``JS::Handle<JSObject*>``.  They will be
   in the compartment of the passed-in JSContext.
-  ``object`` return values become a ``JS::MutableHandle<JSObject*>``
   out param appended to the argument list. This comes after all IDL
   arguments, but before the ``ErrorResult&``, if any, for the method.
   The return value is allowed to be in any compartment; bindings will
   wrap it into the context compartment as needed.
-  ``object`` dictionary members and sequence elements become
   ``JSObject*``. The dictionary members and sequence elements are
   guaranteed to be marked by whoever puts the sequence or dictionary on
   the stack, using ``SequenceRooter`` and ``DictionaryRooter``.

Methods using ``object`` always get a ``JSContext*`` argument.

For example, this WebIDL:

.. code:: notranslate

   interface Test {
     object myAttr;
     object myMethod(object arg1, object? arg2, sequence<object> arg3, optional object arg4,
                     optional object? arg5);
   };

will correspond to these C++ function declarations:

.. code:: brush:

   void GetMyAttr(JSContext* cx, JS::MutableHandle<JSObject*> retval);
   void SetMyAttr(JSContext* cx, JS::Handle<JSObject*> value);
   void MyMethod(JSContext* cx, JS::Handle<JSObject*> arg1, JS::Handle<JSObject*> arg2,
                 const Sequence<JSObject*>& arg3,
                 const Optional<JS::Handle<JSObject*> >& arg4,
                 const Optional<JS::Handle<JSObject*> >& arg5,
                 JS::MutableHandle<JSObject*> retval);

.. _Interface_types:

Interface types
^^^^^^^^^^^^^^^

There are four kinds of interface types in the WebIDL bindings. Callback
interfaces are used to represent script objects that browser code can
call into. External interfaces are used to represent objects that have
not been converted to the WebIDL bindings yet. WebIDL interfaces are
used to represent WebIDL binding objects. "SpiderMonkey" interfaces are
used to represent objects that are implemented natively by the
JavaScript engine (e.g., typed arrays).

.. _Callback_interfaces:

Callback interfaces
'''''''''''''''''''

Callback interfaces are represented in C++ as objects inheriting from
```mozilla::dom::CallbackInterface`` <#CallbackInterface>`__, whose
name, in the ``mozilla::dom`` namespace, matches the name of the
callback interface in the WebIDL. The exact representation depends on
how the type is being used.

-  Nullable arguments become ``Foo*``.
-  Non-nullable arguments become ``Foo&``.
-  Return values become ``already_AddRefed<Foo>`` or ``Foo*`` as
   desired. The pointer form is preferred because it results in faster
   code, but it should only be used if the return value was not addrefed
   (and so it can only be used if the return value is kept alive by the
   callee until at least the binding method has returned).
-  WebIDL callback interfaces in sequences, dictionaries, owning unions,
   and variadic arguments are represented by ``RefPtr<Foo>`` if nullable
   and ```OwningNonNull<Foo>`` <#OwningNonNull>`__ otherwise.

If the interface is a single-operation interface, the object exposes two
methods that both invoke the same underlying JS callable. The first of
these methods allows the caller to pass in a ``this`` object, while the
second defaults to ``undefined`` as the ``this`` value. In either case,
the ``this`` value is only used if the callback interface is implemented
by a JS callable. If it's implemented by an object with a property whose
name matches the operation, the object itself is always used as
``this``.

If the interface is not a single-operation interface, it just exposes a
single method for every IDL method/getter/setter.

The signatures of the methods correspond to the signatures for throwing
IDL methods/getters/setters with an additional trailing
"``mozilla::dom::CallbackObject::ExceptionHandling``
``aExceptionHandling``" argument, defaulting to ``eReportExceptions``.
If ``aReportExceptions`` is set to ``eReportExceptions``, the methods
will report JS exceptions before returning. If ``aReportExceptions`` is
set to ``eRethrowExceptions``, JS exceptions will be stashed in the
``ErrorResult`` and will be reported when the stack unwinds to wherever
the ``ErrorResult`` was set up.

For example, this WebIDL:

.. code:: notranslate

   callback interface MyCallback {
     attribute long someNumber;
     short someMethod(DOMString someString);
   };
   callback interface MyOtherCallback {
     // single-operation interface
     short doSomething(Node someNode);
   };
   interface MyInterface {
     attribute MyCallback foo;
     attribute MyCallback? bar;
   };

will lead to these C++ class declarations in the ``mozilla::dom``
namespace:

.. code:: brush:

   class MyCallback : public CallbackInterface
   {
     int32_t GetSomeNumber(ErrorResult& rv, ExceptionHandling aExceptionHandling = eReportExceptions);
     void SetSomeNumber(int32_t arg, ErrorResult& rv,
                        ExceptionHandling aExceptionHandling = eReportExceptions);
     int16_t SomeMethod(const nsAString& someString, ErrorResult& rv,
                        ExceptionHandling aExceptionHandling = eReportExceptions);
   };

   class MyOtherCallback : public CallbackInterface
   {
   public:
     int16_t
     DoSomething(nsINode& someNode, ErrorResult& rv,
                 ExceptionHandling aExceptionHandling = eReportExceptions);

     template<typename T>
     int16_t
     DoSomething(const T& thisObj, nsINode& someNode, ErrorResult& rv,
                 ExceptionHandling aExceptionHandling = eReportExceptions);
   };

and these C++ function declarations on the implementation of
``MyInterface``:

.. code:: notranslate

   already_AddRefed<MyCallback> GetFoo();
   void SetFoo(MyCallback&);
   already_AddRefed<MyCallback> GetBar();
   void SetBar(MyCallback*);

A consumer of MyCallback would be able to use it like this:

.. code:: brush:

   void
   SomeClass::DoSomethingWithCallback(MyCallback& aCallback)
   {
     ErrorResult rv;
     int32_t number = aCallback.GetSomeNumber(rv);
     if (rv.Failed()) {
       // The error has already been reported to the JS console; you can handle
       // things however you want here.
       return;
     }

     // For some reason we want to catch and rethrow exceptions from SetSomeNumber, say.
     aCallback.SetSomeNumber(2*number, rv, eRethrowExceptions);
     if (rv.Failed()) {
       // The exception is now stored on rv. This code MUST report
       // it usefully; otherwise it will assert.
     }
   }

.. _External_interfaces:

External interfaces
'''''''''''''''''''

External interfaces are represented in C++ as objects that XPConnect
knows how to unwrap to. This can mean XPCOM interfaces (whether declared
in XPIDL or not) or it can mean some type that there's a castable native
unwrapping function for. The C++ type to be used should be the
``nativeType`` listed for the external interface in the
```Bindings.conf`` <#Bindings.conf>`__ file. The exact representation
depends on how the type is being used.

-  Arguments become ``nsIFoo*``.
-  Return values can be ``already_AddRefed<nsIFoo>`` or ``nsIFoo*`` as
   desired. The pointer form is preferred because it results in faster
   code, but it should only be used if the return value was not addrefed
   (and so it can only be used if the return value is kept alive by the
   callee until at least the binding method has returned).
-  External interfaces in sequences, dictionaries, owning unions, and
   variadic arguments are represented by ``RefPtr<nsIFoo>.``

.. _WebIDL_interfaces:

WebIDL interfaces
'''''''''''''''''

WebIDL interfaces are represented in C++ as C++ classes. The class
involved must either be refcounted or must be explicitly annotated in
``Bindings.conf`` as being directly owned by the JS object. If the class
inherits from ``nsISupports``, then the canonical ``nsISupports`` must
be on the primary inheritance chain of the object. If the interface has
a parent interface, the C++ class corresponding to the parent must be on
the primary inheritance chain of the object. This guarantees that a
``void*`` can be stored in the JSObject which can then be
``reinterpret_cast`` to any of the classes that correspond to interfaces
the object implements. The C++ type to be used should be the
``nativeType`` listed for the interface in the
```Bindings.conf`` <#Bindings.conf>`__ file, or
``mozilla::dom::InterfaceName`` if none is listed. The exact
representation depends on how the type is being used.

-  Nullable arguments become ``Foo*``.
-  Non-nullable arguments become ``Foo&``.
-  Return values become ``already_AddRefed<Foo>`` or ``Foo*`` as
   desired. The pointer form is preferred because it results in faster
   code, but it should only be used if the return value was not addrefed
   (and so it can only be used if the return value is kept alive by the
   callee until at least the binding method has returned).
-  WebIDL interfaces in sequences, dictionaries, owning unions, and
   variadic arguments are represented by ``RefPtr<Foo>`` if nullable and
   ```OwningNonNull<Foo>`` <#OwningNonNull>`__ otherwise.

For example, this WebIDL:

.. code:: notranslate

   interface MyInterface {
     attribute MyInterface myAttr;
     void passNullable(MyInterface? arg);
     MyInterface? doSomething(sequence<MyInterface> arg);
     MyInterface doTheOther(sequence<MyInterface?> arg);
     readonly attribute MyInterface? nullableAttr;
     readonly attribute MyInterface someOtherAttr;
     readonly attribute MyInterface someYetOtherAttr;
   };

Would correspond to these C++ function declarations:

.. code:: brush:

   already_AddRefed<MyClass> MyAttr();
   void SetMyAttr(MyClass& value);
   void PassNullable(MyClass* arg);
   already_AddRefed<MyClass> doSomething(const Sequence<OwningNonNull<MyClass> >& arg);
   already_AddRefed<MyClass> doTheOther(const Sequence<RefPtr<MyClass> >& arg);
   already_Addrefed<MyClass> GetNullableAttr();
   MyClass* SomeOtherAttr();
   MyClass* SomeYetOtherAttr(); // Don't have to return already_AddRefed!

.. _SpiderMonkey_interfaces:

"SpiderMonkey" interfaces
'''''''''''''''''''''''''

Typed array, array buffer, and array buffer view arguments are
represented by the objects in ```TypedArray.h`` <#TypedArray>`__.  For
example, this WebIDL:

.. code:: notranslate

   interface Test {
     void passTypedArrayBuffer(ArrayBuffer arg);
     void passTypedArray(ArrayBufferView arg);
     void passInt16Array(Int16Array? arg);
   }

will correspond to these C++ function declarations:

.. code:: brush:

   void PassTypedArrayBuffer(const ArrayBuffer& arg);
   void PassTypedArray(const ArrayBufferView& arg);
   void PassInt16Array(const Nullable<Int16Array>& arg);

Typed array return values become a ``JS::MutableHandle<JSObject*>`` out
param appended to the argument list. This comes after all IDL arguments,
but before the ``ErrorResult&``, if any, for the method.  The return
value is allowed to be in any compartment; bindings will wrap it into
the context compartment as needed.

Typed arrays store a ``JSObject*`` and hence need to be rooted
properly.  On-stack typed arrays can be declared as
``RootedTypedArray<TypedArrayType>`` (e.g.
``RootedTypedArray<Int16Array>``).  Typed arrays on the heap need to be
traced.

.. _Dictionary_types:

Dictionary types
^^^^^^^^^^^^^^^^

A dictionary argument is represented by a const reference to a struct
whose name is the dictionary name in the ``mozilla::dom`` namespace.
The struct has one member for each of the dictionary's members with the
same name except the first letter uppercased and prefixed with "m". The
members that are required or have default values have types as described
under the corresponding WebIDL type in this document. The members that
are not required and don't have default values have those types wrapped
in ```Optional<>`` <#Optional>`__.

Dictionary return values are represented by an out parameter whose type
is a non-const reference to the struct described above, with all the
members that have default values preinitialized to those default values.

Note that optional dictionary arguments are always forced to have a
default value of an empty dictionary by the IDL parser and code
generator, so dictionary arguments are never wrapped in ``Optional<>``.

If necessary, dictionaries can be directly initialized from a
``JS::Value`` in C++ code by invoking their ``Init()`` method. Consumers
doing this should declare their dictionary as
``RootedDictionary<DictionaryName>``. When this is done, passing in a
null ``JSContext*`` is allowed if the passed-in ``JS::Value`` is
``JS::NullValue()``. Likewise, a dictionary struct can be converted to a
``JS::Value`` in C++ by calling ``ToJSValue`` with the dictionary as the
second argument. If ``Init()`` or ``ToJSValue()`` returns false, they
will generally set a pending exception on the JSContext; reporting those
is the responsibility of the caller.

For example, this WebIDL:

.. code:: notranslate

   dictionary Dict {
     long foo = 5;
     DOMString bar;
   };

   interface Test {
     void initSomething(optional Dict arg = {});
   };

will correspond to this C++ function declaration:

.. code:: brush:

   void InitSomething(const Dict& arg);

and the ``Dict`` struct will look like this:

.. code:: brush:

   struct Dict {
     bool Init(JSContext* aCx, JS::Handle<JS::Value> aVal, const char* aSourceDescription = "value");

     Optional<nsString> mBar;
     int32_t mFoo;
   }

Note that the dictionary members are sorted in the struct in
alphabetical order.

.. _API_for_working_with_dictionaries:

API for working with dictionaries
'''''''''''''''''''''''''''''''''

There are a few useful methods found on dictionaries and dictionary
members that you can use to quickly determine useful things.

-  **member.WasPassed()** - as the name suggests, was a particular
   member passed?
   (e.g., ``if (arg.foo.WasPassed() { /* do nice things!*/ }``)
-  **dictionary.IsAnyMemberPresent()** - great for checking if you need
   to do anything.
   (e.g., ``if (!arg.IsAnyMemberPresent()) return; // nothing to do``)
-  **member.Value()** - getting the actual data/value of a member that
   was passed.
   (e.g., ``mBar.Assign(args.mBar.value())``)

Example implementation using all of the above:

.. code:: brush:

   void
   MyInterface::InitSomething(const Dict& aArg){
     if (!aArg.IsAnyMemberPresent()) {
       return; // nothing to do!
     }
     if (aArg.mBar.WasPassed() && !mBar.Equals(aArg.mBar.value())) {
       mBar.Assign(aArg.mBar.Value());
     }
   }

.. _Enumeration_types:

Enumeration types
^^^^^^^^^^^^^^^^^

WebIDL enumeration types are represented as C++ enum classes. The values
of the C++ enum are named by taking the strings in the WebIDL
enumeration, replacing all non-alphanumerics with underscores, and
uppercasing the first letter, with a special case for the empty string,
which becomes the value ``_empty``.

For a WebIDL enum named ``MyEnum``, the C++ enum is named ``MyEnum`` and
placed in the ``mozilla::dom`` namespace, while the values are placed in
the ``mozilla::dom::MyEnum`` namespace. There is also a
``mozilla::dom::MyEnumValues::strings`` which is an array of
``mozilla::dom::EnumEntry`` structs that gives access to the string
representations of the values.

The type of the enum class is automatically selected to be the smallest
unsigned integer type that can hold all the values.  In practice, this
is always uint8_t, because WebIDL enums tend to not have more than 255
values.

For example, this WebIDL:

.. code:: notranslate

   enum MyEnum {
     "something",
     "something-else",
     "",
     "another"
   };

would lead to this C++ enum declaration:

.. code:: brush:

   enum class MyEnum : uint8_t {
     Something,
     Something_else,
     _empty,
     Another
   };

   namespace MyEnumValues {
   extern const EnumEntry strings[10];
   } // namespace MyEnumValues

.. _Callback_function_types:

Callback function types
^^^^^^^^^^^^^^^^^^^^^^^

Callback functions are represented as an object, inheriting from
```mozilla::dom::CallbackFunction`` <#CallbackFunction>`__, whose name,
in the ``mozilla::dom`` namespace, matches the name of the callback
function in the WebIDL. If the type is nullable, a pointer is passed in;
otherwise a reference is passed in.

The object exposes two ``Call`` methods, which both invoke the
underlying JS callable. The first ``Call`` method has the same signature
as a throwing method declared just like the callback function, with an
additional trailing "``mozilla::dom::CallbackObject::ExceptionHandling``
``aExceptionHandling``" argument, defaulting to ``eReportExceptions``,
and calling it will invoke the callable with ``undefined`` as the
``this`` value. The second ``Call`` method allows passing in an explicit
``this`` value as the first argument. This second call method is a
template on the type of the first argument, so the ``this`` value can be
passed in in whatever form is most convenient, as long as it's either a
type that can be wrapped by XPConnect or a WebIDL interface type.

If ``aReportExceptions`` is set to ``eReportExceptions``, the ``Call``
methods will report JS exceptions before returning.  If
``aReportExceptions`` is set to ``eRethrowExceptions``, JS exceptions
will be stashed in the ``ErrorResult`` and will be reported when the
stack unwinds to wherever the ``ErrorResult`` was set up.

For example, this WebIDL:

.. code:: notranslate

   callback MyCallback = long (MyInterface arg1, boolean arg2);
   interface MyInterface {
     attribute MyCallback foo;
     attribute MyCallback? bar;
   };

will lead to this C++ class declaration, in the ``mozilla::dom``
namespace:

.. code:: brush:

   class MyCallback : public CallbackFunction
   {
   public:
     int32_t
     Call(MyInterface& arg1, bool arg2, ErrorResult& rv,
          ExceptionHandling aExceptionHandling = eReportExceptions);

     template<typename T>
     int32_t
     Call(const T& thisObj, MyInterface& arg1, bool arg2, ErrorResult& rv,
          ExceptionHandling aExceptionHandling = eReportExceptions);
   };

and these C++ function declarations in the ``MyInterface`` class:

.. code:: notranslate

   already_AddRefed<MyCallback> GetFoo();
   void SetFoo(MyCallback&);
   already_AddRefed<MyCallback> GetBar();
   void SetBar(MyCallback*);

A consumer of MyCallback would be able to use it like this:

.. code:: brush:

   void
   SomeClass::DoSomethingWithCallback(MyCallback& aCallback, MyInterface& aInterfaceInstance)
   {
     ErrorResult rv;
     int32_t number = aCallback.Call(aInterfaceInstance, false, rv);
     if (rv.Failed()) {
       // The error has already been reported to the JS console; you can handle
       // things however you want here.
       return;
     }

     // Now for some reason we want to catch and rethrow exceptions from the callback,
     // and use "this" as the this value for the call to JS.
     number = aCallback.Call(*this, true, rv, eRethrowExceptions);
     if (rv.Failed()) {
       // The exception is now stored on rv.  This code MUST report
       // it usefully; otherwise it will assert.
     }
   }

.. _sect1:

.. _Sequences:

Sequences
^^^^^^^^^

Sequence arguments are represented by
```const Sequence<T>&`` <#Sequence>`__, where ``T`` depends on the type
of elements in the WebIDL sequence.

Sequence return values are represented by an ``nsTArray<T>`` out param
appended to the argument list, where ``T`` is the return type for the
elements of the WebIDL sequence. This comes after all IDL arguments, but
before the ``ErrorResult&``, if any, for the method.

.. _Arrays:

Arrays
^^^^^^

IDL array objects are not supported yet. The spec on these is likely to
change drastically anyway.

.. _Union_types:

Union types
^^^^^^^^^^^

Union types are reflected as a struct in the ``mozilla::dom`` namespace.
There are two kinds of union structs: one kind does not keep its members
alive (is "non-owning"), and the other does (is "owning"). Const
references to non-owning unions are used for plain arguments. Owning
unions are used in dictionaries, sequences, and for variadic arguments.
Union return values become a non-const owning union out param. The name
of the struct is the concatenation of the names of the types in the
union, with "Or" inserted between them, and for an owning struct
"Owning" prepended. So for example, this IDL:

.. code:: notranslate

   void passUnion((object or long) arg);
   (object or long) receiveUnion();
   void passSequenceOfUnions(sequence<(object or long)> arg);
   void passOtherUnion((HTMLDivElement or ArrayBuffer or EventInit) arg);

would correspond to these C++ function declarations:

.. code:: brush:

   void PassUnion(const ObjectOrLong& aArg);
   void ReceiveUnion(OwningObjectObjectOrLong& aArg);
   void PassSequenceOfUnions(const Sequence<OwningObjectOrLong>& aArg);
   void PassOtherUnion(const HTMLDivElementOrArrayBufferOrEventInit& aArg);

Union structs expose accessors to test whether they're of a given type
and to get hold of the data of that type. They also expose setters that
set the union as being of a particular type and return a reference to
the union's internal storage where that type could be stored. The one
exception is the ``object`` type, which uses a somewhat different form
of setter where the ``JSObject*`` is passed in directly. For example,
``ObjectOrLong`` would have the following methods:

.. code:: brush:

   bool IsObject() const;
   JSObject* GetAsObject() const;
   void SetToObject(JSContext*, JSObject*);
   bool IsLong() const;
   int32_t GetAsLong() const;
   int32_t& SetAsLong()

Owning unions used on the stack should be declared as a
``RootedUnion<UnionType>``, for example,
``RootedUnion<OwningObjectOrLong>``.

.. _Date:

``Date``
^^^^^^^^

WebIDL ``Date`` types are represented by a ``mozilla::dom::Date``
struct.

.. _C_reflections_of_WebIDL_declarations:

C++ reflections of WebIDL declarations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

WebIDL declarations (maplike/setlike/iterable) are turned into a set of
properties and functions on the interface they are declared on. Each has
a different set of helper functions it comes with. In addition, for
iterable, there are requirements for C++ function implementation by the
interface developer.

.. _Maplike:

Maplike
^^^^^^^

Example Interface:

.. code:: notranslate

   interface StringToLongMap {
     maplike<DOMString, long>;
   };

The bindings for this interface will generate the storage structure for
the map, as well as helper functions for accessing that structure from
C++. The generated C++ API will look as follows:

.. code:: brush:

   namespace StringToLongMapBinding {
   namespace MaplikeHelpers {
   void Clear(mozilla::dom::StringToLongMap* self, ErrorResult& aRv);
   bool Delete(mozilla::dom::StringToLongMap* self, const nsAString& aKey, ErrorResult& aRv);
   bool Has(mozilla::dom::StringToLongMap* self, const nsAString& aKey, ErrorResult& aRv);
   void Set(mozilla::dom::StringToLongMap* self, const nsAString& aKey, int32_t aValue, ErrorResult& aRv);
   } // namespace MaplikeHelpers
   } // namespace StringToLongMapBindings

.. _Setlike:

Setlike
^^^^^^^

Example Interface:

.. code:: notranslate

   interface StringSet {
     setlike<DOMString>;
   };

The bindings for this interface will generate the storage structure for
the set, as well as helper functions for accessing that structure from
c++. The generated C++ API will look as follows:

.. code:: brush:

   namespace StringSetBinding {
   namespace SetlikeHelpers {
   void Clear(mozilla::dom::StringSet* self, ErrorResult& aRv);
   bool Delete(mozilla::dom::StringSet* self, const nsAString& aKey, ErrorResult& aRv);
   bool Has(mozilla::dom::StringSet* self, const nsAString& aKey, ErrorResult& aRv);
   void Add(mozilla::dom::StringSet* self, const nsAString& aKey, ErrorResult& aRv);
   } // namespace SetlikeHelpers
   }

.. _Iterable:

Iterable
^^^^^^^^

Unlike maplike and setlike, iterable does not have any C++ helpers, as
the structure backing the iterable data for the interface is left up to
the developer. With that in mind, the generated iterable bindings expect
the wrapper object to provide certain methods for the interface to
access.

Iterable interfaces have different requirements, based on if they are
single or pair value iterators.

Example Interface for a single value iterator:

.. code:: notranslate

   interface LongIterable {
     iterable<long>;
     getter long(unsigned long index);
     readonly attribute unsigned long length;
   };

For single value iterator interfaces, we treat the interface as an
`indexed getter <#Indexed_getters>`__, as required by the spec. See the
`indexed getter implementation section <#Indexed_getters>`__ for more
information on building this kind of structure.

Example Interface for a pair value iterator:

.. code:: notranslate

   interface StringAndLongIterable {
     iterable<DOMString, long>;
   };

The bindings for this pair value iterator interface require the
following methods be implemented in the C++ object:

.. code:: brush:

   class StringAndLongIterable {
   public:
     // Returns the number of items in the iterable storage
     size_t GetIterableLength();
     // Returns key of pair at aIndex in iterable storage
     nsAString& GetKeyAtIndex(uint32_t aIndex);
     // Returns value of pair at aIndex in iterable storage
     uint32_t& GetValueAtIndex(uint32_t aIndex);
   }

.. _Stringifiers:

Stringifiers
~~~~~~~~~~~~

Named stringifiers operations in WebIDL will just invoke the
corresponding C++ method.

Anonymous stringifiers in WebIDL will invoke the C++ method called
``Stringify``. So, for example, given this IDL:

.. code:: notranslate

   interface FirstInterface {
     stringifier;
   };

   interface SecondInterface {
     stringifier DOMString getStringRepresentation();
   };

the corresponding C++ would be:

.. code:: notranslate

   class FirstInterface {
   public:
     void Stringify(nsAString& aResult);
   };

   class SecondInterface {
   public:
     void GetStringRepresentation(nsAString& aResult);
   };

.. _Legacy_Callers:

Legacy Callers
~~~~~~~~~~~~~~

Only anonymous legacy callers are supported, and will invoke the C++
method called ``LegacyCall``. This will be passed the JS "this" value as
the first argument, then the arguments to the actual operation. A
``JSContext`` will be passed if any of the operation arguments need it.
So for example, given this IDL:

.. code:: notranslate

   interface InterfaceWithCall {
     legacycaller long (float arg);
   };

the corresponding C++ would be:

.. code:: brush:

   class InterfaceWithCall {
   public:
     int32_t LegacyCall(JS::Handle<JS::Value> aThisVal, float aArgument);
   };

.. _Named_getters:

Named getters
~~~~~~~~~~~~~

If the interface has a named getter, the binding will expect several
methods on the C++ implementation:

-  A ``NamedGetter`` method. This takes a property name and returns
   whatever type the named getter is declared to return. It also has a
   boolean out param for whether a property with that name should exist
   at all.
-  A ``NameIsEnumerable`` method. This takes a property name and
   returns a boolean that indicates whether the property is enumerable.
-  A ``GetSupportedNames`` method. This takes an unsigned integer which
   corresponds to the flags passed to the ``iterate`` proxy trap and
   returns a list of property names. For implementations of this method,
   the important flags is ``JSITER_HIDDEN``. If that flag is set, the
   call needs to return all supported property names. If it's not set,
   the call needs to return only the enumerable ones.

The ``NameIsEnumerable`` and ``GetSupportedNames`` methods need to agree
on which names are and are not enumerable. The ``NamedGetter`` and
``GetSupportedNames`` methods need to agree on which names are
supported.

So for example, given this IDL:

.. code:: notranslate

   interface InterfaceWithNamedGetter {
     getter long(DOMString arg);
   };

the corresponding C++ would be:

.. code:: brush:

   class InterfaceWithNamedGetter
   {
   public:
     int32_t NamedGetter(const nsAString& aName, bool& aFound);
     bool NameIsEnumerable(const nsAString& aName);
     void GetSupportedNames(unsigned aFlags, nsTArray<nsString>& aNames);
   };

.. _Indexed_getters:

Indexed getters
~~~~~~~~~~~~~~~

If the interface has a indexed getter, the binding will expect the
following methods on the C++ implementation:

-  A ``IndexedGetter`` method. This takes an integer index value and
   returns whatever type the indexed getter is declared to return. It
   also has a boolean out param for whether a property with that index
   should exist at all.  The implementation must set this out param
   correctly.  The return value is guaranteed to be ignored if the out
   param is set to false.

So for example, given this IDL:

.. code:: notranslate

   interface InterfaceWithIndexedGetter {
     getter long(unsigned long index);
     readonly attribute unsigned long length;
   };

the corresponding C++ would be:

.. code:: brush:

   class InterfaceWithIndexedGetter
   {
   public:
     uint32_t Length() const;
     int32_t IndexedGetter(uint32_t aIndex, bool& aFound) const;
   };

.. _Throwing_exceptions_from_WebIDL_methods_getters_and_setters:

Throwing exceptions from WebIDL methods, getters, and setters
-------------------------------------------------------------

WebIDL methods, getters, and setters that are `explicitly marked as
allowed to throw <#Throws>`__ have an ``ErrorResult&`` argument as their
last argument.  To throw an exception, simply call ``Throw()`` on the
``ErrorResult&`` and return from your C++ back into the binding code.

In cases when the specification calls for throwing a ``TypeError``, you
should use ``ErrorResult::ThrowTypeError()`` instead of calling
``Throw()``.

.. _Custom_extended_attributes:

Custom extended attributes
--------------------------

Our WebIDL parser and code generator recognize several extended
attributes that are not present in the WebIDL spec.

.. _Alias:

``[Alias=propName]``
~~~~~~~~~~~~~~~~~~~~

This extended attribute can be specified on a method and indicates that
another property with the specified name will also appear on the
interface prototype object and will have the same Function object value
as the property for the method. For example:

.. code:: notranslate

   interface MyInterface {
     [Alias=performSomething] void doSomething();
   };

``MyInterface.prototype.performSomething`` will have the same Function
object value as ``MyInterface.prototype.doSomething``.

Multiple ``[Alias]`` extended attribute can be used on the one method.
``[Alias]`` cannot be used on a static method, nor on methods on a
global interface (such as ``Window``).

Aside from regular property names, the name of an alias can be
`Symbol.iterator </en-US/docs/Web/JavaScript/Reference/Global_Objects/Symbol#Well-known_symbols>`__.
This is specified by writing ``[Alias="@@iterator"]``.

.. _BindingAlias:

``[BindingAlias=propName]``
~~~~~~~~~~~~~~~~~~~~~~~~~~~

This extended attribute can be specified on an attribute and indicates
that another property with the specified name will also appear on the
interface prototype object and will call the same underlying C++
implementation for the getter and setter. This is more efficient than
using the same ``BinaryName`` for both attributes, because it shares the
binding glue code between them. The properties still have separate
getter/setter functions in JavaScript, so from the point of view of web
consumers it's as if you actually had two separate attribute
declarations on your interface. For example:

.. code:: notranslate

   interface MyInterface {
     [BindingAlias=otherAttr] readonly attribute boolean attr;
   };

``MyInterface.prototype.otherAttr`` and ``MyInterface.prototype.attr``
will both exist, have separate getter/setter functions, but call the
same binding glue code and implementation function on the objects
implementing ``MyInterface``.

Multiple ``[BindingAlias]`` extended attributes can be used on a single
attribute.

.. _ChromeOnly:

``[ChromeOnly]``
~~~~~~~~~~~~~~~~

This extended attribute can be specified on any method, attribute, or
constant on an interface or on an interface as a whole.  It can also be
specified on dictionary members.

Interface members flagged as ``[ChromeOnly]`` are only exposed in chrome
Windows (and in particular, are not exposed to webpages). From the point
of view of web content, it's as if the interface member were not there
at all. These members *are* exposed to chrome script working with a
content object via Xrays.

If specified on an interface as a whole, this functions like
```[Func]`` <#Func>`__ except that the binding code will automatically
check whether the caller script has the system principal (is chrome or a
worker started from a chrome page) instead of calling into the C++
implementation to determine whether to expose the interface object on
the global. This means that accessing a content global via Xrays will
show ``[ChromeOnly]`` interface objects on it.

If specified on a dictionary member, then the dictionary member will
only appear to exist in system-privileged code.

This extended attribute can be specified together with ``[Func]``, and
``[Pref]``. If more than one of these is specified, all conditions will
need to test true for the interface or interface member to be exposed.

.. _Pref:

``[Pref=prefname]``
~~~~~~~~~~~~~~~~~~~

This extended attribute can be specified on any method, attribute, or
constant on an interface or on an interface as a whole. It can also be
specified on dictionary members.  It takes a value, which must be the
name of a boolean preference exposed from ``StaticPrefs``. The
``StaticPrefs`` function that will be called is calculated from the
value of the extended attribute, with dots replaced by underscores
(``StaticPrefs::my_pref_name()`` in the example below).

If specified on an interface member, the interface member involved is
only exposed if the preference is set to ``true``. An example of how
this can be used:

.. code:: notranslate

   interface MyInterface {
     attribute long alwaysHere;
     [Pref="my.pref.name"] attribute long onlyHereIfEnabled;
   };

If specified on an interface as a whole, this functions like
```[Func]`` <#Func>`__ except that the binding will check the value of
the preference directly without calling into the C++ implementation of
the interface at all. This is useful when the enable check is simple and
it's desirable to keep the prefname with the WebIDL declaration.

If specified on a dictionary member, the web-observable behavior when
the pref is set to false will be as if the dictionary did not have a
member of that name defined.  That means that on the JS side no
observable get of the property will happen.  On the C++ side, the
behavior would be as if the passed-in object did not have a property
with the relevant name: the dictionary member would either be
``!Passed()`` or have the default value if there is a default value.

  An example of how this can be used:

.. code:: notranslate

   [Pref="my.pref.name"]
   interface MyConditionalInterface {
   };

This extended attribute can be specified together with ``[ChromeOnly]``,
and ``[Func]``. If more than one of these is specified, all conditions
will need to test true for the interface or interface member to be
exposed.

.. _Func:

``[Func="funcname"]``
~~~~~~~~~~~~~~~~~~~~~

This extended attribute can be specified on any method, attribute, or
constant on an interface or on an interface as a whole. It can also be
specified on dictionary members.  It takes a value, which must be the
name of a static function.

If specified on an interface member, the interface member involved is
only exposed if the specified function returns ``true``. An example of
how this can be used:

.. code:: notranslate

   interface MyInterface {
     attribute long alwaysHere;
     [Func="MyClass::StuffEnabled"] attribute long onlyHereIfEnabled;
   };

The function is invoked with two arguments: the ``JSContext`` that the
operation is happening on and the ``JSObject`` for the global of the
object that the property will be defined on if the function returns
true. In particular, in the Xray case the ``JSContext`` is in the caller
compartment (typically chrome) but the ``JSObject`` is in the target
compartment (typically content). This allows the method implementation
to select which compartment it cares about in its checks.

The above IDL would also require the following C++:

.. code:: brush:

   class MyClass {
     static bool StuffEnabled(JSContext* cx, JSObject* obj);
   };

If specified on an interface as a whole, then lookups for the interface
object for this interface on a DOM Window will only find it if the
specified function returns true. For objects that can only be created
via a constructor, this allows disabling the functionality altogether
and making it look like the feature is not implemented at all.

If specified on a dictionary member, the web-observable behavior when
the function returns false will be as if the dictionary did not have a
member of that name defined.  That means that on the JS side no
observable get of the property will happen.  On the C++ side, the
behavior would be as if the passed-in object did not have a property
with the relevant name: the dictionary member would either be
``!Passed()`` or have the default value if there is a default value.

An example of how ``[Func]`` can be used:

.. code:: notranslate

   [Func="MyClass::MyConditionalInterfaceEnabled"]
   interface MyConditionalInterface {
   };

In this case, the C++ function is passed a ``JS::Handle<JSObject*>``. So
the C++ in this case would look like this:

.. code:: brush:

   class MyClass {
     static bool MyConditionalInterfaceEnabled(JSContext* cx, JS::Handle<JSObject*> obj);
   };

Just like in the interface member case, the ``JSContext`` is in the
caller compartment but the ``JSObject`` is the actual object the
property would be defined on. In the Xray case that means obj is in the
target compartment (typically content) and ``cx`` is typically chrome.

This extended attribute can be specified together with ``[ChromeOnly]``,
and ``[Pref]``. If more than one of these is specified, all conditions
will need to test true for the interface or interface member to be
exposed.

Binding code will include the headers necessary for a ``[Func]``, unless
the interface is using a non-deafault heder file.  If a non-default
header file is used, that header file needs to do any header inclusions
necessary for ``[Func]`` annotations.

.. _Throws:

``[Throws]``, ``[GetterThrows]``, ``[SetterThrows]``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Used to flag methods or attributes as allowing the C++ callee to throw.
This causes the binding generator, and in many cases the JIT, to
generate extra code to handle possible exceptions. Possibly-throwing
methods and attributes get an ``ErrorResult&`` argument.

``[Throws]`` applies to both methods and attributes; for attributes it
means both the getter and the setter can throw. ``[GetterThrows]``
applies only to attributes. ``[SetterThrows]`` applies only to
non-readonly attributes.

For interfaces flagged with ``[JSImplementation]``, all methods and
properties are assumed to be able to throw and do not need to be flagged
as throwing.

.. _DependsOn:

``[DependsOn]``
~~~~~~~~~~~~~~~

Used for a method or attribute to indicate what the return value depends
on. Possible values are:

``Everything``
   This value can't actually be specified explicitly; this is the
   default value you get when ``[DependsOn]`` is not specified. This
   means we don't know anything about the return value's dependencies
   and hence can't rearrange other code that might change values around
   the method or attribute.
``DOMState``
   The return value depends on the state of the "DOM", by which we mean
   all objects specified via Web IDL. The return value is guaranteed to
   not depend on the state of the JS heap or other JS engine data
   structures, and is guaranteed to not change unless some function with
   ```[Affects=Everything]`` <#Affects=Everything>`__ is executed.
``DeviceState``
   The return value depends on the state of the device we're running on
   (e.g., the system clock). The return value is guaranteed to not be
   affected by any code running inside Gecko itself, but we might get a
   new value every time the method or getter is called even if no Gecko
   code ran between the calls.
``Nothing``
   The return value is a constant that never changes. This value cannot
   be used on non-readonly attributes, since having a non-readonly
   attribute whose value never changes doesn't make sense.

Values other than ``Everything``, when used in combination with
```[Affects=Nothing]`` <#Affects=Nothing>`__, can used by the JIT to
perform loop-hoisting and common subexpression elimination on the return
values of IDL attributes and methods.

.. _Affects:

``[Affects]``
~~~~~~~~~~~~~

Used for a method or attribute getter to indicate what sorts of state
can be affected when the function is called. Attribute setters are, for
now, assumed to affect everything. Possible values are:

``Everything``
   This value can't actually be specified explicitly; this is the
   default value you get when ``[Affects]`` is not specified. This means
   that calling the method or getter might change any mutable state in
   the DOM or JS heap.
``Nothing``
   Calling the method or getter will have no side-effects on either the
   DOM or the JS heap.

Methods and attribute getters with ``[Affects=Nothing]`` are allowed to
throw exceptions, as long as they do so deterministically. In the case
of methods, whether an exception is thrown is allowed to depend on the
arguments, as long as calling the method with the same arguments will
always either throw or not throw.

The ``Nothing`` value, when used with ``[DependsOn]`` values other than
``Everything``, can used by the JIT to perform loop-hoisting and common
subexpression elimination on the return values of IDL attributes and
methods, as well as code motion past DOM methods that might depend on
system state but have no side effects.

.. _Pure:

``[Pure]``
~~~~~~~~~~

This is an alias for ``[Affects=Nothing, DependsOn=DOMState]``.
Attributes/methods flagged in this way promise that they will keep
returning the same value as long as nothing that has
``[Affects=Everything]`` executes.

.. _Constant:

``[Constant]``
~~~~~~~~~~~~~~

This is an alias for ``[Affects=Nothing, DependsOn=Nothing]``. Used to
flag readonly attributes or methods that could have been annotated with
``[Pure]`` and also always return the same value. This should only be
used when it's absolutely guaranteed that the return value of the
attribute getter will always be the same from the JS engine's point of
view.

The spec's ``[SameObject]`` extended attribute is an alias for
``[Constant]``, but can only be applied to things returning objects,
whereas ``[Constant]`` can be used for any type of return value.

.. _NeedResolve:

``[NeedResolve]``
~~~~~~~~~~~~~~~~~

Used to flag interfaces which have a custom resolve hook. This
annotation will cause the ``DoResolve`` method to be called on the
underlying C++ class when a property lookup happens on the object. The
signature of this method is:
``bool DoResolve(JSContext*, JS::Handle<JSObject*>, JS::Handle<jsid>, JS::MutableHandle<JS::Value>)``.
Here the passed-in object is the object the property lookup is happening
on (which may be an Xray for the actual DOM object) and the jsid is the
property name. The value that the property should have is returned in
the ``MutableHandle<Value>``, with ``UndefinedValue()`` indicating that
the property does not exist.

If this extended attribute is used, then the underlying C++ class must
also implement a method called ``GetOwnPropertyNames`` with the
signature
``void GetOwnPropertyNames(JSContext* aCx, nsTArray<nsString>& aNames, ErrorResult& aRv)``.
This method will be called by the JS engine's enumerate hook and must
provide a superset of all the property names that ``DoResolve`` might
resolve. Providing names that ``DoResolve`` won't actually resolve is
OK.

.. _HeaderFile:

``[HeaderFile="path/to/headerfile.h"]``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Indicates where the implementation can be found. Similar to the
headerFile annotation in Bindings.conf.  Just like headerFile in
Bindings.conf, should be avoided.

.. _JSImplementation:

``[JSImplementation="@mozilla.org/some-contractid;1"]``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Used on an interface to provide the contractid of the `JavaScript
component implementing the
interface <#Implementing_WebIDL_using_Javascript>`__.

.. _StoreInSlot:

``[StoreInSlot]``
~~~~~~~~~~~~~~~~~

Used to flag attributes that can be gotten very quickly from the JS
object by the JIT. Such attributes will have their getter called
immediately when the JS wrapper for the DOM object is created, and the
returned value will be stored directly on the JS object. Later gets of
the attribute will not call the C++ getter and instead use the cached
value. If the value returned by the attribute needs to change, the C++
code should call the ``ClearCachedFooValue`` method in the namespace of
the relevant binding, where ``foo`` is the name of the attribute. This
will immediately call the C++ getter and cache the value it returns, so
it needs a ``JSContext`` to work on. This extended attribute can only be
used in on attributes whose getters are ```[Pure]`` <#Pure>`__ or
```[Constant]`` <#Constant>`__ and which are not
```[Throws]`` <#Throws>`__ or ```[GetterThrows]`` <#Throws>`__.

So for example, given this IDL:

.. code:: notranslate

   interface MyInterface {
     [Pure, StoreInSlot] attribute long myAttribute;
   };

the C++ implementation of MyInterface would clear the cached value by
calling
``mozilla::dom::MyInterface_Binding::ClearCachedMyAttributeValue(cx, this)``.
This function will return false on error and the caller is responsible
for handling any JSAPI exception that is set by the failure.

If the attribute is not readonly, setting it will automatically clear
the cached value and reget it again before the setter returns.

.. _Cached:

``[Cached]``
~~~~~~~~~~~~

Used to flag attributes that, when their getter is called, will cache
the returned value on the JS object. This can be used to implement
attributes whose value is a sequence or dictionary (which would
otherwise end up returning a new object each time and hence not be
allowed in WebIDL).

Unlike ```[StoreInSlot]`` <#StoreInSlot>`__ this does *not* cause the
getter to be eagerly called at JS wrapper creation time; the caching is
lazy. ``[Cached]`` attributes must be ```[Pure]`` <#Pure>`__ or
```[Constant]`` <#Constant>`__, because otherwise not calling the C++
getter would be observable, but are allowed to have throwing getters.
Their cached value can be cleared by calling the ``ClearCachedFooValue``
method in the namespace of the relevant binding, where ``foo`` is the
name of the attribute. Unlike ``[StoreInSlot]`` attributes, doing so
will not immediately invoke the getter, so it does not need a
``JSContext``.

So for example, given this IDL:

.. code:: notranslate

   interface MyInterface {
     [Pure, StoreInSlot] attribute long myAttribute;
   };

the C++ implementation of MyInterface would clear the cached value by
calling
``mozilla::dom::MyInterface_Binding::ClearCachedMyAttributeValue(this)``.
JS-implemented WebIDL can clear the cached value by calling
``this.__DOM_IMPL__._clearCachedMyAttributeValue()``.

If the attribute is not readonly, setting it will automatically clear
the cached value.

.. _Frozen:

``[Frozen]``
~~~~~~~~~~~~

Used to flag attributes that, when their getter is called, will call
```Object.freeze`` </en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/freeze>`__
on the return value before returning it. This extended attribute is only
allowed on attributes that return sequences, dictionaries and
``MozMap``, and corresponds to returning a frozen ``Array`` (for the
sequence case) or ``Object`` (for the other two cases).

.. _BinaryName:

``[BinaryName]``
~~~~~~~~~~~~~~~~

``[BinaryName]`` can be specified on method or attribute to change the
C++ function name that will be used for the method or attribute. It
takes a single string argument, which is the name you wish the method or
attribute had instead of the one it actually has.

For example, given this IDL:

.. code:: notranslate

   interface InterfaceWithRenamedThings {
     [BinaryName="renamedMethod"]
     void someMethod();
     [BinaryName="renamedAttribute"]
     attribute long someAttribute;
   };

the corresponding C++ would be:

.. code:: brush:

   class InterfaceWithRenamedThings
   {
   public:
     void RenamedMethod();
     int32_t RenamedAttribute();
     void SetRenamedAttribute(int32_t);
   };

.. _Deprecatedtag:

``[Deprecated="tag"]``
~~~~~~~~~~~~~~~~~~~~~~

When deprecating an interface or method, the ``[Deprecated]`` annotation
causes the WebIDL compiler to insert code that generates deprecation
warnings.  This annotation can be added to interface methods or
interfaces.  Adding this to an interface causes a warning to be issued
the first time the object is constructed, or any static method on the
object is invoked.

The complete list of valid deprecation tags is maintained in
`nsDeprecatedOperationList.h <https://searchfox.org/mozilla-central/source/dom/base/nsDeprecatedOperationList.h>`__.
Each new tag requires that a localized string be defined, containing the
deprecation message to display.

.. _CrossOriginReadable:

``[CrossOriginReadable]``
~~~~~~~~~~~~~~~~~~~~~~~~~

Used to flag an attribute that, when read, will not have the same-origin
constraint tested: it can be read from a context with a different
origin.

.. _CrossOriginWrite:

``[CrossOriginWrite]``
~~~~~~~~~~~~~~~~~~~~~~

Used to flag an attribute that, when written, will not have the
same-origin constraint tested: it can be written from a context with a
different origin.

.. _CrossOriginCallable:

``[CrossOriginCallable]``
~~~~~~~~~~~~~~~~~~~~~~~~~

Used to flag a method that, when called, will not have the same-origin
constraint tested: it can be called from a context with a different
origin.

.. _SecureContext:

``[SecureContext]``
~~~~~~~~~~~~~~~~~~~

We implement the `standard extended
attribute <https://heycam.github.io/webidl/#SecureContext>`__ with a few
details specific to Gecko:

-  System principals are considered secure.
-  An extension poking at non-secured DOM objects will see APIs marked
   with ``[SecureContext]``.
-  XPConnect sandboxes doesn't see ``[SecureContext]`` APIs, but this
   may change in {{bug(1273687)}}.

.. _NeedsSubjectPrincipal_GetterNeedsSubjectPrincipal_SetterNeedsSubjectPrincipal:

``[NeedsSubjectPrincipal]``, ``[GetterNeedsSubjectPrincipal]``, ``[SetterNeedsSubjectPrincipal]``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Used to flag a method or an attribute that needs to know the subject
principal. This principal will be passed as argument.  If the interface
is not exposed on any worker global, the argument will be a
``nsIPrincipal&`` because a subject principal is always available in
mainthread globals.  If the interface is exposed on some worker global,
the argument will be a ``const Maybe<nsIPrincipal*>&``. This ``Maybe<>``
object contains the principal *only* on the main thread; when the method
is called on a {{domxref("Worker")}} thread, the value of the object
will be ``Nothing()``.  Note that, in workers, it is always possible to
retrieve the correct subject principal from the ``WorkerPrivate``
object, though it cannot be used on the worker thread.

``[NeedsSubjectPrincipal]`` applies to both methods and attributes; for
attributes it means both the getter and the setter need a subject
principal. ``[GetterNeedsSubjectPrincipal]`` applies only to attributes.
``[SetterNeedsSubjectPrincipal]`` applies only to non-readonly
attributes.

.. _NeedsCallerType:

``[NeedsCallerType]``
~~~~~~~~~~~~~~~~~~~~~

Used to flag a method or an attribute that needs to know the caller
type, in the ``mozilla::dom::CallerType`` sense.  This can be safely
used for APIs exposed in workers; there it will indicate whether the
worker involved is a ``ChromeWorker`` or not.  At the momen the only
possible caller types are ``System`` (representing system-principal
callers) and ``NonSystem``.

.. _Helper_objects:

Helper objects
--------------

The C++ side of the bindings uses a number of helper objects.

.. _Nullable:

``Nullable<T>``
~~~~~~~~~~~~~~~

``Nullable<>`` is a struct declared in
```Nullable.h`` <https://searchfox.org/mozilla-central/source/dom/bindings/Nullable.h>`__
and exported to ``mozilla/dom/Nullable.h`` that is used to represent
nullable values of types that don't have a natural way to represent
null.

``Nullable<T>`` has an ``IsNull()`` getter that returns whether null is
represented and a ``Value()`` getter that returns a ``const T&`` and can
be used to get the value when it's not null.

``Nullable<T>`` has a ``SetNull()`` setter that sets it as representing
null and two setters that can be used to set it to a value:
``"void SetValue(T)"`` (for setting it to a given value) and
``"T& SetValue()"`` for directly modifying the underlying ``T&``.

.. _Optional:

``Optional<T>``
~~~~~~~~~~~~~~~

``Optional<>`` is a struct declared in
```BindingDeclarations.h`` <https://searchfox.org/mozilla-central/source/dom/bindings/BindingDeclarations.h>`__
and exported to ``mozilla/dom/BindingDeclarations.h`` that is used to
represent optional arguments and dictionary members, but only those that
have no default value.

``Optional<T>`` has a ``WasPassed()`` getter that returns true if a
value is available. In that case, the ``Value()`` getter can be used to
get a ``const T&`` for the value.

.. _NonNull:

``NonNull<T>``
~~~~~~~~~~~~~~

``NonNull<T>`` is a struct declared in
```BindingUtils.h`` <https://searchfox.org/mozilla-central/source/dom/bindings/BindingUtils.h>`__
and exported to ``mozilla/dom/BindingUtils.h`` that is used to represent
non-null C++ objects. It has a conversion operator that produces ``T&``.

.. _OwningNonNull:

``OwningNonNull<T>``
~~~~~~~~~~~~~~~~~~~~

``OwningNonNull<T>`` is a struct declared in
```OwningNonNull.h`` <https://searchfox.org/mozilla-central/source/xpcom/base/OwningNonNull.h>`__
and exported to ``mozilla/OwningNonNull.h`` that is used to represent
non-null C++ objects and holds a strong reference to them. It has a
conversion operator that produces ``T&``.

.. _TypedArrays:

Typed arrays, arraybuffers, array buffer views
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``TypedArray.h`` is exported to ``mozilla/dom/TypedArray.h`` and exposes
structs that correspond to the various typed array types, as well as
``ArrayBuffer`` and ``ArrayBufferView``, all in the ``mozilla::dom``
namespace. Each struct has a ``Data()`` method that returns a pointer to
the relevant type (``uint8_t`` for ``ArrayBuffer`` and
``ArrayBufferView``) and a ``Length()`` method that returns the length
in units of ``*Data()``. So for example, ``Int32Array`` has a ``Data()``
returning ``int32_t*`` and a ``Length()`` that returns the number of
32-bit ints in the array..

.. _Sequence:

``Sequence<T>``
~~~~~~~~~~~~~~~

``Sequence<>`` is a type declared in
```BindingDeclarations.h`` <https://searchfox.org/mozilla-central/source/dom/bindings/BindingDeclarations.h>`__
and exported to ``mozilla/dom/BindingDeclarations.h`` that is used to
represent sequence arguments. It's some kind of typed array, but which
exact kind is opaque to consumers. This allows the binding code to
change the exact definition (e.g., to use auto arrays of different sizes
and so forth) without having to update all the callees.

.. _CallbackFunction:

``CallbackFunction``
~~~~~~~~~~~~~~~~~~~~

``CallbackFunction`` is a type declared in
`CallbackFunction.h <https://searchfox.org/mozilla-central/source/dom/bindings/CallbackFunction.h>`__
and exported to ``mozilla/dom/CallbackFunction.h`` that is used as a
common base class for all the generated callback function
representations. This class inherits from ``nsISupports``, and consumers
must make sure to cycle-collect it, since it keeps JS objects alive.

.. _CallbackInterface:

``CallbackInterface``
~~~~~~~~~~~~~~~~~~~~~

``CallbackInterface`` is a type declared in
`CallbackInterface.h <https://searchfox.org/mozilla-central/source/dom/bindings/CallbackInterface.h>`__
and exported to ``mozilla/dom/CallbackInterface.h`` that is used as a
common base class for all the generated callback interface
representations. This class inherits from ``nsISupports``, and consumers
must make sure to cycle-collect it, since it keeps JS objects alive.

.. _DOMString-helper:

``DOMString``
~~~~~~~~~~~~~

``DOMString`` is a class declared in
`BindingDeclarations.h <https://searchfox.org/mozilla-central/source/dom/bindings/BindingDeclarations.h>`__
and exported to ``mozilla/dom/BindingDeclarations.h`` that is used for
WebIDL ``DOMString`` return values. It has a conversion operator to
``nsString&`` so that it can be passed to methods that take that type or
``nsAString&``, but callees that care about performance, have an
``nsStringBuffer`` available, and promise to hold on to the
``nsStringBuffer`` at least until the binding code comes off the stack
can also take a ``DOMString`` directly for their string return value and
call its ``SetStringBuffer`` method with the ``nsStringBuffer`` and its
length. This allows the binding code to avoid extra reference-counting
of the string buffer in many cases, and allows it to take a faster
codepath even if it does end up having to addref the ``nsStringBuffer``.

.. _GlobalObject:

``GlobalObject``
~~~~~~~~~~~~~~~~

``GlobalObject`` is a class declared in
`BindingDeclarations.h <https://searchfox.org/mozilla-central/source/dom/bindings/BindingDeclarations.h>`__
and exported to ``mozilla/dom/BindingDeclarations.h`` that is used to
represent the global object for static attributes and operations
(including constructors). It has a ``Get()`` method that returns the
``JSObject*``  for the global and a ``GetAsSupports()`` method that
returns an ``nsISupports*`` for the global on the main thread, if such
is available. It also has a ``Context()`` method that returns the
``JSContext*`` the call is happening on. A caveat: the compartment of
the ``JSContext`` may not match the compartment of the global!

.. _Date-struct:

``Date``
~~~~~~~~

``Date`` is a class declared in
`BindingDeclarations.h <https://searchfox.org/mozilla-central/source/dom/bindings/BindingDeclarations.h>`__
and exported to ``mozilla/dom/BindingDeclarations.h`` that is used to
represent WebIDL Dates. It has a ``TimeStamp()`` method returning a
double which represents a number of milliseconds since the epoch, as
well as ``SetTimeStamp()`` methods that can be used to initialize it
with a double timestamp or a JS ``Date`` object. It also has a
``ToDateObject()`` method that can be used to create a new JS ``Date``.

.. _ErrorResult:

``ErrorResult``
~~~~~~~~~~~~~~~

``ErrorResult`` is a class declared in
`ErrorResult.h <https://searchfox.org/mozilla-central/source/dom/bindings/ErrorResult.h>`__
and exported to ``mozilla/ErrorResult.h`` that is used to represent
exceptions in WebIDL bindings. This has the following methods:

-  ``Throw``: allows throwing an ``nsresult``. The ``nsresult`` must be
   a failure code.
-  ``ThrowTypeError``: allows throwing a ``TypeError`` with the given
   error message. The list of allowed ``TypeError``\s and corresponding
   messages is in
   ```dom/bindings/Errors.msg`` <https://searchfox.org/mozilla-central/source/dom/bindings/Errors.msg>`__.
-  ``ThrowJSException``: allows throwing a preexisting JS exception
   value. However, the ``MightThrowJSException()`` method must be called
   before any such exceptions are thrown (even if no exception is
   thrown).
-  ``Failed``: checks whether an exception has been thrown on this
   ``ErrorResult``.
-  ``ErrorCode``: returns a failure ``nsresult`` representing (perhaps
   incompletely) the state of this ``ErrorResult``.
-  ``operator=``: takes an ``nsresult`` and acts like ``Throw`` if the
   result is an error code, and like a no-op otherwise (unless an
   exception has already been thrown, in which case it asserts). This
   should only be used for legacy code that has nsresult everywhere; we
   would like to get rid of this operator at some point.

.. _Events:

Events
------

Simple ``Event`` interfaces can be automatically generated by adding the
interface file to GENERATED_EVENTS_WEBIDL_FILES in the
appropriate dom/webidl/moz.build file. You can also take a simple
generated C++ file pair and use it to build a more complex event (i.e.,
one that has methods).

.. _Event_handler_attributes:

Event handler attributes
~~~~~~~~~~~~~~~~~~~~~~~~

A lot of interfaces define event handler attributes, like:

.. code:: notranslate

   attribute EventHandler onthingchange;

If you need to implement an event handler attribute for an interface, in
the definition (header file), you use the handy
"IMPL_EVENT_HANDLER" macro:

.. code:: notranslate

   IMPL_EVENT_HANDLER(onthingchange);

The "onthingchange" needs to be added to the StaticAtoms.py file:

.. code:: notranslate

   Atom("onthingchange", "onthingchange")

The actual implementation (.cpp) for firing the event would then look
something like:

.. code:: notranslate

   nsresult
   MyInterface::DispatchThingChangeEvent()
   {
      NS_NAMED_LITERAL_STRING(type, "thingchange");
      EventInit init;
      init.mBubbles = false;
      init.mCancelable = false;
      RefPtr<Event> event = Event::Constructor(this, type, init);
      event->SetTrusted(true);
      ErrorResult rv;
      DispatchEvent(*event, rv);
      return rv.StealNSResult();  // Assuming the caller cares about the return code.
   }

.. _Bindings.conf:

``Bindings.conf`` details
-------------------------

Write me. In particular, need to describe at least use of ``concrete``,
``prefable``, and ``addExternalInterface``.

.. _How_to_get_a_JSContext_passed_to_a_given_method:

How to get a JSContext passed to a given method
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In some rare cases you may need a ``JSContext*`` argument to be passed
to a C++ method that wouldn't otherwise get such an argument. To see how
to achieve this, search for ``implicitJSContext`` in
`dom/bindings/Bindings.conf <#Bindings.conf>`__.

.. _Implementing_WebIDL_using_Javascript:

Implementing WebIDL using Javascript
------------------------------------

.. warning::

   Implementing WebIDL using Javascript is deprecated. New interfaces
   should always be implemented in C++!

It is possible to implement WebIDL interfaces in JavaScript within Gecko
-- however, **this is limited to interfaces that are not exposed in Web
Workers**. When the binding occurs, two objects are created:

-  *Content-exposed object:* what gets exposed to the web page.
-  *Implementation object:* running as a chrome-privileged script. This
   allows the implementation object to have various APIs that the
   content-exposed object does not.

Because there are two types of objects, you have to be careful about
which object you are creating.

.. _Creating_JS-implemented_WebIDL_objects:

Creating JS-implemented WebIDL objects
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To create a JS-implemented WebIDL object, one must create both the
chrome-side implementation object and the content-side page-exposed
object. There are three ways to do this.

.. _Using_the_WebIDL_constructor:

Using the WebIDL constructor
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If the interface has a constructor, a content-side object can be created
by getting that constructor from the relevant content window and
invoking it. For example:

.. code:: brush:

   var contentObject = new contentWin.RTCPeerConnection();

The returned object will be an Xray wrapper for the content-side object.
Creating the object this way will automatically create the chrome-side
object using its contractID.

This method is limited to the constructor signatures exposed to
webpages. Any additional configuration of the object needs to be done
methods on the interface.

Creating many objects this way can be slow due to the createInstance
overhead involved.

.. _Using_a__create_method:

Using a ``_create`` method
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A content-side object can be created for a given chrome-side object by
invoking the static ``_create`` method on the interface. This method
takes two arguments: the content window in which to create the object
and the chrome-side object to use. For example:

.. code:: brush:

   var contentObject = RTCPeerConnection._create(contentWin, new
   MyPeerConnectionImpl());

However, if you are in a JS component, you may only be able to get to
the correct interface object via some window object. In this case, the
code would look more like:

.. code:: brush:

   var contentObject = contentWin.RTCPeerConnection._create(contentWin,
   new MyPeerConnectionImpl());

Creating the object this way will not invoke its ``__init`` method or
``init`` method.

.. _By_returning_a_chrome-side_object_from_a_JS-implemented_WebIDL_method:

By returning a chrome-side object from a JS-implemented WebIDL method
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If a JS-implemented WebIDL method is declared as returning a
JS-implemented interface, then a non-WebIDL object returned from that
method will be treated as the chrome-side part of a JS-implemented
WebIdL object and the content-side part will be automatically created.

Creating the object this way will not invoke its ``__init`` method or
``init`` method.

.. _Implementing_a_WebIDL_object_in_JavaScript:

Implementing a WebIDL object in JavaScript
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To implement a WebIDL interface in JavaScript, first add a WebIDL file,
in the same way as you would for a C++-implemented interface. To support
implementation in JS, you must add an extended attribute
``JSImplementation="CONTRACT_ID_STRING"`` on your interface, where
CONTRACT_ID_STRING is the XPCOM component contract ID of the JS
implementation -- note ";1" is just a Mozilla convention for versioning
APIs. Here's an example:

.. code:: notranslate

   [Constructor(optional long firstNumber), JSImplementation="@mozilla.org/my-number;1"]
   interface MyNumber {
     attribute long value;
     readonly attribute long otherValue;
     void doNothing();
   };

Next, create an XPCOM component that implements this interface. `Basic
directions </en-US/docs/How_to_Build_an_XPCOM_Component_in_Javascript>`__
for how to do this can be found elsewhere on MDN. Use the same contract
ID as you specified in the WebIDL file. The class ID doesn't matter,
except that it should be a newly generated one. For ``QueryInterface``,
you only need to implement ``nsISupports``, not anything corresponding
to the WebIDL interface. The name you use for the XPCOM component should
be distinct from the name of the interface, to avoid confusing error
messages.

WebIDL attributes are implemented as properties on the JS object or its
prototype chain, whereas WebIDL methods are implemented as methods on
the object or prototype. Note that any other instances of the interface
that you are passed in as arguments are the full web-facing version of
the object, and not the JS implementation, so you currently cannot
access any private data.

The WebIDL constructor invocation will first create your object. If the
XPCOM component implements ``nsIDOMGlobalPropertyInitializer``, then
the object's ``init`` method will be invoked with a single argument:
the content window the constructor came from. This allows the JS
implementation to know which content window it's associated with.
The ``init`` method should not return anything. After this, the
content-side object will be created. Then,if there are any constructor
arguments, the object's ``__init`` method will be invoked, with the
constructor arguments as its arguments.

.. _Static_Members:

Static Members
~~~~~~~~~~~~~~

Static attributes and methods are not supported on JS-implemented WebIDL
(see `bug
863952 <https://bugzilla.mozilla.org/show_bug.cgi?id=863952>`__).
However, with the changes in `bug
1172785 <https://bugzilla.mozilla.org/show_bug.cgi?id=1172785>`__ you
can route static methods to a C++ implementation on another object using
a ``StaticClassOverride`` annotation.  This annotation includes the
full, namespace-qualified name of the class that contains an
implementation of the named method.  The include for that class must be
found in a directory based on its name.

.. code:: notranslate

   [JSImplementation="@mozilla.org/dom/foo;1"]
   interface Foo {
     [StaticClassOverride="mozilla::dom::OtherClass"]
     static Promise<void> doSomething();
   };

Rather than calling into a method on the JS implementation; calling
``Foo.doSomething()`` will result in calling
``mozilla::dom::OtherClass::DoSomething()``.

.. _Checking_for_Permissions_or_Preferences:

Checking for Permissions or Preferences
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

With JS-implemented WebIDL, the ``init`` method should only return
undefined. If any other value, such as ``null``, is returned, the
bindings code will assert or crash. In other words, it acts like it has
a "void" return type. Preference or permission checking should be
implemented by adding an extended attribute to the WebIDL interface.
This has the advantage that if the check fails, the constructor or
object will not show up at all.

For preference checking, add an extended attribute
``Pref="myPref.enabled"`` where ``myPref.enabled`` is the preference
that should be checked. ``SettingsLock`` is an example of this.

For permissions or other kinds of checking, add an extended attribute
``Func="MyPermissionChecker"`` where ``MyPermissionChecker`` is a
function implemented in C++ that returns true if the interface should be
enabled. This function can do whatever checking is needed. One example
of this is ``PushManager``.

.. _Example:

Example
~~~~~~~

Here's an example JS implementation of the above interface. The
``invisibleValue`` field will not be accessible to web content, but is
usable by the doNothing() method.

.. code:: brush:

   Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

   function MyNumberInner() {
     this.value = 111;
     this.invisibleValue = 12345;
   }

   MyNumberInner.prototype = {
     classDescription: "Get my number XPCOM Component",
     classID: Components.ID("{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}"), // dummy UUID
     contractID: "@mozilla.org/my-number;1",
     QueryInterface: XPCOMUtils.generateQI([Components.interfaces.nsISupports]),
     doNothing: function() {},
     get otherValue() { return this.invisibleValue - 4; },
     __init: function(firstNumber) {
       if (arguments.length > 0) {
         this.value = firstNumber;
       }
     }
   }

   var components = [MyNumberInner];
   var NSGetFactory = XPCOMUtils.generateNSGetFactory(components);

Finally, add a component and a contract and whatever other manifest
stuff you need to implement an XPCOM component.

.. _Guarantees_provided_by_bindings:

Guarantees provided by bindings
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When implementing a WebIDL interface in JavaScript, certain guarantees
will be provided by the binding implementation. For example, string or
numeric arguments will actually be primitive strings or numbers.
Dictionaries will contain only the properties that they are declared to
have, and they will have the right types. Interface arguments will
actually be objects implementing that interface.

What the bindings will NOT guarantee is much of anything about
``object`` and ``any`` arguments. They will get cross-compartment
wrappers that make touching them from chrome code not be an immediate
security bug, but otherwise they can have quite surprising behavior if
the page is trying to be malicious. Try to avoid using these types if
possible.

.. _Accessing_the_content_object_from_the_implementation:

Accessing the content object from the implementation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If the JS implementation of the WebIDL interface needs to access the
content object, it is available as a property called ``__DOM_IMPL__`` on
the chrome implementation object. This property only appears after the
content-side object has been created. So it is available in ``__init``
but not in ``init``.

.. _Determining_the_principal_of_the_caller_that_invoked_the_WebIDL_API:

Determining the principal of the caller that invoked the WebIDL API
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This can be done by calling
``Component.utils.getWebIDLCallerPrincipal()``.

.. _Throwing_exceptions_from_JS-implemented_APIs:

Throwing exceptions from JS-implemented APIs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

There are two reasons a JS implemented API might throw. The first reason
is that some unforeseen condition occurred and the second is that a
specification requires an exception to be thrown.

When throwing for an unforeseen condition, the exception will be
reported to the console, and a sanitized NS_ERROR_UNEXPECTED exception
will be thrown to the calling content script, with the file/line of the
content code that invoked your API. This will avoid exposing chrome URIs
and other implementation details to the content code.

When throwing because a specification requires an exception, you need to
create the exception from the window your WebIDL object is associated
with (the one that was passed to your ``init`` method). The binding code
will then rethrow that exception to the web page.  An example of how
this could work:

.. code:: brush:

   if (!isValid(passedInObject)) {
     throw new this.contentWindow.TypeError("Object is invalid");
   }

or

.. code:: brush:

   if (!isValid(passedInObject)) {
     throw new this.contentWindow.DOMException("Object is invalid", "InvalidStateError");
   }

depending on which exact exception the specification calls for throwing
in this situation.

In some cases you may need to perform operations whose exception message
you just want to propagate to the content caller. This can be done like
so:

.. code:: brush:

   try {
     someOperationThatCanThrow();
   } catch (e) {
     throw new this.contentWindow.Error(e.message);
   }

.. _Inheriting_from_interfaces_implemented_in_C:

Inheriting from interfaces implemented in C++
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It's possible to have an interface implemented in JavaScript inherit
from an interface implemented in C++. To do so, simply have one
interface inherit from the other and the bindings code will
auto-generate a C++ object inheriting from the implementation of the
parent interface. The class implementing the parent interface will need
a constructor that takes an ``nsPIDOMWindow*`` (though it doesn't have
to do anything with that argument).

If the class implementing the parent interface is abstract and you want
to use a specific concrete class as the implementation to inherit from,
you will need to add a ``defaultImpl`` annotation to the descriptor for
the parent interface in ``Bindings.conf``. The value of the annotation
is the C++ class to use as the parent for JS-implemented descendants; if
``defaultImpl`` is not specified, the ``nativeType`` will be used.

For example, consider this interface that we wish to implement in
JavaScript:

.. code:: notranslate

   [JSImplementation="some-contract"]
   interface MyEventTarget : EventTarget {
     attribute EventHandler onmyevent;
     void dispatchTheEvent(); // Sends a "myevent" event to this EventTarget
   }

The implementation would look something like this, ignoring most of the
XPCOM boilerplate:

.. code:: brush:

   function MyEventTargetImpl() {
   }
   MyEventTargetImpl.prototype = {
     // QI to nsIDOMGlobalPropertyInitializer so we get init() called on us.
     QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),

     init: function(contentWindow) {
       this.contentWindow = contentWindow;
     },

     get onmyevent() {
       return this.__DOM_IMPL__.getEventHandler("onmyevent");
     },

     set onmyevent(handler) {
       this.__DOM_IMPL__.setEventHandler("onmyevent", handler);
     },

     dispatchTheEvent: function() {
       var event = new this.contentWindow.Event("myevent");
       this.__DOM_IMPL__.dispatchEvent(event);
     },
   };

The implementation would automatically support the API exposed on
``EventTarget`` (so, for example, ``addEventListener``). Calling the
``dispatchTheEvent`` method would cause dispatch of an event that
content script can see via listeners it has added.

Note that in this case the chrome implementation is relying on some
``[ChromeOnly]`` methods on EventTarget that were added specifically to
make it possible to easily implement event handlers. Other cases can do
similar things as needed.
