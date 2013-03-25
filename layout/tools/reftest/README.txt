Layout Engine Visual Tests (reftest)
L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
July 19, 2006

This code is designed to run tests of Mozilla's layout engine.  These
tests consist of an HTML (or other format) file along with a reference
in the same format.  The tests are run based on a manifest file, and for
each test, PASS or FAIL is reported, and UNEXPECTED is reported if the
result (PASS or FAIL) was not the expected result noted in the manifest.

Why this way?
=============

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
===============

The test manifest format is a plain text file.  A line starting with a
"#" is a comment.  Lines may be commented using whitespace followed by
a "#" and the comment.  Each non-blank line (after removal of comments)
must be one of the following:

1. Inclusion of another manifest

   <failure-type>* include <relative_path>

   <failure-type> is the same as listed below for a test item.  As for 
   test items, multiple failure types listed on the same line are 
   combined by using the last matching failure type listed.  However, 
   the failure type on a manifest is combined with the failure type on 
   the test (or on a nested manifest) with the rule that the last in the
   following list wins:  fails, random, skip.  (In other words, skip 
   always wins, and random beats fails.)

2. A test item

   [ <failure-type> | <preference> ]* [<http>] <type> <url> <url_ref>

   where

   a. <failure-type> (optional) is one of the following:

      fails  The test passes if the images of the two renderings DO NOT
             meet the conditions specified in the <type>.

      fails-if(condition) If the condition is met, the test passes if the 
                          images of the two renderings DO NOT meet the 
                          conditions of <type>. If the condition is not met,
                          the test passes if the conditions of <type> are met.

      needs-focus  The test fails or times out if the reftest window is not
                   focused.

      random  The results of the test are random and therefore not to be
              considered in the output.

      random-if(condition) The results of the test are random if a given
                           condition is met.

      silentfail This test may fail silently, and if that happens it should
                 count as if the test passed. This is useful for cases where
                 silent failure is the intended behavior (for example, in
                 an out of memory situation in JavaScript, we stop running
                 the script silently and immediately, in hopes of reclaiming
                 enough memory to keep the browser functioning).

      silentfail-if(condition) This test may fail silently if the condition
                               is met.

      skip  This test should not be run. This is useful when a test fails in a
            catastrophic way, such as crashing or hanging the browser. Using
            'skip' is preferred to simply commenting out the test because we
            want to report the test failure at the end of the test run.

      skip-if(condition) If the condition is met, the test is not run. This is
                         useful if, for example, the test crashes only on a
                         particular platform (i.e. it allows us to get test
                         coverage on the other platforms).

      slow  The test may take a long time to run, so run it if slow tests are
            either enabled or not disabled (test manifest interpreters may
            choose whether or not to run such tests by default).

      slow-if(condition) If the condition is met, the test is treated as if
                         'slow' had been specified.  This is useful for tests
                         which are slow only on particular platforms (e.g. a
                         test which exercised out-of-memory behavior might be
                         fast on a 32-bit system but inordinately slow on a
                         64-bit system).

      fuzzy(maxDiff, diffCount)
          This allows a test to pass if the pixel value differences are <=
          maxDiff and the total number of different pixels is <= diffCount.
          It can also be used with '!=' to ensure that the difference is
          greater than maxDiff.

      fuzzy-if(condition, maxDiff, diffCount)
          If the condition is met, the test is treated as if 'fuzzy' had been
          specified. This is useful if there are differences on particular
          platforms.

      require-or(cond1&&cond2&&...,fallback)
          Require some particular setup be performed or environmental
          condition(s) made true (eg setting debug mode) before the test
          is run. If any condition is unknown, unimplemented, or fails,
          revert to the fallback failure-type.
          Example: require-or(debugMode,skip)

      asserts(count)
          Loading the test and reference is known to assert exactly
          count times.
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

      asserts(minCount-maxCount)
          Loading the test and reference is known to assert between
          minCount and maxCount times, inclusive.
          NOTE: See above regarding canvas caching.

      asserts-if(condition,count)
      asserts-if(condition,minCount-maxCount)
          Same as above, but only if condition is true.

      Conditions are JavaScript expressions *without spaces* in them.
      They are evaluated in a sandbox in which a limited set of
      variables are defined.  See the BuildConditionSandbox function in
      layout/tools/reftest.js for details.

      Examples of using conditions:
          fails-if(winWidget) == test reference
          asserts-if(cocoaWidget,2) load crashtest

   b. <preference> (optional) is a string of the form

          pref(<name>,<value>)
          test-pref(<name>,<value>)
          ref-pref(<name>,<value>)

      where <name> is the name of a preference setting, as seen in
      about:config, and <value> is the value to which this preference should
      be set. <value> may be a boolean (true/false), an integer, or a
      quoted string *without spaces*, according to the type of the preference.

      The preference will be set to the specified value prior to
      rendering the test and/or reference canvases (pref() applies to
      both, test-pref() only to the test, and ref-pref() only to the
      reference), and will be restored afterwards so that following
      tests are not affected. Note that this feature is only useful for
      "live" preferences that take effect immediately, without requiring
      a browser restart.

   c. <http>, if present, is one of the strings (sans quotes) "HTTP" or
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

      [<http-status>]
      <http-header>*

      <http-status> A line of the form "HTTP ###[ <description>]", where
                    ### indicates the desired HTTP status and <description>
                    indicates a desired HTTP status description, if any.
                    If this line is omitted, the default is "HTTP 200 OK".
      <http-header> A line in standard HTTP header line format, i.e.
                    "Field-Name: field-value".  You may not repeat the use
                    of a Field-Name and must coalesce such headers together,
                    and each header must be specified on a single line, but
                    otherwise the format exactly matches that from HTTP
                    itself.

      HTTP tests may also incorporate SJS files.  SJS files provide similar
      functionality to CGI scripts, in that the response they produce can be
      dependent on properties of the incoming request.  Currently these
      properties are restricted to method type and headers, but eventually
      it should be possible to examine data in the body of the request as
      well when computing the generated response.  An SJS file is a JavaScript
      file with a .sjs extension which defines a global |handleRequest|
      function (called every time that file is loaded during reftests) in this
      format:

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
      definitions in <netwerk/test/httpserver/nsIHttpServer.idl>.

   d. <type> is one of the following:

      ==     The test passes if the images of the two renderings are the
             SAME.
      !=     The test passes if the images of the two renderings are 
             DIFFERENT.
      load   The test passes unconditionally if the page loads.  url_ref
             must be omitted, and the test cannot be marked as fails or
             random.  (Used to test for crashes, hangs, assertions, and
             leaks.)
      script The loaded page records the test's pass or failure status
             in a JavaScript data structure accessible through the following
             API.

             getTestCases() returns an array of test result objects
             representing the results of the tests performed by the page.

             Each test result object has two methods:

             testPassed() returns true if the test result object passed,
             otherwise it returns false.

             testDescription() returns a string describing the test
             result.

             url_ref must be omitted. The test may be marked as fails or
             random. (Used to test the JavaScript Engine.)

   e. <url> is either a relative file path or an absolute URL for the
      test page

   f. <url_ref> is either a relative file path or an absolute URL for
      the reference page

   The only difference between <url> and <url_ref> is that results of
   the test are reported using <url> only.

