use-isInstance
==============

Prefer ``.isInstance()`` in chrome scripts over the standard ``instanceof``
operator for DOM interfaces, since the latter will return false when the object
is created from a different context.

These files are covered:

- ``*.sys.mjs``
- ``*.jsm``
- ``*.jsm.js``
- ``*.xhtml`` with ``there.is.only.xul``
- ``*.js`` with a heuristic

Since there is no straightforward way to detect chrome scripts, currently the
linter assumes that any script including the following words are chrome
privileged. This of course may not be sufficient and is open for change:

- ``ChromeUtils``, but not ``SpecialPowers.ChromeUtils``
- ``BrowserTestUtils``, ``PlacesUtils``
- ``document.createXULElement``
- ``loader.lazyRequireGetter``
- ``Services.foo``, but not ``SpecialPowers.Services.foo``

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    node instanceof Node
    text instanceof win.Text
    target instanceof this.contentWindow.HTMLAudioElement

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    Node.isInstance(node)
    win.Text.isInstance(text)
    this.contentWindow.HTMLAudioElement.isInstance(target)
