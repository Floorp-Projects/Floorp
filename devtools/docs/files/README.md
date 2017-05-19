# Directories Overview

This page provides a very top level overview of what is on each directory in the DevTools source code:

* `devtools/shared`: Code shared by both the client (front-end UI) and server. If we are using any third party libraries, or importing external repositories into our tree, those libraries generally live here (eg, `devtools/shared/acorn`), assuming they are used by both client and server.
  * `devtools/shared/client`: Code for the [Remote Debugging Protocol](../backend/protocol.md) (RDP) client. You may wonder why this is not in `devtools/client` below: it's mainly because tests in server also need access to the RDP client.
  * `devtools/shared/locales`: Strings used in either the server only, or shared with both the client and server.
* `devtools/server`: Code for the [RDP](../backend/protocol.md) server and transport layer.
  * `devtools/server/actors`: [RDP Actors](../backend/protocol.md#actors). Note that if you're modifying actors, you may need to worry about [backwards compatibility](../backend/backward-compatibility.md) with older clients.
* `devtools/client`: Code for the front-end side of our tools. In theory, each directory corresponds to a panel, but this is not always the case. This directory is only shipped with desktop Firefox, as opposed to other directories above, which are shipped with all Gecko products (Firefox for Android, etc.)
  * `devtools/client/locales`: Strings used in the client front-end.
  * `devtools/client/themes`: CSS and images used in the client front-end.

