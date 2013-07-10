<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Cross-domain Content Scripts #

By default, content scripts don't have any cross-domain privileges.
In particular, they can't:

* [access content hosted in an `iframe`, if that content is served from a different domain](dev-guide/guides/content-scripts/cross-domain.html#Cross-domain iframes)
* [make cross-domain XMLHttpRequests](dev-guide/guides/content-scripts/cross-domain.html#Cross-domain XMLHttpRequest)

However, you can enable these features for specific domains
by adding them to your add-on's [package.json](dev-guide/package-spec.html)
under the `"cross-domain-content"` key, which itself lives under the
`"permissions"` key:

<pre>
"permissions": {
    "cross-domain-content": ["http://example.org/", "http://example.com/"]
}
</pre>

* The domains listed must include the scheme and fully qualified domain name,
and these must exactly match the domains serving the content - so in the
example above, the content script will not be allowed to access content
served from `https://example.com/`.
* Wildcards are not allowed.
* This feature is currently only available for content scripts, not for page
scripts included in HTML files shipped with your add-on.

## Cross-domain iframes ##

The following "main.js" creates a page-worker which loads a local HTML file
called "page.html", attaches a content script called "page.js" to the
page, waits for messages from the script, and logs the payload.

    //main.js
    var data = require("sdk/self").data;

    var pageWorker = require("sdk/page-worker").Page({
      contentURL: data.url("page.html"),
      contentScriptFile: data.url("page-script.js")
    });

    pageWorker.on("message", function(message) {
      console.log(message);
    });

The "page.html" file embeds an iframe whose content is
served from "http://en.m.wikipedia.org/":

<pre class="brush: html">
    &lt;!doctype html&gt;
    &lt;!-- page.html --&gt;
    &lt;html&gt;
      &lt;head>&lt;/head&gt;
      &lt;body&gt;
        &lt;iframe id="wikipedia" src="http://en.m.wikipedia.org/"&gt;&lt;/iframe&gt;
      &lt;/body&gt;
    &lt;/html&gt;
</pre>

The "page-script.js" file locates "Today's Featured Article" and sends its
content to "main.js":

    // page-script.js
    var iframe = window.document.getElementById("wikipedia");
    var todaysFeaturedArticle = iframe.contentWindow.document.getElementById("mp-tfa");
    self.postMessage(todaysFeaturedArticle.textContent);

For this to work, we need to add the `"cross-domain-content"` key to
"package.json":

<pre>
"permissions": {
  "cross-domain-content": ["http://en.m.wikipedia.org/"]
}
</pre>

The add-on should successfully retrieve the iframe's content.

## Cross-domain XMLHttpRequest ##

The following add-on creates a panel whose content is the summary weather
forecast for [Shetland](https://en.wikipedia.org/wiki/Shetland).
If you want to try it out, you'll need to
[register](http://www.metoffice.gov.uk/datapoint/support/API)
and get an API key.

The "main.js":

* creates a panel whose content is supplied by "panel.html" and
adds a content script "panel-script.js" to it
* sends the panel a "show" message when it is shown
* attaches the panel to a widget

<!-- terminate Markdown list -->

    // main.js
    var data = require("sdk/self").data;

    var forecast_panel = require("sdk/panel").Panel({
      height: 50,
      contentURL: data.url("panel.html"),
      contentScriptFile: data.url("panel-script.js")
    });

    forecast_panel.on("show", function(){
      forecast_panel.port.emit("show");
    });

    require("sdk/widget").Widget({
      id: "forecast",
      label: "Weather Forecast",
      contentURL: "http://www.metoffice.gov.uk/favicon.ico",
      panel: forecast_panel
    });

The "panel.html" just includes a `<div>` block for the forecast:

<pre class="brush: html">
&lt;!doctype HTML&gt;
&lt;!-- panel.html --&gt;

&lt;html&gt;
  &lt;head&gt;&lt;/head&gt;
  &lt;body&gt;
    &lt;div id="forecast_summary">&lt;/div&gt;
  &lt;/body&gt;
&lt;/html&gt;
</pre>

The "panel-script.js" uses [XMLHttpRequest](https://developer.mozilla.org/en-US/docs/DOM/XMLHttpRequest)
to fetch the latest forecast:

    // panel-script.js

    var url = "http://datapoint.metoffice.gov.uk/public/data/txt/wxfcs/regionalforecast/json/500?key=YOUR-API-KEY";

    self.port.on("show", function () {
      var request = new XMLHttpRequest();
      request.open("GET", url, true);
      request.onload = function () {
        var jsonResponse = JSON.parse(request.responseText);
        var summary = getSummary(jsonResponse);
        var element = document.getElementById("forecast_summary");
        element.textContent = summary;
      };
      request.send();
    });

    function getSummary(forecast) {
      return forecast.RegionalFcst.FcstPeriods.Period[0].Paragraph[0].$;
    }


Finally, we need to add the `"cross-domain-content"` key to "package.json":

<pre>
"permissions": {
  "cross-domain-content": ["http://datapoint.metoffice.gov.uk"]
}
</pre>

## Content Permissions and unsafeWindow ##

If you use `"cross-domain-content"`, then JavaScript values in content
scripts will not be available from pages. Suppose your content script includes
a line like:

    // content-script.js:
    unsafeWindow.myCustomAPI = function () {};

If you have included the `"cross-domain-content"` key, when the page script
tries to access `myCustomAPI` this will result in a "permission denied"
exception.
