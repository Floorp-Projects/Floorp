prefer-formatValues
===================

Rejects multiple calls to document.l10n.formatValue in the same code block, to
reduce localization overheads.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    {
      document.l10n.formatValue('foobar');
      document.l10n.formatValue('foobaz');
    }

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    document.l10n.formatValue('foobar');
    document.l10n.formatValues(['foobar', 'foobaz']);
