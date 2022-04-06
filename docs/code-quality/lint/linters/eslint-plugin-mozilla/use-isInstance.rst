use-isInstance
==============

Prefer ``.isInstance()`` in chrome scripts over the standard ``instanceof``
operator for DOM interfaces, since the latter will return false when the object
is created from a different context.

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
