no-comparison-or-assignment-inside-ok
=====================================

This rule prevents two misuses of the ``ok()`` test framework function.

Assignments
-----------

The first is one where people accidentally put assignments into ``ok``, like so:

.. code-block:: js

    ok(foo = bar, "Foo should be equal to bar");

This is similar to the builtin eslint rule `no-cond-assign`_.

There is no autofix as the linter cannot tell if the assignment was intentional
and should be moved out of the ``ok()`` call, or if it was intended to be some
kind of binary comparison expression (and if so, with what operator).

.. _no-cond-assign: https://eslint.org/docs/latest/rules/no-cond-assign

Comparisons
-----------

The second is a stylistic/legibility/debuggability preference, where the linter
encourages use of the dedicated ``Assert`` framework comparison functions. For
more details on ``Assert``, see :ref:`assert-module`.

This rule is autofixed, and will correct incorrect examples such as the
following:

Examples of incorrect code for this rule:
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: js

   ok(foo == bar);
   ok(foo < bar);
   ok(foo !== bar);

to something like:

Examples of correct code for this rule:
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: js

   Assert.equal(foo, bar);
   Assert.less(foo, bar);
   Assert.notStrictEqual(foo, bar);


Rationale
^^^^^^^^^

There are a few reasons the more verbose form is preferable:

- On failure, the framework will log both values rather than just one, making
  it much easier to debug failing tests without having to manually add
  logging, push to try, and then potentially retrigger to reproduce intermittents
  etc.
- The :ref:`assert-module` is standardized and more widely understood than the old
  mocha/mochikit forms.
- It is harder to make typos like the assignment case above, and accidentally
  end up with tests that assert non-sensical things.
- Subjectively, when an additional message is long enough to cause ``prettier``
  to split the statement across multiple lines, this makes it easier to
  see what the two operands of the comparison are.
