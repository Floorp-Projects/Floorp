Layout Engine Visual Tests (reftest)
====================================

Layout Engine Visual Tests (reftest)
L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
July 19, 2006

This code is designed to run tests of Mozilla's layout engine.  These
tests consist of an HTML (or other format) file along with a reference
in the same format.  The tests are run based on a manifest file, and for
each test, PASS or FAIL is reported, and UNEXPECTED is reported if the
result (PASS or FAIL) was not the expected result noted in the manifest.

Images of the display of both tests are captured, and most test types
involve comparing these images (e.g., test types == or !=) to determine
whether the test passed.  The captures of the tests are taken in a
viewport that is 800 pixels wide and 1000 pixels tall, so any content
outside that area will be ignored (except for any scrollbars that are
displayed).  Ideally, however, tests should be written so that they fit
within 600x600, since we may in the future want to switch to 600x600 to
match http://lists.w3.org/Archives/Public/www-style/2012Sep/0562.html .

Why this way?
-------------

Writing HTML tests where the reference rendering is also in HTML is
harder than simply writing bits of HTML that can be regression-tested by
comparing the rendering of an older build to that of a newer build
(perhaps using stored reference images from the older build).  However,
comparing across time has major disadvantages:

 * Comparisons across time either require two runs for every test, or
   they require stored reference images appropriate for the platform and
   configuration (often limiting testing to a very specific
   configuration).

 * Comparisons across time may fail due to expected changes, for
   example, changes in the default style sheet for HTML, changes in the
   appearance of form controls, or changes in default preferences like
   default font size or default colors.

Using tests for which the pass criteria were explicitly chosen allows
running tests at any time to see whether they still pass.

Manifest Format
---------------

The test manifest format is a plain text file.  A line starting with a
``"#"`` is a comment.  Lines may be commented using whitespace followed by
a ``"#"`` and the comment.  Each non-blank line (after removal of comments)
must be one of the following:

Inclusion of another manifest
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   ``<skip-type>* include <relative_path>``

   ``<skip-type>`` is one of the skip or skip-if items (see their definitions
   in ``<failure-type>`` below). If any of the skip types evaluate to true (i.e.
   they are a plain ``skip`` or they are a ``skip-if`` with a condition that
   evaluates to true), then the include statement is skipped. Otherwise,
   reftests in the specified manifest are included in the set of reftests
   that are run.

