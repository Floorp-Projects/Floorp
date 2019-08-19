# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### These strings are used inside the Application panel which is available
### by setting the preference `devtools-application-enabled` to true.

### The correct localization of this file might be to keep it in English, or another
### language commonly spoken among web developers. You want to make that choice consistent
### across the developer tools. A good criteria is the language in which you'd find the
### best documentation on web development on the web.

# Header for the list of Service Workers displayed in the application panel for the current page.
serviceworker-list-header = Service Workers

# Text displayed next to the list of Service Workers to encourage users to check out
# about:debugging to see all registered Service Workers.
serviceworker-list-aboutdebugging = Open <a>about:debugging</a> for Service Workers from other domains

# Text for the button to unregister a Service Worker. Displayed for active Service Workers.
serviceworker-worker-unregister = Unregister

# Text for the debug link displayed for an already started Service Worker. Clicking on the
# link opens a new devtools toolbox for this service worker. The title attribute is only
# displayed when the link is disabled.
serviceworker-worker-debug = Debug
  .title = Only running service workers can be debugged

# Text for the debug link displayed for an already started Service Worker, when we
# are in multi e10s mode, which effectively disables this link.
serviceworker-worker-debug-forbidden = Debug
  .title = Can only debug service workers if multi e10s is disabled

# Text for the start link displayed for a registered but not running Service Worker.
# Clicking on the link will attempt to start the service worker.
serviceworker-worker-start2 = Start
  .title = Can only start service workers if multi e10s is disabled

# Text displayed for the updated time of the service worker. The <time> element will
# display the last update time of the service worker script.
serviceworker-worker-updated = Updated <time>{ DATETIME($date, month: "long", year: "numeric", day: "numeric", hour: "numeric", minute: "numeric", second: "numeric") }</time>

# Text displayed next to the URL for the source of the service worker (e-g. "Source my/path/to/worker-js")
serviceworker-worker-source = Source

# Text displayed next to the current status of the service worker.
serviceworker-worker-status = Status

## Service Worker status strings: all serviceworker-worker-status-* strings are also
## defined in aboutdebugging.properties and should be synchronized with them.

# Service Worker status. A running service worker is registered, currently executed, can
# be debugged and stopped.
serviceworker-worker-status-running = Running

# Service Worker status. A stopped service worker is registered but not currently active.
serviceworker-worker-status-stopped = Stopped

# Service Worker status. A registering service worker is not yet registered and cannot be
# started or debugged.
serviceworker-worker-status-registering = Registering

# Text displayed when no service workers are visible for the current page. Clicking on the
# link will open https://developer-mozilla-org/docs/Web/API/Service_Worker_API/Using_Service_Workers
serviceworker-empty-intro = You need to register a Service Worker to inspect it here. <a>Learn more</a>

# Text displayed when there are no Service Workers to display for the current page,
# introducing hints to debug Service Worker issues.
serviceworker-empty-suggestions = If the current page should have a service worker, here are some things you can try

# Suggestion to check for errors in the Console to investigate why a service worker is not
# registered. Clicking on the link opens the webconsole.
serviceworker-empty-suggestions-console = Look for errors in the Console. <a>Open the Console</a>

# Suggestion to use the debugger to investigate why a service worker is not registered.
# Clicking on the link will switch from the Application panel to the debugger.
serviceworker-empty-suggestions-debugger = Step through your Service Worker registration and look for exceptions. <a>Open the Debugger</a>

# Suggestion to go to about:debugging in order to see Service Workers for all domains.
# Clicking on the link will open about:debugging in a new tab.
serviceworker-empty-suggestions-aboutdebugging = Inspect Service Workers from other domains. <a>Open about:debugging</a>

# Text displayed while we are loading the manifest file
manifest-loading = Loading manifest…

# Text displayed when the manifest has been successfully loaded
manifest-loaded-ok = Manifest loaded.

# Text displayed when there has been an error while trying to load the manifest
manifest-loaded-error = There was an error while loading the manifest:

# Text displayed when the page has no manifest available
manifest-non-existing = No manifest found to inspect.
