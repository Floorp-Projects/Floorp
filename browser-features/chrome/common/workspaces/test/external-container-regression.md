# External URLs and workspace containers (#2823)

Choose the container **before** creating the content browser. `TabOpen` only
assigns the workspace ID; it cannot change the browsing context, principal,
Cookie jar or storage partition by changing `usercontextid` on an existing tab.

For real external URL opens:

- An existing window uses its selected workspace when Firefox has no host guess.
- A new window or cold launch uses the persisted default workspace before the
  chrome overlay initializes. Multiple command-line URLs use the same container.
- Firefox's host guesses, including a valid guess of **0**, retain precedence.
- Explicit container 0/N in normal link/tab APIs retains precedence.
- Forced default, disabled Workspaces, unavailable/deleted identities and
  private browsing do not introduce a workspace container.
- Loading into the current tab retains its existing browsing context/container.

The patch does not migrate cookies/storage or change identities of existing
tabs. The upstream special case for an empty external URL or `about:blank` still
invokes the normal new-tab command.

## Automated checks

Host policy tests execute the helper and guess expression from the actual
Runtime patch, and check that production and development patches agree:

```sh
deno test --frozen --allow-read tools/src/workspace_external_containers_patch.test.ts
```

Apply Runtime patches through the normal development/package path. For
development, `deno task feles-build dev` applies
`tools/patches/workspace-external-containers.patch`. Packaging applies the
matching common source patch and checks the installed patch; macOS also
synchronizes the patched modules into the packaged omnijar.

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
its own tabs, identities, cookies and storage.

The cold runner owns an isolated temporary profile and subprocesses. It tests
the actual command-line handler on first launch, restart with persisted
Cookie/storage jars, multiple URLs, forced default and `--private-window`. Keep
the development servers running if the tested Runtime uses the HTTP chrome
overlay. Both runners fail if either the Runtime patch or the Workspace getter
is missing; an unpatched Runtime is never silently skipped.

## Platform checks

Use a workspace with a container and open a URL from another application, first
with Floorp running and then with it closed. Check that the container identity
and the actual logged-in session agree. Repeat Windows
default-browser/ShellExecute and macOS LaunchServices handling before release;
the automated cold runner covers the command-line handler rather than
registering either OS integration.
