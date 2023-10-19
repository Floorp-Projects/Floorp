====================================
DOM Workers & Storage C++ Code Style
====================================

This page describes the code style for the components maintained by the DOM Workers & Storage team. They live in-tree under the 'dom/docs/indexedDB' directory.

.. contents::
   :depth: 4

Introduction
============

This code style currently applies to the components living in the following directories:

* ``dom/file``
* ``dom/indexedDB``
* ``dom/localstorage``
* ``dom/payments``
* ``dom/quota``
* ``dom/serviceworkers``
* ``dom/workers``

In the long-term, the code is intended to use the
:ref:`Mozilla Coding Style <Coding style>`,
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
| We prefer using "static", instead of anonymous C++ namespaces.                                         | Place all symbols that should have internal linkage in a single anonymous                  | All files       | Unclear. The recommendation in the Mozilla code style says this might change in the |
|                                                                                                        | namespace block at the top of an implementation file, rather than declaring them static.   |                 | future depending on debugger support, so this deviation might become obsolete.      |
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
   accommodate for these as far as possible. Note that gtest recommends not to
   use underscores in test names in general, because this may lead to reserved
   names and naming conflicts, but the rules stated here should avoid that.
   In case of any problems arising, we can evolve the rules to accommodate
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

Pointer types
-------------

Plain pointers
~~~~~~~~~~~~~~

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

Also, to create an ``already_AddRefed<>`` to pass as a parameter or return from
a function without the need to dereference it, use ``MakeAndAddRef`` instead of
creating a dereferenceable ``RefPtr`` (or similar) first and then using
``.forget()``.

Smart pointers
~~~~~~~~~~~~~~

In function signatures, prefer accepting or returning ``RefPtr`` instead of
``already_AddRefed`` in conjunction with regular ``std::move`` rather than
``.forget()``. This improves readability and code generation. Prevailing
legimitate uses of ``already_AddRefed`` are described in its
`documentation <https://searchfox.org/mozilla-central/rev/4df8821c1b824db5f40f381f48432f219d99ae36/mfbt/AlreadyAddRefed.h#31>`_.

Prefer using ``mozilla::UniquePtr`` over ``nsAutoPtr``, since the latter is
deprecated (and e.g. has no factory function, see Bug 1600079).

Use ``nsCOMPtr<T>`` iff ``T`` is an XPCOM interface type
(`more details on MDN <https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/nsCOMPtr_versus_RefPtr>`).

Enums
-----

Use scoped resp. strongly typed enums (``enum struct``) rather than non-scoped
enums. Use PascalCase for naming the values of scoped enums.

Evolution Process
=================

This section explains the process to evolve the coding style described in this
document. For clarity, we will distinguish coding tasks from code style
evolution tasks in this section.

Managing code style evolution tasks
-----------------------------------

A code style evolution task is a task that ought to amend or revise the
coding style as described in this document.

Code style evolution tasks should be managed in Bugzilla, as individual bugs
for each topic. All such tasks
should block the meta-bug
`1586788 <https://bugzilla.mozilla.org/show_bug.cgi?id=1586788>`.

When you take on to work on a code style evolution task:

- The task may already include a sketch of a resolution. If no preferred
  solution is obvious, discuss options to resolve it via comments on the bug
  first.
- When the general idea is ready to be spelled out in this document, amend or
  revise it accordingly.
- Submit the changes to this document as a patch to Phabricator, and put it up
  for review. Since this will affect a number of people, every change should
  be reviewed by at least two people. Ideally, this should include the owner
  of this style document and one person with good knowledge of the parts of
  the code base this style applies to.
- If there are known violations of the amendment to the coding style, consider
  fixing some of them, so that the amendment is tested on actual code. If
  the code style evolution task refers to a particular code location from a
  review, at least that location should be fixed to comply with the amended
  coding style.
- When you have two r+, land the patch.
- Report on the addition in the next team meeting to raise awareness.

Basis for code style evolution tasks
------------------------------------

The desire or necessity to evolve the code style can originate from
different activities, including
- reviews
- reading or writing code locally
- reading the coding style
- general thoughts on coding style

The code style should not be cluttered with aspects that are rarely
relevant or rarely leads to discussions, as the maintenance of the
code style has a cost as well. The code style should be as comprehensive
as necessary to reduce the overall maintenance costs of the code and
code style combined.

A particular focus is therefore on aspects that led to some discussion in
a code review, as reducing the number or verbosity of necessary style
discussions in reviews is a major indicator for the effectiveness of the
documented style.

Evolving code style based on reviews
------------------------------------

The goal of the process described here is to take advantage of style-related
discussions that originate from a code review, but to decouple evolution of
the code style from the review process, so that it does not block progress on
the underlying bug.

The following should be considered when performing a review:

- Remind yourself of the code style, maybe skim through the document before
  starting the review, or have it open side-by-side while doing the review.
- If you find a violation of an existing rule, add an inline comment.
- Have an eye on style-relevant aspects in the code itself or after a
  discussions with the author. Consider if this could be generalized into a
  style rule, but is not yet  covered by the documented global or local style.
  This might be something that is in a different style as opposed to other
  locations, differs from your personal style, etc.
- In that case, find an acceptable temporary solution for the code fragments
  at hand, which is acceptable for an r+ of the patch. Maybe agree with the
  code author on adding a comment that this should be revised later, when
  a rule is codified.
- Create a code style evolution task in Bugzilla as described above. In the
  description of the bug, reference the review comment that gave rise to it.
  If you can suggest a resolution, include that in the description, but this
  is not a necessary condition for creating the task.

Improving code style compliance when writing code
-------------------------------------------------

Periodically look into the code style document, and remind yourself of its
rules, and give particular attention to recent changes.

When writing code, i.e. adding new code or modify existing code,
remind yourself of checking the code for style compliance.

Time permitting, resolve existing violations on-the-go as part of other work
in the code area. Submit such changes in dedicated patches. If you identify
major violations that are too complex to resolve on-the-go, consider
creating a bug dedicated to the resolution of that violation, which
then can be scheduled in the planning process.

Syncing with the global Mozilla C++ Coding Style
------------------------------------------------

Several aspects of the coding style described here will be applicable to
the overall code base. However, amendments to the global coding style will
affect a large number of code authors and may require extended discussion.
Deviations from the global coding style should be limited in the long term.
On the other hand, amendments that are not relevant to all parts of the code
base, or where it is difficult to reach a consensus at the global scope,
may make sense to be kept in the local style.

The details of synchronizing with the global style are subject to discussion
with the owner and peers of the global coding style (see
`Bug 1587810 <https://bugzilla.mozilla.org/show_bug.cgi?id=1587810>`).

FAQ
---

* When someone introduces new code that adheres to the current style, but the
  remainder of the function/class/file does not, is it their responsibility
  to update that remainder on-the-go?

  The code author is not obliged to update the remainder, but they are
  encouraged to do so, time permitting. Whether that is the case depends on a
  number of factors, including the number and complexity of existing style
  violations, the risk introduced by changing that on the go etc. Judging this
  is left to the code author.
  At the very least, the function/class/file should not be left in a worse
  state than before.

* Are stylistic inconsistencies introduced by applying the style as defined
  here only to new code considered acceptable?

  While this is certainly not optimal, accepting such inconsistencies to
  some degree is inevitable to allow making progress towards an improved style.
  Personal preferences regarding the degree may differ, but in doubt such
  inconsistencies should be considered acceptable. They should not block a bug
  from being closed.
