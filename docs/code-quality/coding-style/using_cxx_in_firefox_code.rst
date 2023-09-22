Using C++ in Mozilla code
=========================

C++ language features
---------------------

Mozilla code only uses a subset of C++. Runtime type information (RTTI)
is disabled, as it tends to cause a very large increase in codesize.
This means that ``dynamic_cast``, ``typeid()`` and ``<typeinfo>`` cannot
be used in Mozilla code. Also disabled are exceptions; do not use
``try``/``catch`` or throw any exceptions. Libraries that throw
exceptions may be used if you are willing to have the throw instead be
treated as an abort.

On the side of extending C++, we compile with ``-fno-strict-aliasing``.
This means that when reinterpreting a pointer as a differently-typed
pointer, you don't need to adhere to the "effective type" (of the
pointee) rule from the standard (aka. "the strict aliasing rule") when
dereferencing the reinterpreted pointer. You still need make sure that
you don't violate alignment requirements and need to make sure that the
data at the memory location pointed to forms a valid value when
interpreted according to the type of the pointer when dereferencing the
pointer for reading. Likewise, if you write by dereferencing the
reinterpreted pointer and the originally-typed pointer might still be
dereferenced for reading, you need to make sure that the values you
write are valid according to the original type. This value validity
issue is moot for e.g. primitive integers for which all bit patterns of
their size are valid values.

-  As of Mozilla 59, C++14 mode is required to build Mozilla.
-  As of Mozilla 67, MSVC can no longer be used to build Mozilla.
-  As of Mozilla 73, C++17 mode is required to build Mozilla.

This means that C++17 can be used where supported on all platforms. The
list of acceptable features is given below:

.. list-table::
   :widths: 25 25 25 25
   :header-rows: 3

   * -
     - GCC
     - Clang
     -
   * - Current minimal requirement
     - 8.1
     - 8.0
     -
   * - Feature
     - GCC
     - Clang
     - Can be used in code
   * - ``type_t &&``
     - 4.3
     - 2.9
     - Yes (see notes)
   * - ref qualifiers on methods
     - 4.8.1
     - 2.9
     - Yes
   * - default member-initializers (except for bit-fields)
     - 4.7
     - 3.0
     - Yes
   * - default member-initializers (for bit-fields)
     - 8
     - 6
     - **No**
   * - variadic templates
     - 4.3
     - 2.9
     - Yes
   * - Initializer lists
     - 4.4
     - 3.1
     - Yes
   * - ``static_assert``
     - 4.3
     - 2.9
     - Yes
   * - ``auto``
     - 4.4
     - 2.9
     - Yes
   * - lambdas
     - 4.5
     - 3.1
     - Yes
   * - ``decltype``
     - 4.3
     - 2.9
     - Yes
   * - ``Foo<Bar<T>>``
     - 4.3
     - 2.9
     - Yes
   * - ``auto func() -> int``
     - 4.4
     - 3.1
     - Yes
   * - Templated aliasing
     - 4.7
     - 3.0
     - Yes
   * - ``nullptr``
     - 4.6
     - 3.0
     - Yes
   * - ``enum foo : int16_t`` {};
     - 4.4
     - 2.9
     - Yes
   * - ``enum class foo {}``;
     - 4.4
     - 2.9
     - Yes
   * - ``enum foo;``
     - 4.6
     - 3.1
     - Yes
   * - ``[[attributes]]``
     - 4.8
     - 3.3
     - **No** (see notes)
   * - ``constexpr``
     - 4.6
     - 3.1
     - Yes
   * - ``alignas``
     - 4.8
     - 3.3
     - Yes
   * - ``alignof``
     - 4.8
     - 3.3
     - Yes, but see notes ; only clang 3.6 claims as_feature(cxx_alignof)
   * - Delegated constructors
     - 4.7
     - 3.0
     - Yes
   * - Inherited constructors
     - 4.8
     - 3.3
     - Yes
   * - ``explicit operator bool()``
     - 4.5
     - 3.0
     - Yes
   * - ``char16_t/u"string"``
     - 4.4
     - 3.0
     - Yes
   * - ``R"(string)"``
     - 4.5
     - 3.0
     - Yes
   * - ``operator""()``
     - 4.7
     - 3.1
     - Yes
   * - ``=delete``
     - 4.4
     - 2.9
     - Yes
   * - ``=default``
     - 4.4
     - 3.0
     - Yes
   * - unrestricted unions
     - 4.6
     - 3.1
     - Yes
   * - ``for (auto x : vec)`` (`be careful about the type of the iterator <https://stackoverflow.com/questions/15176104/c11-range-based-loop-get-item-by-value-or-reference-to-const>`__)
     - 4.6
     - 3.0
     - Yes
   * - ``override``/``final``
     - 4.7
     - 3.0
     - Yes
   * - ``thread_local``
     - 4.8
     - 3.3
     - **No** (see notes)
   * - function template default arguments
     - 4.3
     - 2.9
     - Yes
   * - local structs as template parameters
     - 4.5
     - 2.9
     - Yes
   * - extended friend declarations
     - 4.7
     - 2.9
     - Yes
   * - ``0b100`` (C++14)
     - 4.9
     - 2.9
     - Yes
   * - `Tweaks to some C++ contextual conversions` (C++14)
     - 4.9
     - 3.4
     - Yes
   * - Return type deduction (C++14)
     - 4.9
     - 3.4
     - Yes (but only in template code when you would have used ``decltype (complex-expression)``)
   * - Generic lambdas (C++14)
     - 4.9
     - 3.4
     - Yes
   * - Initialized lambda captures (C++14)
     - 4.9
     - 3.4
     - Yes
   * - Digit separator (C++14)
     - 4.9
     - 3.4
     - Yes
   * - Variable templates (C++14)
     - 5.0
     - 3.4
     - Yes
   * - Relaxed constexpr (C++14)
     - 5.0
     - 3.4
     - Yes
   * - Aggregate member initialization (C++14)
     - 5.0
     - 3.3
     - Yes
   * - Clarifying memory allocation (C++14)
     - 5.0
     - 3.4
     - Yes
   * - [[deprecated]] attribute (C++14)
     - 4.9
     - 3.4
     - **No** (see notes)
   * - Sized deallocation (C++14)
     - 5.0
     - 3.4
     - **No** (see notes)
   * - Concepts (Concepts TS)
     - 6.0
     - —
     - **No**
   * - Inline variables (C++17)
     - 7.0
     - 3.9
     - Yes
   * - constexpr_if (C++17)
     - 7.0
     - 3.9
     - Yes
   * - constexpr lambdas (C++17)
     - —
     - —
     - **No**
   * - Structured bindings (C++17)
     - 7.0
     - 4.0
     - Yes
   * - Separated declaration and condition in ``if``, ``switch`` (C++17)
     - 7.0
     - 3.9
     - Yes
   * - `Fold expressions <https://en.cppreference.com/w/cpp/language/fold>`__ (C++17)
     - 6.0
     - 3.9
     - Yes
   * - [[fallthrough]],  [[maybe_unused]], [[nodiscard]] (C++17)
     - 7.0
     - 3.9
     - Yes
   * - Aligned allocation/deallocation (C++17)
     - 7.0
     - 4.0
     - **No** (see notes)
   * - Designated initializers (C++20)
     - 8.0 (4.7)
     - 10.0 (3.0)
     - Yes [*sic*] (see notes)
   * - #pragma once
     - 3.4
     - Yes
     - **Not** until we `normalize headers <https://groups.google.com/d/msg/mozilla.dev.platform/PgDjWw3xp8k/eqCFlP4Kz1MJ>`__
   * - `Source code information capture <https://en.cppreference.com/w/cpp/experimental/lib_extensions_2#Source_code_information_capture>`__
     - 8.0
     - —
     - **No**

Sources
~~~~~~~

* GCC: https://gcc.gnu.org/projects/cxx-status.html
* Clang: https://clang.llvm.org/cxx_status.html

Notes
~~~~~

rvalue references
  Implicit move method generation cannot be used.

Attributes
  Several common attributes are defined in
  `mozilla/Attributes.h <https://searchfox.org/mozilla-central/source/mfbt/Attributes.h>`__
  or nscore.h.

Alignment
  Some alignment utilities are defined in `mozilla/Alignment.h
  <https://searchfox.org/mozilla-central/source/mfbt/Alignment.h>`__.

  .. caution::
    ``MOZ_ALIGNOF`` and ``alignof`` don't have the same semantics. Be careful of what you
    expect from them.

``[[deprecated]]``
  If we have deprecated code, we should be removing it rather than marking it as
  such. Marking things as ``[[deprecated]]`` also means the compiler will warn
  if you use the deprecated API, which turns into a fatal error in our
  automation builds, which is not helpful.

Sized deallocation
  Our compilers all support this (custom flags are required for GCC and Clang),
  but turning it on breaks some classes' ``operator new`` methods, and `some
  work <https://bugzilla.mozilla.org/show_bug.cgi?id=1250998>`__ would need to
  be done to make it an efficiency win with our custom memory allocator.

Aligned allocation/deallocation
  Our custom memory allocator doesn't have support for these functions.

Thread locals
  ``thread_local`` is not supported on Android.

Designated initializers
  Despite their late addition to C++ (and lack of *official* support by
  compilers until relatively recently), `C++20's designated initializers
  <https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0329r4.pdf>`__ are
  merely a subset of `a feature originally introduced in C99
  <https://gcc.gnu.org/onlinedocs/gcc/Designated-Inits.html>`__ -- and this
  subset has been accepted without comment in C++ code since at least GCC 4.7
  and Clang 3.0.


C++ and Mozilla standard libraries
----------------------------------

The Mozilla codebase contains within it several subprojects which follow
different rules for which libraries can and can't be used it. The rules
listed here apply to normal platform code, and assume unrestricted
usability of MFBT or XPCOM APIs.

.. warning::

   The rest of this section is a draft for expository and exploratory
   purposes. Do not trust the information listed here.

What follows is a list of standard library components provided by
Mozilla or the C++ standard. If an API is not listed here, then it is
not permissible to use it in Mozilla code. Deprecated APIs are not
listed here. In general, prefer Mozilla variants of data structures to
standard C++ ones, even when permitted to use the latter, since Mozilla
variants tend to have features not found in the standard library (e.g.,
memory size tracking) or have more controllable performance
characteristics.

A list of approved standard library headers is maintained in
`config/stl-headers.mozbuild <https://searchfox.org/mozilla-central/source/config/stl-headers.mozbuild>`__.


Data structures
~~~~~~~~~~~~~~~

.. list-table::
   :widths: 25 25 25 25
   :header-rows: 1

   * - Name
     - Header
     - STL equivalent
     - Notes
   * - ``nsAutoTArray``
     - ``nsTArray.h``
     -
     - Like ``nsTArray``, but will store a small amount as stack storage
   * - ``nsAutoTObserverArray``
     - ``nsTObserverArray.h``
     -
     - Like ``nsTObserverArray``, but will store a small amount as stack storage
   * - ``mozilla::BloomFilter``
     - ``mozilla/BloomFilter.h``
     -
     - Probabilistic set membership (see `Wikipedia <https://en.wikipedia.org/wiki/Bloom_filter#Counting_filters>`__)
   * - ``nsClassHashtable``
     - ``nsClassHashtable.h``
     -
     - Adaptation of nsTHashtable, see :ref:`XPCOM Hashtable Guide`
   * - ``nsCOMArray``
     - ``nsCOMArray.h``
     -
     - Like ``nsTArray<nsCOMPtr<T>>``
   * - ``nsDataHashtable``
     - ``nsClassHashtable.h``
     - ``std::unordered_map``
     - Adaptation of ``nsTHashtable``, see :ref:`XPCOM Hashtable Guide`
   * - ``nsDeque``
     - ``nsDeque.h``
     - ``std::deque<void *>``
     -
   * - ``mozilla::EnumSet``
     - ``mozilla/EnumSet.h``
     -
     - Like ``std::set``, but for enum classes.
   * - ``mozilla::Hash{Map,Set}``
     - `mozilla/HashTable.h <https://searchfox.org/mozilla-central/source/mfbt/HashTable.h>`__
     - ``std::unordered_{map,set}``
     - A general purpose hash map and hash set.
   * - ``nsInterfaceHashtable``
     - ``nsInterfaceHashtable.h``
     - ``std::unordered_map``
     - Adaptation of ``nsTHashtable``, see :ref:`XPCOM Hashtable Guide`
   * - ``mozilla::LinkedList``
     - ``mozilla/LinkedList.h``
     - ``std::list``
     - Doubly-linked list
   * - ``nsRef PtrHashtable``
     - ``nsRefPtrHashtable.h``
     - ``std::unordered_map``
     - Adaptation of ``nsTHashtable``, see :ref:`XPCOM Hashtable Guide`
   * - ``mozilla::SegmentedVector``
     - ``mozilla/SegmentedVector.h``
     - ``std::deque`` w/o O(1) pop_front
     - Doubly-linked list of vector elements
   * - ``mozilla::SplayTree``
     - ``mozilla/SplayTree.h``
     -
     - Quick access to recently-accessed elements (see `Wikipedia <https://en.wikipedia.org/wiki/Splay_tree>`__)
   * - ``nsTArray``
     - ``nsTArray.h``
     - ``std::vector``
     -
   * - ``nsTHashtable``
     - ``nsTHashtable.h``
     - ``std::unordered_{map,set}``
     - See :ref:`XPCOM Hashtable Guide`,  you probably want a subclass
   * - ``nsTObserverArray``
     - ``nsTObserverArray.h``
     -
     - Like ``nsTArray``, but iteration is stable even through mutation
   * - ``nsTPriorityQueue``
     - ``nsTPriorityQueue.h``
     - ``std::priority_queue``
     - Unlike the STL class, not a container adapter
   * - ``mozilla::Vector``
     - ``mozilla/Vector.h``
     - ``std::vector``
     -
   * - ``mozilla::Buffer``
     - ``mozilla/Buffer.h``
     -
     - Unlike ``Array``, has a run-time variable length. Unlike ``Vector``, does not have capacity and growth mechanism. Unlike  ``Span``, owns  its buffer.


Safety utilities
~~~~~~~~~~~~~~~~

.. list-table::
   :widths: 25 25 25 25
   :header-rows: 1

   * - Name
     - Header
     - STL equivalent
     - Notes
   * - ``mozilla::Array``
     - ``mfbt/Array.h``
     -
     - safe array index
   * - ``mozilla::AssertedCast``
     - ``mfbt/Casting.h``
     -
     - casts
   * - ``mozilla::CheckedInt``
     - ``mfbt/CheckedInt.h``
     -
     - avoids overflow
   * - ``nsCOMPtr``
     - ``xpcom/base/nsCOMPtr.h``
     - ``std::shared_ptr``
     -
   * - ``mozilla::EnumeratedArray``
     - ``mfbt/EnumeratedArray.h``
     - ``mozilla::Array``
     -
   * - ``mozilla::Maybe``
     - ``mfbt/Maybe.h``
     - ``std::optional``
     -
   * - ``mozilla::RangedPtr``
     - ``mfbt/RangedPtr.h``
     -
     - like ``mozilla::Span`` but with two pointers instead of pointer and length
   * - ``mozilla::RefPtr``
     - ``mfbt/RefPtr.h``
     - ``std::shared_ptr``
     -
   * - ``mozilla::Span``
     - ``mozilla/Span.h``
     - ``gsl::span``, ``absl::Span``, ``std::string_view``, ``std::u16string_view``
     - Rust's slice concept for C++ (without borrow checking)
   * - ``StaticRefPtr``
     - ``xpcom/base/StaticPtr.h``
     -
     - ``nsRefPtr`` w/o static constructor
   * - ``mozilla::UniquePtr``
     - ``mfbt/UniquePtr.h``
     - ``std::unique_ptr``
     -
   * - ``mozilla::WeakPtr``
     - ``mfbt/WeakPtr.h``
     - ``std::weak_ptr``
     -
   * - ``nsWeakPtr``
     - ``xpcom/base/nsWeakPtr.h``
     - ``std::weak_ptr``
     -


Strings
~~~~~~~

See the :doc:`Mozilla internal string guide </xpcom/stringguide>` for
usage of ``nsAString`` (our copy-on-write replacement for
``std::u16string``) and ``nsACString`` (our copy-on-write replacement
for ``std::string``).

Be sure not to introduce further uses of ``std::wstring``, which is not
portable! (Some uses exist in the IPC code.)


Algorithms
~~~~~~~~~~

.. list-table::
   :widths: 25 25

   * - ``mozilla::BinarySearch``
     - ``mfbt/BinarySearch.h``
   * - ``mozilla::BitwiseCast``
     - ``mfbt/Casting.h`` (strict aliasing-safe cast)
   * - ``mozilla/MathAlgorithms.h``
     - (rotate, ctlz, popcount, gcd, abs, lcm)
   * - ``mozilla::RollingMean``
     - ``mfbt/RollingMean.h`` ()


Concurrency
~~~~~~~~~~~

.. list-table::
   :widths: 25 25 25 25
   :header-rows: 1

   * - Name
     - Header
     - STL/boost equivalent
     - Notes
   * - ``mozilla::Atomic``
     - mfbt/Atomic.h
     - ``std::atomic``
     -
   * - ``mozilla::CondVar``
     - xpcom/threads/CondVar.h
     - ``std::condition_variable``
     -
   * - ``mozilla::DataMutex``
     - xpcom/threads/DataMutex.h
     - ``boost::synchronized_value``
     -
   * - ``mozilla::Monitor``
     - xpcom/threads/Monitor.h
     -
     -
   * - ``mozilla::Mutex``
     - xpcom/threads/Mutex.h
     - ``std::mutex``
     -
   * - ``mozilla::ReentrantMonitor``
     - xpcom/threads/ReentrantMonitor.h
     -
     -
   * - ``mozilla::StaticMutex``
     - xpcom/base/StaticMutex.h
     - ``std::mutex``
     - Mutex that can (and in fact, must) be used as a global/static variable.


Miscellaneous
~~~~~~~~~~~~~

.. list-table::
   :widths: 25 25 25 25
   :header-rows: 1

   * - Name
     - Header
     - STL/boost equivalent
     - Notes
   * - ``mozilla::AlignedStorage``
     - mfbt/Alignment.h
     - ``std::aligned_storage``
     -
   * - ``mozilla::MaybeOneOf``
     - mfbt/MaybeOneOf.h
     - ``std::optional<std::variant<T1, T2>>``
     - ~ ``mozilla::Maybe<union {T1, T2}>``
   * - ``mozilla::Pair``
     - mfbt/Pair.h
     - ``std::tuple<T1, T2>``
     - minimal space!
   * - ``mozilla::TimeStamp``
     - xpcom/ds/TimeStamp.h
     - ``std::chrono::time_point``
     -
   * -
     - mozilla/PodOperations.h
     -
     - C++ versions of ``memset``, ``memcpy``, etc.
   * -
     - mozilla/ArrayUtils.h
     -
     -
   * -
     - mozilla/Compression.h
     -
     -
   * -
     - mozilla/Endian.h
     -
     -
   * -
     - mozilla/FloatingPoint.h
     -
     -
   * -
     - mozilla/HashFunctions.h
     - ``std::hash``
     -
   * -
     - mozilla/Move.h
     - ``std::move``, ``std::swap``, ``std::forward``
     -