A test item
~~~~~~~~~~~

   ``[ <failure-type> | <preference> ]* [<http>] <type> <url> <url_ref>``

   where

   a. ``<failure-type>`` (optional) is one of the following:

      ``fails``
          The test passes if the images of the two renderings DO NOT meet the
          conditions specified in the ``<type>``.

      ``fails-if(condition)``
          If the condition is met, the test passes if the images of the two
          renderings DO NOT meet the conditions of ``<type>``. If the condition
          is not met, the test passes if the conditions of ``<type>`` are met.

      ``needs-focus``
          The test fails or times out if the reftest window is not focused.

      ``random``
          The results of the test are random and therefore not to be considered in the output.

      ``random-if(condition)``
          The results of the test are random if a given condition is met.

      ``silentfail``
          This test may fail silently, and if that happens it should count as if
          the test passed. This is useful for cases where silent failure is the
          intended behavior (for example, in an out of memory situation in
          JavaScript, we stop running the script silently and immediately, in
          hopes of reclaiming enough memory to keep the browser functioning).

      ``silentfail-if(condition)``
          This test may fail silently if the condition is met.

      ``skip``
          This test should not be run. This is useful when a test fails in a
          catastrophic way, such as crashing or hanging the browser. Using
          ``skip`` is preferred to simply commenting out the test because we
          want to report the test failure at the end of the test run.

      ``skip-if(condition)``
          If the condition is met, the test is not run. This is useful if, for
          example, the test crashes only on a particular platform (i.e. it
          allows us to get test coverage on the other platforms).

      ``slow``
          The test may take a long time to run, so run it if slow tests are
          either enabled or not disabled (test manifest interpreters may choose
          whether or not to run such tests by default).

      ``slow-if(condition)``
          If the condition is met, the test is treated as if ``slow`` had been
          specified.  This is useful for tests which are slow only on particular
          platforms (e.g. a test which exercised out-of-memory behavior might be
          fast on a 32-bit system but inordinately slow on a 64-bit system).

      ``fuzzy(minDiff-maxDiff,minPixelCount-maxPixelCount)``
          This allows a test to pass if the pixel value differences are between
          ``minDiff`` and ``maxDiff``, inclusive, and the total number of
          different pixels is between ``minPixelCount`` and ``maxPixelCount``,
          inclusive. It can also be used with ``!=`` to ensure that the
          difference is outside the specified interval. Note that with ``!=``
          tests the minimum bounds of the ranges must be zero.

          Fuzzy tends to be used for two different sorts of cases.  The main
          case is tests that are expected to be equal, but actually fail in a
          minor way (e.g., an antialiasing difference), and we want to ensure
          that the test doesn't regress further so we don't want to mark the
          test as failing.  For these cases, test annotations should be the
          tightest bounds possible:  if the behavior is entirely deterministic
          this means a range like ``fuzzy(1-1,8-8)``, and if at all possible,
          the ranges should not include 0.  In cases where the test only
          sometimes fails, this unfortunately requires using 0 in both ranges,
          which means that we won't get reports of an unexpected pass if the
          problem is fixed (allowing us to remove the ``fuzzy()`` annotation
          and expect the test to pass from then on).

          The second case where fuzzy is used is tests that are supposed
          to allow some amount of variability (i.e., tests where the
          specification allows variability such that we can't assert
          that all pixels are the same).  Such tests should generally be
          avoided (for example, by covering up the pixels that can vary
          with another element), but when they are needed, the ranges in
          the ``fuzzy()`` annotation should generally include 0.

      ``fuzzy-if(condition,minDiff-maxDiff,minPixelCount-maxPixelCount)``
          If the condition is met, the test is treated as if ``fuzzy`` had been
          specified. This is useful if there are differences on particular
          platforms. See ``fuzzy()`` above.

      ``require-or(cond1&&cond2&&...,fallback)``
          Require some particular setup be performed or environmental
          condition(s) made true (eg setting debug mode) before the test
          is run. If any condition is unknown, unimplemented, or fails,
          revert to the fallback failure-type.
          Example: ``require-or(debugMode,skip)``

      ``asserts(count)``
          Loading the test and reference is known to assert exactly
          ``count`` times.
          NOTE: An asserts() notation with a non-zero count or maxCount
          suppresses use of a cached canvas for the test with the
          annotation.  However, if later occurrences of the same test
          are not annotated, they will use the cached canvas
          (potentially from the load that asserted).  This allows
          repeated use of the same test or reference to be annotated
          correctly (which may be particularly useful when the uses are
          in different subdirectories that can be tested independently),
          but does not force them to be, nor does it force suppression
          of caching for a common reference when it is the test that
          asserts.

      ``asserts-if(condition,count)``
          Same as above, but only if condition is true.

      ``asserts(minCount-maxCount)``
          Loading the test and reference is known to assert between
          minCount and maxCount times, inclusive.
          NOTE: See above regarding canvas caching.

      ``asserts-if(condition,minCount-maxCount)``
          Same as above, but only if condition is true.

      ``noautofuzz``
          Disables the autofuzzing behaviour hard-coded in the reftest harness
          for specific platform configurations. The autofuzzing is intended to
          compensate for inherent nondeterminism that results in intermittently
          fuzzy results (with small amounts of fuzz) across many/all tests on
          a given platform. Specifying 'noautofuzz' on the test will disable
          the autofuzzing for that test and require an exact match.

      Conditions are JavaScript expressions *without spaces* in them.
      They are evaluated in a sandbox in which a limited set of
      variables are defined.  See the BuildConditionSandbox function in
      ``layout/tools/reftest.js`` for details.

      Examples of using conditions: ::


          fails-if(winWidget) == test reference
          asserts-if(cocoaWidget,2) load crashtest


   b. ``<preference>`` (optional) is a string of the form

          ``pref(<name>,<value>)``

          ``test-pref(<name>,<value>)``

          ``ref-pref(<name>,<value>)``

      where ``<name>`` is the name of a preference setting, as seen in
      about:config, and ``<value>`` is the value to which this preference
      should be set. ``<value>`` may be a boolean (true/false), an integer,
      or a quoted string *without spaces*, according to the type of the
      preference.

      The preference will be set to the specified value prior to
      rendering the test and/or reference canvases (pref() applies to
      both, test-pref() only to the test, and ref-pref() only to the
      reference), and will be restored afterwards so that following
      tests are not affected. Note that this feature is only useful for
      "live" preferences that take effect immediately, without requiring
      a browser restart.

   c. ``<http>``, if present, is one of the strings (sans quotes) "HTTP" or
      "HTTP(..)" or "HTTP(../..)" or "HTTP(../../..)", etc. , indicating that
      the test should be run over an HTTP server because it requires certain
      HTTP headers or a particular HTTP status.  (Don't use this if your test
      doesn't require this functionality, because it unnecessarily slows down
      the test.)

      With "HTTP", HTTP tests have the restriction that any resource an HTTP
      test accesses must be accessed using a relative URL, and the test and
      the resource must be within the directory containing the reftest
      manifest that describes the test (or within a descendant directory).
      The variants "HTTP(..)", etc., can be used to relax this restriction by
      allowing resources in the parent directory, etc.

      To modify the HTTP status or headers of a resource named FOO, create a
      sibling file named FOO^headers^ with the following contents:

      ``[<http-status>]``

      ``<http-header>*``

      ``<http-status>``
          A line of the form "HTTP ###[ <description>]", where ### indicates
          the desired HTTP status and <description> indicates a desired HTTP
          status description, if any. If this line is omitted, the default is
          "HTTP 200 OK".

      ``<http-header>``
          A line in standard HTTP header line format, i.e.
          "Field-Name: field-value". You may not repeat the use of a Field-Name
          and must coalesce such headers together, and each header must be
          specified on a single line, but otherwise the format exactly matches
          that from HTTP itself.

      HTTP tests may also incorporate SJS files.  SJS files provide similar
      functionality to CGI scripts, in that the response they produce can be
      dependent on properties of the incoming request.  Currently these
      properties are restricted to method type and headers, but eventually
      it should be possible to examine data in the body of the request as
      well when computing the generated response.  An SJS file is a JavaScript
      file with a .sjs extension which defines a global `handleRequest`
      function (called every time that file is loaded during reftests) in this
      format: ::

          function handleRequest(request, response)
          {
            response.setStatusLine(request.httpVersion, 200, "OK");

            // You *probably* want this, or else you'll get bitten if you run
            // reftest multiple times with the same profile.
            response.setHeader("Cache-Control", "no-cache");

            response.write("any ASCII data you want");

            var outputStream = response.bodyOutputStream;
            // ...anything else you want to do, synchronously...
          }

      For more details on exactly which functions and properties are available
      on request/response in handleRequest, see the nsIHttpRe(quest|sponse)
      definitions in ``netwerk/test/httpserver/nsIHttpServer.idl``.

      HTTP tests can also make use of example.org URIs in order to test cross
      site behaviour. "example.org/filename" will access filename in the same
      directly as the current reftest. (Not currently implemented for android.)

   d. ``<type>`` is one of the following:

      ``==``
          The test passes if the images of the two renderings are the SAME.

      ``!=``
          The test passes if the images of the two renderings are DIFFERENT.

      ``load``
          The test passes unconditionally if the page loads. url_ref must be
          omitted, and the test cannot be marked as fails or random. (Used to
          test for crashes, hangs, assertions, and leaks.)

      ``script``
          The loaded page records the test's pass or failure status in a
          JavaScript data structure accessible through the following API.

          ``getTestCases()`` returns an array of test result objects
          representing the results of the tests performed by the page.

          Each test result object has two methods:

          ``testPassed()`` returns true if the test result object passed,
          otherwise it returns false.

          ``testDescription()`` returns a string describing the test
          result.

          ``url_ref`` must be omitted. The test may be marked as fails or
          random. (Used to test the JavaScript Engine.)

      ``print``
          The test passes if the printouts (as PDF) of the two renderings
          are the SAME by applying the following comparisons:

            - The number of pages generated for both printouts must match.
            - The text content of both printouts must match (rasterized text
              does not match real text).

          You can specify a print range by setting the reftest-print-range
          attribute on the document element. Example: ::

              <html reftest-print-range="2-3">
              ...


          The following example would lead to a single page print: ::

              <html reftest-print-range="2-2">
              ...

          You can also print selected elements only: ::

              <html reftest-print-range="selection">
              ...

          Make sure to include code in your test that actually selects something.

          Future additions to the set of comparisons might include:

            - Matching the paper size
            - Validating printed headers and footers
            - Testing (fuzzy) position of elements
            - Testing specific print related CSS properties

          The main difference between ``print`` and ``==/!=`` reftests is that
          ``print`` makes us compare the structure of print results (by parsing
          the output PDF) rather than taking screenshots and comparing pixel
          values. This allows us to test for common printing related issues
          like text being rasterized when it shouldn't. This difference in
          behavior is also why this is its own reftest operator, rather than
          a flavor of ``==/!=``. It would be somewhat misleading to list these
          print reftests as ``==/!=``, because they don't actually check for
          pixel matching.

          See the chapter about Pagination Tests if you are looking for testing
          layout in pagination mode.

   e. ``<url>`` is either a relative file path or an absolute URL for the
      test page

   f. ``<url_ref>`` is either a relative file path or an absolute URL for
      the reference page

   The only difference between ``<url>`` and ``<url_ref>`` is that results of
   the test are reported using ``<url>`` only.

