# Metrics we collect

By default, the about:newtab, about:welcome and about:home pages in Firefox (the pages you see when you open a new tab and when you start the browser), will send data back to Mozilla servers about usage of these pages. The intent is to collect data in order to improve the user's experience while using Activity Stream. Data about your specific browsing behaior or the sites you visit is **never transmitted to any Mozilla server**. At any time, it is easy to **turn off** this data collection by [opting out of Firefox telemetry](https://support.mozilla.org/kb/share-telemetry-data-mozilla-help-improve-firefox).

Data is sent to our servers in the form of discrete HTTPS 'pings' or messages whenever you do some action on the Activity Stream about:home, about:newtab or about:welcome pages. We try to minimize the amount and frequency of pings by batching them together.

At Mozilla, [we take your privacy very seriously](https://www.mozilla.org/privacy/). The Activity Stream page will never send any data that could personally identify you. We do not transmit what you are browsing, searches you perform or any private settings. Activity Stream does not set or send cookies, and uses [Transport Layer Security](https://en.wikipedia.org/wiki/Transport_Layer_Security) to securely transmit data to Mozilla servers.

The data collected in the Activity Stream is documented
(along with the other data collected in Firefox Desktop)
in the [Glean Dictionary](https://dictionary.telemetry.mozilla.org/apps/firefox_desktop).

Ping specifically collected on Firefox Home (New Tab) include:
* ["newtab"](https://dictionary.telemetry.mozilla.org/apps/firefox_desktop/pings/newtab)
* ["pocket-button"](https://dictionary.telemetry.mozilla.org/apps/firefox_desktop/pings/pocket-button)
* ["top-sites"](https://dictionary.telemetry.mozilla.org/apps/firefox_desktop/pings/top-sites)
* ["quick-suggest"](https://dictionary.telemetry.mozilla.org/apps/firefox_desktop/pings/quick-suggest)
* ["messaging-system"](https://dictionary.telemetry.mozilla.org/apps/firefox_desktop/pings/messaging-system)
* ["spoc"](https://dictionary.telemetry.mozilla.org/apps/firefox_desktop/pings/spoc)
