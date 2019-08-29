====================================
DOM Workers & Storage C++ Code Style
====================================

This page describes the code style for the components maintained by the DOM Workers & Storage team. They live in-tree under the 'dom/docs/indexedDB' directory.

.. contents::
   :depth: 4

Introduction
============

This code style currently applies to the components living in the following directories:

* ``dom/indexedDB``
* ``dom/localstorage``
* ``dom/quota``

In the long-term, the code is intended to use the
`Mozilla Coding Style <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Coding_Style>`_,
which references the `Google C++ Coding Style <https://google.github.io/styleguide/cppguide.html>`_.

However, large parts of the code were written before rules and in particular
the reference to the Google C++ Coding Style were enacted, and due to the
size of the code, this misalignment cannot be fixed in the short term.
To avoid that an arbitrary mixture of old-style and new-style code grows,
this document makes deviations from the "global" code style explicit, and
will be amended to describe migration paths in the future.

In addition, to achieve higher consistency within the components maintained by
the team and to reduce style discussions during reviews, allowing them to focus
on more substantial issues, more specific rules are described here that go
beyond the global code style. These topics might have been deliberately or
accidentally omitted from the global code style. Depending on wider agreement
and applicability, these specific rules might be migrated into the global code
style in the future.

Note that this document does not cover pure formatting issues. The code is and
must be kept formatted automatically by clang-format using the supplied
configuration file, and whatever clang-format does takes precedence over any
other stated rules regarding formatting.

Deviations from the Google C++ Coding Style
===========================================

Deviations not documented yet.

Deviations from the Mozilla C++ Coding Style
============================================

.. the table renders impractically, cf. https://github.com/readthedocs/sphinx_rtd_theme/issues/117

.. tabularcolumns:: |p{4cm}|p{4cm}|p{2cm}|p{2cm}|

+--------------------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------+-----------------+-------------------------------------------------------------------------------------+
|                                             Mozilla style                                              |                                    Prevalent WAS style                                     | Deviation scope |                                      Evolution                                      |
+========================================================================================================+============================================================================================+=================+=====================================================================================+
| `We prefer using "static", instead of anonymous C++ namespaces.                                        | Place all symbols that should have internal linkage in a single anonymous                  | All files       | Unclear. The recommendation in the Mozilla code style says this might change in the |
| <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Coding_Style#Anonymous_namespaces>`_ | namespace block at the top of an implementation file, rather than declarating them static. |                 | future depending on debugger support, so this deviation might become obsolete.      |
|                                                                                                        |                                                                                            |                 |                                                                                     |
+--------------------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------+-----------------+-------------------------------------------------------------------------------------+
| `All parameters passed by lvalue reference must be labeled const. [...] Input parameters may be const  | Non-const reference parameters may be used.                                                | All files       | Unclear. Maybe at least restrict the use of non-const reference parameters to       |
| pointers, but we never allow non-const reference parameters except when required by convention, e.g.,  |                                                                                            |                 | cases that are not clearly output parameters (i.e. which are assigned to).          |
| swap(). <https://google.github.io/styleguide/cppguide.html#Reference_Arguments>`_                      |                                                                                            |                 |                                                                                     |
+--------------------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------+-----------------+-------------------------------------------------------------------------------------+

Additions to the Google/Mozilla C++ Code Style
==============================================

This section contains style guidelines that do not conflict with the Google or
Mozilla C++ Code Style, but may make guidelines more specific or add guidelines
on topics not covered by those style guides at all.

Naming
------

gtest test names
~~~~~~~~~~~~~~~~

gtest constructs a full test name from different fragments. Test names are
constructed somewhat differently for basic and parametrized tests.

The *prefix* for a test should start with an identifier of the component
and class, based on the name of the source code directory, transformed to
PascalCase and underscores as separators, so e.g. for a class ``Key`` in
``dom/indexedDB``, use ``DOM_IndexedDB_Key`` as a prefix.

For basic tests constructed with ``TEST(test_case_name, test_name)``: Use
the *prefix* as the ``test_case_name``. Test ``test_name`` should start with
the name of tested method(s), and a . Use underscores as a separator within
the ``test_name``.

