CSS Guidelines
==============

This document contains guidelines defining how CSS inside the Firefox
codebase should be written, it is notably relevant for Firefox front-end
engineers.

Basics
------

Here are some basic tips that can optimize reviews if you are changing
CSS:

-  Avoid ``!important`` but if you have to use it, make sure it's
   obvious why you're using it (ideally with a comment). The
   `Overriding CSS`_ section contains more information about this.
-  Avoid magic numbers; prefer automatic sizing or alignment methods.
   Some examples to avoid:

   -  absolutely positioned elements
   -  hardcoded values such as: ``vertical-align: -2px;`` . The reason
      you should avoid such "hardcoded" values is that, they don't
      necessarily work for all font-size configurations.

-  Avoid setting styles in JavaScript. It's generally better to set a
   class and then specify the styles in CSS.
-  ``classList`` is generally better than ``className``. There's less
   chance of overwriting an existing class.
-  Only use generic selectors such as ``:last-child``, when it is what
   you mean semantically. If not, using a semantic class name is more
   descriptive and usually better.

Boilerplate
~~~~~~~~~~~

Make sure each file starts with the standard copyright header (see
`License Boilerplate <https://www.mozilla.org/MPL/headers/>`__).

Before adding more CSS
~~~~~~~~~~~~~~~~~~~~~~

It is good practice to check if the CSS that is being written is needed,
it can be the case that a common component has been already written
could be reused with or without changes. Most of the time, the common
component already follows the a11y/theme standards defined in this
guide. So, when possible, always prefer editing common components to
writing your own.

Also, it is good practice to introduce a common class when the new
element you are styling reuses some styles from another element, this
allows the maintenance cost and the amount of code duplication to be
reduced.

Formatting
----------

Spacing & Indentation
~~~~~~~~~~~~~~~~~~~~~

-  2 spaces indentation is preferred
-  Add a space after each comma, **except** within color functions:

.. code:: css

   linear-gradient(to bottom, black 1px, rgba(255,255,255,0.2) 1px)

-  Always add a space before ``!important``.

Omit units on 0 values
~~~~~~~~~~~~~~~~~~~~~~

Do this:

.. code:: css

     margin: 0;

Not this:

.. code:: css

     margin: 0px;

Use expanded syntax
~~~~~~~~~~~~~~~~~~~

It is often harder to understand what the shorthand is doing and the
shorthand can also hide some unwanted default values. It is good to
privilege expanded syntax to make your intentions explicit.

Do this:

.. code:: css

     border-color: red;

Not this:

.. code:: css

     border: red;

Put multiple selectors on different lines
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Do this:

.. code:: css

   h1,
   h2,
   h3 {
     font-family: sans-serif;
     text-align: center;
   }

Not this:

.. code:: css

   h1, h2, h3 {
     font-family: sans-serif;
     text-align: center;
   }

Naming standards for class names
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  ``lower-case-with-dashes`` is the most common.
-  But ``camelCase`` is also used sometimes. Try to follow the style of
   existing or related code.

Other tips
~~~~~~~~~~

-  Assume ``="true"`` in attribute selectors.

   -  Example: Use ``option[checked]``, not ``option[checked="true"]``.

-  Avoid ID selectors unless it is really the wanted goal, since IDs
   have higher specificity and therefore are harder to override.
-  Using descendant selectors is good practice for performance when
   possible:

   -  For example:
      ``.autocomplete-item[selected] > .autocomplete-item-title`` would
      be more efficient than
      ``.autocomplete-item[selected] .autocomplete-item-title``

Overriding CSS
--------------

Before overriding any CSS rules, check whether overriding is really
needed. Sometimes, when copy-pasting older code, it happens that the
code in question contains unnecessary overrides. This could be because
the CSS that it was overriding got removed in the meantime. In this
case, dropping the override should work.

It is also good practice to look at whether the rule you are overriding
is still needed: maybe the UX spec for the component has changed and
that rule can actually be updated or removed. When this is the case,
don't be afraid to remove or update that rule.

Once the two things above have been checked, check if the other rule you
are overriding contains ``!important``, if that is case, try putting it
in question, because it might have become obsolete.

