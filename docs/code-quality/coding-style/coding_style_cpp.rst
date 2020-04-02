================
C++ Coding style
================


This document attempts to explain the basic styles and patterns used in
the Mozilla codebase. New code should try to conform to these standards,
so it is as easy to maintain as existing code. There are exceptions, but
it's still important to know the rules!

This article is particularly for those new to the Mozilla codebase, and
in the process of getting their code reviewed. Before requesting a
review, please read over this document, making sure that your code
conforms to recommendations.

.. container:: blockIndicator warning

   Firefox code base uses the `Google Coding style for C++
   code <https://google.github.io/styleguide/cppguide.html>`__


Formatting code
---------------

Formatting is done automatically via clang-format, and controlled via in-tree
configuration files. See :ref:`Formatting C++ Code With clang-format`
for more information.

Unix-style linebreaks (``\n``), not Windows-style (``\r\n``). You can
convert patches, with DOS newlines to Unix via the ``dos2unix`` utility,
or your favorite text editor.

Static analysis
---------------

Several of the rules in the Google C++ coding styles and the additions mentioned below
can be checked via clang-tidy (some rules are from the upstream clang-tidy, some are
provided via a mozilla-specific plugin). Some of these checks also allow fixes to
be automatically applied.

``mach static-analysis`` provides a convenient way to run these checks. For example,
for the check called ``google-readability-braces-around-statements``, you can run:

.. code-block:: shell

   ./mach static-analysis check --checks="-*,google-readability-braces-around-statements" --fix <file>

It may be necessary to reformat the files after automatically applying fixes, see
:ref:`Formatting C++ Code With clang-format`.

Additional rules
----------------

*The norms in this section should be followed for new code. For existing code,
use the prevailing style in a file or module, ask the owner if you are
in another team's codebase or it's not clear what style to use.*




Control structures
~~~~~~~~~~~~~~~~~~

Always brace controlled statements, even a single-line consequent of
``if else else``. This is redundant, typically, but it avoids dangling
else bugs, so it's safer at scale than fine-tuning.

Examples:

.. code-block:: cpp

   if (...) {
   } else if (...) {
   } else {
   }

   while (...) {
   }

   do {
   } while (...);

   for (...; ...; ...) {
   }

   switch (...) {
     case 1: {
       // When you need to declare a variable in a switch, put the block in braces.
       int var;
       break;
     }
     case 2:
       ...
       break;
     default:
       break;
   }

