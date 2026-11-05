# KAN-49: actual full-package baseline / PR3032 comparison

**PR3032 gives both PWA windows their own generated launcher identity on X11
and native Wayland. The ordinary browser's Wayland identity still inherits the
first cold-started PWA in both baseline and candidate. KAN-49 is therefore a
validated partial fix, not complete resolution of every reported grouping case.**

## Authenticated full products

| Subject | Exact source | Package workflow run | Uploaded installer artifact |
| --- | --- | --- | --- |
| Baseline | `984509d2053be7bae9027133bda7259cd706b208` | [42068377281](https://github.com/Floorp-Projects/Floorp/actions/runs/42068377281) | `13435702404` |
| Draft PR3032 | `8249e234adfbb3e877e4a20cf5f57d7264d9a447` | [42068055038](https://github.com/Floorp-Projects/Floorp/actions/runs/42068055038) | `13436721142` |

Both are completed/success unsigned `package.yml` stable-v2-validation runs.
The package agent/root authenticated each exact source/run, artifact ownership,
name, nonexpiry, size, API digest and downloaded ZIP digest, safely extracted the
single full package, and recorded complete product inventories. The runner
rechecked that inventory before and after every test.

- Baseline artifact ZIP SHA256:
  `83667970ddcfc2d3f350367f511106ddaf6f52c5ca8ae3069a44870aee22913c`.
- PR3032 artifact ZIP SHA256:
  `fc338fa4cd3307c7a8937b2b99c97c18a1665e23b3b561680662fddfc3b336e5`.

These products came directly from the full production application packaging
workflow. No JavaScript, manifest, omni.ja or product file was substituted.
They package the application source with the authenticated prebuilt native
Runtime; they are not fresh all-Gecko native compilations or signed releases.

## Actual execution

Debian13 cloud host; owned Xvfb X11 and separate owned nested KWin6.3.6 native
Wayland, each in a dedicated normal session bus. Every source/platform pair used
a **fresh output root, HOME and profile**. Each package installed its own Alpha
and Beta fixtures through the actual URL-bar and menu UI, generated its own
desktop entries/icons/store, normally quit, then cold-started Alpha → Beta →
ordinary browser using those exact generated launcher Exec arguments.
All input was synthetic; no physical-input claim is made.

| Full package / environment | Alpha native identity | Beta native identity | Ordinary browser native identity |
| --- | --- | --- | --- |
| Baseline / X11 | shared `floorp` — launcher mismatch | shared `floorp` — launcher mismatch | `floorp` |
| PR3032 / X11 | own generated Alpha class | own generated Beta class | `floorp` |
| Baseline / Wayland | own Alpha app_id | **inherits Alpha app_id** — launcher mismatch | **inherits Alpha app_id** |
| PR3032 / Wayland | own Alpha app_id | own Beta app_id | **still inherits Alpha app_id** |

X11 uses own-PID `WM_CLASS` observations. Native Wayland uses actual protocol
`xdg_toplevel.set_app_id` calls plus KWin resourceClass/desktopFileName. The
ordinary browser is independently identified by its about:blank URL, null
windowclass override, final `Ablaze Floorp` title, matching protocol toplevel and
KWin row. The browser-first UI control advertises normal `floorp`, establishing
the expected default identity without assuming a fix.

Thus the original first-PWA identity inheritance pattern is directly reproduced
in baseline native Wayland, and Beta's identity is corrected by PR3032. The
remaining ordinary-browser behavior is demonstrably preexisting. No speculative
additional fix or broadened PR scope was introduced.

## Protection, legacy records and cleanup

Across all four full-package runs:

- **8 actual UI installations** succeeded, with their own real generated launchers.
- **36 actual legacy/malformed window API calls** opened successfully (9 per run).
- Content sandbox level **6** and Linux taskbar experiment **false** remained
  unchanged in UI, cold-start and legacy phases; no enrollment was forced.
- **12 normal in-app quits**, all `cause:shutdown`, `forced:false`.
- Complete product inventories remained identical, and owned browser process
  counts were zero after each run. Owned display/compositor processes were also
  terminated; the separate branding test display was not touched.

Baseline's existing launcher normalizer throws on numeric/object ids while its
window API still opens them. PR3032's final string-candidate guard avoids moving
that TypeError into startup: both helper and window API use the usable name
fallback. Missing, brace-only, punctuation-only, uppercase and missing-name cases
preserve the existing fallback order. Wholly invalid records still produce
independent random fallback tokens; equality across independent calls, migration
or record repair is not claimed.

## Measured effective Runtime versus unused root INI

The products' root INI files explicitly state that they are not used. Their
packaging-time values are preserved as artifacts, not rewritten to satisfy the
Runtime lock:

| Subject | Unused root INI Version / BuildID | Actual `Services.appinfo.version` / appBuildID |
| --- | --- | --- |
| Baseline | `12.21.1@159.0.1` / `20261105165328` | `159.0.1` / `20261103153232` |
| PR3032 | `12.21.1@159.0.1` / `20261105165142` | `159.0.1` / `20261103153232` |

Where additionally queried, effective platformBuildID was also
`20261103153232`. The actual product behavior is reported separately from strict
release/native-provenance qualification; package success and GUI checks do not
replace that separate gate.

## Evidence and remaining conditions

Machine-readable comparison: `pwa-fullpackage-matrix.json`.
Each `baseline|3032` / `x11|wayland-product-qa` tree contains result.json,
native-identity-projection.json, actual UI result/screenshots, generated command
records, legacy-api.json, native properties/protocol and post-test inventory.
The original intake and before-inventory remain under each subject directory.
Reproduce with `full_package_pwa_probe.py`; measured native-identity projections
are generated by `project_pwa_identity.py`.

The remaining conditions are the preexisting ordinary-browser native Wayland
grouping, original Manjaro/CachyOS/KDE/package parity, and KAN-48's separately
unresolved actual Plasma taskbar icon pixels/task-model condition. KWin identity
matching alone does not authenticate icon pixels. Keep Draft3032 open; no merge,
release or Done transition follows from these results.

The initial environment-only display readiness check queried D-Bus immediately
after socket creation and saw a service-registration race. That non-product
failure remains in `display-readiness/`; the full runner waits for the actual
KWin session-bus service, and both real Wayland product runs completed. It is
not counted as a product failure or nonreproduction.