Afterwards, check the specificity of the other selector; if it is
causing your rule to be overridden, you can try reducing its
specificity, either by simplifying the selector or by changing where the
rule is placed in the stylesheet. If this isn't possible, you can also
try introducing a ``:not()`` to prevent the other rule from applying,
this is especially relevant for different element states (``:hover``,
``:active``, ``[checked]`` or ``[disabled]``). However, never try to
increase the selector of the rule you are adding as it can easily become
hard to understand.

Finally, once you have checked all the things above, you can permit
yourself to use ``!important`` along with a comment why it is needed.

Using CSS variables
-------------------

Adding new variables
~~~~~~~~~~~~~~~~~~~~

Before adding new CSS variables, please consider the following
questions:

#. **Is the variable value changed at runtime?**
   *(Either from JavaScript or overridden by another CSS file)*
   **If the answer is no**, consider using a preprocessor variable or
   inlining the value.

#. **Is the variable value used multiple times?**
   **If the answer is no and the value isn't changed at runtime**, then
   you likely don't need a CSS variable.

#. **Is there an alternative to using the variable like inheriting or
   using the ``currentcolor`` keyword?**
   Using inheriting or using ``currentcolor`` will prevent repetition of
   the value and it is usually good practice to do so.

In general, it's good to first think of how some CSS could be written
cleanly without the CSS variable(s) and then think of how the CSS
variable could improve that CSS.

Using variables
~~~~~~~~~~~~~~~

Use the variable according to its naming
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Do this:

.. code:: css

   xul|tab:hover {
     background-color: var(--in-content-box-background-hover);
   }

Not this:

.. code:: css

   #certificateErrorDebugInformation {
     background-color: var(--in-content-box-background-hover);
   }

Localization
------------

Text Direction
~~~~~~~~~~~~~~

-  For margins, padding and borders, use
   ``inline-start``/``inline-end`` rather than ``left``/``right``.
   *Example:* Use ``margin-inline-start: 3px;`` instead of
   ``margin-left: 3px``.
-  For RTL-aware positioning (left/right), use
   ``inset-inline-start``/``inset-inline-end``.
-  For RTL-aware float layouts, ``float: inline-start|inline-end`` can
   be used instead of ``float: left|right``.
-  The RTL-aware equivalents of
   ``border-{top/bottom}-{left/right}-radius`` are
   ``border-{start/end}-{start/end}-radius``
-  When there is no special RTL-aware property available, use the pseudo
   ``:-moz-locale-dir(ltr|rtl)`` (for XUL files) or ``:dir(ltr|rtl)``
   (for HTML files).
-  Remember that while a tab content's scrollbar still shows on the
   right in RTL, an overflow scrollbar will show on the left.
