.. _buildtelemetry:

===============
Build Telemetry
===============

The build system (specifically, all the build tooling hooked
up to ``./mach``) has been configured to collect metrics data
points and errors for various build system actions. This data
helps drive team planning for the build team and ensure that
resources are applied to build processes that need them most.
You can adjust your telemetry settings by editing your
``~/.mozbuild/machrc`` file.

Glean Telemetry
===============

Mozbuild reports data using `Glean <https://mozilla.github.io/glean/>`_ via
:ref:`mach_telemetry`.  The metrics collected are documented :ref:`here<metrics>`.

Error Reporting
===============

``./mach`` uses `Sentry <https://sentry.io/welcome/>`_
to automatically report errors to `our issue-tracking dashboard
<https://sentry.prod.mozaws.net/operations/mach/>`_.

Information captured
++++++++++++++++++++

Sentry automatically collects useful information surrounding
the error to help the build team discover what caused the
issue and how to reproduce it. This information includes:

* Environmental information, such as the computer name, timestamp, Python runtime and Python module versions
* Process arguments
* The stack trace of the error, including contextual information:

    * The data contained in the exception
    * Functions and their respective source file names, line numbers
    * Variables in each frame
* `Sentry "Breadcrumbs" <https://docs.sentry.io/platforms/python/default-integrations/>`_,
  which are important events that have happened which help contextualize the error, such as:

    * An HTTP request has occurred
    * A subprocess has been spawned
    * Logging has occurred

Note that file paths may be captured, which include absolute paths (potentially including usernames).
