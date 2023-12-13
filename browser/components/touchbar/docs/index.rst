Touch Bar
=========

The Touch Bar is a hardware component on some MacBook Pros released from 2016.
It is a display above the keyboard that allows more flexible types of
input than is otherwise possible with a normal keyboard. Apple offers Touch Bar
APIs so developers can extend the Touch Bar to display inputs specific to their
application. Firefox consumes these APIs to offer a customizable row of inputs
in the Touch Bar.

In Apple's documentation, the term "the Touch Bar" refers to the hardware.
The term "a Touch Bar" refers not to the hardware but to a collection of inputs
shown on the Touch Bar. This means that there can be multiple "Touch Bars" that
switch out as the user switches contexts. The same naming convention is used in
this document.

In this document and in the code, the word "input" is used to refer to
an interactive element in the Touch Bar. It is often interchangeable with
"button", but "input" can also refer to any element displayed in the Touch Bar.

The Touch Bar should never offer functionality unavailable to Firefox users
without the Touch Bar. Most macOS Firefox users do not have the Touch Bar and
some choose to disable it. Apple's own `Human Interface Guidelines`_ (HIG)
forbids this kind of Touch Bar functionality. Please read the HIG for more
design considerations before you plan on implementing a new Touch Bar feature.

If you have questions about the Touch Bar that are not answered in this
document, feel free to reach out to `Harry Twyford`_ (:harry on Slack).
He wrote this document and Firefox's initial Touch Bar implementation.

.. _Human Interface Guidelines: https://developer.apple.com/design/human-interface-guidelines/macos/touch-bar/touch-bar-overview/

.. _Harry Twyford: mailto:harry@mozilla.com

.. contents:: Table of Contents

Overview
~~~~~~~~

Firefox's Touch Bar implementation is equal parts JavaScript and Cocoa
(Objective-C++). The JavaScript code lives in ``browser/components/touchbar``
and the Cocoa code lives in ``widget/cocoa``, mostly in ``nsTouchBar.mm``. The
Cocoa code is a consumer of Apple's Touch Bar APIs and defines what types of
Touch Bar inputs are available to its own consumers. The JS code in
``browser/components/touchbar`` provides services to ``nsTouchBar.mm`` and
defines what inputs the user actually sees in the Touch Bar. There is two-way
communication between the JS and the Cocoa: the Cocoa code asks the JS what
inputs it should display, and the JS asks the Cocoa code to update those inputs
when needed.

JavaScript API
~~~~~~~~~~~~~~

``browser/components/touchbar/MacTouchBar.sys.mjs`` defines what specific inputs are
available to the user, what icon they will have, what action they will perform,
and so on. Inputs are defined in the ``gBuiltInInputs`` object `in that file`_.
When creating a new object in ``gBuiltInInputs``, the available properties are
documented in the JSDoc for ``TouchBarInput``:

.. code:: JavaScript

  /**
   * A representation of a Touch Bar input.
   *     @param {string} input.title
   *            The lookup key for the button's localized text title.
   *     @param {string} input.image
   *            A URL pointing to an SVG internal to Firefox.
   *     @param {string} input.type
   *            The type of Touch Bar input represented by the object.
   *            Must be a value from kInputTypes.
   *     @param {Function} input.callback
   *            A callback invoked when a touchbar item is touched.
   *     @param {string} [input.color]
   *            A string in hex format specifying the button's background color.
   *            If omitted, the default background color is used.
   *     @param {bool} [input.disabled]
   *            If `true`, the Touch Bar input is greyed out and inoperable.
   *     @param {Array} [input.children]
   *            An array of input objects that will be displayed as children of
   *            this input. Available only for types KInputTypes.POPOVER and
   *            kInputTypes.SCROLLVIEW.
   */

Clarification on some of these properties is warranted.

* ``title`` is the key to a Fluent translation defined in ``browser/locales/<LOCALE>/browser/touchbar/touchbar.ftl``.
* ``type`` must be a value from the ``kInputTypes`` enum in ``MacTouchBar.sys.mjs``.
  For example, ``kInputTypes.BUTTON``. More information on input types follows
  below.
* ``callback`` points to a JavaScript function. Any chrome-level JavaScript can
  be executed. ``execCommand`` is a convenience method in ``MacTouchBar.sys.mjs``
  that takes a XUL command as a string and executes that command. For instance,
  one input sets ``callback`` to ``execCommand("Browser:Back")``.
* ``children`` is an array of objects with the same properties as members of
  ``gBuiltInInputs``. When used with an input of type
  ``kInputTypes.SCROLLVIEW``, ``children`` can only contain inputs of type
  ``kInputTypes.BUTTON``. When used with an input of type
  ``kInputTypes.POPOVER``, any input type except another ``kInputTypes.POPOVER``
  can be used.

.. _in that file: https://searchfox.org/mozilla-central/rev/ebe492edacc75bb122a2b380e4cafcca3470864c/browser/components/touchbar/MacTouchBar.jsm#82

Input types
-----------

