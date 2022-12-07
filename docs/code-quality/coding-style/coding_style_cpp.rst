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

   The Firefox code base adopts parts of the `Google Coding style for C++
   code <https://google.github.io/styleguide/cppguide.html>`__, but not all of its rules.
   A few rules are followed across the code base, others are intended to be
   followed in new or significantly revised code. We may extend this list in the
   future, when we evaluate the Google Coding Style for C++ Code further and/or update
   our coding practices. However, the plan is not to adopt all rules of the Google Coding
   Style for C++ Code. Some rules are explicitly unlikely to be adopted at any time.

   Followed across the code base:

   - `Formatting <https://google.github.io/styleguide/cppguide.html#Formatting>`__,
     except for subsections noted here otherwise
   - `Implicit Conversions <https://google.github.io/styleguide/cppguide.html#Implicit_Conversions>`__,
     which is enforced by a custom clang-plugin check, unless explicitly overridden using
     ``MOZ_IMPLICIT``

   Followed in new/significantly revised code:

   - `Include guards <https://google.github.io/styleguide/cppguide.html#The__define_Guard>`__

   Unlikely to be ever adopted:

   - `Forward declarations <https://google.github.io/styleguide/cppguide.html#Forward_Declarations>`__
   - `Formatting/Conditionals <https://google.github.io/styleguide/cppguide.html#Conditionals>`__
     w.r.t. curly braces around inner statements, we require them in all cases where the
     Google style allows to leave them out for single-line conditional statements

   This list reflects the state of the Google Google Coding Style for C++ Code as of
   2020-07-17. It may become invalid when the Google modifies its Coding Style.


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
set for XUL, script, and properties files is UTF-8, which is not easily
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
   API <https://firefox-source-docs.mozilla.org/modules/libpref/index.html>`__ for
   working with preferences.
-  One-argument constructors, that are not copy or move constructors,
   should generally be marked explicit. Exceptions should be annotated
   with ``MOZ_IMPLICIT``.
-  Use ``char32_t`` as the return type or argument type of a method that
   returns or takes as argument a single Unicode scalar value. (Don't
   use UTF-32 strings, though.)
-  Prefer unsigned types for semantically-non-negative integer values.
-  When operating on integers that could overflow, use ``CheckedInt``.
-  Avoid the usage of ``typedef``, instead, please use ``using`` instead.

.. note::

   For parts of this rule, clang-tidy provides the ``modernize-use-using``
   check with autofixes.


Header files
------------

Since the Firefox code base is huge and uses a monolithic build, it is
of utmost importance for keeping build times reasonable to limit the
number of included files in each translation unit to the required minimum.
Exported header files need particular attention in this regard, since their
included files propagate, and many of them are directly or indirectly
included in a large number of translation units.

-  Include guards are named per the Google coding style (i.e. upper snake
   case with a single trailing underscore). They should not include a
   leading ``MOZ_`` or ``MOZILLA_``. For example, ``dom/media/foo.h``
   would use the guard ``DOM_MEDIA_FOO_H_``.
