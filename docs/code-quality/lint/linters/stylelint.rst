Stylelint
=========

`Stylelint`__ is a popular linter for CSS.

Run Locally
-----------

The mozlint integration of Stylelint can be run using mach:

.. parsed-literal::

    $ mach lint --linter stylelint <file paths>

Alternatively, omit the ``--linter stylelint`` and run all configured linters, which will include
Stylelint.

Stylelint also supports the ``--fix`` option to autofix most errors raised from most of the rules.

See the `Usage guide`_ for more options.

Understanding Rules and Errors
------------------------------

* Only some files are linted, see the :searchfox:`configuration <tools/lint/stylelint.yml>` for details.

  * By design we do not lint/format reftests not crashtests as these are specially crafted tests.

* If you don't understand a rule, you can look it in `stylelint.io's rule list`_ for more
  information about it.

Common Issues and How To Solve Them
-----------------------------------

This code should neither be linted nor formatted
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* If it is a third-party piece of code, please add it to :searchfox:`ThirdPartyPaths.txt <tools/rewriting/ThirdPartyPaths.txt>`.
* If it is a generated file, please add it to :searchfox:`Generated.txt <tools/rewriting/Generated.txt>`.
* If intentionally invalid, please add it to :searchfox:`.stylelintignore <.stylelintignore>`.

Configuration
-------------

The global configuration file lives in ``topsrcdir/.stylelintrc.js``.
For an overview of the supported configuration, see `Stylelint's documentation`_.

Please keep differences in rules across the tree to a minimum. We want to be consistent to
make it easier for developers.

Sources
-------

* :searchfox:`Configuration (YAML) <tools/lint/stylelint.yml>`
* :searchfox:`Source <tools/lint/stylelint/__init__.py>`

Builders
--------

`Gijs Kruitbosch (gijs) <https://people.mozilla.org/s?query=gijs>`__ owns
the builders. Questions can also be asked on #lint:mozilla.org on Matrix.

Stylelint task
^^^^^^^^^^^^^^

This is a tier-1 task. For test failures the patch causing the
issue should be backed out or the issue fixed.

Some failures can be fixed with ``./mach lint -l stylelint --fix path/to/file``.

For test harness issues, file bugs in Developer Infrastructure :: Lint and Formatting.


.. __: https://stylelint.io/
.. _Usage guide: ../usage.html
.. _Stylelint's documentation: https://stylelint.io/user-guide/configure/
.. _stylelint.io's rule list: https://stylelint.io/user-guide/rules/
