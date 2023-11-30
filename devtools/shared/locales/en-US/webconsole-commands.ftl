# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# These strings are used inside the Web Console commands
# which can be executed in the Developer Tools, available in the
# Browser Tools sub-menu -> 'Web Developer Tools'

# Usage string for :block command
webconsole-commands-usage-block =
  :block URL_STRING

  Start blocking network requests

    It accepts only one URL_STRING argument, an unquoted string which will be used to block all requests whose URL includes this string.
    Use :unblock or the Network Monitor request blocking sidebar to undo this.

# Usage string for :unblock command
webconsole-commands-usage-unblock =
  :unblock URL_STRING

  Stop blocking network requests

    It accepts only one argument, the exact same string previously passed to :block.

# Usage string for :trace command
webconsole-commands-usage-trace =
  :trace

  Toggles the JavaScript tracer

    It supports the following arguments:
      --logMethod to be set to ‘console’ for logging to the web console (the default), or ‘stdout’ for logging to the standard output,
      --prefix Optional string which will be logged in front of all the trace logs,
      --help or --usage to show this message.
