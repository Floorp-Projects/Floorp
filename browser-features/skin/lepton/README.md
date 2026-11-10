# Vendored Lepton

`LEPTON`, `css/`, `icons/` and `userjs/` are copies of one release of upstream
[Lepton](https://github.com/black7375/Firefox-UI-Fix). `UPSTREAM.json` records
that release (repository, tag, commit, publication date, SHA-256 of the release
zips) and the SHA-256 of every vendored file.

**The vendored files must stay byte-identical to the recorded release.** Do not
edit them, not even to fix a typo or a Gecko incompatibility. Floorp changes go
to `overlay/` (below). `tools/src/lepton_vendor.test.ts` (`deno task test:host`)
fails when a vendored file differs from `UPSTREAM.json`, is missing, is a
symlink or has an unsafe name, or a new file is added next to them. The only
other entries allowed directly in this directory are the Floorp-owned
`README.md`, `UPSTREAM.json` and `overlay/`. `.gitattributes` marks the vendored
paths `-text`, so git keeps upstream's bytes (including upstream's line endings)
on every platform.

| Vendored path         | Source in the release             |
| --------------------- | --------------------------------- |
| `LEPTON`              | `Lepton.zip:chrome/LEPTON`        |
| `css/`                | `Lepton.zip:chrome/css/`          |
| `icons/`              | `Lepton.zip:chrome/icons/`        |
| `userjs/lepton.js`    | `Lepton.zip:user.js`              |
| `userjs/photon.js`    | `Lepton-Photon-Style.zip:user.js` |
| `userjs/protonfix.js` | `Lepton-Proton-Style.zip:user.js` |

Floorp uses the same `css/` and `icons/` for the lepton, photon and protonfix
designs, so an update stops if the three zips ship different `chrome/css/` or
`chrome/icons/`.

## Updating

`.github/workflows/update_lepton.yml` runs daily (and on demand with an optional
`tag` input; empty means the latest release). It downloads the three release
zips, checks them against the SHA-256 digests GitHub publishes for the release
assets, refuses zips with unsafe names (each path segment must match
`[A-Za-z0-9_+-][A-Za-z0-9._+-]*`, so no `.git`, dotfiles or control characters),
symlinks, special files or unknown top-level entries, replaces the vendored
files, rewrites `UPSTREAM.json` and the lepton entries of `../jar.mn`, and opens
or updates a pull request from `bot/update-lepton`. It never pushes to `main`.
It does nothing when the vendored files already match the release, does not push
when `bot/update-lepton` already holds the same vendored files, and on scheduled
runs skips a release whose bot PR was closed without merging. To propose such a
release again, run the workflow manually with that `tag`.

CI on the bot PR: pushes and PRs made with `GITHUB_TOKEN` do not trigger
workflows, so the bot dispatches `colocated_runner_test.yml` on
`bot/update-lepton` after each push (this reports the required `smoke` and
`Native Runtime / macos-latest` checks on the PR head). The required
`review-gate` check runs only on `pull_request` events: a maintainer closes and
reopens the PR to start it (which also reruns the other checks), then reviewers
post receipts as for any PR. If `main` has moved, use "Update branch" on the PR.

Do not push commits to `bot/update-lepton`: the next update force-pushes it.
Review the bot PR with the design tests
(`deno task test -- --near browser-features/chrome/common/designs`) and a look
at the Lepton, Photon and ProtonFix designs; make any Floorp adjustment in
`overlay/` (or the compat layers below) in a separate PR.

To run the same update locally (for example to try a release before the bot):

```sh
TAG=v8.7.6
R=black7375/Firefox-UI-Fix
W=$(mktemp -d)
gh api "repos/$R/releases/tags/$TAG" > "$W/release.json"
ref=$(gh api "repos/$R/git/ref/tags/$TAG")
COMMIT=$(jq -r .object.sha <<< "$ref")
if [ "$(jq -r .object.type <<< "$ref")" = tag ]; then # annotated tag
  COMMIT=$(gh api "repos/$R/git/tags/$COMMIT" --jq .object.sha)
fi
gh release download "$TAG" --repo "$R" --dir "$W/zips" \
  -p Lepton.zip -p Lepton-Photon-Style.zip -p Lepton-Proton-Style.zip
for z in Lepton Lepton-Photon-Style Lepton-Proton-Style; do
  unzip -q "$W/zips/$z.zip" -d "$W/extracted/$z"
done
deno run --allow-read --allow-write tools/src/lepton_vendor.ts apply \
  --release-json "$W/release.json" --commit "$COMMIT" \
  --zips-dir "$W/zips" --extracted-dir "$W/extracted"
```

## Where Floorp changes go

- `overlay/leptonChromeOverlay.css`: loaded right after `css/leptonChrome.css`
  as a chrome document sheet, for lepton, photon and protonfix.
- `overlay/leptonContentOverlay.css`: loaded right after every load of
  `css/leptonContent.css`, at the same level. In production that is a chrome
  document sheet for lepton, photon and protonfix, plus an `AGENT_SHEET` (every
  document, web pages included) for photon and protonfix. In dev builds lepton
  loads it as a chrome document sheet only, and photon and protonfix load it as
  an `AGENT_SHEET` only (dev does not load `leptonContent.css` into the chrome
  document for them).

Both are wired in `browser-features/chrome/common/designs/utils/css.ts` and
pinned by `browser-features/chrome/common/designs/test/lepton-overlay.test.ts`.
Relative icon URLs (`../icons/...`) resolve to the vendored icons as they do
from `css/`.

Writing overlay CSS:

- Repeat Lepton's `@namespace` lines (`xul` and `html` prefixes, same URLs) at
  the top of the overlay when you use those prefixes, and do not declare a
  default namespace. Production loads each sheet separately, so the overlay
  needs its own declarations; dev builds concatenate the chrome sheets into one
  `<style>`, where the overlay's `@namespace` lines come after rules and are
  ignored, and Lepton's declarations apply instead. Matching prefixes behave the
  same in both.
- Do not use `@import`: in the dev `<style>` it comes after other rules and is
  dropped.
- Scope rules in the content overlay with `@-moz-document` like
  `leptonContent.css` does, since as an `AGENT_SHEET` it applies to web pages.

The existing TypeScript compat layers in `designs/utils/`
(`lepton-compat-152.css.ts`, the nav-bar rules in `css.ts`) load after the
overlays and stay where they are. Pref-gated Lepton options are configured
through `userjs/` and the design settings, not by editing the vendored CSS.
