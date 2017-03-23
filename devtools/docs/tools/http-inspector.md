# HTTP Inspector (aka XHR Spy)
This document is intended as a description of HTTP Inspector feature allowing
inline inspection of HTTP logs displayed in the Console panel. The documents
focuses on internal architecture.

For detailed feature description see the following doc
(many screenshots included):
https://docs.google.com/document/d/1zQniwU_dkt-VX1qY1Vp-SWxEVA4uFcDCrtH03tGoHHM/edit#

_HTTP Inspector feature is available in the Console panel (for web developers)
as well as in the Browser Console (for devtools and extension developers)._

The current implementation is based on React (no XUL) and some of the existing
components should also be used when porting the Network panel to HTML.

The entire feature lives in `devtools/client/webconsole/net` directory.

## General Description
The entry point for HTTP log inspection is represented by an expand/toggle
button displayed in front a log in the Console panel:

[+] GET XHR http://example.com/test-request.php

Clicking on the [+] button expands the log and shows a body with HTTP details
right underneath. The info body is rendered by:
`devtools/client/webconsole/net/components/net-info-body` component.

HTTP info is divided into several tabs:

* Headers: send and received HTTP headers
* Params: URL parameters (query string)
* Post: HTTP post body
* Response: HTTP response body
* Cookies: Sent and received cookies

### Headers Tab
`devtools/client/webconsole/net/components/headers-tab`

This is the default active tab and it's responsible for rendering
HTTP headers. There are several header groups displayed:

* Response Headers
* Requests Headers
* Cached Headers (not implemented yet)

Individual sections are expandable/collapsible.

Rendering of the groups is done by `NetInfoGroup` and `NetInfoGroupList`
components.

### Params Tab
`devtools/client/webconsole/net/components/params-tab`

This tab is responsible for rendering URL parameters (query string)
and it's available only if the URL has any parameters. Individual
parameters are parsed and displayed as a list of name/value pairs.

Rendering of the parameter list is done by `NetInfoParams` component.

### Post Tab
`devtools/client/webconsole/net/components/post-tab`

This tab is responsible for rendering HTTP post body sent to the server.

### Response Tab
`devtools/client/webconsole/net/components/response-tab`

This tab is responsible for rendering HTTP response body received from
the server. There might be more than one section displaying the data
depending on the current response mime-type.

* Raw Data: This section is always displayed. It renders data in a raw
form just like they are received from the server.
* JSON: This section is available in case of JSON responses [1].
It parses the response and displays it as an expandable tree.
* Image: This section is available in case of image responses [2].
The response is decoded and displayed as an image.
* XML: this section is available in case of HTML/XML responses [3]
The response is parsed using DOM parser and displayed as an XML markup.

[1] List of JSON mime-types: `devtools/client/webconsole/net/utils/json`
[2] List of Image mime-types: `devtools/client/webconsole/net/utils/json`
[3] List of XML/HTML mime-types: `devtools/client/webconsole/net/utils/net`

Response data are fetched using `LongStringClient`, so if data are bigger
than defined limit (see `devtools/server/main.js - LONG_STRING_LENGTH)
the user needs to manually require the rest (there is a link at the end
of incomplete response body that allows this).

The raw section is collapsed by default if there is another presentation
of the data.

### Cookies Tab
`devtools/client/webconsole/net/components/cookies-tab`

This tab is responsible for displaying HTTP cookies.
There are two groups:

* Request Cookies
* Response Cookies

Rendering of the groups is done by `NetInfoGroup` and `NetInfoGroupList`
components. The tab is not presented if there are no cookies.

## Architecture
This sections describes internal architecture of HTTPi feature.

### Main
`devtools/client/webconsole/net/main`

This is the main module of HTTPi. It represents the root module
of the feature.

The main responsibility of the module is handling network logs forwarded
from webconsole.js. This modules creates one instance of `NetRequest`
object for every `NetworkEvent` (one object for every HTTP request).

### NetRequest
`devtools/client/webconsole/net/net-request`

This module represents `NetRequest` object. It's the internal representation
of HTTP request and it keeps its state. All HTTP details fetched dynamically
from the backend are stored in this object.

This object is responsible for:
* Adding a toggle button in Console UI (displayed in front of HTTP requests)
* Listening for a click event on the toggle button.
* Sending messages to web console client object to request HTTP details.
* Refreshing the UI as HTTP details are coming from the overlay.

Note that `NetRequest` is using a small helper object `DataProvider` for
requesting HTTP details. `DataProvider` is the connection between `NetRequest`
and the backend.

### Data Provider
`devtools/client/webconsole/net/data-provider`

This module is using webconsole client object to get data from the backend.

### Utils
`devtools/client/webconsole/net/utils`

There are also some utility modules implementing helper functions.
The important thing is that these modules doesn't require any chrome
privileges and are ready to run inside content scope.

### Components
* `NetInfoBody` Renders the entire HTTP details body displayed when the
  user expands a network log.
* `NetInfoGroup` Renders a group (a section within tab). For example,
  Request Headers section in Headers tab corresponds to one group.
* `NetInfoGroupList` List of groups. There might be more groups of data
  within one tab. For example, the Headers tab has Requested and Response
  headers groups.
* `NetInfoParams` List of name-value pairs. It's used e.g. by the Headers
  or Params tab.
* `HeadersTab` Displays HTTP headers.
* `PostTab` Displays HTTP posted body data.
* `ParamsTab` Displays URL query string.
* `ResponseTab` Displays HTTP response body data.
* `CookiesTab` Displays cookies.
* `Spinner` Represents a throbber displayed when the UI is waiting for
  incoming data.
* `SizeLimit` Represents a link that can be used to fetch the
  rest of data from the backend (debugger server). Used for HTTP post
  and response bodies.
* `XmlView` Renders XML markup.