-  Forward-declare classes in your header files, instead of including
   them, whenever possible. For example, if you have an interface with a
   ``void DoSomething(nsIContent* aContent)`` function, forward-declare
   with ``class nsIContent;`` instead of ``#include "nsIContent.h"``.
   If a "forwarding header" is provided for a type, include that instead of
   putting the literal forward declaration(s) in your header file. E.g. for
   some JavaScript types, there is ``js/TypeDecls.h``, for the string types
   there is ``StringFwd.h``. One reason for this is that this allows
   changing a type to a type alias by only changing the forwarding header.
   The following uses of a type can be done with a forward declaration only:

   -  Parameter or return type in a function declaration
   -  Member/local variable pointer or reference type
   -  Use as a template argument (not in all cases) in a member/local variable type
   -  Defining a type alias

   The following uses of a type require a full definition:

   -  Base class
   -  Member/local variable type
   -  Use with delete or new
   -  Use as a template argument (not in all cases)
   -  Any uses of non-scoped enum types
   -  Enum values of a scoped enum type

   Use as a template argument is somewhat tricky. It depends on how the
   template uses the type. E.g. ``mozilla::Maybe<T>`` and ``AutoTArray<T>``
   always require a full definition of ``T`` because the size of the
   template instance depends on the size of ``T``. ``RefPtr<T>`` and
   ``UniquePtr<T>`` don't require a full definition (because their
   pointer member always has the same size), but their destructor
   requires a full definition. If you encounter a template that cannot
   be instantiated with a forward declaration only, but it seems
   it should be possible, please file a bug (if it doesn't exist yet).

   Therefore, also consider the following guidelines to allow using forward
   declarations as widely as possible.
-  Inline function bodies in header files often pull in a lot of additional
   dependencies. Be mindful when adding or extending inline function bodies,
   and consider moving the function body to the cpp file or to a separate
   header file that is not included everywhere. Bug 1677553 intends to provide
   a more specific guideline on this.
-  Consider the use of the `Pimpl idiom <https://en.cppreference.com/w/cpp/language/pimpl>`__,
   i.e. hide the actual implementation in a separate ``Impl`` class that is
   defined in the implementation file and only expose a ``class Impl;`` forward
   declaration and ``UniquePtr<Impl>`` member in the header file.
-  Do not use non-scoped enum types. These cannot be forward-declared. Use
   scoped enum types instead, and forward declare them when possible.
-  Avoid nested types that need to be referenced from outside the class.
   These cannot be forward declared. Place them in a namespace instead, maybe
   in an extra inner namespace, and forward declare them where possible.
-  Avoid mixing declarations with different sets of dependencies in a single
   header file. This is generally advisable, but even more so when some of these
   declarations are used by a subset of the translation units that include the
   combined header file only. Consider such a badly mixed header file like:

   .. code-block:: cpp

      /* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
      /* vim: set ts=8 sts=2 et sw=2 tw=80: */
      /* This Source Code Form is subject to the terms of the Mozilla Public
      * License, v. 2.0. If a copy of the MPL was not distributed with this file,
      * You can obtain one at http://mozilla.org/MPL/2.0/. */

      #ifndef BAD_MIXED_FILE_H_
      #define BAD_MIXED_FILE_H_

      // Only this include is needed for the function declaration below.
      #include "nsCOMPtr.h"

      // These includes are only needed for the class definition.
      #include "nsIFile.h"
      #include "mozilla/ComplexBaseClass.h"

      namespace mozilla {

      class WrappedFile : public nsIFile, ComplexBaseClass {
      // ... class definition left out for clarity
      };

      // Assuming that most translation units that include this file only call
      // the function, but don't need the class definition, this should be in a
      // header file on its own in order to avoid pulling in the other
      // dependencies everywhere.
      nsCOMPtr<nsIFile> CreateDefaultWrappedFile(nsCOMPtr<nsIFile>&& aFileToWrap);

      } // namespace mozilla

      #endif // BAD_MIXED_FILE_H_


An example header file based on these rules (with some extra comments):

.. code-block:: cpp

   /* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
   /* vim: set ts=8 sts=2 et sw=2 tw=80: */
   /* This Source Code Form is subject to the terms of the Mozilla Public
   * License, v. 2.0. If a copy of the MPL was not distributed with this file,
   * You can obtain one at http://mozilla.org/MPL/2.0/. */

   #ifndef DOM_BASE_FOO_H_
   #define DOM_BASE_FOO_H_

   // Include guards should come at the very beginning and always use exactly
   // the style above. Otherwise, compiler optimizations that avoid rescanning
   // repeatedly included headers might not hit and cause excessive compile
   // times.

   #include <cstdint>
   #include "nsCOMPtr.h"  // This is needed because we have a nsCOMPtr<T> data member.

   class nsIFile;  // Used as a template argument only.
   enum class nsresult : uint32_t; // Used as a parameter type only.
   template <class T>
   class RefPtr;   // Used as a return type only.

   namespace mozilla::dom {

   class Document; // Used as a template argument only.

   // Scoped enum, not as a nested type, so it can be
   // forward-declared elsewhere.
   enum class FooKind { Small, Big };

   class Foo {
   public:
     // Do not put the implementation in the header file, it would
     // require including nsIFile.h
     Foo(nsCOMPtr<nsIFile> aFile, FooKind aFooKind);

     RefPtr<Document> CreateDocument();

     void SetResult(nsresult aResult);

     // Even though we will default this destructor, do this in the
     // implementation file since we would otherwise need to include
     // nsIFile.h in the header.
     ~Foo();

   private:
     nsCOMPtr<nsIFile> mFile;
   };

   } // namespace mozilla::dom

   #endif // DOM_BASE_FOO_H_


Corresponding implementation file:

.. code-block:: cpp

   /* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
   /* vim: set ts=8 sts=2 et sw=2 tw=80: */
   /* This Source Code Form is subject to the terms of the Mozilla Public
   * License, v. 2.0. If a copy of the MPL was not distributed with this file,
   * You can obtain one at http://mozilla.org/MPL/2.0/. */

   #include "mozilla/dom/Foo.h"  // corresponding header

   #include "mozilla/Assertions.h"  // Needed for MOZ_ASSERT.
   #include "mozilla/dom/Document.h" // Needed because we construct a Document.
   #include "nsError.h"  // Needed because we use NS_OK aka nsresult::NS_OK.
   #include "nsIFile.h"  // This is needed because our destructor indirectly calls delete nsIFile in a template instance.

   namespace mozilla::dom {

   // Do not put the implementation in the header file, it would
   // require including nsIFile.h
   Foo::Foo(nsCOMPtr<nsIFile> aFile, FooKind aFooKind)
    : mFile{std::move(aFile)} {
   }

   RefPtr<Document> Foo::CreateDocument() {
     return MakeRefPtr<Document>();
   }

   void Foo::SetResult(nsresult aResult) {
      MOZ_ASSERT(aResult != NS_OK);

      // do something with aResult
   }

   // Even though we default this destructor, do this in the
   // implementation file since we would otherwise need to include
   // nsIFile.h in the header.
   Foo::~Foo() = default;

   } // namespace mozilla::dom


Include directives
------------------

- Ordering:

  - In an implementation file (cpp file), the very first include directive
    should include the corresponding header file, followed by a blank line.
  - Any conditional includes (depending on some ``#ifdef`` or similar) follow
    after non-conditional includes. Don't mix them in.
  - Don't place comments between non-conditional includes.

  Bug 1679522 addresses automating the ordering via clang-format, which
  is going to enforce some stricter rules. Expect the includes to be reordered.
  If you include third-party headers that are not self-contained, and therefore
  need to be included in a particular order, enclose those (and only those)
  between ``// clang-format off`` and ``// clang-format on``. This should not be
  done for Mozilla headers, which should rather be made self-contained if they
  are not.

- Brackets vs. quotes: C/C++ standard library headers are included using
  brackets (e.g. ``#include <cstdint>``), all other include directives use
  (double) quotes (e.g. ``#include "mozilla/dom/Document.h``).
- Exported headers should always be included from their exported path, not
  from their source path in the tree, even if available locally. E.g. always
  do ``#include "mozilla/Vector.h"``, not ``#include "Vector.h"``, even
  from within `mfbt`.
- Generally, you should include exactly those headers that are needed, not
  more and not less. Unfortunately this is not easy to see. Maybe C++20
  modules will bring improvements to this, but it will take a long time
  to be adopted.
- The basic rule is that if you literally use a symbol in your file that
  is declared in a header A.h, include that header. In particular in header
  files, check if a forward declaration or including a forwarding header is
  sufficient, see section :ref:`Header files`.

  There are cases where this basic rule is not sufficient. Some cases where
  you need to include additional headers are:

  - You reference a member of a type that is not literally mentioned in your
    code, but, e.g. is the return type of a function you are calling.

  There are also cases where the basic rule leads to redundant includes. Note
  that "redundant" here does not refer to "accidentally redundant" headers,
  e.g. at the time of writing ``mozilla/dom/BodyUtil.h`` includes
  ``mozilla/dom/FormData.h``, but it doesn't need to (it only needs a forward
  declaration), so including ``mozilla/dom/FormData.h`` is "accidentally
  redundant" when including ``mozilla/dom/BodyUtil.h``. The includes of
  ``mozilla/dom/BodyUtil.h`` might change at any time, so if a file that
  includes ``mozilla/dom/BodyUtil.h`` needs a full definition of
  ``mozilla::dom::FormData``, it should includes ``mozilla/dom/FormData.h``
  itself. In fact, these "accidentally redundant" headers MUST be included.
  Relying on accidentally redundant includes makes any change to a header
  file extremely hard, in particular when considering that the set of
  accidentally redundant includes differs between platforms.
  But some cases in fact are non-accidentally redundant, and these can and
  typically should not be repeated:

  - The includes of the header file do not need to be repeated in its
    corresponding implementation file. Rationale: the implementation file and
    its corresponding header file are tightly coupled per se.

  Macros are a special case. Generally, the literal rule also applies here,
  i.e. if the macro definition references a symbol, the file containing the
  macro definition should include the header defining the symbol. E.g.
  ``NS_IMPL_CYCLE_COLLECTING_NATIVE_RELEASE`` defined in ``nsISupportsImpl.h``
  makes use of ``MOZ_ASSERT`` defined in ``mozilla/Assertions.h``, so
  ``nsISupportsImpl.h`` includes ``mozilla/Assertions.h``. However, this
  requires human judgment of what is intended, since technically only the
  invocations of the macro reference a symbol (and that's how
  include-what-you-use handles this). It might depend on the
  context or parameters which symbol is actually referenced, and sometimes
  this is on purpose. In these cases, the user of the macro needs to include
  the required header(s).



COM and pointers
----------------

-  Use ``nsCOMPtr<>``
   If you don't know how to use it, start looking in the code for
   examples. The general rule, is that the very act of typing
   ``NS_RELEASE`` should be a signal to you to question your code:
   "Should I be using ``nsCOMPtr`` here?". Generally the only valid use
   of ``NS_RELEASE`` is when you are storing refcounted pointers in a
   long-lived datastructure.
-  Declare new XPCOM interfaces using :doc:`XPIDL </xpcom/xpidl>`, so they
   will be scriptable.
-  Use :doc:`nsCOMPtr </xpcom/refptr>` for strong references, and
   ``nsWeakPtr`` for weak references.
-  Don't use ``QueryInterface`` directly. Use ``CallQueryInterface`` or
   ``do_QueryInterface`` instead.
-  Use :ref:`Contract IDs <contract_ids>`,
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

There is also a static analysis attribute ``[[nodiscard]]``, which can
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

.. note::

   This section overlaps with the more verbose advice given in
   :doc:`String guide </xpcom/stringguide>`.
   These should eventually be merged. For now, please refer to that guide for
   more advice.

-  String arguments to functions should be declared as ``[const] nsA[C]String&``.
-  Prefer using string literals. In particular, use empty string literals,
   i.e. ``u""_ns`` or ``""_ns``, instead of ``Empty[C]String()`` or
   ``const nsAuto[C]String empty;``. Use ``Empty[C]String()`` only if you
   specifically need a ``const ns[C]String&``, e.g. with the ternary operator
   or when you need to return/bind to a reference or take the address of the
   empty string.
-  For 16-bit literal strings, use ``u"..."_ns`` or, if necessary
   ``NS_LITERAL_STRING_FROM_CSTRING(...)`` instead of ``nsAutoString()``
   or other ways that would do a run-time conversion.
   See :ref:`Avoid runtime conversion of string literals <Avoid runtime conversion of string literals>` below.
-  To compare a string with a literal, use ``.EqualsLiteral("...")``.
-  Use ``str.IsEmpty()`` instead of ``str.Length() == 0``.
-  Use ``str.Truncate()`` instead of ``str.SetLength(0)``,
   ``str.Assign(""_ns)`` or ``str.AssignLiteral("")``.
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

.. _Avoid runtime conversion of string literals:

Avoid runtime conversion of string literals
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It is very common to need to assign the value of a literal string, such
as ``"Some String"``, into a unicode buffer. Instead of using ``nsString``'s
``AssignLiteral`` and ``AppendLiteral``, use a user-defined literal like `u"foo"_ns`
instead. On most platforms, this will force the compiler to compile in a
raw unicode string, and assign it directly. In cases where the literal is defined
via a macro that is used in both 8-bit and 16-bit ways, you can use
`NS_LITERAL_STRING_FROM_CSTRING` to do the conversion at compile time.

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

   constexpr auto warning = u"danger will robinson!"_ns;
   ...
   // if you'll be using the 'warning' string, you can still use it as before:
   foo->SetStringValue(warning);
   ...
   bar->SetUnicodeValue(warning.get());

   // alternatively, use the wide string directly:
   foo->SetStringValue(u"danger will robinson!"_ns);
   ...

   // if a macro is the source of a 8-bit literal and you cannot change it, use
   // NS_LITERAL_STRING_FROM_CSTRING, but only if necessary.
   #define MY_MACRO_LITERAL "danger will robinson!"
   foo->SetStringValue(NS_LITERAL_STRING_FROM_CSTRING(MY_MACRO_LITERAL));

   // If you need to pass to a raw const char16_t *, there's no benefit to
   // go through our string classes at all, just do...
   bar->SetUnicodeValue(u"danger will robinson!");

   // .. or, again, if a macro is the source of a 8-bit literal
   bar->SetUnicodeValue(u"" MY_MACRO_LITERAL);


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

Use of SpiderMonkey rooting typedefs
------------------------------------
The rooting typedefs in ``js/public/TypeDecls.h``, such as ``HandleObject`` and
``RootedObject``, are deprecated both in and outside of SpiderMonkey. They will
eventually be removed and should not be used in new code.
