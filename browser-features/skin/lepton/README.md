# Vendored Lepton

`LEPTON`, `css/`, `icons/` and `userjs/` are copies of one release of upstream
[Lepton](https://github.com/black7375/Firefox-UI-Fix). `UPSTREAM.json` records
that release (repository, tag, commit, publication date, SHA-256 of the release
zips) and the SHA-256 of every vendored file.

**The vendored files must stay byte-identical to the recorded release.** Do not
edit them, not even to fix a typo or a Gecko incompatibility. Floorp changes go
to `overlay/` (below). `tools/src/lepton_vendor.test.ts` (`deno task test:host`)
fails when a vendored file differs from `UPSTREAM.json`, is missing, or a new
file is added next to them. `.gitattributes` marks these paths `-text`, so git
keeps upstream's bytes (including upstream's line endings) on every platform.

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
assets, replaces the vendored files, rewrites `UPSTREAM.json` and the lepton
entries of `../jar.mn`, and opens or updates a pull request from
`bot/update-lepton`. It never pushes to `main`, and does nothing when the
vendored files already match the release.

The bot force-pushes `bot/update-lepton` on each new update, so do not push
fixes to that branch. Review the bot PR with the design tests
(`deno task test -- --near browser-features/chrome/common/designs`) and a look
at the Lepton, Photon and ProtonFix designs; make any Floorp adjustment in
`overlay/` (or the compat layers below) in a separate PR.

To run the same update locally (for example to try a release before the bot):

```sh
TAG=v8.7.6
W=$(mktemp -d)
gh api repos/black7375/Firefox-UI-Fix/releases/tags/$TAG > "$W/release.json"
gh api repos/black7375/Firefox-UI-Fix/git/ref/tags/$TAG --jq .object.sha  # commit
gh release download $TAG --repo black7375/Firefox-UI-Fix --dir "$W/zips" \
  -p Lepton.zip -p Lepton-Photon-Style.zip -p Lepton-Proton-Style.zip
for z in Lepton Lepton-Photon-Style Lepton-Proton-Style; do
  unzip -q "$W/zips/$z.zip" -d "$W/extracted/$z"
done
deno run --allow-read --allow-write tools/src/lepton_vendor.ts apply \
  --release-json "$W/release.json" --commit <commit> \
  --zips-dir "$W/zips" --extracted-dir "$W/extracted"
```

## Where Floorp changes go

- `overlay/leptonChromeOverlay.css`: loaded right after `css/leptonChrome.css`
  as a chrome document sheet, for lepton, photon and protonfix.
- `overlay/leptonContentOverlay.css`: loaded right after every load of
  `css/leptonContent.css` at the same level: as a chrome document sheet for
  lepton, photon and protonfix, and also as an `AGENT_SHEET` (every document,
  web pages included) for photon and protonfix.

Both are wired in `browser-features/chrome/common/designs/utils/css.ts` and
pinned by `browser-features/chrome/common/designs/test/lepton-overlay.test.ts`.
Relative icon URLs (`../icons/...`) resolve to the vendored icons as they do
from `css/`.

The existing TypeScript compat layers in `designs/utils/`
(`lepton-compat-152.css.ts`, the nav-bar rules in `css.ts`) load after the
overlays and stay where they are. Pref-gated Lepton options are configured
through `userjs/` and the design settings, not by editing the vendored CSS.