-  Write ``padding: 0 3px 4px;`` instead of
   ``padding: 0 3px 4px 3px;``. This makes it more obvious that the
   padding is symmetrical (so RTL won't be an issue).

.. note::

   See `CSS Logical Properties and
   Values <https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_Logical_Properties>`__
   for more information.

Writing cross-platform CSS
--------------------------

Firefox supports many different platforms and each of those platforms
can contain many different configurations:

-  Windows 7, 8 and 10

   -  Default theme
   -  Aero basic (Windows 7, 8)
   -  Windows classic (Windows 7)
   -  High contrast (All versions)

-  Linux
-  macOS

File structure
~~~~~~~~~~~~~~

-  The ``browser/`` directory contains styles specific to Firefox
-  The ``toolkit/`` directory contains styles that are shared across all
   toolkit applications (Thunderbird and SeaMonkey)

Under each of those two directories, there is a ``themes`` directory
containing 4 sub-directories:

-  ``shared``
-  ``linux``
-  ``osx``
-  ``windows``

The ``shared`` directories contain styles shared across all 3 platforms,
while the other 3 directories contain styles respective to their
platform.

For new CSS, when possible try to privilege using the ``shared``
directory, instead of writing the same CSS for the 3 platform specific
directories, especially for large blocks of CSS.

Content CSS vs. Theme CSS
^^^^^^^^^^^^^^^^^^^^^^^^^

The following directories also contain CSS:

-  ``browser/base/content/``
-  ``toolkit/content/``

These directories contain content CSS, that applies on all platforms,
which is styling deemed to be essential for the browser to behave
correctly. To determine whether some CSS is theme-side or content-side,
it is useful to know that certain CSS properties are going to lean one
way or the other: color - 99% of the time it will be theme CSS, overflow
- 99% content.

+-----------------+--------------+----------------+----------------+
| 99% theme       | 70% theme    | 70% content    | 99% content    |
+=================+==============+================+================+
| font-\*, color, | line-height, | cursor, width, | overflow,      |
| \*-color,       | padding,     | max-width,     | direction,     |
| border-\*,      | margin       | top,           | display,       |
| -moz-appearance |              | bottom [2]_,   | \*-align,      |
| [1]_            |              | etc            | align-\*,      |
|                 |              |                | \*-box-\*,     |
|                 |              |                | flex-\*, order |
+-----------------+--------------+----------------+----------------+

If some CSS is layout or functionality related, then it is likely
content CSS. If it is esthetics related, then it is likely theme CSS.

When importing your stylesheets, it's best to import the content CSS
before the theme CSS, that way the theme values get to override the
content values (which is probably what you want), and you're going to
want them both after the global values, so your imports will look like
this:

.. code:: html

   <?xml-stylesheet href="chrome://global/skin/global.css" type="text/css"?>
   <?xml-stylesheet href="chrome://browser/content/path/module.css" type="text/css"?>
   <?xml-stylesheet href="chrome://browser/skin/path/module.css" type="text/css"?>

.. [1] -moz-appearance is tricky. Generally, when specifying
   -moz-appearance: foo; you're giving hints as to how something should
   act, however -moz-appearance: none; is probably saying 'ignore
   browser preconceptions - I want a blank sheet', so that's more
   visual. However -moz-appearance values aren't implemented and don't
   behave consistently across platforms, so idealism aside
   -moz-appearance should always be in theme CSS.

.. [2] However there is probably a better way than using absolute
   positioning.

Colors
~~~~~~

For common areas of the Firefox interface (panels, toolbar buttons,
etc.), mozilla-central often comes with some useful CSS variables that
are adjusted with the correct values for different platform
configurations, so using those CSS variables can definitively save some
testing time, as you can assume they already work correctly.

Using the ``currentcolor`` keyword or inheriting is also good practice,
because sometimes the needed value is already in the color or on the
parent element. This is especially useful in conjunction with icons
using ``-moz-context-properties: fill;`` where the icon can adjust to
the right platform color automatically from the text color. It is also
possible to use ``currentcolor`` with other properties like
``opacity`` or ``fill-opacity`` to have different
opacities of the platform color.

High contrast mode
~~~~~~~~~~~~~~~~~~

Content area
^^^^^^^^^^^^

On Windows high contrast mode, in the content area, Gecko does some
automatic color adjustments regarding page colors. Part of those
adjustments include making all ``box-shadow`` invisible, so this is
something to be aware of if you create a focus ring or a border using
the ``box-shadow`` property: consider using a ``border`` or an
``outline`` if you want the border/focus ring to stay visible in
high-contrast mode. An example of such bug is `bug
1516767 <https://bugzilla.mozilla.org/show_bug.cgi?id=1516767>`__.

Another adjustment to be aware of is that Gecko removes all the
``background-image`` when high contrast mode is enabled. Consider using
an actual ``<img>`` tag (for HTML documents) or ``list-style-image``
(for XUL documents) if rendering the image is important.

If you are not using Windows, one way to test against those adjustments
on other platforms is:

-  Going to about:preferences
-  Clicking on the "Colors..." button in the "Fonts & Colors"
   sub-section of the "Language and Appearance" section
-  Under "Override the colors specified by the page with your selections
   above", select the "Always" option

Chrome area
^^^^^^^^^^^

The automatic adjustments previously mentioned only apply to pages
rendered in the content area. The chrome area of Firefox uses colors as
authored, which is why using pre-defined variables, ``currentcolor`` or
inheritance is useful to integrate with the system theme with little
hassle.

If not, as a last resort, using `system
colors <https://developer.mozilla.org/en-US/docs/Web/CSS/color_value#system_colors>`__
also works for non-default Windows themes or Linux. In general, the
following colors are used:

-  ``-moz-Field``: textbox or field background colors, also used as the
   background color of listboxes or trees.
-  ``-moz-FieldText``: textbox or field text colors, also used as the
   text color of listboxes or trees.
-  ``-moz-Dialog``: window or dialog background color.
-  ``-moz-DialogText``: window or dialog text color.
-  ``GrayText``: used on disabled items as text color. Do not use it on
   text that is not disabled to desemphsize text, because it does not
   guarantee a sufficient contrast ratio for non-disabled text.
-  ``ThreeDShadow``: Used as border on elements.
-  ``ThreeDLightShadow``: Used as light border on elements.

Using the background/text pairs is especially important to ensure the
contrast is respected in all situations. Never mix custom text colors
with a system background color and vice-versa.

Note that using system colors is only useful for the chrome area, since
content area colors are overridden by Gecko anyway.

Writing media queries
~~~~~~~~~~~~~~~~~~~~~

Boolean media queries
^^^^^^^^^^^^^^^^^^^^^

Do this:

.. code:: css

   @media (-moz-mac-yosemite-theme: 0) {

Not this:

.. code:: css

   @media not all and (-moz-mac-yosemite-theme) {

Privilege CSS for most common configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

It is better to put the most common configuration (latest version of an
OS, or default theme for example) outside of the media query. In the
following example, ``-moz-mac-yosemite-theme`` targets macOS 10.10 and
higher, so it should be privileged over the styling for macOS 10.9.

Do this:

.. code:: css

   @media (-moz-mac-yosemite-theme: 0) {
     #placesList {
       box-shadow: inset -2px 0 0 hsla(0,0%,100%,.2);
     }
   }

Not this:

.. code:: css

   #placesList {
     box-shadow: inset -2px 0 0 hsla(0,0%,100%,.2);
   }

   @media (-moz-mac-yosemite-theme) {
     #placesList {
       box-shadow: none;
     }
   }

