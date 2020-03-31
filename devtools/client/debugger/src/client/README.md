# DevTools Client

The DevTools client is responsible for managing the communication between the
client application and JS server.

* When the server sends a notification to the client, the client receives an
  "event" and notifies the application via redux actions.
* When the application, wants to send a command to the server, it invokes
  "commands" in the client.

The Debugger supports a Firefox and a Chrome client, which lets it attach and
debug Firefox, Chrome, and Node contexts. The clients are defined in
`src/client` and have an `onConnect` function, and a `commands` and `events`
module.

Both clients implement client adapters for translating commands and events into
JSON packets. The chrome client debugger adapter is defined in
[chrome-remote-interface][chrome-remote-interface]. The Firefox client adapters
are defined in two places:

* The launchpad client adapter is maintained in the package
  [devtools-connection][dt-connect].
* The panel client adapter is maintained in
  [devtools-client.js][devtools-client.js].

## Firefox

### Remote Debugger Protocol

The [Remote Debugger Protocol][protocol] specifies the client / server API.

### Interrupt

When the client wants to add a breakpoint, it avoids race conditions by doing
temporary pauses called interrupts.

We want to do these interrupts transparently, so we've decided that the client
should not notify the application that the thread has been paused or resumed.

[protocol]: https://searchfox.org/mozilla-central/source/devtools/docs/backend/protocol.md
[dt-connect]: https://github.com/firefox-devtools/devtools-core/tree/master/packages/devtools-connection
[devtools-client.js]: https://searchfox.org/mozilla-central/source/devtools/client/devtools-client.js

## Chrome

### Chrome Debugger Protocol

The chrome debugger protocol is available [here][devtools-protocol-viewer]. And
is maintained in the devtools-protocol [repo][devtools-protocol-gh].

[chrome-remote-interface]: https://github.com/cyrus-and/chrome-remote-interface
[devtools-protocol-viewer]: https://chromedevtools.github.io/devtools-protocol/
[devtools-protocol-gh]: https://github.com/ChromeDevTools/devtools-protocol
