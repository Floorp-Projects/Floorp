# KAN-8 — full pinned PR198 Flatpak build and actual execution

2026-11-06 JST (2026-11-05 15:09–15:17 UTC). **Completed:** the unchanged canonical PR198 manifest was fully built, finished, exported and installed by standard `flatpak-builder --install`, then its actual installed launcher was run through three normal profile/permission cycles. This resolves the earlier local full-candidate-package test gap; Bazzite44/Flatseal/Bazaar/full-desktop conditions remain open.

## Immutable subjects

- Manifest/launcher/helper source: Flathub PR198 head `d9aa24d58fe633bf765721f95b4ac13132ee397e`, independently fetched; `git diff --exit-code` confirmed no change to any of those three files.
- Manifest SHA256 `72117410eddb76798a072361c86c989d7b6b42ac221d63e3fe233dfddba8885f`.
- Manifest's official Floorp12.21.1 x86_64 archive SHA256 `7dcedcbfef747819153b0b9c541a9c1f4f4965b0720f16d5d237d39b0c30d613`; download/checksum verification was left enabled. Floorp application source/tag `984509d2053be7bae9027133bda7259cd706b208`.
- Locally built app OSTree commit **`7a3b387dc2c79a14b8f49b603b5b8d85d720fb233b325238d9d4d7a03bafde5a`**, app `one.ablaze.floorp/x86_64/master`, version12.21.1, Gecko159.0.1, BuildID20261103153232.
- SDK26.08 `a1b744b072d9f55d061e35382547ae80dad8d92abecc1da7dc1818de2daabfd3`.
- Runtime26.08 `df26daaf9d07610b4a72cc4585566194051275ac68396597b06474396f66427c`.
- Firefox BaseApp26.08 `e3a6bba88801b1daacf5cd9a6fd1d350a83a7a2da8924d0788b1167506eb6d12`. Its normal informational end-of-life notice says it is intended for Firefox/Thunderbird rather than direct user installation; the manifest already requires it and was not altered.
- Deployed `/app/bin/floorp` and `/app/bin/floorp-profile-root.sh` SHA256 values exactly match PR source files. Full values and script digests are in `full-package-subject.json`.

**Signature boundary:** this is a locally built **unsigned test package** installed by standard builder local-export/install. It is not a Flathub-signed production build and no release/publication is implied. Flathub remote GPG configuration, TLS checks, source SHA checks and product protections were not disabled or replaced. No `--no-gpg-verify`, `--insecure`, validation-skip or manifest modification was used.

## Standard build and recovered dependencies

Flatpak-builder1.4.4, AppStream and dependencies were downloaded as Debian packages using the dedicated dependency container, then extracted into the QA sysroot. Host build execution uses the already verified host namespace capability, not the Docker container whose namespace creation is denied. New `full-build-home` user installation has its own Flathub remote, SDK, BaseApp, runtime and app; the prior official application installation was not changed.

`full-build-exec.py dbus-run-session -- flatpak-builder --user --assumeyes --install-deps-from=flathub --repo=.../full-build-repo --force-clean --install .../full-build-dir .../flathub/one.ablaze.floorp.yml` completed **exit0**. The normal run performed the manifest build commands, cleanup, AppStream compose (Success!), finish, app/locale/debug exports and local installation. The final successful retry reused build cache from the same unchanged source after the first full module build had executed; retained earlier logs show all module commands. A separate clean-from-no-cache rebuild was not repeated.

Two earlier infrastructure failures are retained, not presented as product defects:

1. A sysroot-extracted `fusermount3` lacks root ownership/setuid metadata. Because it preceded the existing OS helper in PATH, cleanup could not unmount the builder's own rofiles FUSE mount. The existing **OS-standard root-owned `/usr/bin/fusermount3`** successfully unmounted it. Dedicated PATH symlinks now select that existing helper. The rofiles read-only/cache mechanism was retained; `--disable-rofiles-fuse` was not used.
2. Extracted `appstreamcli` expected its compose plugin at absolute `/usr/libexec/appstreamcli-compose`. A small dependency-path wrapper forwards the `compose` subcommand and **every original argument unchanged** to the same unmodified Debian plugin in the sysroot. It does not substitute validation output or skip validation. Compose then returned Success! (`full-build-final.log`). `FLATPAK_VALIDATE_ICON` and `FLATPAK_BUILDER_DEBUGEDIT` similarly select their actual packaged standard binaries at extracted paths.

No long-lived host system configuration was changed. Default builder state/cache was initially created at `/workspace/.flatpak-builder`; large build artifacts and installations are private QA artifacts and are not intended for the shareable commit.

## Actual installed-package test

`python3 KAN-8/full_package_profile.py` ran the newly installed candidate's real default `floorp` launcher on new Xvfb`:89`. It did **not** source or inject a helper, override the command, seed a profile registry, or specify a profile path. It used a fresh isolated HOME and normal isolated session bus plus the same explicitly incomplete system test bus described in the earlier report.

| Step | Actual outcome |
|---|---|
| home permission absent | app-data legacy profile created; marker saved; normal in-app shutdown/forced=false/exit0 |
| one-run `--filesystem=home` | exactly same profile and local/cache directory; marker retained; normal shutdown/exit0 |
| permission removed | exactly same profile and local/cache directory; marker retained; normal shutdown/exit0 |

All three report `security.sandbox.content.level=6` and absent `MOZ_DISABLE_CONTENT_SANDBOX`. Product logs retain nested namespace/portal/network warnings; no protection was lowered to achieve the result. Cache **path** consistency is verified; arbitrary cache-file reuse/migration semantics are not exhaustively qualified.

Evidence: `full-package-normal-result.json`, three `full-package-normal-*.log` launch logs, three matching PNGs, `full-package-info.txt`, `full-package-subject.json`, `full-build-final.log`, earlier failed `full-build.log`/`full-build-retry.log`, and the execution/build wrappers.

## Disposition

Across the whole KAN-8 task there are now **9 actual product launches with normal exit**: three official failing comparison runs, three exact-helper-injection runs, and three full-built-package runs. The nine existing PR unit tests pass separately. The full built candidate fixes the controlled permission/profile switch; do not claim full original Bazzite reproduction or package distribution qualification. Keep PR198 canonical and Draft, do not merge/release/Done. Remaining conditions: Bazzite44/Flatseal/Bazaar, complete desktop/portal/system service setup, exceptional mixed registry flows in an actual package, and deeper cache-content/migration checks.
