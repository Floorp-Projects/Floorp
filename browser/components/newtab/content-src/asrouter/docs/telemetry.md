# Messaging System & Onboarding Telemetry

For historical reasons, the current (but soon-to-be legacy) Messaging System & Onboarding
telemetry documentation is mixed in with the Activity Stream documentation. All
except the main documentation is at [metrics we
collect](/browser/components/newtab/docs/v2-system-addon/data_events.md) and the
[data dictionary](/browser/components/newtab/docs/v2-system-addon/data_dictionary.md).

## Migration to Glean

Code all over the messaging system passes JSON ping objects up to a few
central spots. It may be [annotated with
attribution](https://searchfox.org/mozilla-central/search?q=symbol:AboutWelcomeTelemetry%23_maybeAttachAttribution&redirect=false)
along the way, and/or adjusted by some [policy
routines](https://searchfox.org/mozilla-central/search?q=symbol:TelemetryFeed%23createASRouterEvent&redirect=false)
before it's sent. [A version of the JSON that's been transformed slightly further is sent to
Glean](https://searchfox.org/mozilla-central/search?q=.submitGleanPingForPing&path=*.jsm&case=false&regexp=false).
The original annotated, policy-abiding JSON is sent to PingCentre immediately
after. After more validation and usage of Glean data has happened, we'll [stop
sending the data to PingCentre entirely](https://bugzilla.mozilla.org/show_bug.cgi?id=1849006).

## Adding or changing telemetry

For now, do the same stuff we've always done in OMC: follow the [process in
the Activity Stream telemetry
document](/browser/components/newtab/docs/v2-system-addon/telemetry.md) with one
exception: avoid adding any new nested objects, as these end up being flattened
or stringified before being sent to glean.  Note that when you need to add new metrics
(i.e. JSON keys), they MUST to be
[added](https://mozilla.github.io/glean/book/user/metrics/adding-new-metrics.html) to
[browser/components/newtab/metrics.yaml](https://searchfox.org/mozilla-central/source/browser/components/newtab/metrics.yaml)
in order to show up correctly in the Glean data.

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
