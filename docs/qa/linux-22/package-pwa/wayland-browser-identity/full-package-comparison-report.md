# Ordinary-browser Wayland identity fix: completed production package

The new full production application package at source
`2deb165d75290ffd4f602ad078512dc42fc3a332` passes all six targeted native Wayland
identity cases on a fresh untraced strict rerun. Normal and private browser
windows use `floorp`, PWA windows retain their generated ids, existing-profile
ids are retained, and explicit custom browser naming survives a remote PWA and
a second normal window. The first product-tree lock anomaly remains unresolved
and is preserved separately; it is not erased by this successful rerun.

| Native Wayland case | Prior PR3032 full package | New source copy | New completed package, untraced R2 |
| --- | --- | --- | --- |
| browser → PWA | passes | passes | passes |
| PWA → ordinary browser | normal inherits Alpha | passes | passes |
| multiple PWA → ordinary browser | normal inherits Alpha | passes | passes |
| existing-profile PWA → browser → PWA | normal inherits old Alpha | passes | passes |
| custom `--name` browser → remote PWA → real `--new-window` | custom preserved | preserved | preserved |
| PWA → actual `--private-window` | actual private window inherits Alpha | passes | passes |

## Actual completed package and provenance

- Canonical `package.yml` run
  [42077174018](https://github.com/Floorp-Projects/Floorp/actions/runs/42077174018),
  completed/success, exact source `2deb165d75290ffd4f602ad078512dc42fc3a332`.
- Unsigned installer Actions artifact `13439144608`, API/download ZIP SHA256
  `febce14e382da315a7579464f1052768a0e51854a752de8a926ee898db6ce996`.
- Inner package tar SHA256
  `bafdaf44e310aa22b41a9cb740dff47b07c51ada37978008c47d6952c6564976`.
- Root/package agent authenticated ownership, source/run/name/expiry/size/digest
  and safely extracted the original package with a complete before-inventory.
  R2 independently revalidated the same archive into a separate empty product
  root. No archive resource, JavaScript, manifest, native file or INI was edited.
- This is a full production **application** package using the authenticated
  prebuilt native engine, not a new all-Gecko compile or signed distribution.
  The earlier six-resource product copy remains separately labelled and is not
  substituted for these actual completed-package results.

## Strict successful full-package rerun

`full-package/actual-qa-r2/suite-result.json` records `qualified:true`, six native
checks passed. The three groups use separate fresh HOME/profile/output roots,
owned Xvfb96/nested KWin6.3.6 and dedicated normal session buses. Each group
normally shuts down before the next starts. Each selected original package
inventory is checked before and after execution with no exception list.

- Four core order/migration cases plus real custom-name and private controls.
- Seven normal in-app quits (`cause:shutdown`, `forced:false`).
- All product before/after inventories identical; no added lock or other file.
- Original prior3032 migration profile, HOME and SSB-store hash unchanged.
- Content sandbox level6, Linux taskbar experiment false, no forced enrollment
  or lowered protection. Native Wayland set_app_id and own-PID KWin
  resourceClass/desktopFileName corroborate the product/launcher observations.
- Real `PrivateBrowsingUtils.isWindowPrivate` identifies the private window.
- Owned processes and display96 are absent after cleanup; temporary private
  runtime directories are removed. `final-process-cleanup.json` records this.

The profile fixture comes from prior actual UI installation in the completed
PR3032 product. Copies omit only transient locks and change the automation port.
Saved identities, URLs and desktop contents are never manually rewritten;
executable/profile paths are mechanically relocated only as invocation arguments.
Ordinary titles alone are tagged to disambiguate restored fixture-page content
from PWA windows. No native class/app_id is assigned by the harness. Input is
synthetic UI/CLI, not physical hardware input.

## Preserved first anomaly and limited diagnostic

The initial untraced full-package attempt at `full-package/actual-qa` completed
all four native identity cases successfully, but the integrity gate detected two
unexpected added product-root entries: empty `.parentlock` and a `lock` symlink
containing the first UI browser PID127417. Existing product file bytes were
unchanged. The gate correctly failed, and custom/private groups then refused
startup because their before-inventory did not match the original intake.
That first attempt is **not** counted as a qualified suite.

The failed suite and original mutated product tree are preserved. Details are in
`full-package/product-mutation-observation.json`. The whitelisted environment
observation found no XRE_PROFILE_PATH/LOCAL_PATH, MOZ_PROFILE* or crash-restart
profile arguments; CWD was /workspace and each observed ProfD matched the
dedicated fresh profile. This does not establish a cause.

A separate authenticated fresh extraction ran only first ordinary UI startup,
actual Alpha/Beta UI installation and normal shutdown under main-process-only
strace file-operation/backtrace capture, without attaching to sandboxed content
children. It observed no targeted app-directory lock operations and no product
inventory change (`full-package/lock-diagnostic-run`). It did not run the four
order cases and is not qualification evidence. Instrumentation/timing effects
and nondeterminism remain possible; no root cause is claimed.

Root then authorized the separate fresh, **untraced** R2 full suite, which passed
all six strict cases without reproducing the extra files. No inventory allowlist,
permission weakening, protection switch, source change or edited manifest made
the rerun pass. Independent read-only comparison found the delivered native files
and profile-manager actors byte-identical between3032 and3040; that alone does
not attribute the anomaly to those actors or to another task's XPT work.

## Effective Runtime versus unused root INI

All full-rerun product observations report actual Services.version `159.0.1`,
appBuildID and platformBuildID `20261103153232`. The package's unused root INI
records Version `12.21.1@159.0.1` and packaging BuildID `20261105174535`.
Both are preserved and distinguished; no INI was rewritten. Strict native/release
provenance, signing and publication qualification are separate from this GUI test.

## Results and remaining conditions

`final-comparison-matrix.json` contains eighteen qualifying comparison cases:
six prior-product cases (two controls pass, four inheritance failures), six
candidate-copy passes, and six completed-package R2 passes. Raw observations,
native protocol and strict projection JSON remain in each labelled origin tree.

The initial product-root lock artifact anomaly remains unattributed and should
remain visible to reviewers. Original Manjaro/CachyOS package/KDE parity and
actual Plasma taskbar icon pixels/task-model behavior are not established by
these nested compositor identity checks. No physical-input, signed-release,
merge or Done claim follows. No additional source changes or pushes were made
by this harness.
