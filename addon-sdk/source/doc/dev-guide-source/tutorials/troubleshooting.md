<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Troubleshooting #

If you're having trouble getting the Add-on SDK up and running, don't panic!
This page lists some starting points that might help you track down your
problem.

Quarantine Problem on Mac OS X
------------------------------
On Mac OS X, you might see the following error when you try to run `cfx`:

<pre>
/path/to/sdk/bin/cfx: /usr/bin/env: bad interpreter: Operation not permitted
</pre>

This might be because the `cfx` executable file has been placed in quarantine
during download from the Internet.

To get it out of quarantine, use the `xattr -d` command, specifying
`com.apple.quarantine` as the name of the attribute to delete, and `cfx` as
the file from which to delete that attribute:

<pre>
xattr -d com.apple.quarantine /path/to/sdk/bin/cfx
</pre>

Check Your Python
-----------------

The SDK's `cfx` tool runs on Python.  If you're having trouble getting `cfx` to
run at all, make sure you have Python correctly installed.

Try running the following from a command line:

<pre>
  python --version
</pre>

`cfx` currently expects Python 2.5 or 2.6.  Older and newer versions may or may
not work.


Check Your Firefox or XULRunner
-------------------------------

`cfx` searches well known locations on your system for Firefox or XULRunner.
`cfx` may not have found an installation, or if you have multiple installations,
`cfx` may have found the wrong one.  In those cases you need to use `cfx`'s
`--binary` option.  See the [cfx Tool][] guide for more information.

When you run `cfx` to test your add-on or run unit tests, it prints out the
location of the Firefox or XULRunner binary that it found, so you can check its
output to be sure.

[cfx Tool]: dev-guide/cfx-tool.html


Check Your Text Console
-----------------------

When errors are generated in the SDK's APIs and your code, they are logged to
the text console.  This should be the same console or shell from which you ran
the `cfx` command.


Don't Leave Non-SDK Files Lying Around
------------------------------------------

Currently the SDK does not gracefully handle files and directories that it does
not expect to encounter.  If there are empty directories or directories or files
that are not related to the SDK inside your `addon-sdk` directory or its
sub-directories, try removing them.


Search for Known Issues
-----------------------

Someone else might have experienced your problem, too.  Other users often post
problems to the [project mailing list][jetpack-list].  You can also browse the
list of [known issues][bugzilla-known] or [search][bugzilla-search] for
specific keywords.

[bugzilla-known]: https://bugzilla.mozilla.org/buglist.cgi?order=Bug%20Number&resolution=---&resolution=DUPLICATE&query_format=advanced&product=Add-on%20SDK

[bugzilla-search]: https://bugzilla.mozilla.org/query.cgi?format=advanced&product=Add-on%20SDK


Contact the Project Team and User Group
---------------------------------------

SDK users and project team members discuss problems and proposals on the
[project mailing list][jetpack-list].  Someone else may have had the same
problem you do, so try searching the list.
You're welcome to post a question, too.

You can also chat with other SDK users in [#jetpack][#jetpack] on
[Mozilla's IRC network][IRC].

And if you'd like to [report a bug in the SDK][bugzilla-report], that's always
welcome!  You will need to create an account with Bugzilla, Mozilla's bug
tracker.

[jetpack-list]: http://groups.google.com/group/mozilla-labs-jetpack/topics

[#jetpack]:http://mibbit.com/?channel=%23jetpack&server=irc.mozilla.org

[IRC]: http://irc.mozilla.org/

[bugzilla-report]: https://bugzilla.mozilla.org/enter_bug.cgi?product=Add-on%20SDK&component=General


Run the SDK's Unit Tests
------------------------

The SDK comes with a suite of tests which ensures that its APIs work correctly.
You can run it with the following command:

<pre>
  cfx testall
</pre>

Some of the tests will open Firefox windows to check APIs related to the user
interface, so don't be alarmed.  Please let the suite finish before resuming
your work.

When the suite is finished, your text console should contain output that looks
something like this:

<pre>
  Testing cfx...
  .............................................................
  ----------------------------------------------------------------------
  Ran 61 tests in 4.388s

  OK
  Testing reading-data...
  Using binary at '/Applications/Firefox.app/Contents/MacOS/firefox-bin'.
  Using profile at '/var/folders/FL/FLC+17D+ERKgQe4K+HC9pE+++TI/-Tmp-/tmpu26K_5.mozrunner'.
  .info: My ID is 6724fc1b-3ec4-40e2-8583-8061088b3185
  ..
  3 of 3 tests passed.
  OK
  Total time: 4.036381 seconds
  Program terminated successfully.
  Testing all available packages: nsjetpack, test-harness, api-utils, development-mode.
  Using binary at '/Applications/Firefox.app/Contents/MacOS/firefox-bin'.
  Using profile at '/var/folders/FL/FLC+17D+ERKgQe4K+HC9pE+++TI/-Tmp-/tmp-dzeaA.mozrunner'.
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  .........................................................................
  ...............................................

  3405 of 3405 tests passed.
  OK
  Total time: 43.105498 seconds
  Program terminated successfully.
  All tests were successful. Ship it!
</pre>

If you get lots of errors instead, that may be a sign that the SDK does not work
properly on your system.  In that case, please file a bug or send a message to
the project mailing list.  See the previous section for information on doing so.
