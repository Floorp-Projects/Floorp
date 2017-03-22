.. _experiments_manifests:

=====================
Experiments Manifests
=====================

*Experiments Manifests* are documents that describe the set of active
experiments a client may run.

*Experiments Manifests* are fetched periodically by clients. When
fetched, clients look at the experiments within the manifest and
determine which experiments are applicable. If an experiment is
applicable, the client may download and start the experiment.

Manifest Format
===============

Manifests are JSON documents where the main element is an object.

The *schema* of the object is versioned and defined by the presence
of a top-level ``version`` property, whose integer value is the
schema version used by that manifest. Each version is documented
in the sections below.

Version 1
---------

Version 1 is the original manifest format.

The following properties may exist in the root object:

experiments
   An array of objects describing candidate experiments. The format of
   these objects is documented below.

   An array is used to create an explicit priority of experiments.
   Experiments listed at the beginning of the array take priority over
   experiments that follow.

Experiments Objects
^^^^^^^^^^^^^^^^^^^

Each object in the ``experiments`` array may contain the following
properties:

id
   (required) String identifier of this experiment. The identifier should
   be treated as opaque by clients. It is used to uniquely identify an
   experiment for all of time.

xpiURL
   (required) String URL of the XPI that implements this experiment.

   If the experiment is activated, the client will download and install this
   XPI.

xpiHash
   (required) String hash of the XPI that implements this experiment.

   The value is composed of a hash identifier followed by a colon
   followed by the hash value. e.g.
   `sha1:f677428b9172e22e9911039aef03f3736e7f78a7`. `sha1` and `sha256`
   are the two supported hashing mechanisms. The hash value is the hex
   encoding of the binary hash.

   When the client downloads the XPI for the experiment, it should compare
   the hash of that XPI against this value. If the hashes don't match,
   the client should not install the XPI.

   Clients may also use this hash as a means of determining when an
   experiment's XPI has changed and should be refreshed.

startTime
   Integer seconds since UNIX epoch that this experiment should
   start. Clients should not start an experiment if *now()* is less than
   this value.

maxStartTime
   (optional) Integer seconds since UNIX epoch after which this experiment
   should no longer start.

   Some experiments may wish to impose hard deadlines after which no new
   clients should activate the experiment. This property may be used to
   facilitate that.

endTime
   Integer seconds since UNIX epoch after which this experiment
   should no longer run. Clients should cease an experiment when the current
   time is beyond this value.

maxActiveSeconds
   Integer seconds defining the max wall time this experiment should be
   active for.

   The client should deactivate the experiment this many seconds after
   initial activation.

   This value only involves wall time, not browser activity or session time.

appName
   Array of application names this experiment should run on.

   An application name comes from ``nsIXULAppInfo.name``. It is a value
   like ``Firefox``, ``Fennec``, or `B2G`.

   The client should compare its application name against the members of
   this array. If a match is found, the experiment is applicable.

minVersion
   (optional) String version number of the minimum application version this
   experiment should run on.

   A version number is something like ``27.0.0`` or ``28``.

   The client should compare its version number to this value. If the client's
   version is greater or equal to this version (using a version-aware comparison
   function), the experiment is applicable.

   If this is not specified, there is no lower bound to versions this
   experiment should run on.

maxVersion
   (optional) String version number of the maximum application version this
   experiment should run on.

   This is similar to ``minVersion`` except it sets the upper bound for
   application versions.

   If the client's version is less than or equal to this version, the
   experiment is applicable.

   If this is not specified, there is no upper bound to versions this
   experiment should run on.

version
   (optional) Array of application versions this experiment should run on.

   This is similar to ``minVersion`` and ``maxVersion`` except only a
   whitelisted set of specific versions are allowed.

   The client should compare its version to members of this array. If a match
   is found, the experiment is applicable.

minBuildID
   (optional) String minimum Build ID this experiment should run on.

   Build IDs are values like ``201402261424``.

   The client should perform a string comparison of its Build ID against this
   value. If its value is greater than or equal to this value, the experiment
   is applicable.

