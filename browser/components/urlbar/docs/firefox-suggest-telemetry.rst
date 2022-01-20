Firefox Suggest Telemetry
=========================

This document describes the telemetry that Firefox records for the Firefox
Suggest feature. That is, it describes Firefox Suggest telemetry recorded on the
client. It also discusses the data that Firefox sends to the Merino service.

For information on other telemetry related to the address bar, see the general
address bar :doc:`telemetry` document. For information on all telemetry in
Firefox, see the toolkit :doc:`/toolkit/components/telemetry/index` document.

.. toctree::
   :caption: Table of Contents

   firefox-suggest-telemetry

Histograms
----------

The following histograms are recorded for Firefox Suggest. For general
information on histogram telemetry in Firefox, see the
:doc:`/toolkit/components/telemetry/collection/histograms` document.

FX_URLBAR_MERINO_LATENCY_MS
~~~~~~~~~~~~~~~~~~~~~~~~~~~

This histogram records the latency in milliseconds of the Merino source for
suggestions, or in other words, the time from Firefox's request to the Merino
server to the time Firefox receives a response. It is an exponential histogram
with 50 buckets and values between 0 and 30000 (0s and 30s).

Changelog
  Firefox 93.0
    Introduced. [Bug 1727799_]

.. _1727799: https://bugzilla.mozilla.org/show_bug.cgi?id=1727799

FX_URLBAR_MERINO_RESPONSE
~~~~~~~~~~~~~~~~~~~~~~~~~

This categorical histogram records a summary of each fetch from the Merino
server. It has the following categories:

:0 "success":
   The fetch completed without any error before the timeout elapsed.
:1 "timeout":
   The timeout elapsed before the fetch completed or otherwise failed.
:2 "network_error":
   The fetch failed due to a network error before the timeout elapsed. e.g., the
   user's network or the Merino server was down.
:3 "http_error":
   The fetch completed before the timeout elapsed but the server returned an
   error.

Changelog
  Firefox 94.0.2
    Introduced. [Bug 1737923_]

.. _1737923: https://bugzilla.mozilla.org/show_bug.cgi?id=1737923

FX_URLBAR_QUICK_SUGGEST_REMOTE_SETTINGS_LATENCY_MS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This histogram records the latency in milliseconds of the remote settings source
for suggestions, or in other words, the time from when Firefox starts fetching a
suggestion from remote settings to the time the suggestion is retrieved. It is
an exponential histogram with 50 buckets and values between 0 and 30000 (0s and
30s).

Note that unlike Merino, fetches from remote settings happen entirely on the
client, so remote settings latencies are expected to be much smaller than Merino
latencies.

Changelog
  Firefox 94.0.2
    Introduced. [Bug 1737651_]

.. _1737651: https://bugzilla.mozilla.org/show_bug.cgi?id=1737651

Scalars
-------

The following scalars are recorded for Firefox Suggest. For general information
on scalar telemetry in Firefox, see the
:doc:`/toolkit/components/telemetry/collection/scalars` document.

contextual.services.quicksuggest.click
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This keyed scalar is incremented each time the user picks a suggestion. Each key
is the index at which a suggestion appeared in the results (1-based), and the
corresponding value is the number of clicks at that index.

Changelog
  Firefox 87.0
    Introduced. [Bug 1693927_]

.. _1693927: https://bugzilla.mozilla.org/show_bug.cgi?id=1693927

contextual.services.quicksuggest.help
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This keyed scalar is incremented each time the user picks the help button in a
suggestion. Each key is the index at which a suggestion appeared in the results
(1-based), and the corresponding value is the number of help button clicks at
that index.

Changelog
  Firefox 87.0
    Introduced. [Bug 1693927_]

.. _1693927: https://bugzilla.mozilla.org/show_bug.cgi?id=1693927

contextual.services.quicksuggest.impression
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This keyed scalar records suggestion impressions. It is incremented each time
the user is shown a suggestion and the following two conditions hold:

