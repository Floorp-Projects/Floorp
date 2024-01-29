# Messaging System & Onboarding Telemetry

This document (combined with the [messaging system ping section of the Glean Dictionary](https://dictionary.telemetry.mozilla.org/apps/firefox_desktop/pings/messaging-system)), is now the place to look first for Messaging System and Onboarding telemetry information. For historical reasons, there is still some related documentation mixed in with the Activity Stream documentation. If you can't find what you need here, check [old metrics we collect](/browser/components/newtab/docs/v2-system-addon/data_events.md) and the
[old data dictionary](/browser/components/newtab/docs/v2-system-addon/data_dictionary.md).

## Collection with Glean

Code all over the messaging system passes JSON ping objects up to a few
central spots. It may be [annotated with
attribution](https://searchfox.org/mozilla-central/search?q=symbol:AboutWelcomeTelemetry%23_maybeAttachAttribution&redirect=false)
along the way, and/or adjusted by some [policy
routines](https://searchfox.org/mozilla-central/search?q=symbol:TelemetryFeed%23createASRouterEvent&redirect=false)
before it's sent. The JSON will be transformed slightly further before being [sent to
Glean][submit-glean-for-glean].

## Design of Messaging System Data Collections

Data is sent in the
[Messaging System Ping](https://dictionary.telemetry.mozilla.org/apps/firefox_desktop/pings/messaging-system).
Which Messaging System Ping you get is recorded with
[Ping Type](https://dictionary.telemetry.mozilla.org/apps/firefox_desktop/metrics/messaging_system_ping_type).
If you wish to expand the collection of data,
consider whether data belongs on the `messaging-system` ping
(usually when data is timely to the ping itself)
or if it's a more general collection,
in which case the data can go on the default `send_in_pings` entry,
which is the `metrics` ping.
In either case, you can add a metric definition in the
[metrics.yaml][metrics-yaml]
file.

## Adding or changing telemetry

A general process overview can be found in the
[Activity Stream telemetry document](/browser/components/newtab/docs/v2-system-addon/telemetry.md).

Note that when you need to add new metrics (i.e. JSON keys),
they MUST to be
[added](https://mozilla.github.io/glean/book/user/metrics/adding-new-metrics.html) to
[browser/components/newtab/metrics.yaml][metrics-yaml]
in order to show up correctly in the Glean data.

Avoid adding any new nested objects, because Glean can't handle these. In the best case, any such additions will end up being flattened or stringified before being sent.

## Monitoring FxMS Telemetry Health

The OMC team owns an [OpMon](https://github.com/mozilla/opmon) dashboard for the FxMS Desktop Glean telemetry with
alerts. Note that it can only show one channel at any given time, here's a link
to [Windows
Release](https://mozilla.cloud.looker.com/dashboards/operational_monitoring::firefox_messaging_system?Percentile=50&Normalized+Channel=release&Normalized+Os=Windows).
The dashboard is specified in
[firefox-messaging-system.toml](https://github.com/mozilla/metric-hub/blob/main/opmon/firefox-messaging-system.toml),
and reading the source can help clarify exactly what it means. We are the owner
of this file, and are encouraged to adjust it to our needs, though it's probably
a good idea to get review from someone in Data Science.

The current plan is to review the OpMon dashboard as a group in our weekly
triage meeting, note anything that seems unusual to our [Google docs
log](https://docs.google.com/document/d/1d16GCuul9sENMOMDAcD1kKNBtnJLouDxZtIgz2u-70U/edit),
and, if we want to investigate further, file [a bug that blocks
`fxms-glean`](https://bugzilla.mozilla.org/showdependencytree.cgi?id=1843409&hide_resolved=1).

The dashboard is configured to alert in various cases, and those alerts can be
seen at the bottom of the dashboard. As of this writing, the alerts have some
noise [to be cleaned up](https://bugzilla.mozilla.org/show_bug.cgi?id=1843406)
before we can automatically act on them.

## Appendix: A Short Glean Primer, as it applies to this project (courtesy of Chris H-C)

* [Glean](https://mozilla.github.io/glean/book/) is a data collection library by
  Mozilla for Mozilla. You define metrics like counts and timings and things,
  and package those into pings which are the payloads sent to our servers.
* You can see current [FxMS Glean metrics and pings](https://dictionary.telemetry.mozilla.org/apps/firefox_desktop/pings/messaging-system)
  in the desktop section of the Glean Dictionary. The layer embedding Glean into
  Firefox Desktop is called [Firefox on Glean (FOG)](https://firefox-source-docs.mozilla.org/toolkit/components/glean/index.html).
* Documentation will be automatically generated and hosted on the Glean
  Dictionary, so write long rich-text Descriptions and augment them off-train
  with Glean Annotations.
* Schemas for ingestion are automatically generated. You can go from landing a
  new ping to querying the data being sent [within two
  days](https://blog.mozilla.org/data/2021/12/14/this-week-in-glean-how-long-must-i-wait-before-i-can-see-my-data/).
* Make a mistake? No worries. Changes are quick and easy and are reflected in
  the received data within a day.
* Local debugging involves using Glean's ergonomic test APIs and/or the Glean
  Debug Ping Viewer which you can learn more about on `about:glean`.
* If you have any questions, the Glean Team is available across a lot of
  timezones on the [`#glean:mozilla.org` channel](https://chat.mozilla.org/#/room/#glean:mozilla.org) on Matrix and Slack `#data-help`.

  [submit-glean-for-glean]: https://searchfox.org/mozilla-central/search?q=.submitGleanPingForPing&path=*.jsm&case=false&regexp=false
  [metrics-yaml]: https://searchfox.org/mozilla-central/source/browser/components/newtab/metrics.yaml
