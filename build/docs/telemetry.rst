.. _buildtelemetry:

===============
Build Telemetry
===============

The build system (specifically, all the build tooling hooked
up to ``./mach``) has been configured to collect metrics data
points and errors for various build system actions. This data
helps drive team planning for the build team and ensure that
resources are applied to build processes that need them most.
You can opt-in to send telemetry to Mozilla during
``./mach bootstrap`` or by editing your ``.mozbuild/machrc``
file.

Telemetry
=========

The build telemetry schema can be found in-tree under
``python/mozbuild/mozbuild/telemetry.py`` in Voluptuous schema
format. You can use the ``export_telemetry_schema.py`` script in
that same directory to get the schema in JSON-schema format.
Details of the schema are specified below:

.. _telemetry.json#/:

:type: ``object``

:Required: :ref:`telemetry.json#/properties/argv`, :ref:`telemetry.json#/properties/build_opts`, :ref:`telemetry.json#/properties/client_id`, :ref:`telemetry.json#/properties/command`, :ref:`telemetry.json#/properties/duration_ms`, :ref:`telemetry.json#/properties/success`, :ref:`telemetry.json#/properties/system`, :ref:`telemetry.json#/properties/time`

**Properties:** :ref:`telemetry.json#/properties/argv`, :ref:`telemetry.json#/properties/build_opts`, :ref:`telemetry.json#/properties/client_id`, :ref:`telemetry.json#/properties/command`, :ref:`telemetry.json#/properties/duration_ms`, :ref:`telemetry.json#/properties/exception`, :ref:`telemetry.json#/properties/file_types_changed`, :ref:`telemetry.json#/properties/success`, :ref:`telemetry.json#/properties/system`, :ref:`telemetry.json#/properties/time`


.. _telemetry.json#/properties/argv:

argv
++++

Full mach commandline. If the commandline contains absolute paths they will be sanitized.

:type: ``array``

.. container:: sub-title

 Every element of **argv**  is:

:type: ``string``


.. _telemetry.json#/properties/build_opts:

build_opts
++++++++++

Selected build options

:type: ``object``

**Properties:** :ref:`telemetry.json#/properties/build_opts/properties/artifact`, :ref:`telemetry.json#/properties/build_opts/properties/ccache`, :ref:`telemetry.json#/properties/build_opts/properties/compiler`, :ref:`telemetry.json#/properties/build_opts/properties/debug`, :ref:`telemetry.json#/properties/build_opts/properties/icecream`, :ref:`telemetry.json#/properties/build_opts/properties/opt`, :ref:`telemetry.json#/properties/build_opts/properties/sccache`


.. _telemetry.json#/properties/build_opts/properties/artifact:

artifact
########

true if --enable-artifact-builds

:type: ``boolean``


.. _telemetry.json#/properties/build_opts/properties/ccache:

ccache
######

true if ccache is in use (--with-ccache)

:type: ``boolean``


.. _telemetry.json#/properties/build_opts/properties/compiler:

compiler
########

The compiler type in use (CC_TYPE)

**Allowed values:**

- clang
- clang-cl
- gcc
- msvc


.. _telemetry.json#/properties/build_opts/properties/debug:

debug
#####

true if build is debug (--enable-debug)

:type: ``boolean``


.. _telemetry.json#/properties/build_opts/properties/icecream:

icecream
########

true if icecream in use

:type: ``boolean``


.. _telemetry.json#/properties/build_opts/properties/opt:

opt
###

true if build is optimized (--enable-optimize)

:type: ``boolean``


.. _telemetry.json#/properties/build_opts/properties/sccache:

sccache
#######

true if ccache in use is sccache

:type: ``boolean``


.. _telemetry.json#/properties/build_attrs:

build_attrs
+++++++++++

Selected runtime attributes of the build

:type: ``object``

**Properties:** :ref:`telemetry.json#/properties/build_attrs/properties/cpu_percent`, :ref:`telemetry.json#/properties/build_attrs/properties/clobber`

.. _telemetry.json#/properties/build_attrs/properties/cpu_percent:

cpu_percent
###########

cpu utilization observed during the build

:type: ``number``

.. _telemetry.json#/properties/build_attrs/properties/clobber:

clobber
#######

true if the build was a clobber/full build

:type: ``boolean``


.. _telemetry.json#/properties/client_id:

client_id
+++++++++

A UUID to uniquely identify a client

:type: ``string``


.. _telemetry.json#/properties/command:

command
+++++++

The mach command that was invoked

:type: ``string``


.. _telemetry.json#/properties/duration_ms:

duration_ms
+++++++++++

Command duration in milliseconds

:type: ``number``


.. _telemetry.json#/properties/exception:

exception
+++++++++

If a Python exception was encountered during the execution of the command, this value contains the result of calling `repr` on the exception object.

:type: ``string``


.. _telemetry.json#/properties/file_types_changed:

file_types_changed
++++++++++++++++++

This array contains a list of objects with {ext, count} properties giving the count of files changed since the last invocation grouped by file type

:type: ``array``

.. container:: sub-title

 Every element of **file_types_changed**  is:

:type: ``object``

:Required: :ref:`telemetry.json#/properties/file_types_changed/items/properties/count`, :ref:`telemetry.json#/properties/file_types_changed/items/properties/ext`

**Properties:** :ref:`telemetry.json#/properties/file_types_changed/items/properties/count`, :ref:`telemetry.json#/properties/file_types_changed/items/properties/ext`


.. _telemetry.json#/properties/file_types_changed/items/properties/count:

count
#####

Count of changed files with this extension

:type: ``number``


.. _telemetry.json#/properties/file_types_changed/items/properties/ext:

ext
###

File extension

:type: ``string``


.. _telemetry.json#/properties/success:

success
+++++++

true if the command succeeded

:type: ``boolean``


.. _telemetry.json#/properties/system:

system
++++++

:type: ``object``

:Required: :ref:`telemetry.json#/properties/system/properties/os`

**Properties:** :ref:`telemetry.json#/properties/system/properties/cpu_brand`, :ref:`telemetry.json#/properties/system/properties/drive_is_ssd`, :ref:`telemetry.json#/properties/system/properties/logical_cores`, :ref:`telemetry.json#/properties/system/properties/memory_gb`, :ref:`telemetry.json#/properties/system/properties/os`, :ref:`telemetry.json#/properties/system/properties/physical_cores`, :ref:`telemetry.json#/properties/system/properties/virtual_machine`


.. _telemetry.json#/properties/system/properties/cpu_brand:

cpu_brand
#########

CPU brand string from CPUID

:type: ``string``


.. _telemetry.json#/properties/system/properties/drive_is_ssd:

drive_is_ssd
############

true if the source directory is on a solid-state disk

:type: ``boolean``


.. _telemetry.json#/properties/system/properties/logical_cores:

logical_cores
#############

Number of logical CPU cores present

:type: ``number``


.. _telemetry.json#/properties/system/properties/memory_gb:

memory_gb
#########

System memory in GB

:type: ``number``


.. _telemetry.json#/properties/system/properties/os:

os
##

Operating system

**Allowed values:**

- windows
- macos
- linux
- other


.. _telemetry.json#/properties/system/properties/physical_cores:

physical_cores
##############

Number of physical CPU cores present

:type: ``number``


.. _telemetry.json#/properties/system/properties/virtual_machine:

virtual_machine
###############

true if the OS appears to be running in a virtual machine

:type: ``boolean``


.. _telemetry.json#/properties/time:

time
++++

Time at which this event happened

:type: ``string``

:format: ``date-time``


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
