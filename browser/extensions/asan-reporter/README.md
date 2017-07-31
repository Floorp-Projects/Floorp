# firefox-asan-reporter

The ASan reporter addon for Firefox is an internal addon used in conjunction
with special ASan (AddressSanitizer) builds of Firefox Nightly. Its main purpose
is to scan for ASan crash report files on startup and submit them back to our
crash handling infrastructure. Due to the way AddressSanitizer produces its
crash information, it would be challenging (if possible at all), to get it
working with our regular crash reporter.

The addon is only enabled in builds with the --enable-address-sanitizer-reporter
flag set at build time. Currently, we don't produce such builds, but this might
change once we decide to hand out Firefox+ASan Nightly builds as part of a
special opt-in program.

## Prefs

### asanreporter.apiurl

The URL to the server receiving the crash information.

### asanreporter.clientid

The client id to send along with the crash information. By default, this is
empty. If the user wishes, they can set this pref to e.g. an email address to
allow us to contact them.

### asanreporter.authtoken

This is an authorization token that can be used in test setups with a non-public
API endpoint that requires authentication. In the final setup, this pref remains
empty.

### asanreporter.loglevel

This optional variable can be used to change the default logging level. The
reporter addon uses Log.jsm which defines the following values for different
levels of logging:

| Level Name | Value |
| ---------- | ----- |
| ALL        | 0     |
| TRACE      | 10    |
| DEBUG      | 20    |
| CONFIG     | 30    |
| INFO       | 40    |
| WARN       | 50    |
| ERROR      | 60    |
| FATAL      | 70    |

The default logging level is INFO. All log messages are emitted to the browser
console and stdout. Switching the logging level to DEBUG causes additional
debug messages related to server requests (XHR) to be emitted.