Mozilla data structures and standard C++ ranges and iterators
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Some Mozilla-defined data structures provide STL-style
`iterators <https://en.cppreference.com/w/cpp/named_req/Iterator>`__ and
are usable in `range-based for
loops <https://en.cppreference.com/w/cpp/language/range-for>`__ as well
as STL `algorithms <https://en.cppreference.com/w/cpp/algorithm>`__.

Currently, these include:

.. list-table::
   :widths: 16 16 16 16 16
   :header-rows: 1

   * - Name
     - Header
     - Bug(s)
     - Iterator category
     - Notes
   * - ``nsTArray``
     - ``xpcom/ds/n sTArray.h``
     - `1126552 <https://bugzilla.mozilla.org/show_bug.cgi?id=1126552>`__
     - Random-access
     - Also reverse-iterable. Also supports remove-erase pattern via RemoveElementsAt method. Also supports back-inserting output iterators via ``MakeBackInserter`` function.
   * - ``nsBaseHashtable`` and subclasses: ``nsClassHashtable`` ``nsDataHashtable`` ``nsInterfaceHashtable`` ``nsJSThingHashtable`` ``nsRefPtrHashtable``
     - ``xpcom/ds/nsBaseHashtable.h`` ``xpcom/ds/nsClassHashtable.h`` ``xpcom/ds/nsDataHashtable.h`` ``xpcom/ds/nsInterfaceHashtable.h`` ``xpcom/ds/nsJSThingHashtable.h`` ``xpcom/ds/nsRefPtrHashtable.h``
     - `1575479 <https://bugzilla.mozilla.org/show_bug.cgi?id=1575479>`__
     - Forward
     -
   * - ``nsCOMArray``
     - ``xpcom/ds/nsCOMArray.h``
     - `1342303 <https://bugzilla.mozilla.org/show_bug.cgi?id=1342303>`__
     - Random-access
     - Also reverse-iterable.
   * - ``Array`` ``EnumerationArray`` ``RangedArray``
     - ``mfbt/Array.h`` ``mfbt/EnumerationArray.h`` ``mfbt/RangedArray.h``
     - `1216041 <https://bugzilla.mozilla.org/show_bug.cgi?id=1216041>`__
     - Random-access
     - Also reverse-iterable.
   * - ``Buffer``
     - ``mfbt/Buffer.h``
     - `1512155 <https://bugzilla.mozilla.org/show_bug.cgi?id=1512155>`__
     - Random-access
     - Also reverse-iterable.
   * - ``DoublyLinkedList``
     - ``mfbt/DoublyLinkedList.h``
     - `1277725 <https://bugzilla.mozilla.org/show_bug.cgi?id=1277725>`__
     - Forward
     -
   * - ``EnumeratedRange``
     - ``mfbt/EnumeratedRange.h``
     - `1142999 <https://bugzilla.mozilla.org/show_bug.cgi?id=1142999>`__
     - *Missing*
     - Also reverse-iterable.
   * - ``IntegerRange``
     - ``mfbt/IntegerRange.h``
     - `1126701 <https://bugzilla.mozilla.org/show_bug.cgi?id=1126701>`__
     - *Missing*
     - Also reverse-iterable.
   * - ``SmallPointerArray``
     - ``mfbt/SmallPointerArray.h``
     - `1331718 <https://bugzilla.mozilla.org/show_bug.cgi?id=1331718>`__
     - Random-access
     -
   * - ``Span``
     - ``mfbt/Span.h``
     - `1295611 <https://bugzilla.mozilla.org/show_bug.cgi?id=1295611>`__
     - Random-access
     - Also reverse-iterable.

Note that if the iterator category is stated as "missing", the type is
probably only usable in range-based for. This is most likely just an
omission, which could be easily fixed.

Useful in this context are also the class template ``IteratorRange``
(which can be used to construct a range from any pair of iterators) and
function template ``Reversed`` (which can be used to reverse any range),
both defined in ``mfbt/ReverseIterator.h``


Further C++ rules
-----------------


Don't use static constructors
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

