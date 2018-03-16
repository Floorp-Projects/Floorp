.. _browsererrorreporter:

=======================
Browser Error Reporter
=======================

The `BrowserErrorReporter.jsm <https://dxr.mozilla.org/mozilla-central/source/browser/modules/BrowserErrorReporter.jsm>`_ module collects errors logged to the Browser Console and sends them to a remote error aggregation service.

.. note::
   This module and the related service is a prototype and will be removed from Firefox in later 2018.

Opt-out
=======
Collection is enabled by default in the Nightly channel, except for local builds, where it is disabled. It is not available outside of the Nightly channel.

To opt-out of collection:

1. Open ``about:preferences``.
2. Select the Privacy and Security panel and go to the Nightly Data Collection and Use section.
3. Uncheck "Allow Nightly to send browser error reports (including error messages) to Mozilla".

Collected Error Data
====================
Errors are first sampled at the rate specified by the ``browser.chrome.errorReporter.sampleRate`` preference.

The payload sent to the remote collection service contains the following info:

- Firefox version number
- Firefox update channel (usually "Nightly")
- Firefox build ID
- Revision used to build Firefox
- Timestamp when the error occurred
- A project ID specified by the ``browser.chrome.errorReporter.projectId`` preference
- The error message
- The filename that the error was thrown from
- A stacktrace, if available, of the code being executed when the error was thrown

Privacy-sensitive info
======================
Error reports may contain sensitive information about the user:

- Error messages may inadvertently contain personal info depending on the code that generated the error.
- Filenames in the stack trace may contain add-on IDs of currently-installed add-ons. They may also contain local filesystem paths.

.. seealso::

   `Browser Error Collection wiki page <https://wiki.mozilla.org/Firefox/BrowserErrorCollection>`_
      Wiki page with up-to-date information on error collection and how we restrict access to the collected data.
