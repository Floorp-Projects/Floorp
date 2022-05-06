RTL Guidelines
==============

RTL languages such as Arabic, Hebrew, Persian and Urdu are read and
written from right-to-left, and the user interface for these languages
should be mirrored to ensure the content is easy to understand.

When a UI is changed from LTR to RTL (or vice-versa), it’s often called
mirroring. An RTL layout is the mirror image of an LTR layout, and it
affects layout, text, and graphics.

In RTL, anything that relates to time should be depicted as moving from
right to left. For example, forward points to the left, and backwards
points to the right.

Mirroring layout
~~~~~~~~~~~~~~~~

When a UI is mirrored, these changes occur:

-  Text fields icons are displayed on the opposite side of a field
-  Navigation buttons are displayed in reverse order
-  Icons that communicate direction, like arrows, are mirrored
-  Text is usually aligned to the right

In CSS, while it's possible to apply a rule for LTR and a separate one
specifically for RTL, it's usually better to use CSS Logical Properties
which provide the ability to control layout through logical, rather than
physical mappings.

+----------------------------------+----------------------------------+
| Do                               | Don't do                         |
+==================================+==================================+
| ``margin-inline-start: 5px;``    | ``margin-left: 5px;``            |
+----------------------------------+----------------------------------+
| ``padding-inline-end: 5px;``     | ``padding-right: 5px;``          |
+----------------------------------+----------------------------------+
| ``float: inline-start;``         | ``float: left;``                 |
+----------------------------------+----------------------------------+
| ``inset-inline-start: 5px;``     | ``left: 5px;``                   |
+----------------------------------+----------------------------------+
| ``border-inline-end: 1px;``      | ``border-right: 1px;``           |
+----------------------------------+----------------------------------+
| ``border-{start                  | ``border-{top/bot                |
| /end}-{start/end}-radius: 2px;`` | tom}-{left/right}-radius: 2px;`` |
+----------------------------------+----------------------------------+
| ``padding: 1px 2px;``            | ``padding: 1px 2px 1px 2px;``    |
+----------------------------------+----------------------------------+
| ``margin-block: 1px 3px;`` &&    | ``margin: 1px 2px 3px 4px;``     |
| ``margin-inline: 4px 2px;``      |                                  |
+----------------------------------+----------------------------------+
| ``text-align: start;`` or        | ``text-align: left;``            |
| ``text-align: match-parent;``    |                                  |
| (depends on the context)         |                                  |
+----------------------------------+----------------------------------+

When there is no special RTL-aware property available, or when
left/right properties must be used specifically for RTL, use the pseudo
``:-moz-locale-dir(rtl)`` (for XUL files) or ``:dir(rtl)`` (for HTML
files).

For example, this rule covers LTR to display searchicon.svg 7 pixels
from the left:

.. code:: css

   .search-box {
     background-image: url(chrome://path/to/searchicon.svg);
     background-position: 7px center;
   }

but an additional rule is necessary to cover RTL and place the search
icon on the right:

.. code:: css

   .search-box:dir(rtl) {
     background-position-x: right 7px;
   }

.. warning::

   It may be inappropriate to use logical properties when embedding LTR
   within RTL contexts. This is described further in the document.

Mirroring elements
~~~~~~~~~~~~~~~~~~

RTL content also affects the direction in which some icons and images
are displayed, particularly those depicting a sequence of events.

What to mirror
^^^^^^^^^^^^^^

-  Icons that imply directionality like back/forward buttons
-  Icons that imply text direction, like
   `readerMode.svg <https://searchfox.org/mozilla-central/rev/74cc0f4dce444fe0757e2a6b8307d19e4d0e0212/browser/themes/shared/reader/readerMode.svg>`__
-  Icons that imply location of UI elements in the screen, like
   `sidebars-right.svg <https://searchfox.org/mozilla-central/rev/74cc0f4dce444fe0757e2a6b8307d19e4d0e0212/browser/themes/shared/icons/sidebars-right.svg>`__,
   `open-in-new.svg <https://searchfox.org/mozilla-central/rev/74cc0f4dce444fe0757e2a6b8307d19e4d0e0212/browser/themes/shared/icons/open-in-new.svg>`__,
   `default-theme.svg <https://searchfox.org/mozilla-central/rev/a78233c11a6baf2c308fbed17eb16c6e57b6a2ac/toolkit/mozapps/extensions/content/default-theme.svg>`__
   or
   `pane-collapse.svg <https://searchfox.org/mozilla-central/rev/74cc0f4dce444fe0757e2a6b8307d19e4d0e0212/devtools/client/debugger/images/pane-collapse.svg>`__
-  Icons representing objects that are meant to be handheld should look
   like they're being right-handed, like the `magnifying glass
   icon <https://searchfox.org/mozilla-central/rev/e7c61f4a68b974d5fecd216dc7407b631a24eb8f/toolkit/themes/windows/global/icons/search-textbox.svg>`__
-  Twisties in collapsed state (in RTL context only)

What NOT to mirror
^^^^^^^^^^^^^^^^^^

-  Text/numbers
-  Icons containing text/numbers
-  Icons/animations that are direction neutral
-  Icons that wouldn't look differently if they'd be mirrored, like `X
   buttons <https://searchfox.org/mozilla-central/rev/a78233c11a6baf2c308fbed17eb16c6e57b6a2ac/devtools/client/debugger/images/close.svg>`__
   or the `bookmark
   star <https://searchfox.org/mozilla-central/rev/a78233c11a6baf2c308fbed17eb16c6e57b6a2ac/browser/themes/shared/icons/bookmark-hollow.svg>`__
   icon
-  Icons that should look the same as LTR, like icons related to code
   (which is always LTR) like
   `tool-webconsole.svg <https://searchfox.org/mozilla-central/rev/74cc0f4dce444fe0757e2a6b8307d19e4d0e0212/devtools/client/themes/images/tool-webconsole.svg>`__
-  Checkmark icons
-  Video/audio player controls
-  Product logos
-  Order of size dimensions (e.g., ``1920x1080`` should not become
   ``1080x1920``)
-  Order of size units (e.g., ``10 px`` should not become ``px 10``
   (unless the size unit is localizable))

How
^^^

The most common way is by flipping the X axis:

.. code:: css

   transform: scaleX(-1);

LTR text inside RTL contexts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

By default, in RTL locales, some symbols like "/", "." will be moved
around and won't be displayed in the order that they were typed in. This
may be problematic for URLs for instance, where you don't want dots to
change position.

Here's a non-exhaustive list of elements that should be displayed like
they would be in LTR locales:

-  Paths (e.g., C:\Users\username\Desktop)
-  Full URLs
-  Code and code containers (like the DevTools' Inspector or the CSS
   rules panel)
-  about:config preference names and values
-  Telephone numbers
-  Usernames & passwords (most sites on the web expect LTR
   usernames/passwords, but there may be exceptions)
-  Other text fields where only LTR text is expected

To make sure these are displayed correctly, you can use one of the
following on the relevant element:

-  ``direction: ltr``
-  ``dir="ltr"`` in HTML

Since the direction of such elements is forced to LTR, the text will
also be aligned to the left, which is undesirable from an UI
perspective, given that is inconsistent with the rest of the RTL UI
which has text usually aligned to the right. You can fix this using
``text-align: match-parent``. In the following screenshot, both text
fields (username and password) and the URL have their direction set to
LTR (to display text correctly), but the text itself is aligned to the
right for consistency with the rest of the UI:

.. image:: about-logins-rtl.png
   :alt: about:logins textboxes in RTL layout

However, since the direction in LTR, this also means that the start/end
properties will correspond to left/right respectively, which is probably
not what you expect. This means you have to use extra rules instead of
using logical properties.

Here's a full code example:

.. code:: css

   .url {
     direction: ltr; /* Force text direction to be LTR */

     /* `start` (the default value) will correspond to `left`,
        so we match the parent's direction in order to align the text to the right */
     text-align: match-parent;
   }

   /* :dir(ltr/rtl) isn't meaningful on .url, since it has direction: ltr, hence why it is matched on .container. */
   .container:dir(ltr) .url {
     padding-left: 1em;
   }

   .container:dir(rtl) .url {
     padding-right: 1em;
   }

.. note::

   The LTR rule is separate from the global rule to avoid having the
   left padding apply on RTL without having to reset it in the RTL rule.

Auto-directionality
^^^^^^^^^^^^^^^^^^^

Sometimes, the text direction on an element should vary dynamically
depending on the situation. This can be the case for a search input for
instance, a user may input a query in an LTR language, but may also
input a query in a RTL language. In this case, the search input has to
dynamically pick the correct directionality based on the first word, in
order to display the query text correctly. The typical way to do this is
to use ``dir="auto"`` in HTML. It is essential that
``text-align: match-parent`` is set, to avoid having the text alignment
change based on the query, and logical properties also cannot be used on
the element itself given they can change meaning depending on the query.

Testing
~~~~~~~

To test for RTL layouts in Firefox, you can go to about:config and
set ``intl.l10n.pseudo`` to ``bidi`` or ``intl.uidirection`` to ``1``.
The Firefox UI should immediately flip, but a restart may be required
to take effect in some Firefox features and interactions.

.. note::

   When testing with ``intl.uidirection`` set to ``1``, you may see some
   oddities regarding text ordering due to the nature of displaying LTR
   text in RTL layout.

   .. image:: about-protections-rtl.png
      :alt: about:protections in RTL layout- English vs. Hebrew

   This shouldn't be an issue when using an actual RTL locale or with
   ``intl.l10n.pseudo`` set to ``bidi`` .

How to spot RTL-related issues
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  Punctuation marks should appear on the left side of a
   word/sentence/paragraph on RTL, so if a *localizable* string appears
   in the UI with a dot, colon, ellipsis, question or exclamation mark
   on the right side of the text, this probably means that the text
   field is forced to be displayed as LTR.
-  If icons/images/checkmarks do not appear on the opposite side of
   text, when compared to LTR
-  If buttons (like the close button, "OK" and "Cancel" etc.) do not
   appear on the opposite side of the UI and not in the opposite order,
   when compared to LTR
-  If paddings/margins/borders are not the same from the opposite side,
   when compared to LTR
-  If on an Arabic or Persian build, digits are displayed as ``1 2 3``
   and not ``١ ٢ ٣`` (note that Hebrew uses ``1 2 3``)
-  If navigating in the UI using the left/right arrow keys does not
   select the correct element (i.e., pressing Left selects an item on
   the right)
-  If navigating in the UI using the Tab key does not focus elements
   from right to left, in an RTL context
-  If code is displayed as RTL (e.g., ``;padding: 20px`` - the semicolon
   should appear on the right side of the code). Code can still be
   aligned to the right if it appears in an RTL context

See also
~~~~~~~~

-  `RTL Best
   Practices <https://docs.google.com/document/d/1Rc8rvwsLI06xArFQouTinSh3wNte9Sqn9KWi1r7xY4Y/edit#heading=h.pw54h41h12ct>`__
-  Building RTL-Aware Web Apps & Websites: `Part
   1 <https://hacks.mozilla.org/2015/09/building-rtl-aware-web-apps-and-websites-part-1/>`__,
   `Part
   2 <https://hacks.mozilla.org/2015/10/building-rtl-aware-web-apps-websites-part-2/>`__

Credits
~~~~~~~

Google's `Material Design guide for
RTL <https://material.io/design/usability/bidirectionality.html>`__