Value-parametrized tests are constructed with
``TEST_P(parametrized_test_case_name, parametrized_test_name)``. They require a
custom test base class, whose name is used as the ``parametrized_test_case_name``.
Start the class name with ``TestWithParam_``, and end it with a transliteration
of the parameter type (e.g. ``String_Int_Pair`` for ``std::pair<nsString, int>``),
and place it in an (anonymous) namespace.

.. attention::
   It is important to place the class in an (anonymous) namespace, since its
   name according to this guideline is not unique within libxul-gtest, and name
   clashes are likely, which would lead to ODR violations otherwise.

A ``parametrized_test_name`` is constructed according to the same rules
described for ``test_name`` above.

Instances of value-parametrized tests are constructed using
``INSTANTIATE_TEST_CASE_P(prefix, parametrized_test_case_name, generator, ...)``.
As ``prefix``, use the prefix as described above.

Similar considerations apply to type-parametrized tests. If necessary, specific
rules for type-parametrized tests will be added here.

Rationale
   All gtests (not only from the WAS components) are linked into libxul-gtest,
   which requires names to be unique within that large scope. In addition, it
   should be clear from the test name (e.g. in the test execution log) in what
   source file (or at least which directory) the test code can be found.
   Optimally, test names should be structured hierarchically to allow
   easy selection of groups of tests for execution. However, gtest has some
   restrictions that do not allow that completely. The guidelines try to
   accomodate for these as far as possible. Note that gtest recommends not to
   use underscores in test names in general, because this may lead to reserved
   names and naming conflicts, but the rules stated here should avoid that.
   In case of any problems arising, we can evolve the rules to accomodate
   for that.

Specifying types
----------------

Use of ``auto`` for declaring variables
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The `Google C++ Code Style on auto <https://google.github.io/styleguide/cppguide.html#auto>`_
allows the use of ``auto`` generally with encouragements for specific cases, which still
leaves a rather wide range for interpretation.

We extend this by some more encouragements and discouragements:

* DO use ``auto`` when the type is already present in the
  initialization expression (esp. a template argument or similar),
  e.g. ``auto c = static_cast<uint16_t>(*(iter++)) << 8;`` or
  ``auto x =  MakeRefPtr<MediaStreamError>(mWindow, *aError);``

* DO use ``auto`` if the spelled out type were complex otherwise,
  e.g. a nested typedef or type alias, e.g. ``foo_container::value_type``.

* DO NOT use ``auto`` if the type were spelled out as a builtin
  integer type or one of the types from ``<cstdint>``, e.g.
  instead of ``auto foo = funcThatReturnsUint16();`` use
  ``uint16_t foo = funcThatReturnsUint16();``.

.. note::
   Some disadvantages of using ``auto`` relate to the unavailability of type
   information outside an appropriate IDE/editor. This may be somewhat remedied
   by resolving `Bug 1567464 <https://bugzilla.mozilla.org/show_bug.cgi?id=1567464>`_
   which will make the type information available in searchfox. In consequence,
   the guidelines might be amended to promote a more widespread use of ``auto``.

Plain pointers
--------------

The use of plain pointers is error-prone. Avoid using owning plain pointers. In
particular, avoid using literal, non-placement new. There are various kinds
of smart pointers, not all of which provide appropriate factory functions.
However, where such factory functions exist, do use them (along with auto).
The following is an incomplete list of smart pointer types and corresponding
factory functions:

+------------------------+-------------------------+------------------------+
|          Type          |    Factory function     |      Header file       |
+========================+=========================+========================+
| ``mozilla::RefPtr``    | ``mozilla::MakeRefPtr`` | ``"mfbt/RefPtr.h"``    |
+------------------------+-------------------------+------------------------+
| ``mozilla::UniquePtr`` | ``mozilla::MakeUnique`` | ``"mfbt/UniquePtr.h"`` |
+------------------------+-------------------------+------------------------+
| ``std::unique_ptr``    | ``std::make_unique``    | ``<memory>``           |
+------------------------+-------------------------+------------------------+
| ``std::shared_ptr``    | ``std::make_shared``    | ``<memory>``           |
+------------------------+-------------------------+------------------------+