(You probably shouldn't be using global variables to begin with. Quite
apart from the weighty software-engineering arguments against them,
globals affect startup time! But sometimes we have to do ugly things.)

Non-portable example:

.. code-block:: c++

   FooBarClass static_object(87, 92);

   void
   bar()
   {
     if (static_object.count > 15) {
        ...
     }
   }

Once upon a time, there were compiler bugs that could result in
constructors not being called for global objects. Those bugs are
probably long gone by now, but even with the feature working correctly,
there are so many problems with correctly ordering C++ constructors that
it's easier to just have an init function:

.. code-block:: c++

   static FooBarClass* static_object;

   FooBarClass*
   getStaticObject()
   {
     if (!static_object)
       static_object =
         new FooBarClass(87, 92);
     return static_object;
   }

   void
   bar()
   {
     if (getStaticObject()->count > 15) {
       ...
     }
   }


Don't use exceptions
~~~~~~~~~~~~~~~~~~~~

See the introduction to the "C++ language features" section at the start
of this document.


Don't use Run-time Type Information
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

See the introduction to the "C++ language features" section at the start
of this document.

If you need runtime typing, you can achieve a similar result by adding a
``classOf()`` virtual member function to the base class of your
hierarchy and overriding that member function in each subclass. If
``classOf()`` returns a unique value for each class in the hierarchy,
you'll be able to do type comparisons at runtime.


Don't use the C++ standard library (including iostream and locale)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

See the section "C++ and Mozilla standard libraries".


Use C++ lambdas, but with care
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

C++ lambdas are supported across all our compilers now. Rejoice! We
recommend explicitly listing out the variables that you capture in the
lambda, both for documentation purposes, and to double-check that you're
only capturing what you expect to capture.


Use namespaces
~~~~~~~~~~~~~~

Namespaces may be used according to the style guidelines in :ref:`C++ Coding style`.


Don't mix varargs and inlines
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

What? Why are you using varargs to begin with?! Stop that at once!


Make header files compatible with C and C++
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Non-portable example:

.. code-block:: c++

   /*oldCheader.h*/
   int existingCfunction(char*);
   int anotherExistingCfunction(char*);

   /* oldCfile.c */
   #include "oldCheader.h"
   ...

   // new file.cpp
   extern "C" {
   #include "oldCheader.h"
   };
   ...

If you make new header files with exposed C interfaces, make the header
files work correctly when they are included by both C and C++ files.

(If you need to include a C header in new C++ files, that should just
work. If not, it's the C header maintainer's fault, so fix the header if
you can, and if not, whatever hack you come up with will probably be
fine.)

Portable example:

.. code-block:: c++

   /* oldCheader.h*/
   PR_BEGIN_EXTERN_C
   int existingCfunction(char*);
   int anotherExistingCfunction(char*);
   PR_END_EXTERN_C

   /* oldCfile.c */
   #include "oldCheader.h"
   ...

   // new file.cpp
   #include "oldCheader.h"
   ...

There are number of reasons for doing this, other than just good style.
For one thing, you are making life easier for everyone else, doing the
work in one common place (the header file) instead of all the C++ files
that include it. Also, by making the C header safe for C++, you document
that "hey, this file is now being included in C++". That's a good thing.
You also avoid a big portability nightmare that is nasty to fix...


Use override on subclass virtual member functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``override`` keyword is supported in C++11 and in all our supported
compilers, and it catches bugs.


Always declare a copy constructor and assignment operator
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Many classes shouldn't be copied or assigned. If you're writing one of
these, the way to enforce your policy is to declare a deleted copy
constructor as private and not supply a definition. While you're at it,
do the same for the assignment operator used for assignment of objects
of the same class. Example:

.. code-block:: c++

   class Foo {
     ...
     private:
       Foo(const Foo& x) = delete;
       Foo& operator=(const Foo& x) = delete;
   };

Any code that implicitly calls the copy constructor will hit a
compile-time error. That way nothing happens in the dark. When a user's
code won't compile, they'll see that they were passing by value, when
they meant to pass by reference (oops).


