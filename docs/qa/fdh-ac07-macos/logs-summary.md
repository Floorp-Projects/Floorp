# macOS AC-07 logs summary

Selected lines from the local commands below omit ANSI escapes and unrelated
startup output.

## Dev attempts with the stock Runtime layout

```text
[initializer] WARN: Runtime index fetch failed (attempt 1/5): HTTP 403 while fetching runtime index. Retrying in 1000ms...
[feles-build] ERROR: Unhandled error: HTTP 403 while fetching runtime index
[dev-tool] Dev server exited before Marionette was ready.
```

```text
[initializer] SUCCESS: Installed locked Runtime 160.0.1/20261112140843.
[builder] SUCCESS: Build complete.
[injector] SUCCESS: Manifest injected successfully.
[feles-build] ERROR: Unhandled error: Failed to inject XHTML: error: Uncaught (in promise) Error: ENOENT: no such file or directory, open '/Users/devin/repos/Floorp-ac07/_dist/bin/floorp/Floorp.app/Contents/Resources/browser/chrome/browser/content/browser/browser.xhtml'
```

Each `fdh verify browser` exited 1 with
`[dev-tool] Marionette port not found at /Users/devin/repos/Floorp-ac07/_dist/marionette-port.txt`.
Each `fdh dev down` reported `Stopped (0 processes).`

## Runtime preparation and staging

The root archive contained 2,684 entries including BrowserGlue. The browser
archive contained 5,372 entries including both XHTML files. The bootstrap helper
reported `status: applied`.

```text
[builder] SUCCESS: Build complete.
[injector] SUCCESS: XHTML injection complete.
[feles-build] SUCCESS: Dev servers are ready.
Marionette INFO Listening on port 5280
Connected to browser.
  Title: Floorp Start
  Active tab: Floorp Start
Navigated to chrome://noraneko-settings/content/index.html
Title: Floorp Hub
Navigated to about:preferences
Title: Settings
```

Chrome eval reported `offset=100`, `osServerPort=5275`, and `httpLoader=false`.
`lsof` showed Floorp PID 4474 listening on `127.0.0.1:5275` and
`127.0.0.1:5280`. There was no listener on `58261`. An unauthenticated
`GET /health` on `5275` received an empty reply. With the profile bearer, it
returned `HTTP 200 {"status":"ok"}`.

The console returned eight messages, including a forced-layout warning for the
profile-manager page, `NRProgressiveWebApp` actor remote-type errors, and
`SyntaxError: import declarations may only appear at top level of a module` from
`about-preferences.js`. The native Settings page still rendered.

## Full dev launch with the prepared Runtime

```text
[dev-tool] Using port-offset=100 marionette=5280 os-server=http://127.0.0.1:5275 pref=nora.dev.port_offset
Browser ready! Marionette on port 5280.
Connected to browser.
  Title: Floorp Hub
  URL: chrome://browser/content/browser.xhtml
  Tabs: 1
  Active tab: Floorp Hub
{"brand":"Floorp","build":"20261112140843","offset":100,"osServerPort":5275,"httpLoader":true,"startup":"dev","loader":"loaded","error":"","active":"http://localhost:5283/#/overview/home"}
clicked Workspaces
{"path":"/#/features/workspaces","main":"Workspaces\n\nWorkspaces greatly simplify the process of grouping tabs, making it easier to organize and manage your tabs.\n\nBasic Settings\nEnable workspace feature…"}
```

`lsof` showed Floorp PID 6287 listening on `127.0.0.1:5275` and
`127.0.0.1:5280`, and Deno PID 6070 serving Hub on `127.0.0.1:5283`.

```text
Profile bearer file mode: 0o600
TCP 127.0.0.1:58261: connection refused
GET 127.0.0.1:5275/health (profile bearer) -> 200 {"status":"ok"}
health probe exit=0
```

`dev-tool console --limit 25` returned 12 messages. These included missing
`chrome://noraneko-*` page registrations, `LinkHandlerParent.sys.mjs` tab
`TypeError` messages, a large command-palette preference warning, and a Normandy
configuration warning. The Hub and Workspaces rendered from the HTTP dev origin.

## Teardown

```text
# Staging
Stopping dev processes for this checkout...
Stopped (29 processes).
Surviving PID/start-time identities: []
Processes running from this checkout's bundle: []
Remaining dev control files: []
No listeners in TCP 5273–5292 or on 58261.

# Prepared dev
[fdh] deno run -A tools/dev-tool.ts stop
Stopping dev processes for this checkout...
Stopped (31 processes).
Surviving PID/start-time identities: []
Processes running from this checkout's bundle: []
Remaining dev control files: []
No listeners in TCP 5273–5292 or on 58261.
```