3. Specification of a url prefix

   url-prefix <string>

   <string> will be prepended to relative <url> and <url_ref> for all following
   test items in the manifest.

   <string> will not be prepended to the relative path when including another
   manifest, e.g. include <relative_path>.

   <string> will not be prepended to any <url> or <url_ref> matching the pattern
   /^\w+:/. This will prevent the prefix from being applied to any absolute url
   containing a protocol such as data:, about:, or http:.

   While the typical use of url-prefix is expected to be as the first line of
   a manifest, it is legal to use it anywhere in a manifest. Subsequent uses
   of url-prefix overwrite any existing values.

4. Specification of default preferences

   default-preferences <preference>*

   where <preference> is defined above.

   The <preference> settings will be used for all following test items in the
   manifest.

   If a test item includes its own preference settings, then they will override
   any settings for preferences of the same names that are set using
   default-preferences, just as later items within a line override earlier ones.

   A default-preferences line with no <preference> settings following it will
   reset the set of default preferences to be empty.

   As with url-prefix, default-preferences will often be used at the start of a
   manifest file so that it applies to all test items, but it is legal for
   default-preferences to appear anywhere in the manifest. A subsequent
   default-preferences will reset any previous default preference values and
   overwrite them with the specified <preference> values.

This test manifest format could be used by other harnesses, such as ones
that do not depend on XUL, or even ones testing other layout engines.

Running Tests
=============

