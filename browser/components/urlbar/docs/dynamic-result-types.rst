Dynamic Result Types
====================

This document discusses a special category of address bar results called dynamic
result types. Dynamic result types allow you to easily add new types of results
to the address bar and are especially useful for extensions.

The intended audience for this document is developers who need to add new kinds
of address bar results, either internally in the address bar codebase or through
extensions.

.. contents::
   :depth: 2


Motivation
----------

The address bar provides many different types of results in normal Firefox
usage. For example, when you type a search term, the address bar may show you
search suggestion results from your current search engine. It may also show you
results from your browsing history that match your search. If you typed a
certain phrase like "update Firefox," it will show you a tip result that lets
you know whether Firefox is up to date.

Each of these types of results is built into the address bar implementation. If
you wanted to add a new type of result -- say, a card that shows the weather
forecast when the user types "weather" -- one way to do so would be to add a new
result type. You would need to update all the code paths in the address bar that
relate to result types. For instance, you'd need to update the code path that
handles clicks on results so that your weather card opens an appropriate
forecast URL when clicked; you'd need to update the address bar view (the panel)
so that your card is drawn correctly; you may need to update the keyboard
selection behavior if your card contains elements that can be independently
selected such as different days of the week; and so on.

If you're implementing your weather card in an extension, as you might in an
add-on experiment, then you'd need to land your new result type in
mozilla-central so your extension can use it. Your new result type would ship
with Firefox even though the vast majority of users would never see it, and your
fellow address bar hackers would have to work around your code even though it
would remain inactive most of the time, at least until your experiment
graduated.

Dynamic Result Types
--------------------

**Dynamic result types** are an alternative way of implementing new result
types. Instead of adding a new built-in type along with all that entails, you
add a new provider subclass and register a template that describes how the view
should draw your result type and indicates which elements are selectable. The
address bar takes care of everything else. (Or if you're implementing an
extension, you add a few event handlers instead of a provider subclass, although
we have a shim_ that abstracts away the differences between internal and
extension address bar code.)

Dynamic result types are essentially an abstraction layer: Support for them as a
general category of results is built into the address bar, and each
implementation of a specific dynamic result type fills in the details.

In addition, dynamic result types can be added at runtime. This is important for
extensions that implement new types of results like the weather forecast example
above.

.. _shim: https://github.com/0c0w3/dynamic-result-type-extension/blob/master/src/shim.js

Getting Started
---------------

To get a feel for how dynamic result types are implemented, you can look at the
`example dynamic result type extension <exampleExtension_>`__. The extension
uses the recommended shim_ that makes writing address bar extension code very
similar to writing internal address bar code, and it's therefore a useful
example even if you intend to add a new dynamic result type internally in the
address bar codebase in mozilla-central.

The next section describes the specific steps you need to take to add a new
dynamic result type.

.. _exampleExtension: https://github.com/0c0w3/dynamic-result-type-extension/blob/master/src/background.js

Implementation Steps
--------------------

This section describes how to add a new dynamic result type in either of the
following cases:

* You want to add a new dynamic result type in an extension using the
  recommended shim_.
* You want to add a new dynamic result type internal to the address bar codebase
  in mozilla-central.

The steps are mostly the same in both cases and are described next.

If you want to add a new dynamic result type in an extension but don't want to
use the shim, then skip ahead to `Appendix B: Using the WebExtensions API
Directly`_.

1. Register the dynamic result type
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

First, register the new dynamic result type:

.. code-block:: javascript

    UrlbarResult.addDynamicResultType(name);

``name`` is a string identifier for the new type. It must be unique; that is, it
must be different from all other dynamic result type names. It will also be used
in DOM IDs, DOM class names, and CSS selectors, so it should not contain any
spaces or other characters that are invalid in CSS.

2. Register the view template
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Next, add the view template for the new type:

.. code-block:: javascript

    UrlbarView.addDynamicViewTemplate(name, viewTemplate);

``name`` is the new type's name as described in step 1.

