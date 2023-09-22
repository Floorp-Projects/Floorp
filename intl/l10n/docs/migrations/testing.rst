.. role:: bash(code)
   :language: bash

.. role:: js(code)
   :language: javascript

.. role:: python(code)
   :language: python

=============================
How to Test Migration Recipes
=============================

To test migration recipes, use the following mach command:

.. code-block:: bash

  ./mach fluent-migration-test python/l10n/fluent_migrations/bug_1485002_newtab.py

This will analyze your migration recipe to check that the :python:`migrate`
function exists, and interacts correctly with the migration context. Once that
passes, it clones :bash:`gecko-strings` into :bash:`$OBJDIR/python/l10n`, creates a
reference localization by adding your local Fluent strings to the ones in
:bash:`gecko-strings`. It then runs the migration recipe, both as dry run and
as actual migration. Finally it analyzes the commits, and checks if any
migrations were actually run and the bug number in the commit message matches
the migration name.

At the end of the execution, the output will include a diff if there are
differences between the migrated files and the reference content (blank lines
are automatically ignored). There are cases where a diff is still expected, even
if the recipe is correct:

- If the patch includes new strings that are not being migrated, the diff
  output will show these as removals. This occurs because the migration recipe
  test contains the latest version of strings from :bash:`gecko-strings` with
  only migrations applied, while the reference file contains all string changes
  being introduced by the patch.
- If there are pending changes to FTL files included in the recipe that landed
  in the last few days, and haven't been pushed to :bash:`gecko-strings` yet
  (they're in :bash:`gecko-strings-quarantine`), these will show up as
  additions.

If a diff is displayed and the patch doesn't fall into the highlighted cases,
there may be an issue with the migration recipe.

You can inspect the generated repository further by looking at

.. code-block:: bash

  ls $OBJDIR/python/l10n/bug_1485002_newtab/en-US

Caveats
-------

Be aware of hard-coded English context in migration. Consider for example:


.. code-block:: python

  ctx.add_transforms(
          "browser/browser/preferences/siteDataSettings.ftl",
          "browser/browser/preferences/siteDataSettings.ftl",
          transforms_from(
  """
  site-usage-persistent = { site-usage-pattern } (Persistent)
  """)
  )


This Transform will pass a manual comparison, since the two files are identical,
but will result in :js:`(Persistent)` being hard-coded in English for all
languages.
