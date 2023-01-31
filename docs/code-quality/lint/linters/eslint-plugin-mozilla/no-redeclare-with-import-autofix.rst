no-redeclare-with-import-autofix
================================

This is the
`builtin eslint rule no-redeclare <https://eslint.org/docs/latest/rules/no-redeclare>`_,
but with an additional fixer that can automatically remove superfluous
(duplicate) imports.

For redeclarations that are not imports, there is no automatic fix, as
the author will likely have to rename variables so that they do not
redeclare existing variables or conflict with the name of a builtin
property or global variable.

Typical duplicate imports happen when a `head.js` file, imports
a module and a test then subsequently also attempts to import the same
module.

In browser mochitests, an additional typical scenario is importing
modules that are already imported in the main browser window. Because
these tests run in a scope that inherits from the main browser window
one, there is no need to re-import such modules.