``viewTemplate`` is an object called a view template. It describes in a
declarative manner the DOM that should be created in the view for all results of
the new type. For providers created in extensions, it also declares the
stylesheet that should be applied to results in the view. See `View Templates`_
for a description of this object.

3. Add the provider
~~~~~~~~~~~~~~~~~~~

As with any type of result, results for dynamic result types must be created by
one or more providers. Make a ``UrlbarProvider`` subclass for the new provider
and implement all the usual provider methods as you normally would:

.. code-block:: javascript

    class MyDynamicResultTypeProvider extends UrlbarProvider {
      // ...
    }

The ``startQuery`` method should create ``UrlbarResult`` objects with the
following two requirements:

* Result types must be ``UrlbarUtils.RESULT_TYPE.DYNAMIC``.
* Result payloads must have a ``dynamicType`` property whose value is the name
  of the dynamic result type used in step 1.

The results' sources, other payload properties, and other result properties
aren't relevant to dynamic result types, and you should choose values
appropriate to your use case.

If any elements created in the view for your results can be picked with the
keyboard or mouse, then be sure to implement your provider's ``onEngagement``
method.

For help on implementing providers in general, see the address bar's
`Architecture Overview`__.

If you are creating the provider in the internal address bar implementation in
mozilla-central, then don't forget to register it in ``UrlbarProvidersManager``.

If you are creating the provider in an extension, then it's registered
automatically, and there's nothing else you need to do.

__ https://firefox-source-docs.mozilla.org/browser/urlbar/overview.html#urlbarprovider

4. Implement the provider's getViewUpdate method
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``getViewUpdate`` is a provider method particular to dynamic result type
providers. Its job is to update the view DOM for a specific result. It's called
by the view for each result in the view that was created by the provider. It
returns an object called a view update object.

Recall that the view template was added earlier, in step 2. The view template
describes how to build the DOM structure for all results of the dynamic result
type. The view update object, in this step, describes how to fill in that
structure for a specific result.

Add the ``getViewUpdate`` method to the provider:

.. code-block:: javascript

    /**
     * Returns a view update object that describes how to update the view DOM
     * for a given result.
     *
     * @param {UrlbarResult} result
     *   The view update object describes how to update the view DOM for this
     *   particular result.
     * @param {Map} idsByName
     *   A map from names in the view template to the IDs of their corresponding
     *   elements in the DOM.
     */
    getViewUpdate(result, idsByName) {
      let viewUpdate = {
        // ...
      };
      return viewUpdate;
    }

``result`` is the result from the provider for which the view update is being
requested.

``idsByName`` is a map from names in the view template to the IDs of their
corresponding elements in the DOM. This is useful if parts of the view update
depend on element IDs, as some ARIA attributes do.

The return value is a view update object. It describes in a declarative manner
the updates that should be performed on the view DOM. See `View Update Objects`_
for a description of this object.

5. Style the results
~~~~~~~~~~~~~~~~~~~~

If you are creating the provider in the internal address bar implementation in
mozilla-central, then add styling `urlbar-dynamic-results.css`_.

.. _urlbar-dynamic-results.css: https://searchfox.org/mozilla-central/source/browser/themes/shared/urlbar-dynamic-results.css

If you are creating the provider in an extension, then bundle a CSS file in your
extension and declare it in the top-level ``stylesheet`` property of your view
template, as described in `View Templates`_. Additionally, if any of your rules
override built-in rules, then you'll need to declare them as ``!important``.

The rest of this section will discuss the CSS rules you need to use to style
your results.

There are two DOM annotations that are useful for styling. The first is the
``dynamicType`` attribute that is set on result rows, and the second is a class
that is set on child elements created from the view template.

dynamicType Row Attribute
.........................

The topmost element in the view corresponding to a result is called a
**row**. Rows have a class of ``urlbarView-row``, and rows corresponding to
results of a dynamic result type have an attributed called ``dynamicType``. The
value of this attribute is the name of the dynamic result type that was chosen
in step 1 earlier.

