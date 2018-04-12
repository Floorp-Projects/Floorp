# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

app-manager-window =
    .title = Application details
    .style = width: 30em; min-height: 20em;

app-manager-remove =
    .label = Remove
    .accesskey = R

app-manager-handle-webfeeds = The following applications can be used to handle Web Feeds.

# Variables:
#   $type (String) - the URI scheme of the link (e.g. mailto:)
app-manager-handle-protocol = The following applications can be used to handle { $type } links.

# Variables:
#   $type (String) - the MIME type (e.g. application/binary)
app-manager-handle-file = The following applications can be used to handle { $type } content.

## These strings are followed, on a new line,
## by the URL or path of the application.

app-manager-web-app-info = This web application is hosted at:
app-manager-local-app-info = This application is located at:
