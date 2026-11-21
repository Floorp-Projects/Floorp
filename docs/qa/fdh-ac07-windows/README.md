# FDH v1.0 AC-07 on Windows

On 2026-11-21, the full `dev-tool` launch, browser inspection, KAN-135 port
isolation, and checkout-scoped shutdown passed on Windows Server 2022 Standard
x64 (build 20348). The run used `main` at
`6f621741aeabe173572d8a5abb4618eb81997ea5`, which includes
[KAN-135 (#3167)](https://github.com/Floorp-Projects/Floorp/pull/3167).

See [manifest.json](manifest.json) for the runtime identity, check results, and
screenshot hashes.

## Setup and launch

Fetched `origin/main` into the VM's existing clone. With Deno 2.9.6,
`deno install` exited 0 and warned that it ignored the `@swc/core` install
script. The runtime-lock command confirmed the snapshot already had the locked
Windows runtime (160.0.1, build 20261112140843).

The launch selected the lock-pinned release runtime and used the repository's
`prepareRuntimeBootstrap` helper to apply the BrowserGlue startup hook in
`_dist/bin/floorp` before skipping the full source patcher. The dev build
compiled features, injected XHTML, started the Vite servers, and launched
`floorp.exe` with the checkout's test profile. The staged fallback was unused.

```powershell
deno install
deno run -A tools/runtime-lock/runtime_lock_cli.ts install-native
$env:FLOORP_DEV_RUNTIME_DISTRIBUTION = 'release'
$env:FLOORP_SKIP_PATCHER = '1'
deno eval 'import { prepareRuntimeBootstrap } from ''./tools/src/prepared_runtime_bootstrap.ts''; console.log(await prepareRuntimeBootstrap(''_dist/bin/floorp''));'
deno task dev-tool start --port-offset 100
```

## Browser operation

All browser commands below exited 0. The content and chrome screenshots were
captured through Marionette in the Windows run.

| Command                                          | Observation                                                                                                                    |
| ------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------ |
| `dev-tool status`                                | Connected to Marionette on 5280. One tab titled **Floorp Hub**.                                                                |
| `dev-tool eval … -c chrome`                      | Live prefs reported startup mode `dev`, offset 100, OS-Server port 5275, and Marionette port 5280.                             |
| `dev-tool eval … -c content`                     | The Floorp Hub DOM contained the welcome heading, Tab & Appearance, and Workspaces at `http://localhost:5283/#/overview/home`. |
| `dev-tool screenshot -c content`                 | [Floorp Hub](floorp-settings.png), 1221 × 1350.                                                                                |
| `dev-tool navigate about:preferences -c content` | Returned the title **Settings**. Content eval confirmed `about:preferences` and status showed Settings as the active tab.      |
| `dev-tool screenshot -c chrome`                  | [Browser preferences](browser-preferences.png), 1280 × 1040, including the browser chrome.                                     |
| `dev-tool console --level error --limit 25`      | Returned one message about Quirks Mode in `resource://gfxsanity/sanitytest.html`.                                              |

## Port isolation and shutdown

A loopback HTTP listener simulated the installed-browser port. Before launch,
while the dev browser was running, and after `dev-tool stop`, the listener on
`127.0.0.1:58261` belonged to the same PID (1808) and returned
`ac07-installed-port-sentinel`.

- The dev log announced `[Floorp OS-Server] listening on 127.0.0.1:5275` during
  automatic startup. Windows `netstat` attributed both 5275 (5175 + 100) and
  5280 (Marionette) to `floorp.exe` PID 9456. The Vite Settings server listened
  on 5283, and the feature loader listened on 5281.
- `deno task dev-tool stop` exited 0 and printed `Stopped (41 processes).`
  Afterward, there were no listeners in the shifted launch range, the three
  `dev-tool` state files were gone, and none of the 42 snapshotted launcher,
  Vite, browser, and helper process identities remained. The independent check
  matched on PID and creation time and found zero processes from the checkout's
  Floorp runtime. The sentinel was torn down separately.

The runtime lacked the Windows private-profile token bridge. The launch log
recorded `Failed to publish server token file`. An unauthenticated `GET /health`
on 5275 connected but curl returned exit 52 (empty reply). Authenticated OS API
health and token advertisement remain unverified on this runtime. The KAN-135
port result comes from the automatic startup log, live preference, Windows
listener ownership, and the independent 58261 sentinel.