Rows of a specific dynamic result type can therefore be selected with the
following CSS selector, where ``TYPE_NAME`` is the name of the type:

.. code-block:: css

    .urlbarView-row[dynamicType=TYPE_NAME]

Child Element Class
...................

As discussed in `View Templates`_, each object in the view template can have a
``name`` property. The elements in the view corresponding to the objects in the
view template receive a class named
``urlbarView-dynamic-TYPE_NAME-ELEMENT_NAME``, where ``TYPE_NAME`` is the name
of the dynamic result type, and ``ELEMENT_NAME`` is the name of the object in
the view template.

Elements in dynamic result type rows can therefore be selected with the
following:

.. code-block:: css

    .urlbarView-dynamic-TYPE_NAME-ELEMENT_NAME

If an object in the view template does not have a ``name`` property, then it
won't receive the class and it therefore can't be selected using this selector.

View Templates
--------------

A **view template** is a plain JS object that declaratively describes how to
build the DOM for a dynamic result type. When a result of a particular dynamic
result type is shown in the view, the type's view template is used to construct
the part of the view that represents the type in general.

The need for view templates arises from the fact that extensions run in a
separate process from the chrome process and can't directly access the chrome
DOM, where the address bar view lives. Since extensions are a primary use case
for dynamic result types, this is an important constraint on their design.

Properties
~~~~~~~~~~

A view template object is a tree-like nested structure where each object in the
nesting represents a DOM element to be created. This tree-like structure is
achieved using the ``children`` property described below. Each object in the
structure may include the following properties:

``{string} name``
  The name of the object. This is required for all objects in the structure
  except the root object and serves two important functions:

  1. The element created for the object will automatically have a class named
     ``urlbarView-dynamic-${dynamicType}-${name}``, where ``dynamicType`` is the
     name of the dynamic result type. The element will also automatically have
     an attribute ``name`` whose value is this name. The class and attribute
     allow the element to be styled in CSS.

  2. The name is used when updating the view, as described in `View Update
     Objects`_.

  Names must be unique within a view template, but they don't need to be
  globally unique. In other words, two different view templates can use the same
  names, and other unrelated DOM elements can use the same names in their IDs
  and classes.

``{string} tag``
  The element tag name of the object. This is required for all objects in the
  structure except the root object and declares the kind of element that will be
  created for the object: ``span``, ``div``, ``img``, etc.

``{object} [attributes]``
  An optional mapping from attribute names to values. For each name-value pair,
  an attribute is set on the element created for the object.

  A special ``selectable`` attribute tells the view that the element is
  selectable with the keyboard. The element will automatically participate in
  the view's keyboard selection behavior.

  Similarly, the ``role=button`` ARIA attribute will also automatically allow
  the element to participate in keyboard selection. The ``selectable`` attribute
  is not necessary when ``role=button`` is specified.

``{array} [children]``
  An optional list of children. Each item in the array must be an object as
  described in this section. For each item, a child element as described by the
  item is created and added to the element created for the parent object.

``{array} [classList]``
  An optional list of classes. Each class will be added to the element created
  for the object by calling ``element.classList.add()``.

``{string} [stylesheet]``
  For dynamic result types created in extensions, this property should be set on
  the root object in the view template structure, and its value should be a
  stylesheet URL. The stylesheet will be loaded in all browser windows so that
  the dynamic result type view may be styled. The specified URL will be resolved
  against the extension's base URI. We recommend specifying a URL relative to
  your extension's base directory.

  For dynamic result types created internally in the address bar codebase, this
  value should not be specified and instead styling should be added to
  `urlbar-dynamic-results.css`_.

Example
~~~~~~~

Let's return to the weather forecast example from `earlier <Motivation_>`__. For
each result of our weather forecast dynamic result type, we might want to
display a label for a city name along with two buttons for today's and
tomorrow's forecasted high and low temperatures. The view template might look
like this:

.. code-block:: javascript

    {
      stylesheet: "style.css",
      children: [
        {
          name: "cityLabel",
          tag: "span",
        },
        {
          name: "today",
          tag: "div",
          classList: ["day"],
          attributes: {
            selectable: true,
          },
          children: [
            {
              name: "todayLabel",
              tag: "span",
              classList: ["dayLabel"],
            },
            {
              name: "todayLow",
              tag: "span",
              classList: ["temperature", "temperatureLow"],
            },
            {
              name: "todayHigh",
              tag: "span",
              classList: ["temperature", "temperatureHigh"],
            },
          },
        },
        {
          name: "tomorrow",
          tag: "div",
          classList: ["day"],
          attributes: {
            selectable: true,
          },
          children: [
            {
              name: "tomorrowLabel",
              tag: "span",
              classList: ["dayLabel"],
            },
            {
              name: "tomorrowLow",
              tag: "span",
              classList: ["temperature", "temperatureLow"],
            },
            {
              name: "tomorrowHigh",
              tag: "span",
              classList: ["temperature", "temperatureHigh"],
            },
          },
        },
      ],
    }

Observe that we set the special ``selectable`` attribute on the ``today`` and
``tomorrow`` elements so they can be selected with the keyboard.

View Update Objects
-------------------

A **view update object** is a plain JS object that declaratively describes how
to update the DOM for a specific result of a dynamic result type. When a result
of a dynamic result type is shown in the view, a view update object is requested
from the result's provider and is used to update the DOM for that result.

Note the difference between view update objects, described in this section, and
view templates, described in the previous section. View templates are used to
build a general DOM structure appropriate for all results of a particular
dynamic result type. View update objects are used to fill in that structure for
a specific result.

When a result is shown in the view, first the view looks up the view template of
the result's dynamic result type. It uses the view template to build a DOM
subtree. Next, the view requests a view update object for the result from its
provider. The view update object tells the view which result-specific attributes
to set on which elements, result-specific text content to set on elements, and
so on. View update objects cannot create new elements or otherwise modify the
structure of the result's DOM subtree.

Typically the view update object is based on the result's payload.

Properties
~~~~~~~~~~

The view update object is a nested structure with two levels. It looks like
this:

.. code-block:: javascript

    {
      name1: {
        // individual update object for name1
      },
      name2: {
        // individual update object for name2
      },
      name3: {
        // individual update object for name3
      },
      // ...
    }

The top level maps object names from the view template to individual update
objects. The individual update objects tell the view how to update the elements
with the specified names. If a particular element doesn't need to be updated,
then it doesn't need an entry in the view update object.

Each individual update object can have the following properties:

``{object} [attributes]``
  A mapping from attribute names to values. Each name-value pair results in an
  attribute being set on the element.

``{object} [style]``
  A plain object that can be used to add inline styles to the element, like
  ``display: none``. ``element.style`` is updated for each name-value pair in
  this object.

``{object} [l10n]``
  An ``{ id, args }`` object that will be passed to
  ``document.l10n.setAttributes()``.

``{string} [textContent]``
  A string that will be set as ``element.textContent``.

Example
~~~~~~~

Continuing our weather forecast example, the view update object needs to update
several things that we declared in our view template:

* The city label
* The "today" label
* Today's low and high temperatures
* The "tomorrow" label
* Tomorrow's low and high temperatures

Typically, each of these, with the possible exceptions of the "today" and
"tomorrow" labels, would come from our results' payloads. There's an important
connection between what's in the view and what's in the payloads: The data in
the payloads serves the information shown in the view.

Our view update object would then look something like this:

.. code-block:: javascript

    {
      cityLabel: {
        textContent: result.payload.city,
      },
      todayLabel: {
        textContent: "Today",
      },
      todayLow: {
        textContent: result.payload.todayLow,
      },
      todayHigh: {
        textContent: result.payload.todayHigh,
      },
      tomorrowLabel: {
        textContent: "Tomorrow",
      },
      tomorrowLow: {
        textContent: result.payload.tomorrowLow,
      },
      tomorrowHigh: {
        textContent: result.payload.tomorrowHigh,
      },
    }

