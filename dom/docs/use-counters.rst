============
Use Counters
============

Use counters are used to report statistics on how much a given web platform feature is used across the Web.
Supported features include:

* WebIDL methods and attributes (getters and setters are reported separately) for pages, documents, and workers,
* CSS properties (including properties that aren't in the web platform, but we're interested in),
* Deprecated DOM operations,
* Other things like SVG filters and APIs specifically unsupported in Private Browsing Mode,
  via custom use counters.

Adding a new Use Counter
========================
How you add a new use counter is different depending on what kind of web platform feature you're instrumenting.
The one constant is that you must run ``./mach gen-use-counter-metrics``
after adding or removing a use counter.

(Why this is a manual step and not part of the build is explained in
`the implementation bug 1852098 <https://bugzilla.mozilla.org/show_bug.cgi?id=1852098#c11>`_.)

WebIDL Methods and Attributes
-----------------------------
Use counters for WebIDL Methods and Attributes are added manually by editing
:searchfox:`UseCounters.conf <dom/base/UseCounters.conf>` or, for workers,
:searchfox:`UseCountersWorker.conf <dom/base/UseCountersWorker.conf>`, and
by annotating the WebIDL Method or Attribute with the ``[UseCounter]``
extended attribute.

(Why you must write this in two places is because generating things from
bindings codegen and ensuring all the dependencies were correct proved to be
rather difficult)

Then run ``./mach gen-use-counter-metrics`` and build as normal.

CSS Properties
--------------
Use counters for CSS properties are automatically generated for every property Gecko supports.

To add a use counter for a CSS property that isn't supported by Gecko,
add it to :searchfox:`counted_unknown_properties.py <servo/components/style/properties/counted_unknown_properties.py>`.

Then run ``./mach gen-use-counter-metrics`` and build as normal.

Deprecated DOM operations
-------------------------
Use counters for deprecated DOM operations are declared in
:searchfox:`nsDeprecatedOperationList.h <dom/base/nsDeprecatedOperationList.h>`.
To add a use counter for a deprecated DOM operation, you'll add an invocation of the
``DEPRECATED_OPERATION(DeprecationReference)`` macro.
The provided parameter must have the same value of the deprecation note added to the *IDL* file.

See `bug 1860635 <https://bugzilla.mozilla.org/show_bug.cgi?id=1860635>`_ for a sample
deprecated operation.

Then run ``./mach gen-use-counter-metrics`` and build as normal.

Custom use counters
-------------------
Custom use counters are for counting per-page, per-document, or per-worker
uses of web platform features that can't be handled directly through WebIDL annotation.

For example, the use of specific SVG filters isn't a WebIDL method or attribute,
but was still an aspect of the web platform of interest.

To add a custom use counter, define it in
:searchfox:`UseCounters.conf <dom/base/UseCounters.conf>` or, for workers,
:searchfox:`UseCountersWorker.conf <dom/base/UseCountersWorker.conf>`
by following the instructions in the file.
Broadly, you'll be writing a line like ``custom feBlend uses the feBlend SVG filter``.

Then, by running the build as normal, an enum in ``enum class UseCounter``
will be generated for your use counter, which you should pass to
``Document::SetUseCounter()`` when it's used.
``Document::SetUseCounter()`` is very cheap,
so do not be afraid to call it every time the feature is used.

Take care to craft the description appropriately.
It will be appended to "Whether a document " or "Whether a shared worker ",
so write only the ending.


The processor scripts
=====================
The definition files are processed during the build to generate C++ headers
included by web platform components (e.g. DOM) that own the features to be tracked.

The definition files are also processed during ``./mach gen-use-counter-metrics``
to generate :searchfox:`use_counter_metrics.yaml <dom/base/use_counter_metrics.yaml>`
which generates the necessary Glean metrics for recording and reporting use counter data.

gen-usecounters.py
------------------
This script is called by the build system to generate:

- the ``UseCounterList.h`` header for the WebIDL, out of the definition files.
- the ``UseCounterWorkerList.h`` header for the WebIDL, out of the definition files.

usecounters.py
--------------
Contains methods for parsing and transforming use counter definition files,
as well as the mechanism that outputs the Glean use counter metrics definitions.

Data Review
===========
The concept of a Use Counter data collection
(being a web platform feature which has the number of pages, documents, workers
(of various types), or other broad category of web platform API surfaces that
*use* it recorded and reported by a data collection mechanism (like Glean))
was approved for opt-out collection in all products using Gecko and Glean in
`bug 1852098 <https://bugzilla.mozilla.org/show_bug.cgi?id=1852098>`_.

As a result,
if you are adding new use counter data collections for WebIDL methods or attributes,
deprecated operations, or CSS properties:
you almost certainly don't need a data collection review.

If you are adding a custom use counter, you might need a data collection review.
The criteria for whether you do or not is whether the custom use counter you're adding
can fall under
`the over-arching data collection review request <https://bugzilla.mozilla.org/show_bug.cgi?id=1852098>`_.
For example: a custom use counter for an SVG filter? Clearly a web platform feature being counted.
A custom use counter that solely increments when you visit a social media website?
Doesn't seem like it'd be covered, no.

If unsure, please ask on
`the #data-stewards channel on Matrix <https://chat.mozilla.org/#/room/#data-stewards:mozilla.org>`_.

The Data
========
Use Counters are, as of Firefox 121, collected using Glean as
``counter`` metrics on the "use-counters" ping.
They are in a variety of metrics categories of ``use.counter.X``
which you can browse on
`the Glean Dictionary <https://dictionary.telemetry.mozilla.org/apps/firefox_desktop?page=1&search=use.counter>`_.
The dictionary also contains information about how to view the data.

Interpreting the data
---------------------
A use counter on its own is minimally useful, as it is solely a count of how many
(pages, documents, workers of a specific type, other web platform API surfaces)
a given part of the web platform was used on.

Knowing a feature was encountered ``0`` times across all of Firefox would be useful to know.
(If you wanted to remove something).
Knowing a feature was encountered *more than* ``0`` times would be useful.
(If you wanted to argue against removing something).

But any other number of, say, pages using a web platform feature is only useful
in context with how many total pages were viewed.

Thus, each use counter has in its description a name of another counter
-- a denominator -- to convert the use counter into a usage rate.

Using pages as an example, knowing the CSS property ``overflow``
is used on ``1504`` pages is... nice. I guess.
But if you sum up ``use.counters.top_level_content_documents_destroyed``
to find that there were only ``1506`` pages loaded?
That's a figure we can do something with.
We can order MDN search results by popularity.
We can prioritize performance efforts in Gecko to focus on the most-encountered features.
We can view the popularity over time and see when we expect we'll be able to deprecate and remove the feature.

This is why you'll more likely encounter use counter data expressed as usage rates.
