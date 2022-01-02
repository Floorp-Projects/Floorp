# HAR
HAR stands for `HTTP Archive` format used by various HTTP monitoring tools
to export collected data. This format is based on JSON and is supported by
many tools on the market including all main browsers (Firefox/Chrome/IE/Edge etc.)

HAR spec:
* http://www.softwareishard.com/blog/har-12-spec/
* https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/HAR/Overview.html

HAR adopters:
* http://www.softwareishard.com/blog/har-adopters/

# Netmonitor
Network monitor tool (in Firefox) supports exporting data in HAR format and
the implementation consists from the following objects.

## HarAutomation
This object is responsible for automated HAR export. It listens for Network
activity and triggers HAR export when the page is loaded.

The user needs to enable `devtools.netmonitor.har.enableAutoExportToFile` pref
and restart Firefox to switch this automation tool on.

## HarBuilder
This object is responsible for building HAR object (JSON). It gets all
HTTP requests currently displayed in the Network panel and builds valid HAR.
This object lazy loads all necessary data from the backend if needed,
so the result structure is complete.

## HarCollector
This object is responsible for collecting data related to all  HTTP requests
executed by the page (including inner iframes). The final export is triggered
by HarAutomation at the right time.

Note: this object is using it's own logic to fetch data from the backend.
It should reuse the Netmonitor Connector (src/connector/index),
so we don't have to maintain two code paths.

## HarExporter
This object represents the main public API designed to access export logic.
Clients, such as the Network panel itself, or WebExtensions API should use
this object to trigger exporting of collected HTTP data from the panel.
