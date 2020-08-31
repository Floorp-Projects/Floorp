Debugging A Hang On OS X
========================

+--------------------------------------------------------------------+
| This page is an import from MDN and the contents might be outdated |
+--------------------------------------------------------------------+

This article contains historical information about older versions of OS X.
See `How to Report a Hung
Firefox <https://developer.mozilla.org/en-US/docs/Mozilla/How_to_report_a_hung_Firefox>`__ instead.

If you find a hang in an application, it is very useful for the
developer to know where in the code this hang happens, especially if he
or she can't reproduce it. Below are steps you can use to attach
so-called "Samples" to bug reports.

Creating the sample on Mac OS X 10.5 (XCode < 4.2)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#. When the application is still hung, open up Spin Control.app (it’s in
   your <tt>/Developer/Applications/Performance Tools/</tt> folder; if
   it is missing, install the latest `Computer Hardware Understanding
   Development (CHUD)
   Tools <http://developer.apple.com/tools/download/>`__ from Apple).
#. By default, sampling of any hanging application will begin
   automatically.
#. After about 3-4 seconds, select the hanging application in the
   “Detected Hangs” window and click the “Interrupt Sampling” button.
   Note that sampling **will quickly hog up a lot of memory** if you
   leave it on for too long!
#. When it's done parsing the data, click the "Show text report" button;
   a new window will open with a couple of rows with stacktraces for all
   the threads in the sampled application.
#. If you need to upload the sample to Bugzilla, select all the sample
   text, copy it into your favorite text editor, and save as a
   plain-text file.
#. For a more detailed view of the sample data, make sure the sample you
   just recorded is selected in the “Detected Hangs” window and click on
   the “Open…” button.

Creating the sample on Mac OS X 10.4
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#. When the application is still hung, open up Sampler.app (it’s in your
   <tt>/Developer/Applications/Performance Tools/</tt> folder; if it is
   missing, install the latest `Computer Hardware Understanding
   Development (CHUD)
   Tools <http://developer.apple.com/tools/download/>`__ from Apple.).
#. Choose File > Attach...
#. Select "firefox-bin" or the hung application you want a sample from.
#. Now, click "Start recording". Note that only 3-4 seconds usually
   suffice, note that **this will quickly hog up a lot of memory** if
   you leave it on for too long!
#. When it's done parsing the data, you should now have a couple of rows
   with stacktraces for all the threads in the sampled application.
#. When you have the raw Sample data, you can't just save that and
   attach it to a bug, because the format is not very usable (unless the
   developer is a Mac hacker). Export it using Tools > Generate Report,
   and attach this as a text file to the bug report.

The final data will show where the application spent all its time when
it was hung, and hopefully your bug will be easier to fix now!

See also
~~~~~~~~

`Debugging on Mac OS X <https://developer.mozilla.org/en-US/docs/Mozilla/Debugging/Debugging_on_Mac_OS_X>`__