Specification of a url prefix
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   ``url-prefix <string>``

   ``<string>`` will be prepended to relative ``<url>`` and ``<url_ref>`` for
   all following test items in the manifest.

   ``<string>`` will not be prepended to the relative path when including
   another manifest, e.g. ``include <relative_path>.``

   ``<string>`` will not be prepended to any ``<url>`` or ``<url_ref>`` matching
   the pattern ``/^\w+:/``. This will prevent the prefix from being applied to
   any absolute url containing a protocol such as ``data:``, ``about:``, or
   ``http:``.

   While the typical use of url-prefix is expected to be as the first line of
   a manifest, it is legal to use it anywhere in a manifest. Subsequent uses
   of url-prefix overwrite any existing values.

Specification of defaults
~~~~~~~~~~~~~~~~~~~~~~~~~

   ``defaults [<failure-type> | <preference> | <http>]``

   where ``<failure-type>``, ``<preference>``, and ``<http>`` are defined above.

   The default settings will be used for all following test items in the manifest.
   Any test specific settings will override the defaults, just as later items
   within a line override earlier ones.

   A defaults line with no settings will reset the defaults to be empty.

   As with url-prefix, defaults will often be used at the start of a manifest file
   so that it applies to all test items, but it is legal for defaults to appear
   anywhere in the manifest. A subsequent defaults will reset any previous default
   settings and overwrite them with the new settings.

   It is invalid to set non-skip defaults before an include line, just as it is
   invalid to specify non-skip settings directly on the include line itself. If a
   manifest needs to use both defaults and include, the include should appear
   before the defaults. If it's important to specify the include later on in the
   manifest, a blank defaults line directly preceding the include can be used to
   reset the defaults.

