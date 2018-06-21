.. role:: bash(code)
   :language: bash

.. role:: js(code)
   :language: javascript

.. role:: python(code)
   :language: python

==================================
Migrating Legacy Strings to Fluent
==================================

Firefox is a project localized in over 100 languages. As the code for existing
features moves away from the old localization systems and starts using
`Fluent`_, we need to ensure that we don’t lose existing translations, which
would have the adverse effect of forcing contributors to localize hundreds of
strings from scratch.

`Fluent Migration`_ is a Python library designed to solve this specific problem:
it allows to migrate legacy translations from `.dtd` and `.properties` files,
not only moving strings and transforming them as needed to adapt to the `FTL`
syntax, but also replicating "blame" for each string in VCS.


Migration Recipes and Their Lifecycle
=====================================

The actual migrations are performed running Python modules called **migration
recipes**, which contain directives on how to migrate strings, which files are
involved, transformations to apply, etc. These recipes are stored in
`mozilla-central`__.

__ https://hg.mozilla.org/mozilla-central/file/default/python/l10n/fluent_migrations

When part of Firefox’s UI is migrated to Fluent, a migration recipe should be
attached to the same patch that adds new strings to `.ftl` files.

Migration recipes can quickly become obsolete, either because the syntax used in
these recipes changes, or because the referenced legacy strings and files are
removed from repositories. For these reasons, l10n-drivers periodically clean up
the `fluent_migrations` folder in mozilla-central, keeping only recipes for 2
shipping versions (Nightly and Beta).


.. hint::

  As a developer you don’t need to bother about updating migration recipes
  already in `mozilla-central`: if a new patch removes a string or file that is
  used in a migration recipe, simply ignore it, since the entire recipe will be
  removed within a couple of cycles.

  See also the `How Migrations Are Run on l10n Repositories`_ section.


How to Write Migration Recipes
==============================

The migration recipe’s filename should start with a reference to the associated
bug number, and include a brief description of the bug, e.g.
:bash:`bug_1451992_preferences_applicationManager.py` is the migration recipe
used to migrate the Application Manager window in preferences. It’s also
possible to look at existing recipes in `mozilla-central`__ for inspiration.

__ https://hg.mozilla.org/mozilla-central/file/default/python/l10n/fluent_migrations


Basic Migration
---------------

Let’s consider a basic example: one string needs to be migrated, without
any further change, from a DTD file to Fluent.

The legacy string is stored in :bash:`toolkit/locales/en-US/chrome/global/findbar.dtd`:


.. code-block:: dtd

  <!ENTITY next.tooltip "Find the next occurrence of the phrase">


The new Fluent string is stored in :bash:`toolkit/locales/en-US/toolkit/main-window/findbar.ftl`:


.. code-block:: properties

  findbar-next =
      .tooltiptext = Find the next occurrence of the phrase


This is how the migration recipe looks:


.. code-block:: python

  # coding=utf8

  # Any copyright is dedicated to the Public Domain.
  # http://creativecommons.org/publicdomain/zero/1.0/

  from __future__ import absolute_import
  import fluent.syntax.ast as FTL
  from fluent.migrate.helpers import transforms_from

  def migrate(ctx):
      """Bug 1411707 - Migrate the findbar XBL binding to a Custom Element, part {index}."""

      ctx.add_transforms(
          "toolkit/toolkit/main-window/findbar.ftl",
          "toolkit/toolkit/main-window/findbar.ftl",
          transforms_from(
  """
  findbar-next =
      .tooltiptext = { COPY(from_path, "next.tooltip") }
  """, from_path="toolkit/chrome/global/findbar.dtd")


The first important thing to notice is that the migration recipe needs file
paths relative to a localization repository, losing :bash:`locales/en-US/`:

 - :bash:`toolkit/locales/en-US/chrome/global/findbar.dtd` becomes
   :bash:`toolkit/chrome/global/findbar.dtd`.
 - :bash:`toolkit/locales/en-US/toolkit/main-window/findbar.ftl` becomes
   :bash:`toolkit/toolkit/main-window/findbar.ftl`.

The recipe includes a :python:`migrate` function, which can contain multiple
:python:`add_transforms` calls. The *docstring* for this function will be used
as a commit message in VCS, that’s why it’s important to make sure the bug
reference is correct, and to keep the `part {index}` section: multiple strings
could have multiple authors, and would be migrated in distinct commits (part 1,
part 2, etc.).

The :python:`context.add_transforms` function takes 3 arguments:

 - Path to the target l10n file.
 - Path to the source (en-US) file.
 - An array of Transforms. Transforms are AST nodes which describe how legacy
   translations should be migrated.

In this case there is only one Transform that migrates the string with ID
:js:`next.tooltip` from :bash:`toolkit/chrome/global/findbar.dtd`, and injects
it in the FTL fragment. The :python:`COPY` Transform allows to copy the string
from an existing file as is, while :python:`from_path` is used to avoid
repeating the same path multiple times, making the recipe more readable. Without
:python:`from_path`, this could be written as:


.. code-block:: python

  ctx.add_transforms(
      "toolkit/toolkit/main-window/findbar.ftl",
      "toolkit/toolkit/main-window/findbar.ftl",
      transforms_from(
  """
  findbar-next =
  .tooltiptext = { COPY("toolkit/chrome/global/findbar.dtd", "next.tooltip") }
  """)


This method of writing migration recipes allows to take the original FTL
strings, and simply replace the value of each message with a :python:`COPY`
Transform. :python:`transforms_from` takes care of converting the FTL syntax
into an array of Transforms describing how the legacy translations should be
migrated. This manner of defining migrations is only suitable to simple strings
where a copy operation is sufficient. For more complex use-cases which require
some additional logic in Python, it’s necessary to resort to the raw AST.


The example above is equivalent to the following syntax, which requires a deeper
understanding of the underlying AST structure:


.. code-block:: python

  ctx.add_transforms(
      "toolkit/toolkit/main-window/findbar.ftl",
      "toolkit/toolkit/main-window/findbar.ftl",
      [
          FTL.Message(
              id=FTL.Identifier("findbar-next"),
              attributes=[
                  FTL.Attribute(
                      id=FTL.Identifier("tooltiptext"),
                      value=COPY(
                          "toolkit/chrome/global/findbar.dtd",
                          "next.tooltip"
                      )
                  )
              ]
          )
      ]
  )

This creates a :python:`Message`, taking the value from the legacy string
:js:`findbar-next`. A message can have an array of attributes, each with an ID
and a value: in this case there is only one attribute, with ID :js:`tooltiptext`
and :js:`value` copied from the legacy string.

Notice how both the ID of the message and the ID of the attribute are
defined as an :python:`FTL.Identifier`, not simply as a string.


.. tip::

  It’s possible to concatenate arrays of Transforms defined manually, like in
  the last example, with those coming from :python:`transforms_from`, by using
  the :python:`+` operator. Alternatively, it’s possible to use multiple
  :python:`add_transforms`.

  The order of Transforms provided in the recipe is not relevant, the reference
  file is used for ordering messages.


Replacing Content in Legacy Strings
-----------------------------------

While :python:`COPY` allows to copy a legacy string as is, :python:`REPLACE`
(from `fluent.migrate`) allows to replace content while performing the
migration. This is necessary, for example, when migrating strings that include
placeholders or entities that need to be replaced to adapt to Fluent syntax.

Consider for example the following string:


.. code-block:: properties

  siteUsage = %1$S %2$S (Persistent)


Which needs to be migrated to:


.. code-block:: properties

  site-usage = { $value } { $unit } (Persistent)


:js:`%1$S` (and :js:`%S`) don’t match the format used in Fluent for variables,
so they need to be replaced within the migration process. This is how the
Transform is defined:


.. code-block:: python

  FTL.Message(
      id=FTL.Identifier("site-usage-pattern"),
      value=REPLACE(
          "browser/chrome/browser/preferences/preferences.properties",
          "siteUsage",
          {
              "%1$S": EXTERNAL_ARGUMENT(
                  "value"
              ),
              "%2$S": EXTERNAL_ARGUMENT(
                  "unit"
              )
          }
      )
  )


This creates an :python:`FTL.Message`, taking the value from the legacy string
:js:`siteUsage`, but replacing specified placeholders with Fluent variables.

It’s also possible to replace content with a specific text: in that case, it
needs to be defined as a :python:`TextElement`. For example, to replace
:js:`%S` with some HTML markup:


.. code-block:: python

  value=REPLACE(
      "browser/chrome/browser/preferences/preferences.properties",
      "searchResults.sorryMessageWin",
      {
          "%S": FTL.TextElement('<span data-l10n-name="query"></span>')
      }
  )


.. note::

  :python:`EXTERNAL_ARGUMENT` and :python:`MESSAGE_REFERENCE` are helper
  Transforms which can be used to save keystrokes in common cases where using
  the raw AST is too verbose.

  :python:`EXTERNAL_ARGUMENT` is used to create a reference to a variable, e.g.
  :js:`{ $variable }`.

  :python:`MESSAGE_REFERENCE` is used to create a reference to another message,
  e.g. :js:`{ another-string }`, or a `term`__, e.g. :js:`{ -brand-short-name }`.

  Both Transforms need to be imported at the beginning of the recipe, e.g.
  :python:`from fluent.migrate.helpers import EXTERNAL_ARGUMENT`

  __ https://projectfluent.org/fluent/guide/terms.html


Concatenating Strings
---------------------

It’s quite common to concatenate multiple strings coming from `DTD` and
`properties`, for example to create sentences with HTML markup. It’s possible to
concatenate strings and text elements in a migration recipe using the
:python:`CONCAT` Transform. This allows to generate a single Fluent message from
these fragments, avoiding run-time transformations as prescribed by
:ref:`Fluent’s social contract <fluent-tutorial-social-contract>`.

Note that, in case of simple migrations using :python:`transforms_from`, the
concatenation is carried out implicitly by using the Fluent syntax interleaved
with COPY() transform calls to define the migration recipe.

Consider the following example:


.. code-block:: properties

  # %S is replaced by a link, using searchResults.needHelpSupportLink as text
  searchResults.needHelp = Need help? Visit %S

  # %S is replaced by "Firefox"
  searchResults.needHelpSupportLink = %S Support


In Fluent:


.. code-block:: properties

  searchResults.needHelpSupportLink = Need help? Visit <a data-l10n-name="url">{ -brand-short-name } Support</a>


This is quite a complex migration: it requires to take 2 legacy strings, and
concatenate their values with HTML markup. Here’s how the Transform is defined:


.. code-block:: python

  FTL.Message(
      id=FTL.Identifier("search-results-help-link"),
      value=REPLACE(
          "browser/chrome/browser/preferences/preferences.properties",
          "searchResults.needHelp",
          {
              "%S": CONCAT(
                  FTL.TextElement('<a data-l10n-name="url">'),
                  REPLACE(
                      "browser/chrome/browser/preferences/preferences.properties",
                      "searchResults.needHelpSupportLink",
                      {
                          "%S": MESSAGE_REFERENCE("-brand-short-name"),
                      }
                  ),
                  FTL.TextElement("</a>")
              )
          }
      )
  ),


:js:`%S` in :js:`searchResults.needHelpSupportLink` is replaced by a reference
to the term :js:`-brand-short-name`, migrating from :js:`%S Support` to :js:`{
-brand-short-name } Support`. The result of this operation is then inserted
between two text elements to create the anchor markup. The resulting text is
finally  used to replace :js:`%S` in :js:`searchResults.needHelp`, and used as
value for the FTL message.


.. important::

  When concatenating existing strings, avoid introducing changes to the original
  text, for example adding spaces or punctuation. Each language has its own
  rules, and this might result in poor migrated strings. In case of doubt,
  always ask for feedback.


Plural Strings
--------------

Migrating plural strings from `.properties` files usually involves two
Transforms from :python:`fluent.migrate.transforms`: the
:python:`REPLACE_IN_TEXT` Transform takes TextElements as input, making it
possible to pass it as the foreach function of the :python:`PLURALS` Transform.

Consider the following legacy string:


.. code-block:: properties

  # LOCALIZATION NOTE (disableContainersOkButton): Semi-colon list of plural forms.
  # See: http://developer.mozilla.org/en/docs/Localization_and_Plurals
  # #1 is the number of container tabs
  disableContainersOkButton = Close #1 Container Tab;Close #1 Container Tabs


In Fluent:


.. code-block:: properties

  containers-disable-alert-ok-button =
      { $tabCount ->
          [one] Close { $tabCount } Container Tab
         *[other] Close { $tabCount } Container Tabs
      }


This is how the Transform for this string is defined:


.. code-block:: python

  FTL.Message(
      id=FTL.Identifier("containers-disable-alert-ok-button"),
      value=PLURALS(
          "browser/chrome/browser/preferences/preferences.properties",
          "disableContainersOkButton",
          EXTERNAL_ARGUMENT("tabCount"),
          lambda text: REPLACE_IN_TEXT(
              text,
              {
                  "#1": EXTERNAL_ARGUMENT("tabCount")
              }
          )
      )
  )


The `PLURALS` Transform will take care of creating the correct number of plural
categories for each language. Notice how `#1` is replaced for each of these
variants with :js:`{ $tabCount }`, using :python:`REPLACE_IN_TEXT` and
:python:`EXTERNAL_ARGUMENT("tabCount")`.

In this case it’s not possible to use :python:`REPLACE` because it takes a file
path and a message ID as arguments, whereas here the recipe needs to operate on
regular text. The replacement is performed on each plural form of the original
string, where plural forms are separated by a semicolon.

Complex Cases
-------------

It’s always possible to migrate strings by manually creating the underlying AST
structure. Consider the following complex Fluent string:


.. code-block:: properties

  use-current-pages =
      .label =
          { $tabCount ->
              [1] Use Current Page
             *[other] Use Current Pages
          }
      .accesskey = C


The migration for this string is quite complex: the :js:`label` attribute is
created from 2 different legacy strings, and it’s not a proper plural form.
Notice how the first string is associated to the :js:`1` case, not the :js:`one`
category used in plural forms. For these reasons, it’s not possible to use
:python:`PLURALS`, the Transform needs to be crafted recreating the AST.


.. code-block:: python


  FTL.Message(
      id=FTL.Identifier("use-current-pages"),
      attributes=[
          FTL.Attribute(
              id=FTL.Identifier("label"),
              value=FTL.Pattern(
                  elements=[
                      FTL.Placeable(
                          expression=FTL.SelectExpression(
                              expression=EXTERNAL_ARGUMENT("tabCount"),
                              variants=[
                                  FTL.Variant(
                                      key=FTL.NumberExpression("1"),
                                      default=False,
                                      value=COPY(
                                          "browser/chrome/browser/preferences/main.dtd",
                                          "useCurrentPage.label",
                                      )
                                  ),
                                  FTL.Variant(
                                      key=FTL.VariantName("other"),
                                      default=True,
                                      value=COPY(
                                          "browser/chrome/browser/preferences/main.dtd",
                                          "useMultiple.label",
                                      )
                                  )
                              ]
                          )
                      )
                  ]
              )
          ),
          FTL.Attribute(
              id=FTL.Identifier("accesskey"),
              value=COPY(
                  "browser/chrome/browser/preferences/main.dtd",
                  "useCurrentPage.accesskey",
              )
          ),
      ],
  ),


This Transform uses several concepts already described in this document. Notable
new elements are:

 - The fact that the `label` attribute is defined as a :python:`Pattern`. This
   is because, in this example, we’re creating a new value from scratch and
   migrating existing translations as its variants. Patterns are one of Fluent’s
   value types and, under the hood, all Transforms like :python:`COPY` or
   :python:`REPLACE` evaluate to Fluent Patterns.
 - A :python:`SelectExpression` is defined, with an array of :python:`Variant`
   objects.


How to Test Migration Recipes
=============================

Unfortunately, testing migration recipes requires several manual steps. We plan
to `introduce automated testing`__ for patches including migration recipes, in
the meantime this is how it’s possible to test migration recipes.

__ https://bugzilla.mozilla.org/show_bug.cgi?id=1353680


1. Install Fluent Migration
---------------------------

The first step is to install the `Fluent Migration`_ Python library. It’s
currently not available as a package, so the repository must be cloned locally
and installed manually, e.g. with :bash:`pip install -e .`.

Installing this package will make a :bash:`migrate-l10n` command available.


2. Clone gecko-strings
----------------------

Migration recipes work against localization repositories, which means it’s not
possible to test them directly against `mozilla-central`, unless the *source*
path (the second argument) in :python:`ctx.add_transforms` is temporarily
tweaked to match `mozilla-central` paths.

To test the actual recipe that will land in the patch, it’s necessary to clone
the `gecko-strings`_ repository on the system twice, in two separate folders.
One will simulate the reference en-US repository after the patch has landed, and
the other will simulate a target localization. For example, let’s call the two
folders `en-US` and `test`.


.. code-block:: bash

  hg clone https://hg.mozilla.org/l10n/gecko-strings en-US
  cp -r en-US test


3. Add new FTL strings to the local en-US repository
------------------------------------------------

The changed (or brand new) FTL files from the patch need to be copied into the
`en-US` repository. Remember that paths are slightly different, with
localization repositories missing the :bash:`locales/en-US` portion. There’s no
need to commit these changes locally.


4. Run the migration recipe
---------------------------

The system is all set to run the recipe with the following commands:


.. code-block:: bash

  cd PATH/TO/recipes

  migrate-l10n \
    --lang test
    --reference-dir PATH/TO/en-US \
    --localization-dir PATH/TO/test \
    --dry-run \
    name_of_the_recipe


The name of the recipe needs to be specified without the :bash:`.py` extension,
since it’s imported as a module.

Alternatively, before running :bash:`migrate-l10n`, it’s possible to update the
value of :bash:`PYTHONPATH` to include the folder storing migration recipes.


.. code-block:: bash

  export PYTHONPATH="${PYTHONPATH}:PATH/TO/recipes/"


The :bash:`--dry-run` option allows to run the recipe without making changes,
and it’s useful to spot syntax errors in the recipe. If there are no errors,
it’s possible to run the migration without :bash:`--dry-run` and actually commit
the changes locally.

This is the output of a migration:


.. code-block:: bash

  Running migration bug_1411707_findbar for test
  WARNING:migrate:Plural rule for "'test'" is not defined in compare-locales
  INFO:migrate:Localization file toolkit/toolkit/main-window/findbar.ftl does not exist and it will be created
    Writing to test/toolkit/toolkit/main-window/findbar.ftl
    Committing changeset: Bug 1411707 - Migrate the findbar XBL binding to a Custom Element, part 1.
    Writing to test/toolkit/toolkit/main-window/findbar.ftl
    Committing changeset: Bug 1411707 - Migrate the findbar XBL binding to a Custom Element, part 2.


.. hint::

  The warning about plural rules is expected, since `test` is not a valid locale
  code. At this point, the result of migration is committed to the local `test`
  folder.


5. Compare the resulting files
------------------------------

Once the migration has run, the `test` repository includes the migrated files,
and it’s possible to compare them with the files in `en-US`. Since the migration
code strips empty line between strings, it’s recommended to use :bash:`diff -B`
between the two files, or use a visual diff to compare their content.


6. Caveats
----------

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


How Migrations Are Run on l10n Repositories
===========================================

Once a patch including new FTL strings and a migration recipe lands in
mozilla-central, l10n-drivers will perform a series of actions to migrate
strings in all 100+ localization repositories:

 - New Fluent strings land in `mozilla-central`, together with a migration
   recipe.
 - New strings are added to `gecko-strings-quarantine`_, a unified repository
   including strings for all shipping versions of Firefox, and used as a buffer
   before exposing strings to localizers.
 - Migration recipes are run against all l10n repositories, migrating strings
   from old to new files, and storing them in VCS.
 - New en-US strings are pushed to the official `gecko-strings`_ repository
   used by localization tools, and exposed to all localizers.

Migration recipes could be run again within a release cycle, in order to migrate
translations for legacy strings added after the first run. They’re usually
removed from `mozilla-central` within 2 cycles, e.g. a migration recipe created
for Firefox 59 would be removed when Firefox 61 is available in Nightly.


.. tip::

  A script to run migrations on all l10n repositories is available in `this
  repository`__, automating part of the steps described for manual testing, and
  it could be adapted to local testing.

  __ https://github.com/flodolo/fluent-migrations


How to Get Help
===============

Writing migration recipes can be challenging for non trivial cases, and it can
require extensive l10n knowledge to avoid localizability issues.

Don’t hesitate to reach out to the l10n-drivers for feedback, help to test or
write the migration recipes:

 - Francesco Lodolo (:flod)
 - Staś Małolepszy (:stas)
 - Zibi Braniecki (:gandalf)
 - Axel Hecht (:pike)



.. _Fluent: http://projectfluent.org/
.. _Fluent Migration: https://hg.mozilla.org/l10n/fluent-migration/
.. _gecko-strings-quarantine: https://hg.mozilla.org/users/axel_mozilla.com/gecko-strings-quarantine
.. _gecko-strings: https://hg.mozilla.org/l10n/gecko-strings