Be careful of overloaded methods with like signatures
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It's best to avoid overloading methods when the type signature of the
methods differs only by one "abstract" type (e.g. ``PR_Int32`` or
``int32``). What you will find as you move that code to different
platforms, is suddenly on the Foo2000 compiler your overloaded methods
will have the same type-signature.


Type scalar constants to avoid unexpected ambiguities
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Non-portable code:

.. code-block:: c++

   class FooClass {
     // having such similar signatures
     // is a bad idea in the first place.
     void doit(long);
     void doit(short);
   };

   void
   B::foo(FooClass* xyz)
   {
     xyz->doit(45);
   }

Be sure to type your scalar constants, e.g., ``uint32_t(10)`` or
``10L``. Otherwise, you can produce ambiguous function calls which
potentially could resolve to multiple methods, particularly if you
haven't followed (2) above. Not all of the compilers will flag ambiguous
method calls.

Portable code:

.. code-block:: c++

   class FooClass {
     // having such similar signatures
     // is a bad idea in the first place.
     void doit(long);
     void doit(short);
   };

   void
   B::foo(FooClass* xyz)
   {
     xyz->doit(45L);
   }


Use nsCOMPtr in XPCOM code
~~~~~~~~~~~~~~~~~~~~~~~~~~

See the ``nsCOMPtr`` `User
Manual <https://developer.mozilla.org/en-US/docs/Using_nsCOMPtr>`__ for
usage details.


Don't use identifiers that start with an underscore
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This rule occasionally surprises people who've been hacking C++ for
decades. But it comes directly from the C++ standard!

According to the C++ Standard, 17.4.3.1.2 Global Names
[lib.global.names], paragraph 1:

Certain sets of names and function signatures are always reserved to the
implementation:

-  Each name that contains a double underscore (__) or begins with an
   underscore followed by an uppercase letter (2.11) is reserved to the
   implementation for any use.
-  **Each name that begins with an underscore is reserved to the
   implementation** for use as a name in the global namespace.


Stuff that is good to do for C or C++
-------------------------------------


Avoid conditional #includes when possible
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Don't write an ``#include`` inside an ``#ifdef`` if you could instead
put it outside. Unconditional includes are better because they make the
compilation more similar across all platforms and configurations, so
you're less likely to cause stupid compiler errors on someone else's
favorite platform that you never use.

Bad code example:

.. code-block:: c++

   #ifdef MOZ_ENABLE_JPEG_FOUR_BILLION
   #include <stdlib.h>   // <--- don't do this
   #include "jpeg4e9.h"  // <--- only do this if the header really might not be there
   #endif

Of course when you're including different system files for different
machines, you don't have much choice. That's different.


Every .cpp source file should have a unique name
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Every object file linked into libxul needs to have a unique name. Avoid
generic names like nsModule.cpp and instead use nsPlacesModule.cpp.


Turn on warnings for your compiler, and then write warning free code
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

What generates a warning on one platform will generate errors on
another. Turn warnings on. Write warning-free code. It's good for you.
Treat warnings as errors by adding
``ac_add_options --enable-warnings-as-errors`` to your mozconfig file.


Use the same type for all bitfields in a ``struct`` or ``class``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Some compilers do not pack the bits when different bitfields are given
different types. For example, the following struct might have a size of
8 bytes, even though it would fit in 1:

.. code-block:: c++

   struct {
     char ch: 1;
     int i: 1;
   };


Don't use an enum type for a bitfield
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The classic example of this is using ``PRBool`` for a boolean bitfield.
Don't do that. ``PRBool`` is a signed integer type, so the bitfield's
value when set will be ``-1`` instead of ``+1``, which---I know,
*crazy*, right? The things C++ hackers used to have to put up with...

You shouldn't be using ``PRBool`` anyway. Use ``bool``. Bitfields of
type ``bool`` are fine.

Enums are signed on some platforms (in some configurations) and unsigned
on others and therefore unsuitable for writing portable code when every
bit counts, even if they happen to work on your system.