Button
  A simple button. If ``image`` is not specified, the buttons displays the text
  label from ``title``. If both ``image`` and ``title`` are specified, only the
  ``image`` is shown. The action specified in ``callback`` is executed when the
  button is pressed.

  .. caution::

    Even if the ``title`` will not be shown in the Touch Bar, you must still
    define a ``title`` property.

Main Button
  Similar to a button, but displayed at double the width. A main button
  displays both the string in ``title`` and the icon in ``image``. Only one
  main button should be shown in the Touch Bar at any time, although this is
  not enforced.

Label
  A non-interactive text label. This input takes only the attributes ``title``
  and ``type``.

Popover
  Initially represented in the Touch Bar as a button, a popover will display an
  entirely different set of inputs when pressed. These different inputs should
  be defined in the ``children`` property of the parent. Popovers can also be
  shown and hidden programmatically, by calling

  .. code:: JavaScript

    gTouchBarUpdater.showPopover(
      TouchBarHelper.baseWindow,
      [POPOVER],
      {true | false}
    );

  where the second argument is a reference to a popover TouchBarInput and
  the third argument is whether the popover should be shown or hidden.

Scroll View
  A Scroll View is a scrolling list of buttons. The buttons should be defined
  in the Scroll View's ``children`` array.

  .. note::

    In Firefox, a list of search shortcuts appears in the Touch Bar when the
    address bar is focused. This is an example of a ScrollView contained within
    a popover. The popover is opened programmatically with
    ``gTouchBarUpdater.showPopover`` when the address bar is focused and it is
    hidden when the address bar is blurred.

Examples
--------
Some examples of ``gBuiltInInputs`` objects follow.

A simple button

  .. code:: JavaScript

    Back: {
      title: "back",
      image: "chrome://browser/skin/back.svg",
      type: kInputTypes.BUTTON,
      callback: () => execCommand("Browser:Back", "Back"),
    },

  A button is defined with a title, icon, type, and a callback. The callback
  simply calls the XUL command to go back.

The search popover
  This is the input that occupies the Touch Bar when the address bar is focused.

  .. code:: JavaScript

    SearchPopover: {
      title: "search-popover",
      image: "chrome://global/skin/icons/search-glass.svg",
      type: kInputTypes.POPOVER,
      children: {
        SearchScrollViewLabel: {
          title: "search-search-in",
          type: kInputTypes.LABEL,
        },
        SearchScrollView: {
          key: "search-scrollview",
          type: kInputTypes.SCROLLVIEW,
          children: {
            Bookmarks: {
              title: "search-bookmarks",
              type: kInputTypes.BUTTON,
              callback: () =>
                gTouchBarHelper.insertRestrictionInUrlbar(
                  UrlbarTokenizer.RESTRICT.BOOKMARK
                ),
            },
            History: {
              title: "search-history",
              type: kInputTypes.BUTTON,
              callback: () =>
                gTouchBarHelper.insertRestrictionInUrlbar(
                  UrlbarTokenizer.RESTRICT.HISTORY
                ),
            },
            OpenTabs: {
              title: "search-opentabs",
              type: kInputTypes.BUTTON,
              callback: () =>
                gTouchBarHelper.insertRestrictionInUrlbar(
                  UrlbarTokenizer.RESTRICT.OPENPAGE
                ),
            },
            Tags: {
              title: "search-tags",
              type: kInputTypes.BUTTON,
              callback: () =>
                gTouchBarHelper.insertRestrictionInUrlbar(
                  UrlbarTokenizer.RESTRICT.TAG
                ),
            },
            Titles: {
              title: "search-titles",
              type: kInputTypes.BUTTON,
              callback: () =>
                gTouchBarHelper.insertRestrictionInUrlbar(
                  UrlbarTokenizer.RESTRICT.TITLE
                ),
            },
          },
        },
      },
    },

  At the top level, a Popover is defined. This allows a collection of children
  to be shown in a separate Touch Bar. The Popover has two children: a Label,
  and a Scroll View. The Scroll View displays five similar buttons that call a
  helper method to insert search shortcut symbols into the address bar.

Adding a new input
------------------
Adding a new input is easy: just add a new object to ``gBuiltInInputs``. This
will make the input available in the Touch Bar customization window (accessible
from the Firefox menu bar item).

If you want to to add your new input to the default set, add its identifier
here_, where ``type`` is a value from ``kAllowedInputTypes`` in that
file and ``key`` is the value you set for ``title`` in ``gBuiltInInputs``.
You should request approval from UX before changing the default set of inputs.

.. _here: https://searchfox.org/mozilla-central/rev/ebe492edacc75bb122a2b380e4cafcca3470864c/widget/cocoa/nsTouchBar.mm#100

If you are interested in adding new features to Firefox's implementation of the
Touch Bar API, read on!


Cocoa API
~~~~~~~~~
Firefox implements Apple's Touch Bar API in its Widget: Cocoa code with an
``nsTouchBar`` class. ``nsTouchBar`` interfaces between Apple's Touch Bar API
and the ``TouchBarHelper`` JavaScript API.

