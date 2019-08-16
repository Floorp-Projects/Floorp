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
  """, from_path="toolkit/chrome/global/findbar.dtd"))


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
  """))


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


.. code-block:: DTD

  <!ENTITY aboutSupport.featuresTitle "&brandShortName; Features">


Which needs to be migrated to:


.. code-block:: fluent

  features-title = { -brand-short-name } Features


The entity :js:`&brandShortName;` needs to be replaced with a term reference:


.. code-block:: python

  FTL.Message(
      id=FTL.Identifier("features-title"),
      value=REPLACE(
          "toolkit/chrome/global/aboutSupport.dtd",
          "aboutSupport.featuresTitle",
          {
              "&brandShortName;": TERM_REFERENCE("brand-short-name"),
          },
      )
  ),


This creates an :python:`FTL.Message`, taking the value from the legacy string
:js:`aboutSupport.featuresTitle`, but replacing the specified text with a
Fluent term reference.

.. note::
  :python:`REPLACE` replaces all occurrences of the specified text.


It’s also possible to replace content with a specific text: in that case, it
needs to be defined as a :python:`TextElement`. For example, to replace
:js:`example.com` with HTML markup:


.. code-block:: python

  value=REPLACE(
      "browser/chrome/browser/preferences/preferences.properties",
      "searchResults.sorryMessageWin",
      {
          "example.com": FTL.TextElement('<span data-l10n-name="example"></span>')
      }
  )


The situation is more complex when a migration recipe needs to replace
:js:`printf` arguments like :js:`%S`. In fact, the format used for localized
and source strings doesn’t need to match, and the two following strings using
unordered and ordered argument are perfectly equivalent:


.. code-block:: properties

  btn-quit = Quit %S
  btn-quit = Quit %1$S


In this scenario, replacing :js:`%S` would work on the first version, but not
on the second, and there’s no guarantee that the localized string uses the
same format as the source string.

Consider also the following string that uses :js:`%S` for two different
variables, implicitly relying on the order in which the arguments appear:


.. code-block:: properties

  updateFullName = %S (%S)


And the target Fluent string:


.. code-block:: fluent

  update-full-name = { $name } ({ $buildID })


As indicated, :python:`REPLACE` would replace all occurrences of :js:`%S`, so
only one variable could be set. The string needs to be normalized and treated
like:


.. code-block:: properties

  updateFullName = %1$S (%2$S)


This can be obtained by calling :python:`REPLACE` with
:python:`normalize_printf=True`:


.. code-block:: python

  FTL.Message(
      id=FTL.Identifier("update-full-name"),
      value=REPLACE(
          "toolkit/chrome/mozapps/update/updates.properties",
          "updateFullName",
          {
              "%1$S": VARIABLE_REFERENCE("name"),
              "%2$S": VARIABLE_REFERENCE("buildID"),
          },
          normalize_printf=True
      )
  )


.. attention::

  To avoid any issues :python:`normalize_printf=True` should always be used when
  replacing :js:`printf` arguments.


.. note::

  :python:`VARIABLE_REFERENCE`, :python:`MESSAGE_REFERENCE`, and
  :python:`TERM_REFERENCE` are helper Transforms which can be used to save
  keystrokes in common cases where using the raw AST is too verbose.

  :python:`VARIABLE_REFERENCE` is used to create a reference to a variable, e.g.
  :js:`{ $variable }`.

  :python:`MESSAGE_REFERENCE` is used to create a reference to another message,
  e.g. :js:`{ another-string }`, e.g. :js:`{ another-string }`.

  :python:`TERM_REFERENCE` is used to create a reference to a `term`__,
  e.g. :js:`{ -brand-short-name }`.

  Both Transforms need to be imported at the beginning of the recipe, e.g.
  :python:`from fluent.migrate.helpers import VARIABLE_REFERENCE`

  __ https://projectfluent.org/fluent/guide/terms.html


Removing Unnecessary Whitespaces in Translations
------------------------------------------------

It’s not uncommon to have lines with unnecessary leading or trailing spaces in
DTDs. These are not meaningful, don’t have practical results on the way the
string is displayed in products, and are added only for formatting reasons. For
example, consider this string:


.. code-block:: DTD

  <!ENTITY aboutAbout.note   "This is a list of “about” pages for your convenience.<br/>
                              Some of them might be confusing. Some are for diagnostic purposes only.<br/>
                              And some are omitted because they require query strings.">


If migrated as is, it would result in:


.. code-block:: fluent

  about-about-note =
      This is a list of “about” pages for your convenience.<br/>
                                  Some of them might be confusing. Some are for diagnostic purposes only.<br/>
                                  And some are omitted because they require query strings.


This can be avoided by trimming the migrated string, with :python:`trim:"True`
or :python:`trim=True`, depending on the context:


.. code-block:: python

  transforms_from(
  """
  about-about-note = { COPY("toolkit/chrome/global/aboutAbout.dtd", "aboutAbout.note", trim:"True") }
  """)

  FTL.Message(
      id=FTL.Identifier("discover-description"),
      value=REPLACE(
          "toolkit/chrome/mozapps/extensions/extensions.dtd",
          "discover.description2",
          {
              "&brandShortName;": TERM_REFERENCE("-brand-short-name")
          },
          trim=True
      )
  ),


.. attention::

  Trimming whitespaces should only be done when migrating strings from DTDs,
  not for other file formats, and when it’s clear that the context makes
  whitespaces irrelevant. A counter example would be the use of a string in
  combination with :js:`white-space: pre`.


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


.. code-block:: fluent

  search-results-need-help-support-link = Need help? Visit <a data-l10n-name="url">{ -brand-short-name } Support</a>


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
                          "%S": TERM_REFERENCE("brand-short-name"),
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


.. code-block:: fluent

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
          VARIABLE_REFERENCE("tabCount"),
          lambda text: REPLACE_IN_TEXT(
              text,
              {
                  "#1": VARIABLE_REFERENCE("tabCount")
              }
          )
      )
  )


The `PLURALS` Transform will take care of creating the correct number of plural
categories for each language. Notice how `#1` is replaced for each of these
variants with :js:`{ $tabCount }`, using :python:`REPLACE_IN_TEXT` and
:python:`VARIABLE_REFERENCE("tabCount")`.

In this case it’s not possible to use :python:`REPLACE` because it takes a file
path and a message ID as arguments, whereas here the recipe needs to operate on
regular text. The replacement is performed on each plural form of the original
string, where plural forms are separated by a semicolon.

Complex Cases
-------------

It’s always possible to migrate strings by manually creating the underlying AST
structure. Consider the following complex Fluent string:


.. code-block:: fluent

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
                              selector=VARIABLE_REFERENCE("tabCount"),
                              variants=[
                                  FTL.Variant(
                                      key=FTL.NumberLiteral("1"),
                                      default=False,
                                      value=COPY(
                                          "browser/chrome/browser/preferences/main.dtd",
                                          "useCurrentPage.label",
                                      )
                                  ),
                                  FTL.Variant(
                                      key=FTL.Identifier("other"),
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

It will also show the diff between the migrated files and the reference, ignoring
blank lines.

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