- The user has completed an engagement with the address bar by picking a result
  in it or by pressing the Enter key.
- At the time the user completed the engagement, a suggestion was present in the
  results.

Each key is the index at which a suggestion appeared in the results (1-based),
and the corresponding value is the number of impressions at that index.

Changelog
  Firefox 87.0
    Introduced. [Bug 1693927_]

.. _1693927: https://bugzilla.mozilla.org/show_bug.cgi?id=1693927

Events
------

The following Firefox Suggest events are recorded in the
``contextservices.quicksuggest`` category. For general information on event
telemetry in Firefox, see the
:doc:`/toolkit/components/telemetry/collection/events` document.

contextservices.quicksuggest.data_collect_toggled
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This event is recorded when the
``browser.urlbar.quicksuggest.dataCollection.enabled`` pref is toggled. The pref
can be toggled in the following ways:

- The user can toggle it in the preferences UI.
- The user can toggle it in about:config.

The event is also recorded when the user opts in to the online modal dialog,
with one exception: If the user has already enabled data collection using the
preferences UI or about:config, then the pref's user value is already
true. Opting in doesn't change the user value, so no event is recorded.

The event's objects are the following:

:enabled:
  Recorded when the pref is flipped from false to true.
:disabled:
  Recorded when the pref is flipped from true to false.

Changelog
  Firefox 94.0.2
    Introduced. [Bug 1735976_]

.. _1735976: https://bugzilla.mozilla.org/show_bug.cgi?id=1735976

contextservices.quicksuggest.enable_toggled
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This event is recorded when the
``browser.urlbar.suggest.quicksuggest.nonsponsored`` pref is toggled. The pref
can be toggled in the following ways:

- The user can toggle it in the preferences UI.
- The user can toggle it in about:config.

The event's objects are the following:

:enabled:
  Recorded when the pref is flipped from false to true.
:disabled:
  Recorded when the pref is flipped from true to false.

Changelog
  Firefox 87.0:
    Introduced. The event corresponds to the
    ``browser.urlbar.suggest.quicksuggest`` pref. [Bug 1693126_]

  Firefox 94.0.2:
    ``browser.urlbar.suggest.quicksuggest`` is replaced with
    ``browser.urlbar.suggest.quicksuggest.nonsponsored``, and this event now
    corresponds to the latter pref. [Bug 1735976_]

  Firefox 96.0:
    The event is no longer recorded when the user interacts with the online
    modal dialog since the ``browser.urlbar.suggest.quicksuggest.nonsponsored``
    pref is no longer set when the user opts in or out. [Bug 1740965_]

.. _1693126: https://bugzilla.mozilla.org/show_bug.cgi?id=1693126
.. _1735976: https://bugzilla.mozilla.org/show_bug.cgi?id=1735976
.. _1740965: https://bugzilla.mozilla.org/show_bug.cgi?id=1740965

contextservices.quicksuggest.opt_in_dialog
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This event is recorded when the user interacts with the online modal dialog.
The event's objects are the following:

:accept:
  The user accepted the dialog and opted in. This object was removed in Firefox
  96.0.2.
:accept_2:
  The user accepted the dialog and opted in.
:close_1:
  The user clicked close button or something similar link on introduction
  section. The user remains opted out in this case.
:dismiss_1:
  The user dismissed the dialog by pressing the Escape key or some unknown way
  on introduction section. The user remains opted out in this case.
:dismiss_2:
  The user dismissed the dialog by pressing the Escape key or some unknown way
  on main section. The user remains opted out in this case.
:dismissed_escape_key:
  The user dismissed the dialog by pressing the Escape key. The user remains
  opted out in this case. This object was removed in Firefox 96.0.2.
:dismissed_other:
  The dialog was dismissed in some unknown way. One case where this can happen
  is when the dialog is replaced with another higher priority dialog like the
  one shown when quitting the app. The user remains opted out in this case.
  This object was removed in Firefox 96.0.2.