This test manifest format could be used by other harnesses, such as ones
that do not depend on XUL, or even ones testing other layout engines.

Running Tests
-------------

(If you're not using a DEBUG build, first set browser.dom.window.dump.enabled,
devtools.console.stdout.chrome and devtools.console.stdout.content to true (in
about:config, in the profile you'll be using to run the tests).
Create the option as a new boolean if it doesn't exist already. If you skip
this step you won't get any output in the terminal.)

At some point in the future there will hopefully be a cleaner way to do
this.  For now, go to your object directory, and run (perhaps using
``MOZ_NO_REMOTE=1`` or the ``-profile <directory>`` option) ::

./firefox -reftest /path/to/srcdir/mozilla/layout/reftests/reftest.list > reftest.out

and then search/grep reftest.out for "UNEXPECTED".

There are two scripts provided to convert the reftest.out to HTML.
clean-reftest-output.pl converts reftest.out into simple HTML, stripping
lines from the log that aren't relevant.  reftest-to-html.pl converts
the output into html that makes it easier to visually check for
failures.

Testable Areas
--------------

This framework is capable of testing many areas of the layout engine.
It is particularly well-suited to testing dynamic change handling (by
comparison to the static end-result as a reference) and incremental
layout (comparison of a script-interrupted layout to one that was not).
However, it is also possible to write tests for many other things that
can be described in terms of equivalence, for example:

 * CSS cascading could be tested by comparing the result of a
   complicated set of style rules that makes a word green to <span
   style="color:green">word</span>.

 * <canvas> compositing operators could be tested by comparing the
   result of drawing using canvas to a block-level element with the
   desired color as a CSS background-color.

 * CSS counters could be tested by comparing the text output by counters
   with a page containing the text written out

 * complex margin collapsing could be tested by comparing the complex
   case to a case where the margin is written out, or where the margin
   space is created by an element with 'height' and transparent
   background

When it is not possible to test by equivalence, it may be possible to
test by non-equivalence.  For example, testing justification in cases
with more than two words, or more than three different words, is
difficult.  However, it is simple to test that justified text is at
least displayed differently from left-, center-, or right-aligned text.

Writing Tests
-------------

When writing tests for this framework, it is important for the test to
depend only on behaviors that are known to be correct and permanent.
For example, tests should not depend on default font sizes, default
margins of the body element, the default style sheet used for HTML, the
default appearance of form controls, or anything else that can be
avoided.

In general, the best way to achieve this is to make the test and the
reference identical in as many aspects as possible.  For example:

Good test markup: ::

    <div style="color:green"><table><tr><td><span>green
    </span></td></tr></table></div>

Good reference markup: ::

    <div><table><tr><td><span style="color:green">green
    </span></td></tr></table></div>

BAD reference markup: ::

    <!-- 3px matches the default cellspacing and cellpadding -->
    <div style="color:green; padding: 3px">green
    </div>

BAD test markup: ::

    <!-- span doesn't change the positioning, so skip it -->
    <div style="color:green"><table><tr><td>green
    </td></tr></table></div>

Asynchronous Tests: class="reftest-wait"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Normally reftest takes a snapshot of the given markup's rendering right
after the load event fires for content. If your test needs to postpone
the moment the snapshot is taken, it should make sure a class
'reftest-wait' is on the root element by the moment the load event
fires. The easiest way to do this is to put it in the markup, e.g.: ::

    <html class="reftest-wait">

When your test is ready, you should remove this class from the root
element, for example using this code: ::

    document.documentElement.className = "";


Note that in layout tests it is often enough to trigger layout using ::

    document.body.offsetWidth  // HTML example

When possible, you should use this technique instead of making your
test async.

Invalidation Tests: MozReftestInvalidate Event
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When a test (or reference) uses reftest-wait, reftest tracks invalidation
via MozAfterPaint and updates the test image in the same way that
a regular window would be repainted. Therefore it is possible to test
invalidation-related bugs by setting up initial content and then
dynamically modifying it before removing reftest-wait. However, it is
important to get the timing of these dynamic modifications right so that
the test doesn't accidentally pass because a full repaint of the window
was already pending. To help with this, reftest fires one MozReftestInvalidate
event at the document root element for a reftest-wait test when it is safe to
make changes that should test invalidation. The event bubbles up to the
document and window so you can set listeners there too. For example, ::

  function doTest() {
    document.body.style.border = "";
    document.documentElement.removeAttribute('class');
  }
  document.addEventListener("MozReftestInvalidate", doTest, false);

Painting Tests: class="reftest-no-paint"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If an element shouldn't be painted, set the class "reftest-no-paint" on it
when doing an invalidation test. Causing a repaint in your
MozReftestInvalidate handler (for example, by changing the body's background
colour) will accurately test whether the element is painted.

Display List Tests: class="reftest-[no-]display-list"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These classes work similarly to reftest-no-paint, but check if the element has
display items created or not. These classes are useful for checking the behaviour
of retained display lists, where the display list is incrementally updated by
changes, rather than thrown out and rebuilt from scratch.

Opaque Layer Tests: class="reftest-opaque-layer"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If an element should be assigned to a PaintedLayer that's opaque, set the class
"reftest-opaque-layer" on it. This checks whether the layer is opaque during
the last paint of the test, and it works whether your test is an invalidation
test or not. In order to pass the test, the element has to have a primary
frame, and that frame's display items must all be assigned to a single painted
layer and no other layers, so it can't be used on elements that create stacking
contexts (active or inactive).

Layerization Tests: reftest-assigned-layer="layer-name"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If two elements should be assigned to the same PaintedLayer, choose any string
value as the layer name and set the attribute reftest-assigned-layer="yourname"
on both elements. Reftest will check whether all elements with the same
reftest-assigned-layer value share the same layer. It will also test whether
elements with different reftest-assigned-layer values are assigned to different
layers.
The same restrictions as with class="reftest-opaque-layer" apply: All elements
must have a primary frame, and that frame's display items must all be assigned
to the same PaintedLayer and no other layers. If these requirements are not
met, the test will fail.

Snapshot The Whole Window: class="reftest-snapshot-all"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In a reftest-wait test, to disable testing of invalidation and force the final
snapshot to be taken of the whole window, set the "reftest-snapshot-all"
class on the root element.

Avoid triggering flushes: class="reftest-no-flush"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The reftest harness normally triggers flushes by calling
getBoundingClientRect on the root element.  If the root element of the
test has class="reftest-no-flush", it doesn't do this.

This is useful for testing animations on the compositor thread, since
the flushing will cause a main thread style update.

Zoom Tests: reftest-zoom="<float>"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When the root element of a test has a "reftest-zoom" attribute, that zoom
factor is applied when rendering the test. The corresponds to the desktop "full
zoom" style zoom. The reftest document will be 800 device pixels wide by 1000
device pixels high. The reftest harness assumes that the CSS pixel dimensions
are 800/zoom and 1000/zoom. For best results therefore, choose zoom factors
that do not require rounding when we calculate the number of appunits per
device pixel; i.e. the zoom factor should divide 60, so 60/zoom is an integer.

Setting Scrollport Size: reftest-scrollport-w/h="<int>"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If either of the "reftest-scrollport-w" and "reftest-scrollport-h" attributes on
the root element are non-zero, sets the scroll-position-clamping scroll-port
size to the given size in CSS pixels. This does not affect the size of the
snapshot that is taken.

Setting Resolution: reftest-resolution="<float>"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If the root element of a test has a "reftest-resolution" attribute, the page
is rendered with the specified resolution (as if the user pinch-zoomed in
to that scale). Note that the difference between reftest-async-zoom and
reftest-resolution is that reftest-async-zoom only applies the scale in
the compositor, while reftest-resolution causes the page to be paint at that
resolution. This attribute can be used together with initial-scale in meta
viewport tag, in such cases initial-scale is applied first then
reftest-resolution changes the scale.

This attributes requires the pref apz.allow_zooming=true to have an effect.

Setting Async Scroll Mode: reftest-async-scroll attribute
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If the "reftest-async-scroll" attribute is set on the root element, we try to
enable async scrolling and zooming for the document. This is unsupported in many
configurations.

Setting Displayport Dimensions: reftest-displayport-x/y/w/h="<int>"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If any of the "reftest-displayport-x", "reftest-displayport-y",
"reftest-displayport-w" and "reftest-displayport-h" attributes on the root
element are nonzero, sets the displayport dimensions to the given bounds in
CSS pixels. This does not affect the size of the snapshot that is taken.

When the "reftest-async-scroll" attribute is set on the root element, *all*
elements in the document are checked for "reftest-displayport-x/y/w/h" and have
displayports set on them when those attributes are present.

Testing Async Scrolling: reftest-async-scroll-x/y="<int>"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When the "reftest-async-scroll" attribute is set on the root element, for any
element where either the "reftest-async-scroll-x" or "reftest-async-scroll-y
attributes are nonzero, at the end of the test take the snapshot with the given
offset (in CSS pixels) added to the async scroll offset.

Testing Async Zooming: reftest-async-zoom="<float>"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When the "reftest-async-zoom" attribute is present on the root element then at
the end of the test take the snapshot with the given async zoom on top of any
existing zoom. Content is not re-rendered at the new zoom level. This
corresponds to the mobile style "pinch zoom" style of zoom. This is unsupported
in many configurations, and any tests using this will probably want to have
pref(apz.allow_zooming,true) on them.

Pagination Tests: class="reftest-paged"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Now that the patch for bug 374050 has landed
(https://bugzilla.mozilla.org/show_bug.cgi?id=374050), it is possible to
create reftests that run in a paginated context.

The page size used is 5in wide and 3in tall (with the default half-inch
margins).  This is to allow tests to have less text and to make the
entire test fit on the screen.

There is a layout/reftests/printing directory for pagination reftests; however,
there is nothing special about this directory.  You can put pagination reftests
anywhere that is appropriate.

The suggested first lines for any pagination test is: ::

<!DOCTYPE html><html class="reftest-paged">
<style>html{font-size:12pt}</style>

The reftest-paged class on the root element triggers the reftest to
switch into page mode. Fixing the font size is suggested, although not
required, because the pages are a fixed size in inches. The switch to page mode
happens on load if the reftest-wait class is not present; otherwise it happens
immediately after firing the MozReftestInvalidate event.

The underlying layout support for this mode isn't really complete; it
doesn't use exactly the same codepath as real print preview/print. In
particular, scripting and frames are likely to cause problems; it is untested,
though.  That said, it should be sufficient for testing layout issues related
to pagination.

Process Crash Tests: class="reftest-expect-process-crash"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you are running a test that causes a process
under Electrolysis to crash as part of a reftest, this will cause process
crash minidump files to be left in the profile directory.  The test
infrastructure that runs the reftests will notice these minidump files and
dump out information from them, and these additional error messages in the logs
can end up erroneously being associated with other errors from the reftest run.
They are also confusing, since the appearance of "PROCESS-CRASH" messages in
the test run output can seem like a real problem, when in fact it is the
expected behavior.

To indicate to the reftest framework that a test is expecting a
process to crash, have the test include "reftest-expect-process-crash" as
one of the root element's classes by the time the test has finished.  This will
cause any minidump files that are generated while running the test to be removed
and they won't cause any error messages in the test run output.

Skip Forcing A Content Process Layer-Tree Update: reftest-no-sync-layers attribute
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Normally when an multi-process reftest test ends, we force the content process
to push a layer-tree update to the compositor before taking the snapshot.
Setting the "reftest-no-sync-layers" attribute on the root element skips this
step, enabling testing that layer-tree updates are being correctly generated.
However the test must manually wait for a MozAfterPaint event before ending.

Debugging Failures
------------------

The Reftest Analyzer has been created to make debugging reftests a bit easier.
If a reftest is failing, upload the log to the Reftest Analyzer to view the
differences between the expected result and the actual outcome of the reftest.
The Reftest Analyzer can be found at the following url:

https://hg.mozilla.org/mozilla-central/raw-file/tip/layout/tools/reftest/reftest-analyzer.xhtml