``else`` should only ever be followed by ``{`` or ``if``; i.e., other
control keywords are not allowed and should be placed inside braces.

.. note::

   For this rule, clang-tidy provides the ``google-readability-braces-around-statements``
   check with autofixes.


C++ namespaces
~~~~~~~~~~~~~~

Mozilla project C++ declarations should be in the ``mozilla``
namespace. Modules should avoid adding nested namespaces under
``mozilla``, unless they are meant to contain names which have a high
probability of colliding with other names in the code base. For example,
``Point``, ``Path``, etc. Such symbols can be put under
module-specific namespaces, under ``mozilla``, with short
all-lowercase names. Other global namespaces besides ``mozilla`` are
not allowed.

No ``using`` directives are allowed in header files, except inside class
definitions or functions. (We don't want to pollute the global scope of
compilation units that use the header file.)

.. note::

   For parts of this rule, clang-tidy provides the ``google-global-names-in-headers``
   check. It only detects ``using namespace`` directives in the global namespace.


``using namespace ...;`` is only allowed in ``.cpp`` files after all
``#include``\ s. Prefer to wrap code in ``namespace ... { ... };``
instead, if possible. ``using namespace ...;``\ should always specify
the fully qualified namespace. That is, to use ``Foo::Bar`` do not
write ``using namespace Foo; using namespace Bar;``, write
``using namespace Foo::Bar;``

Use nested namespaces (ex: ``namespace mozilla::widget {``

.. note::

   clang-tidy provides the ``modernize-concat-nested-namespaces``
   check with autofixes.


Anonymous namespaces
~~~~~~~~~~~~~~~~~~~~

We prefer using ``static``, instead of anonymous C++ namespaces. This may
change once there is better debugger support (especially on Windows) for
placing breakpoints, etc. on code in anonymous namespaces. You may still
use anonymous namespaces for things that can't be hidden with ``static``,
such as types, or certain objects which need to be passed to template
functions.


C++ classes
~~~~~~~~~~~~

.. code-block:: cpp

   namespace mozilla {

   class MyClass : public A
   {
     ...
   };

   class MyClass
     : public X
     , public Y
   {
   public:
     MyClass(int aVar, int aVar2)
       : mVar(aVar)
       , mVar2(aVar2)
     {
        ...
     }

     // Special member functions, like constructors, that have default bodies
     // should use '= default' annotation instead.
     MyClass() = default;

     // Unless it's a copy or move constructor or you have a specific reason to allow
     // implicit conversions, mark all single-argument constructors explicit.
     explicit MyClass(OtherClass aArg)
     {
       ...
     }

     // This constructor can also take a single argument, so it also needs to be marked
     // explicit.
     explicit MyClass(OtherClass aArg, AnotherClass aArg2 = AnotherClass())
     {
       ...
     }

     int LargerFunction()
     {
       ...
       ...
     }

   private:
     int mVar;
   };

   } // namespace mozilla

Define classes using the style given above.

.. note::

   For the rule on ``= default``, clang-tidy provides the ``modernize-use-default``
   check with autofixes.

   For the rule on explicit constructors and conversion operators, clang-tidy
   provides the ``mozilla-implicit-constructor`` check.

Existing classes in the global namespace are named with a short prefix
(For example, ``ns``) as a pseudo-namespace.


Methods and functions
~~~~~~~~~~~~~~~~~~~~~


C/C++
^^^^^

In C/C++, method names should use ``UpperCamelCase``.

Getters that never fail, and never return null, are named ``Foo()``,
while all other getters use ``GetFoo()``. Getters can return an object
value, via a ``Foo** aResult`` outparam (typical for an XPCOM getter),
or as an ``already_AddRefed<Foo>`` (typical for a WebIDL getter,
possibly with an ``ErrorResult& rv`` parameter), or occasionally as a
``Foo*`` (typical for an internal getter for an object with a known
lifetime). See `the bug 223255 <https://bugzilla.mozilla.org/show_bug.cgi?id=223255>`_
for more information.

XPCOM getters always return primitive values via an outparam, while
other getters normally use a return value.

Method declarations must use, at most, one of the following keywords:
``virtual``, ``override``, or ``final``. Use ``virtual`` to declare
virtual methods, which do not override a base class method with the same
signature. Use ``override`` to declare virtual methods which do
override a base class method, with the same signature, but can be
further overridden in derived classes. Use ``final`` to declare virtual
methods which do override a base class method, with the same signature,
but can NOT be further overridden in the derived classes. This should
help the person reading the code fully understand what the declaration
is doing, without needing to further examine base classes.

.. note::

   For the rule on ``virtual/override/final``, clang-tidy provides the
   ``modernize-use-override`` check with autofixes.


Operators
~~~~~~~~~

The unary keyword operator ``sizeof``, should have its operand parenthesized
even if it is an expression; e.g. ``int8_t arr[64]; memset(arr, 42, sizeof(arr));``.


Literals
~~~~~~~~

Use ``\uXXXX`` unicode escapes for non-ASCII characters. The character
set for XUL, DTD, script, and properties files is UTF-8, which is not easily
readable.


Prefixes
~~~~~~~~

Follow these naming prefix conventions:


Variable prefixes
^^^^^^^^^^^^^^^^^

-  k=constant (e.g. ``kNC_child``). Not all code uses this style; some
   uses ``ALL_CAPS`` for constants.
-  g=global (e.g. ``gPrefService``)
-  a=argument (e.g. ``aCount``)
-  C++ Specific Prefixes

   -  s=static member (e.g. ``sPrefChecked``)
   -  m=member (e.g. ``mLength``)
   -  e=enum variants (e.g. ``enum Foo { eBar, eBaz }``). Enum classes
      should use ``CamelCase`` instead (e.g.
      ``enum class Foo { Bar, Baz }``).


Global functions/macros/etc
^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  Macros begin with ``MOZ_``, and are all caps (e.g.
   ``MOZ_WOW_GOODNESS``). Note that older code uses the ``NS_`` prefix;
   while these aren't being changed, you should only use ``MOZ_`` for
   new macros. The only exception is if you're creating a new macro,
   which is part of a set of related macros still using the old ``NS_``
   prefix. Then you should be consistent with the existing macros.


Error Variables
^^^^^^^^^^^^^^^

-  Local variables that are assigned ``nsresult`` result codes should be named ``rv``
   (i.e., e.g., not ``res``, not ``result``, not ``foo``). `rv` should not be
   used for bool or other result types.
-  Local variables that are assigned ``bool`` result codes should be named `ok`.


C/C++ practices
---------------

-  **Have you checked for compiler warnings?** Warnings often point to
   real bugs. `Many of them <https://searchfox.org/mozilla-central/source/build/moz.configure/warnings.configure>`__
   are enabled by default in the build system.
-  In C++ code, use ``nullptr`` for pointers. In C code, using ``NULL``
   or ``0`` is allowed.

.. note::

   For the C++ rule, clang-tidy provides the ``modernize-use-nullptr`` check
   with autofixes.

-  Don't use ``PRBool`` and ``PRPackedBool`` in C++, use ``bool``
   instead.
-  For checking if a ``std`` container has no items, don't use
   ``size()``, instead use ``empty()``.
-  When testing a pointer, use ``(!myPtr)`` or ``(myPtr)``;
   don't use ``myPtr != nullptr`` or ``myPtr == nullptr``.
-  Do not compare ``x == true`` or ``x == false``. Use ``(x)`` or
   ``(!x)`` instead. ``if (x == true)`` may have semantics different from
   ``if (x)``!

.. note::

   clang-tidy provides the ``readability-simplify-boolean-expr`` check
   with autofixes that checks for these and some other boolean expressions
   that can be simplified.

-  In general, initialize variables with ``nsFoo aFoo = bFoo,`` and not
   ``nsFoo aFoo(bFoo)``.

   -  For constructors, initialize member variables with : ``nsFoo
      aFoo(bFoo)`` syntax.

-  To avoid warnings created by variables used only in debug builds, use
   the
   `DebugOnly<T> <https://developer.mozilla.org/docs/Mozilla/Debugging/DebugOnly%3CT%3E>`__
   helper when declaring them.
-  You should `use the static preference
   API <https://developer.mozilla.org/docs/Mozilla/Preferences/Using_preferences_from_application_code>`__ for
   working with preferences.
-  One-argument constructors, that are not copy or move constructors,
   should generally be marked explicit. Exceptions should be annotated
   with ``MOZ_IMPLICIT``.
-  Use ``char32_t`` as the return type or argument type of a method that
   returns or takes as argument a single Unicode scalar value. (Don't
   use UTF-32 strings, though.)
-  Forward-declare classes in your header files, instead of including
   them, whenever possible. For example, if you have an interface with a
   ``void DoSomething(nsIContent* aContent)`` function, forward-declare
   with ``class nsIContent;`` instead of ``#include "nsIContent.h"``
-  Include guards are named per the Google coding style and should not
   include a leading ``MOZ_`` or ``MOZILLA_``. For example
   ``dom/media/foo.h`` would use the guard ``DOM_MEDIA_FOO_H_``.
-  Avoid the usage of ``typedef``, instead, please use ``using`` instead.

.. note::

   For parts of this rule, clang-tidy provides the ``modernize-use-using``
   check with autofixes.


COM and pointers
----------------

-  Use ``nsCOMPtr<>``
   If you don't know how to use it, start looking in the code for
   examples. The general rule, is that the very act of typing
   ``NS_RELEASE`` should be a signal to you to question your code:
   "Should I be using ``nsCOMPtr`` here?". Generally the only valid use
   of ``NS_RELEASE`` is when you are storing refcounted pointers in a
   long-lived datastructure.
-  Declare new XPCOM interfaces using `XPIDL <https://developer.mozilla.org/docs/Mozilla/Tech/XPIDL>`__, so they
   will be scriptable.
-  Use `nsCOMPtr <https://developer.mozilla.org/docs/Mozilla/Tech/XPCOM/Reference/Glue_classes/nsCOMPtr>`__ for strong references, and
   `nsWeakPtr <https://developer.mozilla.org/docs/Mozilla/Tech/XPCOM/Weak_reference>`__ for weak references.
-  Don't use ``QueryInterface`` directly. Use ``CallQueryInterface`` or
   ``do_QueryInterface`` instead.
-  Use `Contract
   IDs <news://news.mozilla.org/3994AE3E.D96EF810@netscape.com>`__,
   instead of CIDs with ``do_CreateInstance``/``do_GetService``.
-  Use pointers, instead of references for function out parameters, even
   for primitive types.


IDL
---


Use leading-lowercase, or "interCaps"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When defining a method or attribute in IDL, the first letter should be
lowercase, and each following word should be capitalized. For example:

.. code-block:: cpp

   long updateStatusBar();


Use attributes wherever possible
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Whenever you are retrieving or setting a single value, without any
context, you should use attributes. Don't use two methods when you could
use an attribute. Using attributes logically connects the getting and
setting of a value, and makes scripted code look cleaner.

This example has too many methods:

.. code-block:: cpp

   interface nsIFoo : nsISupports
   {
       long getLength();
       void setLength(in long length);
       long getColor();
   };

The code below will generate the exact same C++ signature, but is more
script-friendly.

.. code-block:: cpp

   interface nsIFoo : nsISupports
   {
       attribute long length;
       readonly attribute long color;
   };


Use Java-style constants
~~~~~~~~~~~~~~~~~~~~~~~~

When defining scriptable constants in IDL, the name should be all
uppercase, with underscores between words:

.. code-block:: cpp

   const long ERROR_UNDEFINED_VARIABLE = 1;


See also
~~~~~~~~

For details on interface development, as well as more detailed style
guides, see the `Interface development
guide <https://developer.mozilla.org/docs/Mozilla/Developer_guide/Interface_development_guide>`__.


Error handling
--------------


Check for errors early and often
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Every time you make a call into an XPCOM function, you should check for
an error condition. You need to do this even if you know that call will
never fail. Why?

-  Someone may change the callee in the future to return a failure
   condition.
-  The object in question may live on another thread, another process,
   or possibly even another machine. The proxy could have failed to make
   your call in the first place.

Also, when you make a new function which is failable (i.e. it will
return a ``nsresult`` or a ``bool`` that may indicate an error), you should
explicitly mark the return value should always be checked. For example:

::

   // for IDL.
   [must_use] nsISupports
   create();

   // for C++, add this in *declaration*, do not add it again in implementation.
   [[nodiscard]] nsresult
   DoSomething();

There are some exceptions:

-  Predicates or getters, which return ``bool`` or ``nsresult``.
-  IPC method implementation (For example, ``bool RecvSomeMessage()``).
-  Most callers will check the output parameter, see below.

.. code-block:: cpp

   nsresult
   SomeMap::GetValue(const nsString& key, nsString& value);

If most callers need to check the output value first, then adding
``[[nodiscard]]`` might be too verbose. In this case, change the return value
to void might be a reasonable choice.

There is also a static analysis attribute ``MOZ_MUST_USE_TYPE``, which can
be added to class declarations, to ensure that those declarations are
always used when they are returned.


Use the NS_WARN_IF macro when errors are unexpected.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``NS_WARN_IF`` macro can be used to issue a console warning, in debug
builds if the condition fails. This should only be used when the failure
is unexpected and cannot be caused by normal web content.

If you are writing code which wants to issue warnings when methods fail,
please either use ``NS_WARNING`` directly, or use the new ``NS_WARN_IF`` macro.

.. code-block:: cpp

   if (NS_WARN_IF(somethingthatshouldbefalse)) {
     return NS_ERROR_INVALID_ARG;
   }

   if (NS_WARN_IF(NS_FAILED(rv))) {
     return rv;
   }

Previously, the ``NS_ENSURE_*`` macros were used for this purpose, but
those macros hide return statements, and should not be used in new code.
(This coding style rule isn't generally agreed, so use of ``NS_ENSURE_*``
can be valid.)


Return from errors immediately
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In most cases, your knee-jerk reaction should be to return from the
current function, when an error condition occurs. Don't do this:

.. code-block:: cpp

   rv = foo->Call1();
   if (NS_SUCCEEDED(rv)) {
     rv = foo->Call2();
     if (NS_SUCCEEDED(rv)) {
       rv = foo->Call3();
     }
   }
   return rv;

Instead, do this:

.. code-block:: cpp

   rv = foo->Call1();
   if (NS_FAILED(rv)) {
     return rv;
   }

   rv = foo->Call2();
   if (NS_FAILED(rv)) {
     return rv;
   }

   rv = foo->Call3();
   if (NS_FAILED(rv)) {
     return rv;
   }

Why? Error handling should not obfuscate the logic of the code. The
author's intent, in the first example, was to make 3 calls in
succession. Wrapping the calls in nested if() statements, instead
obscured the most likely behavior of the code.

Consider a more complicated example to hide a bug:

.. code-block:: cpp

   bool val;
   rv = foo->GetBooleanValue(&val);
   if (NS_SUCCEEDED(rv) && val) {
     foo->Call1();
   } else {
     foo->Call2();
   }

The intent of the author, may have been, that ``foo->Call2()`` would only
happen when val had a false value. In fact, ``foo->Call2()`` will also be
called, when ``foo->GetBooleanValue(&val)`` fails. This may, or may not,
have been the author's intent. It is not clear from this code. Here is
an updated version:

.. code-block:: cpp

   bool val;
   rv = foo->GetBooleanValue(&val);
   if (NS_FAILED(rv)) {
     return rv;
   }
   if (val) {
     foo->Call1();
   } else {
     foo->Call2();
   }

In this example, the author's intent is clear, and an error condition
avoids both calls to ``foo->Call1()`` and ``foo->Call2();``

*Possible exceptions:* Sometimes it is not fatal if a call fails. For
instance, if you are notifying a series of observers that an event has
fired, it might be trivial that one of these notifications failed:

.. code-block:: cpp

   for (size_t i = 0; i < length; ++i) {
     // we don't care if any individual observer fails
     observers[i]->Observe(foo, bar, baz);
   }

Another possibility, is you are not sure if a component exists or is
installed, and you wish to continue normally, if the component is not
found.

.. code-block:: cpp

   nsCOMPtr<nsIMyService> service = do_CreateInstance(NS_MYSERVICE_CID, &rv);
   // if the service is installed, then we'll use it.
   if (NS_SUCCEEDED(rv)) {
     // non-fatal if this fails too, ignore this error.
     service->DoSomething();

     // this is important, handle this error!
     rv = service->DoSomethingImportant();
     if (NS_FAILED(rv)) {
       return rv;
     }
   }

   // continue normally whether or not the service exists.


Strings
-------

-  String arguments to functions should be declared as ``nsAString``.
-  Use ``EmptyString()`` and ``EmptyCString()`` instead of
   ``NS_LITERAL_STRING("")`` or ``nsAutoString empty;``.
-  Use ``str.IsEmpty()`` instead of ``str.Length() == 0``.
-  Use ``str.Truncate()`` instead of ``str.SetLength(0)`` or
   ``str.Assign(EmptyString())``.
-  For constant strings, use ``NS_LITERAL_STRING("...")`` instead of
   ``NS_ConvertASCIItoUCS2("...")``, ``AssignWithConversion("...")``,
   ``EqualsWithConversion("...")``, or ``nsAutoString()``
-  To compare a string with a literal, use ``.EqualsLiteral("...")``.
-  Don't use functions from ``ctype.h`` (``isdigit()``, ``isalpha()``,
   etc.) or from ``strings.h`` (``strcasecmp()``, ``strncasecmp()``).
   These are locale-sensitive, which makes them inappropriate for
   processing protocol text. At the same time, they are too limited to
   work properly for processing natural-language text. Use the
   alternatives in ``mozilla/TextUtils.h`` and in ``nsUnicharUtils.h``
   in place of ``ctype.h``. In place of ``strings.h``, prefer the
   ``nsStringComparator`` facilities for comparing strings or if you
   have to work with zero-terminated strings, use ``nsCRT.h`` for
   ASCII-case-insensitive comparison.


Use the ``Auto`` form of strings for local values
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When declaring a local, short-lived ``nsString`` class, always use
``nsAutoString`` or ``nsAutoCString``. These pre-allocate a 64-byte
buffer on the stack, and avoid fragmenting the heap. Don't do this:

.. code-block:: cpp

   nsresult
   foo()
   {
     nsCString bar;
     ..
   }

instead:

.. code-block:: cpp

   nsresult
   foo()
   {
     nsAutoCString bar;
     ..
   }


Be wary of leaking values from non-XPCOM functions that return char\* or PRUnichar\*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It is an easy trap to return an allocated string, from an internal
helper function, and then using that function inline in your code,
without freeing the value. Consider this code:

.. code-block:: cpp

   static char*
   GetStringValue()
   {
     ..
     return resultString.ToNewCString();
   }

     ..
     WarnUser(GetStringValue());

In the above example, ``WarnUser`` will get the string allocated from
``resultString.ToNewCString()`` and throw away the pointer. The
resulting value is never freed. Instead, either use the string classes,
to make sure your string is automatically freed when it goes out of
scope, or make sure that your string is freed.

Automatic cleanup:

.. code-block:: cpp

   static void
   GetStringValue(nsAWritableCString& aResult)
   {
     ..
     aResult.Assign("resulting string");
   }

     ..
     nsAutoCString warning;
     GetStringValue(warning);
     WarnUser(warning.get());

Free the string manually:

.. code-block:: cpp

   static char*
   GetStringValue()
   {
     ..
     return resultString.ToNewCString();
   }

     ..
     char* warning = GetStringValue();
     WarnUser(warning);
     nsMemory::Free(warning);


Use MOZ_UTF16() or NS_LITERAL_STRING() to avoid runtime string conversion
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It is very common to need to assign the value of a literal string, such
as ``"Some String"``, into a unicode buffer. Instead of using ``nsString``'s
``AssignLiteral`` and ``AppendLiteral``, use ``NS_LITERAL_STRING()``
instead. On most platforms, this will force the compiler to compile in a
raw unicode string, and assign it directly.

Incorrect:

.. code-block:: cpp

   nsAutoString warning;
   warning.AssignLiteral("danger will robinson!");
   ...
   foo->SetStringValue(warning);
   ...
   bar->SetUnicodeValue(warning.get());

Correct:

.. code-block:: cpp

   NS_NAMED_LITERAL_STRING(warning, "danger will robinson!");
   ...
   // if you'll be using the 'warning' string, you can still use it as before:
   foo->SetStringValue(warning);
   ...
   bar->SetUnicodeValue(warning.get());

   // alternatively, use the wide string directly:
   foo->SetStringValue(NS_LITERAL_STRING("danger will robinson!"));
   ...
   bar->SetUnicodeValue(MOZ_UTF16("danger will robinson!"));

.. note::

   Note: Named literal strings cannot yet be static.


Usage of PR_(MAX|MIN|ABS|ROUNDUP) macro calls
---------------------------------------------

Use the standard-library functions (``std::max``), instead of
``PR_(MAX|MIN|ABS|ROUNDUP)``.

Use ``mozilla::Abs`` instead of ``PR_ABS``. All ``PR_ABS`` calls in C++ code have
been replaced with ``mozilla::Abs`` calls, in `bug
847480 <https://bugzilla.mozilla.org/show_bug.cgi?id=847480>`__. All new
code in ``Firefox/core/toolkit`` needs to ``#include "nsAlgorithm.h"`` and
use the ``NS_foo`` variants instead of ``PR_foo``, or
``#include "mozilla/MathAlgorithms.h"`` for ``mozilla::Abs``.