maxBuildID
   (optional) String maximum Build ID this experiment should run on.

   This is similar to ``minBuildID`` except it sets the upper bound
   for Build IDs.

   The client should perform a string comparison of its Build ID against
   this value. If its value is less than or equal to this value, the
   experiment is applicable.

buildIDs
   (optional) Array of Build IDs this experiment should run on.

   This is similar to ``minBuildID`` and ``maxBuildID`` except only a
   whitelisted set of Build IDs are considered.

   The client should compare its Build ID to members of this array. If a
   match is found, the experiment is applicable.

os
   (optional) Array of operating system identifiers this experiment should
   run on.

   Values for this array come from ``nsIXULRuntime.OS``.

   The client will compare its operating system identifier to members
   of this array. If a match is found, the experiment is applicable to the
   client.

channel
   (optional) Array of release channel identifiers this experiment should run
   on.

   The client will compare its channel to members of this array. If a match
   is found, the experiment is applicable.

   If this property is not defined, the client should assume the experiment
   is to run on all channels.

locale
   (optional) Array of locale identifiers this experiment should run on.

   A locale identifier is a string like ``en-US`` or ``zh-CN`` and is
   obtained by looking at
   ``nsIXULChromeRegistry.getSelectedLocale("global")``.

   The client should compare its locale identifier to members of this array.
   If a match is found, the experiment is applicable.

   If this property is not defined, the client should assume the experiment
   is to run on all locales.

sample
   (optional) Decimal number indicating the sampling rate for this experiment.

   This will contain a value between ``0.0`` and ``1.0``. The client should
   generate a random decimal between ``0.0`` and ``1.0``. If the randomly
   generated number is less than or equal to the value of this field, the
   experiment is applicable.

disabled
   (optional) Boolean value indicating whether an experiment is disabled.

   Normally, experiments are deactivated after a certain time has passed or
   after the experiment itself determines it no longer needs to run (perhaps
   it collected sufficient data already).

   This property serves as a backup mechanism to remotely disable an
   experiment before it was scheduled to be disabled. It can be used to
   kill experiments that are found to be doing wrong or bad things or that
   aren't useful.

   If this property is not defined or is false, the client should assume
   the experiment is active and a candidate for activation.

frozen
   (optional) Boolean value indicating this experiment is frozen and no
   longer accepting new enrollments.

   If a client sees a true value in this field, it should not attempt to
   activate an experiment.

jsfilter
    (optional) JavaScript code that will be evaluated to determine experiment
    applicability.

    This property contains the string representation of JavaScript code that
    will be evaluated in a sandboxed environment using JavaScript's
    ``eval()``.

    The string is expected to contain the definition of a JavaScript function
    ``filter(context)``. This function receives as its argument an object
    holding application state. See the section below for the definition of
    this object.

    The purpose of this property is to allow experiments to define complex
    rules and logic for evaluating experiment applicability in a manner
    that is privacy conscious and doesn't require the transmission of
    excessive data.

    The return value of this filter indicates whether the experiment is
    applicable. Functions should return true if the experiment is
    applicable.

    If an experiment is not applicable, they should throw an Error whose
    message contains the reason the experiment is not applicable. This
    message may be logged and sent to remote servers, so it should not
    contain private or otherwise sensitive data that wouldn't normally
    be submitted.

    If a falsey (or undefined) value is returned, the client should
    assume the experiment is not applicable.

    If this property is not defined, the client does not consider a custom
    JavaScript filter function when determining whether an experiment is
    applicable.

JavaScript Filter Context Objects
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The object passed to a ``jsfilter`` ``filter()`` function contains the
following properties:

healthReportSubmissionEnabled
   This property contains a boolean indicating whether Firefox Health
   Report has its data submission flag enabled (whether Firefox Health
   Report is sending data to remote servers).

healthReportPayload
   This property contains the current Firefox Health Report payload.

   The payload format is documented at :ref:`healthreport_dataformat`.

telemetryPayload
   This property contains the current Telemetry payload.

The evaluation sandbox for the JavaScript filters may be destroyed
immediately after ``filter()`` returns. This function should not assume
async code will finish.

Experiment Applicability and Client Behavior
============================================

