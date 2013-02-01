<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

Provides an HTTP server written in JavaScript for the Mozilla platform, which 
can be used in unit tests.

The most basic usage is:

    var { startServerAsync } = require("sdk/test/httpd");
    var srv = startServerAsync(port, basePath);
    require("sdk/system/unload").when(function cleanup() {
      srv.stop(function() { // you should continue execution from this point.
      })
    });

This starts a server in background (assuming you're running this code in an 
application that has an event loop, such as Firefox). The server listens at 
`http://localhost:port/` and serves files from the specified directory. You 
can serve static content or use
[SJS scripts](https://developer.mozilla.org/en/HTTP_server_for_unit_tests#SJS:_server-side_scripts),
as described in documentation 
on developer.mozilla.org.

You can also use `nsHttpServer` to start the server manually:

    var { nsHttpServer } = require("sdk/test/httpd");
    var srv = new nsHttpServer();
    // further documentation on developer.mozilla.org

See 
[HTTP server for unit tests](https://developer.mozilla.org/En/HTTP_server_for_unit_tests)
for general information.
