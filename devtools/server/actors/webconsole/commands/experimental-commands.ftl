# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# These strings aren't translated and are meant to be used for experimental commands
# which may frequently update their documentations

# Usage string for :trace command
webconsole-commands-usage-trace3 =
  :trace

  Toggles the JavaScript tracer

    It supports the following arguments:
      --logMethod to be set to ‘console’ for logging to the web console (the default), or ‘stdout’ for logging to the standard output,
      --values Optional flag to be passed to log function call arguments as well as returned values (when returned frames are enabled).
      --on-next-interaction Optional flag, when set, the tracer will only start on next mousedown or keydown event.
      --max-depth Optional flag, will restrict logging trace to a given depth passed as argument.
      --max-records Optional flag, will automatically stop the tracer after having logged the passed amount of top level frames.
      --prefix Optional string which will be logged in front of all the trace logs,
      --help or --usage to show this message.