Theme support
-------------

Firefox comes built-in with 3 themes: default, light and dark. The
built-in light/dark themes are a bit special as they load the
``compacttheme.css`` stylesheet. In addition to this, Firefox supports a
variety of WebExtension themes that can be installed from AMO. For
testing purposes, `here is an example of a WebExtension
theme. <https://addons.mozilla.org/en-US/firefox/addon/arc-dark-theme-we/>`__

Writing theme-friendly CSS
~~~~~~~~~~~~~~~~~~~~~~~~~~

-  Some CSS variables that are pre-adjusted for different platforms are
   also pre-adjusted for themes, so it's again a good idea to use them
   for theme support.
-  The text color of elements often contains valuable information from
   the theme colors, so ``currentcolor``/inheritance is again a good
   idea for theme support.
-  Never write CSS specially for the built-in light/dark theme in
   ``compacttheme.css`` unless that CSS isn't supposed to affect
   WebExtension themes.
-  These selectors can be used to target themed areas, though in general it's
   recommended to try to avoid them and use ``light-dark()`` to get the right
   colors automatically:

   - ``:root[lwt-toolbar-field="light/dark"]``: explicitly light or dark address bar and
     searchbar.
   - ``:root[lwt-toolbar-field-focus="light/dark"]``: explicitly light or dark address bar and
     searchbar in the focused state.
   - ``:root[lwt-popup="light/dark"]``: explicitly light or dark arrow panels
     and autocomplete panels.
   - ``:root[lwt-sidebar="light/dark"]``: explicitly light or dark sidebars.

-  If you'd like a different shade of a themed area and no CSS variable
   is adequate, using colors with alpha transparency is usually a good
   idea, as it will preserve the original theme author's color hue.

Variables
~~~~~~~~~

For clarity, CSS variables that are only used when a theme is enabled
have the ``--lwt-`` prefix.

Layout & performance
--------------------

Layout
~~~~~~

Mixing XUL flexbox and HTML flexbox can lead to undefined behavior.

CSS selectors
~~~~~~~~~~~~~

When targeting the root element of a page, using ``:root`` is the most
performant way of doing so.

Reflows and style flushes
~~~~~~~~~~~~~~~~~~~~~~~~~

See :ref:`Performance best practices for Firefox front-end engineers`
for more information about this.

Misc
----

Text aliasing
~~~~~~~~~~~~~

When convenient, avoid setting the ``opacity`` property on
text as it will cause text to be aliased differently.

HDPI support
~~~~~~~~~~~~

It's recommended to use SVG since it keeps the CSS clean when supporting
multiple resolutions. See the :ref:`SVG Guidelines` for more information
on SVG usage.

However, if only 1x and 2x PNG assets are available, you can use this
``@media`` query to target higher density displays (HDPI):

.. code:: css

   @media (min-resolution: 1.1dppx)