The point of an experiment manifest is to define which experiments are
available and where and how to run them. This section explains those
rules in more detail.

Many of the properties in *Experiment Objects* are related to determining
whether an experiment should run on a given client. This evaluation is
performed client side.

1. Multiple conditions in an experiment
---------------------------------------

If multiple conditions are defined for an experiment, the client should
combine each condition with a logical *AND*: all conditions must be
satisfied for an experiment to run. If one condition fails, the experiment
is not applicable.

2. Active experiment disappears from manifest
---------------------------------------------

If a specific experiment disappears from the manifest, the client should
continue conducting an already-active experiment. Furthermore, the
client should remember what the expiration events were for an experiment
and honor them.

The rationale here is that we want to prevent an accidental deletion
or temporary failure on the server to inadvertantly deactivate
supposed-to-be-active experiments. We also don't want premature deletion
of an experiment from the manifest to result in indefinite activation
periods.

3. Inactive experiment disappears from manifest
-----------------------------------------------

If an inactive but scheduled-to-be-active experiment disappears from the
manifest, the client should not activate the experiment.

If that experiment reappears in the manifest, the client should not
treat that experiment any differently than any other new experiment. Put
another way, the fact an inactive experiment disappears and then
reappears should not be significant.

The rationale here is that server operators should have complete
control of an inactive experiment up to it's go-live date.

4. Re-evaluating applicability on manifest refresh
--------------------------------------------------

When an experiment manifest is refreshed or updated, the client should
re-evaluate the applicability of each experiment therein.

The rationale here is that the server may change the parameters of an
experiment and want clients to pick those up.

5. Activating a previously non-applicable experiment
----------------------------------------------------

If the conditions of an experiment change or the state of the client
changes to allow an experiment to transition from previously
non-applicable to applicable, the experiment should be activated.

For example, if a client is running version 28 and the experiment
initially requires version 29 or above, the client will not mark the
experiment as applicable. But if the client upgrades to version 29 or if
the manifest is updated to require 28 or above, the experiment will
become applicable.

6. Deactivating a previously active experiment
----------------------------------------------

If the conditions of an experiment change or the state of the client
changes and an active experiment is no longer applicable, that
experiment should be deactivated.

7. Calculation of sampling-based applicability
----------------------------------------------

For calculating sampling-based applicability, the client will associate
a random value between ``0.0`` and ``1.0`` for each observed experiment
ID. This random value will be generated the first time sampling
applicability is evaluated. This random value will be persisted and used
in future applicability evaluations for this experiment.

By saving and re-using the value, the client is able to reliably and
consistently evaluate applicability, even if the sampling threshold
in the manifest changes.

Clients should retain the randomly-generated sampling value for
experiments that no longer appear in a manifest for a period of at least
30 days. The rationale is that if an experiment disappears and reappears
from a manifest, the client will not have multiple opportunities to
generate a random value that satisfies the sampling criteria.

8. Incompatible version numbers
-------------------------------

If a client receives a manifest with a version number that it doesn't
recognize, it should ignore the manifest.

9. Usage of old manifests
-------------------------

If a client experiences an error fetching a manifest (server not
available) or if the manifest is corrupt, not readable, or compatible,
the client may use a previously-fetched (cached) manifest.

10. Updating XPIs
-----------------

If the URL or hash of an active experiment's XPI changes, the client
should fetch the new XPI, uninstall the old XPI, and install the new
XPI.

Examples
========

Here is an example manifest::

   {
     "version": 1,
     "experiments": [
       {
         "id": "da9d7f4f-f3f9-4f81-bacd-6f0626ffa360",
         "xpiURL": "https://experiments.mozilla.org/foo.xpi",
         "xpiHash": "sha1:cb1eb32b89d86d78b7326f416cf404548c5e0099",
         "startTime": 1393000000,
         "endTime": 1394000000,
         "appName": ["Firefox", "Fennec"],
         "minVersion": "28",
         "maxVersion": "30",
         "os": ["windows", "linux", "osx"],
         "jsfilter": "function filter(context) { return context.healthReportEnabled; }"
       }
     ]
   }
