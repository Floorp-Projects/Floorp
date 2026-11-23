# External URLs and workspace containers (#2823)

Choose the container **before** creating the content browser. `TabOpen` only
assigns the workspace ID; it cannot change the browsing context, principal,
Cookie jar or storage partition by changing `usercontextid` on an existing tab.

For real external URL opens:

- An existing window uses its selected workspace when Firefox has no host guess.
- A container-capable build waits for the new window's workspace controller and
  local container mappings. Cold launches and new windows retain external
  requests on owned `about:blank` placeholders until
  `floorp-workspace-container-ready`; the site is not requested before then.
  Builds without `floorp.workspaces.containerSync.enabled` retain the older
  persisted default-workspace fallback.
- Firefox's host guesses, including a valid guess of **0**, retain precedence.
- Explicit container 0/N in normal link/tab APIs retains precedence.
- Forced default, disabled Workspaces and private browsing do not introduce a
  workspace container. An unresolved/deleted synced default stays pending until
  local setup resolves it; an explicit resolved choice of none remains 0.
- Loading into the current tab retains its existing browsing context/container.
- Modifier-clicked bookmarks select their workspace container before creating
  the content browser, including the mirrored bookmark toolbar. Private windows,
  disabled Workspaces and explicit none use container 0. An unresolved default
  prevents the new-tab command and stops propagation to another bookmark
  handler. Existing-tab navigation remains available.

The patch does not migrate cookies/storage or change identities of existing
tabs. The upstream special case for an empty external URL or `about:blank` still
invokes the normal new-tab command.

A deferred external request keeps its URI, triggering principal, policy and
POST data in the local window. Its placeholder displays the target URI. Closing,
typing into or navigating the placeholder cancels the request and preserves any
user navigation. Switching workspaces delays replay until the placeholder's
workspace is selected again. A browser-API exception after replay starts does
not automatically resubmit a possibly consumed POST stream. Before delayed
startup the queue observes actual window closure, rather than initial-document
unload, so replacing the startup `about:blank` document cannot discard requests.

## Automated checks

Host policy tests execute the helper and guess expression from the actual
Runtime patch, and check that production and development patches agree:

```sh
deno test --frozen --allow-read tools/src/workspace_external_containers_patch.test.ts
deno test --frozen --allow-read tools/src/workspace_external_containers_cleanup.test.ts
deno test --frozen --allow-read tools/src/bookmark_workspace_containers.test.ts
deno task test -- --near browser-features/chrome/common/ui-custom/test/bookmarkWorkspaceContainer.test.ts
```

Apply Runtime patches through the normal development/package path. For
development, `deno task feles-build dev` applies
`tools/patches/workspace-external-containers.patch`. Packaging applies the
matching common source patch and checks the installed patch; macOS also
synchronizes the patched modules into the packaged omnijar.
On Firefox 159, the extracted BrowserContentHandler module lives at
`moz-src/browser/components/BrowserContentHandler.sys.mjs`; BrowserDOMWindow stays
at `browser/modules/BrowserDOMWindow.sys.mjs`. The lazy getter and hot runner
import the helper through
`moz-src:///browser/components/BrowserContentHandler.sys.mjs`, matching the native
component registration. Source-to-artifact path mapping changes patch headers
only, so it does not rewrite this module URI.

With that development browser running on an exclusive test display:

```sh
deno run --frozen -A tools/os-test/verify_workspace_external_containers.ts --port 2828
deno run --frozen -A tools/os-test/verify_workspace_external_containers.ts \
  --binary /absolute/path/to/patched/floorp-bin
```

The hot runner creates two temporary public identities and a private HTTP
fixture, seeds three jars at the same origin, and checks normal new tabs,
external new/adjacent tabs, explicit 0/N, host guesses 0/N, forced default,
disabled Workspaces, existing container preservation, new windows and private
windows. It compares tab/browser attributes, browsing-context and principal
origin attributes, workspace assignment, the server's received Cookie, document
cookies and localStorage. It restores the workspace preference and removes only
its own tabs, identities, cookies and storage. Profile restoration runs before
content cleanup; failures in cleanup still attempt closing the cleanup tab and
the Marionette client.

The cold runner owns an isolated temporary profile and subprocesses. It tests
the actual command-line handler on first launch, restart with persisted
Cookie/storage jars, multiple URLs, forced default and `--private-window`. Keep
the development servers running if the tested Runtime uses the HTTP chrome
overlay. Both runners fail if either the Runtime patch or the Workspace getter
is missing; an unpatched Runtime is never silently skipped.

For the pending-default cases, set the window's `canOpenWorkspaceTab` guard to
false in an isolated fixture and use a local HTTP request counter. Exercise
native `browserDOMWindow.openURI`, cold command-line launch and external new
windows: no request may reach the server before readiness. After resolving the
guard and emitting `floorp-workspace-container-ready`, assert the final
`browsingContext.originAttributes.userContextId`, a single request and removal of
only the owned blank. Repeat with Firefox host guesses 0/N, a navigated or closed
placeholder, and startup document replacement. These native checks supplement
the host queue tests and the production-controller Sync acceptance tests.

## Platform checks

Use a workspace with a container and open a URL from another application, first
with Floorp running and then with it closed. Check that the container identity
and the actual logged-in session agree. Repeat Windows
default-browser/ShellExecute and macOS LaunchServices handling before release;
the automated cold runner covers the command-line handler rather than
registering either OS integration.