Accessibility
-------------

Just like built-in types, dynamic result types support a11y in the view, and you
should make sure your view implementation is fully accessible.

Since the views for dynamic result types are implemented using view templates
and view update objects, in practice supporting a11y for dynamic result types
means including appropriate `ARIA attributes <aria_>`_ in the view template and
view update objects, using the ``attributes`` property.

Many ARIA attributes depend on element IDs, and that's why the ``idsByName``
parameter to the ``getViewUpdate`` provider method is useful.

Usually, accessible address bar results require the ARIA attribute
``role=group`` on their top-level DOM element to indicate that all the child
elements in the result's DOM subtree form a logical group. This attribute can be
set on the root object in the view template.

.. _aria: https://developer.mozilla.org/en-US/docs/Web/Accessibility/ARIA

Example
~~~~~~~

Continuing the weather forecast example, we'd like for screen readers to know
that our result is labeled by the city label so that they announce the city when
the result is selected.

The relevant ARIA attribute is ``aria-labelledby``, and its value is the ID of
the element with the label. In our ``getViewUpdate`` implementation, we can use
the ``idsByName`` map to get the element ID that the view created for our city
label, like this:

.. code-block:: javascript

    getViewUpdate(result, idsByName) {
      return {
        root: {
          attributes: {
            "aria-labelledby": idsByName.get("cityLabel"),
          },
        },
        // *snipping the view update object example from earlier*
      };
    }

Here we're using the name "root" to refer to the root object in the view
template, so we also need to update our view template by adding the ``name``
property to the top-level object, like this:

.. code-block:: javascript

    {
      stylesheet: "style.css",
      name: "root",
      attributes: {
        role: "group",
      },
      children: [
        {
          name: "cityLabel",
          tag: "span",
        },
        // *snipping the view template example from earlier*
      ],
    }

Note that we've also included the ``role=group`` ARIA attribute on the root, as
discussed above. We could have included it in the view update object instead of
the view template, but since it doesn't depend on a specific result or element
ID in the ``idsByName`` map, the view template makes more sense.

Mimicking Built-in Address Bar Results
--------------------------------------

Sometimes it's desirable to create a new result type that looks and behaves like
the usual built-in address bar results. Two conveniences are available that are
useful in this case.

URL Navigation
~~~~~~~~~~~~~~

If a result's payload includes a string ``url`` property and a boolean
``shouldNavigate: true`` property, then picking the result will navigate to the
URL. The ``onEngagement`` method of the result's provider will still be called
before navigation.

Text Highlighting
~~~~~~~~~~~~~~~~~

Most built-in address bar results emphasize occurrences of the user's search
string in their text by boldfacing matching substrings. Search suggestion
results do the opposite by emphasizing the portion of the suggestion that the
user has not yet typed. This emphasis feature is called **highlighting**, and
it's also available to the results of dynamic result types.

Highlighting for dynamic result types is a fairly automated process. The text
that you want to highlight must be present as a property in your result
payload. Instead of setting the property to a string value as you normally
would, set it to an array with two elements, where the first element is the text
and the second element is a ``UrlbarUtils.HIGHLIGHT`` value, like the ``title``
payload property in the following example:

.. code-block:: javascript

    let result = new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.DYNAMIC,
      UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK,
      {
        title: [
          "Some result title",
          UrlbarUtils.HIGHLIGHT.TYPED,
        ],
        // *more payload properties*
      }
    );

``UrlbarUtils.HIGHLIGHT`` is defined in the extensions shim_ and is described
below.

Your view template must create an element corresponding to the payload
property. That is, it must include an object where the value of the ``name``
property is the name of the payload property, like this:

.. code-block:: javascript

    {
      children: [
        {
          name: "title",
          tag: "span",
        },
        // ...
      ],
    }

In contrast, your view update objects must *not* include an update for the
element. That is, they must not include a property whose name is the name of the
payload property.

Instead, when the view is ready to update the DOM of your results, it will
automatically find the elements corresponding to the payload property, set their
``textContent`` to the text value in the array, and apply the appropriate
highlighting, as described next.