:learn_more:
  The user clicked "Learn more". The user remains opted out in this case. This
  object was removed in Firefox 96.0.2.
:learn_more_2:
  The user clicked "Learn more". The user remains opted out in this case.
:not_now:
  The dialog was dismissed in some way without opting in. This object was
  removed in Firefox 94.0.
:not_now_2:
  The user clicked "Not now" link on main section. The user remains opted out in
  this case.
:not_now_link:
  The user clicked "Not now". The user remains opted out in this case. This
  object was removed in Firefox 96.0.2.
:reject_2:
  The user rejected the dialog and opted out.
:settings:
  The user clicked the "Customize" button. The user remains opted out in this
  case. This object was removed in Firefox 96.0.2.

Changelog
  Firefox 92.0.1
    Introduced. Objects are: ``accept``, ``settings``, ``learn_more``, and
    ``not_now``. ``not_now`` is recorded when the dialog is dismissed in any
    manner not covered by the other objects. [Bug 1723860_]

  Firefox 94.0
    Objects changed to: ``accept``, ``dismissed_escape_key``,
    ``dismissed_other``, ``learn_more``, ``not_now_link``, and ``settings``.
    [Bug 1733687_]

  Firefox 96.0.2
    Objects changed to: ``accept_2``, ``reject_2``, ``learn_more_2``,
    ``close_1``, ``not_now_2``, ``dismiss_1`` and ``dismiss_2``.
    [Bug 1745026_]

.. _1723860: https://bugzilla.mozilla.org/show_bug.cgi?id=1723860
.. _1733687: https://bugzilla.mozilla.org/show_bug.cgi?id=1733687
.. _1745026: https://bugzilla.mozilla.org/show_bug.cgi?id=1745026

contextservices.quicksuggest.sponsored_toggled
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This event is recorded when the
``browser.urlbar.suggest.quicksuggest.sponsored`` pref is toggled. The pref can
be toggled in the following ways:

- The user can toggle it in the preferences UI.
- The user can toggle it in about:config.

The event's objects are the following:

:enabled:
  Recorded when the pref is flipped from false to true.
:disabled:
  Recorded when the pref is flipped from true to false.

Changelog
  Firefox 92.0.1
    Introduced. [Bug 1728430_]

  Firefox 96.0:
    The event is no longer recorded when the user interacts with the online
    modal dialog since the ``browser.urlbar.suggest.quicksuggest.sponsored``
    pref is no longer set when the user opts in or out. [Bug 1740965_]

.. _1728430: https://bugzilla.mozilla.org/show_bug.cgi?id=1728430
.. _1740965: https://bugzilla.mozilla.org/show_bug.cgi?id=1740965

Environment
-----------

The following preferences are recorded in telemetry environment data. For
general information on telemetry environment data in Firefox, see the
:doc:`/toolkit/components/telemetry/data/environment` document.

browser.urlbar.quicksuggest.onboardingDialogChoice
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This pref records the user's choice in the online modal dialog. If the dialog
was shown multiple times, it records the user's most recent choice. It is a
string-valued pref with the following possible values:

:<empty string>:
  The user has not made a choice (e.g., because the dialog hasn't been shown).
:accept:
  The user accepted the dialog and opted in. This object was removed in Firefox
  96.0.2.
:accept_2:
  The user accepted the dialog and opted in.
:close_1:
  The user clicked close button or something similar link on introduction
  section. The user remains opted out in this case.
:dismiss_1:
  The user dismissed the dialog by pressing the Escape key or some unknown way
  on introduction section. The user remains opted out in this case.
:dismiss_2:
  The user dismissed the dialog by pressing the Escape key or some unknown way
  on main section. The user remains opted out in this case.
:dismissed_escape_key:
  The user dismissed the dialog by pressing the Escape key. The user remains
  opted out in this case. This object was removed in Firefox 96.0.2.
:dismissed_other:
  The dialog was dismissed in some unknown way. One case where this can happen
  is when the dialog is replaced with another higher priority dialog like the
  one shown when quitting the app. The user remains opted out in this case. This
  object was removed in Firefox 96.0.2.
:learn_more:
  The user clicked "Learn more". The user remains opted out in this case. This
  object was removed in Firefox 96.0.2.
:learn_more_2:
  The user clicked "Learn more". The user remains opted out in this case.
:not_now_2:
  The user clicked "Not now" link on main section. The user remains opted out in
  this case.
:not_now_link:
  The user clicked "Not now". The user remains opted out in this case. This
  object was removed in Firefox 96.0.2.
:reject_2:
  The user rejected the dialog and opted out.
:settings:
  The user clicked the "Customize" button. The user remains opted out in this
  case. This object was removed in Firefox 96.0.2.

Changelog
  Firefox 94.0
    Introduced. [Bug 1734447_]

  Firefox 96.0.2
    Added ``accept_2``, ``reject_2``, ``learn_more_2``, ``close_1``,
    ``not_now_2``, ``dismiss_1``, ``dismiss_2`` and removed ``accept``,
    ``dismissed_escape_key``, ``dismissed_other``, ``learn_more``,
    ``not_now_link``, ``settings``. [Bug 1745026_]

.. _1734447: https://bugzilla.mozilla.org/show_bug.cgi?id=1734447
.. _1745026: https://bugzilla.mozilla.org/show_bug.cgi?id=1745026

browser.urlbar.quicksuggest.dataCollection.enabled
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This boolean pref records whether the user has opted in to data collection for
Firefox Suggest. It is false by default. It is set to true when the user opts in
to the online modal dialog. The user can also toggle it in the preferences UI
and about:config.

Changelog
  Firefox 94.0.2
    Introduced. [Bug 1735976_]

.. _1735976: https://bugzilla.mozilla.org/show_bug.cgi?id=1735976

browser.urlbar.suggest.quicksuggest
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This pref no longer exists and is not recorded. It was replaced with
``browser.urlbar.suggest.quicksuggest.nonsponsored`` in Firefox 94.0.2. Prior to
94.0.2, this boolean pref recorded whether suggestions in general were enabled.

Changelog
  Firefox 92.0.1
    Introduced. [Bug 1730721_]

  Firefox 94.0.2
    Replaced with ``browser.urlbar.suggest.quicksuggest.nonsponsored``. [Bug
    1735976_]

.. _1730721: https://bugzilla.mozilla.org/show_bug.cgi?id=1730721
.. _1735976: https://bugzilla.mozilla.org/show_bug.cgi?id=1735976

browser.urlbar.suggest.quicksuggest.nonsponsored
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This boolean pref records whether non-sponsored suggestions are enabled. In both
the offline and online scenarios it is true by default. The user can also toggle
it in the preferences UI and about:config.

Changelog
  Firefox 94.0.2
    Introduced. It replaces ``browser.urlbar.suggest.quicksuggest``. [Bug
    1735976_]

  Firefox 96.0:
    The pref is now true by default in the online scenario. Previously it was
    false by default in online. For users who were enrolled in the online
    scenario in older versions and who did not opt in or otherwise enable
    non-sponsored suggestions, the pref will remain false when upgrading. For
    all other users, it will default to true when/if they are enrolled in
    online. [Bug 1740965_]

.. _1735976: https://bugzilla.mozilla.org/show_bug.cgi?id=1735976
.. _1740965: https://bugzilla.mozilla.org/show_bug.cgi?id=1740965

browser.urlbar.suggest.quicksuggest.sponsored
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This boolean pref records whether sponsored suggestions are enabled. In both the
offline and online scenarios it is true by default. The user can also toggle it
in the preferences UI and about:config.

Changelog
  Firefox 92.0.1
    Introduced. [Bug 1730721_]

  Firefox 96.0:
    The pref is now true by default in the online scenario. Previously it was
    false by default in online. For users who were enrolled in the online
    scenario in older versions and who did not opt in or otherwise enable
    sponsored suggestions, the pref will remain false when upgrading. For all
    other users, it will default to true when/if they are enrolled in
    online. [Bug 1740965_]

.. _1730721: https://bugzilla.mozilla.org/show_bug.cgi?id=1730721
.. _1740965: https://bugzilla.mozilla.org/show_bug.cgi?id=1740965

Contextual Services Pings
-------------------------

The following custom pings record impressions and clicks on Firefox Suggest
suggestions. For general information on custom ping telemetry in Firefox, see
the `Custom Ping`_ document.

.. _Custom Ping: https://docs.telemetry.mozilla.org/cookbooks/new_ping.html#sending-a-custom-ping

Click
~~~~~

A click ping is recorded when the user picks a suggestion. Its payload includes
the following:

:advertiser:
  The name of the suggestion's advertiser.
:block_id:
  A unique identifier for the suggestion (a.k.a. a keywords block).
:context_id:
  A UUID representing this user. Note that it's not client_id, nor can it be
  used to link to a client_id.
:position:
  The index of the suggestion in the list of results (1-based).
:reporting_url:
  The reporting URL of the suggestion, normally pointing to the ad partner's
  reporting endpoint.
:request_id:
  A request identifier for each API request to Merino. This is only included for
  suggestions provided by Merino.
:scenario:
  The user's Suggest scenario, either "offline" or "online".

Changelog
  Firefox 87.0
    Introduced. The payload is: ``advertiser``, ``block_id``, ``position``, and
    ``reporting_url``. [Bug 1689365_]

  Firefox 92.0.1
    ``scenario`` is added to the payload. [Bug 1729576_]

  Firefox 94.0.2
    ``request_id`` is added to the payload. [Bug 1736117_]

.. _1689365: https://bugzilla.mozilla.org/show_bug.cgi?id=1689365
.. _1729576: https://bugzilla.mozilla.org/show_bug.cgi?id=1729576
.. _1736117: https://bugzilla.mozilla.org/show_bug.cgi?id=1736117

Impression
~~~~~~~~~~

An impression ping is recorded when the user is shown a suggestion and the
following two conditions hold:

- The user has completed an engagement with the address bar by picking a result
  in it or by pressing the Enter key.
- At the time the user completed the engagement, a suggestion was present in the
  results.

The impression ping payload contains the following:

:advertiser:
  The name of the suggestion's advertiser.
:block_id:
  A unique identifier for the suggestion (a.k.a. a keywords block).
:context_id:
  A UUID representing this user. Note that it's not client_id, nor can it be
  used to link to a client_id.
:is_clicked:
  Whether or not the user also clicked the suggestion.
:matched_keywords (**Removed from Firefox 97**):
  The matched keywords that lead to the suggestion. This is only included when
  the user has opted in to data collection and the suggestion is provided by
  remote settings.
:position:
  The index of the suggestion in the list of results (1-based).
:reporting_url:
  The reporting URL of the suggestion, normally pointing to the ad partner's
  reporting endpoint.
:request_id:
  A request identifier for each API request to Merino. This is only included for
  suggestions provided by Merino.
:scenario:
  The user's Suggest scenario, either "offline" or "online".
:search_query (**Removed from Firefox 97**):
  The exact search query typed by the user. This is only included when the user
  has opted in to data collection and the suggestion is provided by remote
  settings.

Changelog
  Firefox 87.0
    Introduced. The payload is: ``advertiser``, ``block_id``, ``is_clicked``,
    ``matched_keywords``, ``position``, ``reporting_url``, and
    ``search_query``. ``matched_keywords`` and ``search_query`` are always
    included in the payload and are always identical: They both record the exact
    search query as typed by the user. [Bug 1689365_]

  Firefox 91.0.1 (Release and ESR)
    ``matched_keywords`` and ``search_query`` are always recorded as empty
    strings. [Bug 1725492_]

  Firefox 92.0.1
    - When the user's scenaro is "online", ``matched_keywords`` records the full
      keyword of the matching suggestion and ``search_query`` records the exact
      search query as typed by the user; otherwise both are recorded as empty
      strings. [Bug 1728188_, 1729576_]
    - ``scenario`` is added to the payload. [Bug 1729576_]

  Firefox 94.0.2
    - When the user has opted in to data collection and the matching suggestion
      is provided by remote settings, ``matched_keywords`` records the full
      keyword of the suggestion and ``search_query`` records the exact search
      query as typed by the user; otherwise both are excluded from the ping.
      [Bug 1736117_, 1735976_]
    - ``request_id`` is added to the payload. [Bug 1736117_]

  Firefox 97.0
    - Stop sending ``search_query`` and ``matched_keywords`` in the custom
      impression ping for Firefox Suggest. [Bug 1748348_]

.. _1689365: https://bugzilla.mozilla.org/show_bug.cgi?id=1689365
.. _1725492: https://bugzilla.mozilla.org/show_bug.cgi?id=1725492
.. _1728188: https://bugzilla.mozilla.org/show_bug.cgi?id=1728188
.. _1729576: https://bugzilla.mozilla.org/show_bug.cgi?id=1729576
.. _1736117: https://bugzilla.mozilla.org/show_bug.cgi?id=1736117
.. _1735976: https://bugzilla.mozilla.org/show_bug.cgi?id=1735976
.. _1748348: https://bugzilla.mozilla.org/show_bug.cgi?id=1748348

Nimbus Exposure Event
---------------------

A `Nimbus exposure event`_ is recorded the first time a user query matches a
Firefox Suggest suggestion while the user is enrolled in a Nimbus experiment or
rollout. At most one event per app session is recorded.

.. _Nimbus exposure event: https://experimenter.info/jetstream/jetstream/#enrollment-vs-exposure

Changelog
  Firefox 92.0
    Introduced. [Bug 1724076_, 1727392_]

.. _1724076: https://bugzilla.mozilla.org/show_bug.cgi?id=1724076
.. _1727392: https://bugzilla.mozilla.org/show_bug.cgi?id=1727392

Merino Search Queries
---------------------

Merino is a Mozilla service that provides Firefox Suggest suggestions. Along
with remote settings on the client, it is one of two possible sources for
Firefox Suggest. When Merino integration is enabled on the client and the user
has opted in to Firefox Suggest data collection, Firefox sends everything the
user types in the address bar to the Merino server. In response, Merino finds
relevant search results from its search providers and sends them to Firefox,
where they are shown to the user in the address bar.

The user opts in to Firefox Suggest data collection when they either opt in to
the online modal dialog or they enable Firefox Suggest data collection in the
preferences UI.

Merino queries are not telemetry per se but we include them in this document
since they necessarily involve data collection.

Merino API
~~~~~~~~~~

Data that Firefox sends to the Merino server is summarized below. When Merino
integration is enabled on the client and the user has opted in to Firefox
Suggest data collection, this data is sent with every user keystroke in the
address bar.

For details on the Merino API, see the `Merino documentation`_.

.. _Merino documentation: https://mozilla-services.github.io/merino/api.html#suggest

Search Query
  The user's search query typed in the address bar.

  API parameter name: ``q``

Client Variants
  Optional. A list of experiments or rollouts that are affecting the Firefox
  Suggest user experience. If Merino recognizes any of them, it will modify its
  behavior accordingly.

  API parameter name: ``client_variants``

Providers
  Optional. A list of providers to use for this request. If specified, only
  suggestions from the listed providers will be returned. Otherwise Merino will
  use a default set of providers.

  API parameter name: ``providers``