The best resource to understand the Touch Bar API is Apple's
`official documentation`_. This documentation will cover how Firefox implements
these APIs and how one might extend ``nsTouchBar`` to enable new Touch Bar
features.

Every new Firefox window initializes ``nsTouchBar`` (link_). The function
``makeTouchBar`` is looked for automatically on every new instance of an
``NSWindow*``. If ``makeTouchBar`` is defined, that window will own a new
instance of ``nsTouchBar``.

At the time of this writing, every window initializes ``nsTouchBar`` with a
default set of inputs. In the future, Firefox windows other than the main
browser window (such as the Library window or DevTools) may initialize
``nsTouchBar`` with a different set of inputs.

``nsTouchBar`` has two different initialization methods: ``init`` and
``initWithInputs``. The former is a convenience method for the latter, calling
``initWithInputs`` with a nil argument. When that happens, a Touch Bar is
created containing a default set of inputs. ``initWithInputs`` can also take an
``NSArray<TouchBarInput*>*``. In that case, a non-customizable Touch Bar will be
initialized with only those inputs available.

.. _official documentation: https://developer.apple.com/documentation/appkit/nstouchbar?language=objc
.. _link: https://searchfox.org/mozilla-central/rev/ebe492edacc75bb122a2b380e4cafcca3470864c/widget/cocoa/nsCocoaWindow.mm#2877

NSTouchBarItemIdentifiers
-------------------------
The architecture of the Touch Bar is based largely around an ``NSString*``
wrapper class called ``NSTouchBarItemIdentifier``. Every input in the Touch Bar
has a unique ``NSTouchBarItemIdentifier``. They are structured in reverse-URI
format like so:

``com.mozilla.firefox.touchbar.[TYPE].[KEY]``

[TYPE] is a string indicating the type of the input, e.g. "button". If an
input is a child of another input, the parent's type is prepended to the child's
type, e.g. "scrubber.button" indicates a button contained in a scrubber.

[KEY] is the ``title`` attribute defined for that input on the JS side.

If you need to generate an identifier, use the convenience method
``[TouchBarInput nativeIdentifierWithType:withKey:]``.

.. caution::

  Do not create a new input that would have the same identifier as any other
  input. All identifiers must be unique.

.. warning::

  ``NSTouchBarItemIdentifier`` `is used in one other place`_: setting
  ``customizationIdentifier``. Do not ever change this string. If it is changed,
  any customizations users have made to the layout of their Touch Bar in Firefox
  will be erased.

Each identifier is tied to a ``TouchBarInput``. ``TouchBarInput`` is a class
that holds the properties specified for each input in ``gBuiltInInputs``.
``nsTouchBar`` uses them to create instances of ``NSTouchBarItem``
which are the actual objects used by Apple's Touch Bar API and displayed in the
Touch Bar. It is important to understand the difference between
``TouchBarInput`` and ``NSTouchBarItem``!

.. _is used in one other place: https://searchfox.org/mozilla-central/rev/ebe492edacc75bb122a2b380e4cafcca3470864c/widget/cocoa/nsTouchBar.mm#71

TouchBarInput creation flow
---------------------------
Creating a Touch Bar and its ``TouchBarInputs`` flows as follows:

#. ``[nsTouchBar init]`` is called from ``[NSWindow makeTouchBar]``.

#. ``init`` populates two NSArrays: ``customizationAllowedItemIdentifiers`` and
   ``defaultItemIdentifiers``. It also initializes a ``TouchBarInput`` object
   for every element in the union of the two arrays and stores them in
   ``NSMutableDictionary<NSTouchBarItemIdentifier, TouchBarInput*>* mappedLayoutItems``.

#. ``touchBar:makeItemForIdentifier:`` is called for every element in the union
   of the two arrays of identifiers. This method retrieves the ``TouchBarInput``
   for the given identifier and uses it to initialize a ``NSTouchBarItem``.
   ``touchBar:makeItemForIdentifier:`` reads the ``type`` attribute from the
   ``TouchBarInput`` to determine what ``NSTouchBarItem`` subclass should be
   initialized. Our Touch Bar code currently supports ``NSCustomTouchBarItem``
   (buttons, main buttons); ``NSPopoverTouchBarItem`` (popovers);
   ``NSTextField`` (labels); and ``NSScrollView`` (ScrollViews).

#. Once the ``NSTouchBarItem`` is initialized, its properties are populated with
   an assortment of "update" methods. These include ``updateButton``,
   ``updateMainButton``, ``updateLabel``, ``updatePopover``, and
   ``updateScrollView``.

#. Since the localization of ``TouchBarInput`` titles happens asynchronously in
   JavaScript code, the l10n callback executes
   ``[nsTouchBarUpdater updateTouchBarInputs:]``. This method reads the
   identifier of the input(s) that need to be updated and calls their respective
   "update" methods. This method is most often used to update ``title`` after
   l10n is complete. It can also be used to update any property of a
   ``TouchBarInput``;  for instance, one might wish to change ``color``
   when a specific event occurs in the browser.