(If you're not using a DEBUG build, first set browser.dom.window.dump.enabled
to true (in about:config, in the profile you'll be using to run the tests).
Create the option as a new boolean if it doesn't exist already. If you skip
this step you won't get any output in the terminal.)

At some point in the future there will hopefully be a cleaner way to do
this.  For now, go to your object directory, and run (perhaps using
MOZ_NO_REMOTE=1 or the -profile <directory> option)

./firefox -reftest /path/to/srcdir/mozilla/layout/reftests/reftest.list > reftest.out

and then search/grep reftest.out for "UNEXPECTED".

There are two scripts provided to convert the reftest.out to HTML.
clean-reftest-output.pl converts reftest.out into simple HTML, stripping
lines from the log that aren't relevant.  reftest-to-html.pl converts
the output into html that makes it easier to visually check for
failures.

Testable Areas
==============

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
=============

When writing tests for this framework, it is important for the test to
depend only on behaviors that are known to be correct and permanent.
For example, tests should not depend on default font sizes, default
margins of the body element, the default style sheet used for HTML, the
default appearance of form controls, or anything else that can be
avoided.

In general, the best way to achieve this is to make the test and the
reference identical in as many aspects as possible.  For example:

  Good test markup:
    <div style="color:green"><table><tr><td><span>green
    </span></td></tr></table></div>

  Good reference markup:
    <div><table><tr><td><span style="color:green">green
    </span></td></tr></table></div>

  BAD reference markup:
    <!-- 3px matches the default cellspacing and cellpadding -->
    <div style="color:green; padding: 3px">green
    </div>

  BAD test markup:
    <!-- span doesn't change the positioning, so skip it -->
    <div style="color:green"><table><tr><td>green
    </td></tr></table></div>

Asynchronous Tests
==================

Normally reftest takes a snapshot of the given markup's rendering right
after the load event fires for content. If your test needs to postpone
the moment the snapshot is taken, it should make sure a class
'reftest-wait' is on the root element by the moment the load event
fires. The easiest way to do this is to put it in the markup, e.g.:
    <html class="reftest-wait">

When your test is ready, you should remove this class from the root
element, for example using this code:
    document.documentElement.className = "";


Note that in layout tests it is often enough to trigger layout using 
    document.body.offsetWidth  // HTML example

When possible, you should use this technique instead of making your
test async.

Invalidation Tests
==================

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
document and window so you can set listeners there too. For example,

function doTest() {
  document.body.style.border = "";
  document.documentElement.removeAttribute('class');
}
document.addEventListener("MozReftestInvalidate", doTest, false);

Painting Tests
==============

If an element shouldn't be painted, set the class "reftest-no-paint" on it
when doing an invalidation test. Causing a repaint in your
MozReftestInvalidate handler (for example, by changing the body's background
colour) will accurately test whether the element is painted.

Zoom Tests
==========

When the root element of a test has a "reftest-zoom" attribute, that zoom
factor is applied when rendering the test. The reftest document will be
800 device pixels wide by 1000 device pixels high. The reftest harness assumes
that the CSS pixel dimensions are 800/zoom and 1000/zoom. For best results
therefore, choose zoom factors that do not require rounding when we calculate
the number of appunits per device pixel; i.e. the zoom factor should divide 60,
so 60/zoom is an integer.

Printing Tests
==============

Now that the patch for bug 374050 has landed
(https://bugzilla.mozilla.org/show_bug.cgi?id=374050), it is possible to
create reftests that run in a paginated context.

The page size used is 5in wide and 3in tall (with the default half-inch
margins).  This is to allow tests to have less text and to make the
entire test fit on the screen.

There is a layout/reftests/printing directory for printing reftests; however,
there is nothing special about this directory.  You can put printing reftests
anywhere that is appropriate.

The suggested first lines for any printing test is
<!DOCTYPE html><html class="reftest-print">
<style>html{font-size:12pt}</style>

The reftest-print class on the root element triggers the reftest to
switch into page mode on load. Fixing the font size is suggested,
although not required, because the pages are a fixed size in inches.

The underlying layout support for this mode isn't really complete; it
doesn't use exactly the same codepath as real print preview/print. In
particular, scripting and frames are likely to cause problems; it is untested,
though.  That said, it should be sufficient for testing layout issues related
to pagination.

Plugin and IPC Process Crash Tests
==================================

If you are running a test that causes an out-of-process plugin or IPC process
under Electrolysis to crash as part of a reftest, this will cause process
crash minidump files to be left in the profile directory.  The test
infrastructure that runs the reftests will notice these minidump files and
dump out information from them, and these additional error messages in the logs
can end up erroneously being associated with other errors from the reftest run.
They are also confusing, since the appearance of "PROCESS-CRASH" messages in
the test run output can seem like a real problem, when in fact it is the
expected behavior.

To indicate to the reftest framework that a test is expecting a plugin or
IPC process crash, have the test include "reftest-expect-process-crash" as
one of the root element's classes by the time the test has finished.  This will
cause any minidump files that are generated while running the test to be removed
and they won't cause any error messages in the test run output.
