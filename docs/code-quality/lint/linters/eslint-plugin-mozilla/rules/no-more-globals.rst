no-more-globals
===============

This rule is used to discourage people from adding ever more global variables to
files where the rule is run.

For any file ``example.js`` where the rule is applied, the rule will read
an allowlist of globals from a sibling ``example.js.globals`` file. The rule
will warn for any globals defined in ``example.js`` that are not listed in the
``globals`` file, and will also warn for any items on the allowlist in
the ``globals`` file that are no longer present in ``example.js``, encouraging
people to remove them from the ``globals`` list.

If you see errors from this rule about new globals, **do not just add items to
the allowlist**. Instead, work to find a way to run your code so that it does
not add to the list of globals.

If you see errors when removing globals, simply remove them from the allowlist
to make sure it is up-to-date and things do not inadvertently end up getting
put back in.