There are two possible ``UrlbarUtils.HIGHLIGHT`` values. Each controls how
highlighting is performed:

``UrlbarUtils.HIGHLIGHT.TYPED``
  Substrings in the payload text that match the user's search string will be
  emphasized.

``UrlbarUtils.HIGHLIGHT.SUGGESTED``
  If the user's search string appears in the payload text, then the remainder of
  the text following the matching substring will be emphasized.

Appendix A: Examples
--------------------

This section lists some example and real-world consumers of dynamic result
types.

`Example Extension`__
  This extension demonstrates a simple use of dynamic result types.

`Weather Quick Suggest Extension`__
  A real-world Firefox extension experiment that shows weather forecasts and
  alerts when the user performs relevant searches in the address bar.

`Tab-to-Search Provider`__
  This is a built-in provider in mozilla-central that uses dynamic result types.

__ https://github.com/0c0w3/dynamic-result-type-extension
__ https://github.com/mozilla-extensions/firefox-quick-suggest-weather/blob/master/src/background.js
__ https://searchfox.org/mozilla-central/source/browser/components/urlbar/UrlbarProviderTabToSearch.sys.mjs

Appendix B: Using the WebExtensions API Directly
------------------------------------------------

If you're developing an extension, the recommended way of using dynamic result
types is to use the shim_, which abstracts away the differences between writing
internal address bar code and extensions code. The `implementation steps`_ above
apply to extensions as long as you're using the shim.

For completeness, in this section we'll document the WebExtensions APIs that the
shim is built on. If you don't use the shim for some reason, then follow these
steps instead. You'll see that each step above using the shim has an analogous
step here.

The WebExtensions API schema is declared in `schema.json`_ and implemented in
`api.js`_.

.. _schema.json: https://github.com/0c0w3/dynamic-result-type-extension/blob/master/src/experiments/urlbar/schema.json
.. _api.js: https://github.com/0c0w3/dynamic-result-type-extension/blob/master/src/experiments/urlbar/api.js

1. Register the dynamic result type
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

First, register the new dynamic result type:

.. code-block:: javascript

    browser.experiments.urlbar.addDynamicResultType(name, type);

``name`` is a string identifier for the new type. See step 1 in `Implementation
Steps`_ for a description, which applies here, too.

``type`` is an object with metadata for the new type. Currently no metadata is
supported, so this should be an empty object, which is the default value.

2. Register the view template
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Next, add the view template for the new type:

.. code-block:: javascript

    browser.experiments.urlbar.addDynamicViewTemplate(name, viewTemplate);

See step 2 above for a description of the parameters.

3. Add WebExtension event listeners
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add all the WebExtension event listeners you normally would in an address bar
extension, including the two required listeners, ``onBehaviorRequested`` and
and ``onResultsRequested``.

.. code-block:: javascript

    browser.urlbar.onBehaviorRequested.addListener(query => {
      return "active";
    }, providerName);

    browser.urlbar.onResultsRequested.addListener(query => {
      let results = [
        // ...
      ];
      return results;
    }, providerName);

See the address bar extensions__ document for help on the urlbar WebExtensions
API.

__ https://firefox-source-docs.mozilla.org/browser/urlbar/experiments.html

4. Add an onViewUpdateRequested event listener
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``onViewUpdateRequested`` is a WebExtensions event particular to dynamic result
types. It's analogous to the ``getViewUpdate`` provider method described
earlier.

.. code-block:: javascript

    browser.experiments.urlbar.onViewUpdateRequested.addListener((payload, idsByName) => {
      let viewUpdate = {
        // ...
      };
      return viewUpdate;
    });

Note that unlike ``getViewUpdate``, here the listener's first parameter is a
result payload, not the result itself.

The listener should return a view update object.

5. Style the results
~~~~~~~~~~~~~~~~~~~~

This step is the same as step 5 above. Bundle a CSS file in your extension and
declare it in the top-level ``stylesheet`` property of your view template.
