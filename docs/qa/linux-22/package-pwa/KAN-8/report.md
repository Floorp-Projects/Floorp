# KAN-8 / GitHub #2837 — actual Flatpak profile follow-up

2026-11-05. Classification: **reproduced in the official Flatpak; existing PR198 helper passes the controlled comparison**. This is not full Bazzite/Flatseal qualification and does not establish a complete rebuilt candidate Flatpak package.

## Immutable subjects

- Floorp source/tag/main: `984509d2053be7bae9027133bda7259cd706b208`.
- Actual official Flathub app: `one.ablaze.floorp` 12.21.1 / Gecko 159.0.1 / BuildID `20261103153232`; app commit `ab3c23d5f3a32863918263c197b483e37d9ed78a6bb9d00a1657e31dc5e04621`; freedesktop runtime 26.08.
- Canonical existing Draft: https://github.com/flathub/one.ablaze.floorp/pull/198 ; fetched head **`d9aa24d58fe633bf765721f95b4ac13132ee397e`**. No duplicate PR or code change was made.
- Helper SHA and test-script SHA: `test-sha256.txt`.

## Environment and recovered capability

Debian GNU/Linux 13 cloud host, non-root UID1000, Xvfb display `:88`, isolated HOME/profile/config/cache, user Flatpak1.16.6 extracted into the dedicated QA sysroot. Host `unshare -Ur` succeeds through the approved execution path; Docker's default container denies namespace creation, so product qualification did not use that container.

The initial Flatpak installation failed at parental-controls metadata retrieval because the host has no system D-Bus socket. A dedicated test D-Bus (not a complete system service bus) supplied that endpoint; no existing parental-control setting was changed. Flatpak logged that parental controls are disabled globally in this isolated setup. The same dedicated bus was used for each run. Each actual browser launch also used **a normal isolated session bus**: no `--no-session-bus` or `--no-a11y-bus` flags. `org.freedesktop.portal.Desktop` was unavailable and emitted warnings. This is an explicit remaining desktop capability, not a reproduction of a portal product defect.

All six measured product runs retained `security.sandbox.content.level=6`; `MOZ_DISABLE_CONTENT_SANDBOX` was absent. Flatpak logs contain `CanCreateUserNamespace() clone() failure: EPERM` for a nested browser namespace attempt; no sandbox preference or environment bypass was applied. The existence of level6 is recorded, but is not an independent audit of every kernel sandbox feature. Chrome automation required the product's explicit `MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1` test opt-in and a separate Marionette port2831. Inputs/quit are synthetic automation, not physical input.

## Executed checks

1. `python3 -m unittest discover -v -s KAN-8/flathub/tests` — **9/9 passed** on PR head d9aa24d.
2. `python3 KAN-8/profile_normal_shutdown.py` — actual official Flatpak browser, three launch/normal-quit cycles without home permission, with one-run `--filesystem=home`, and without permission again. A string preference marker was written only into the new test profile. No user profile was used.
3. The same three cycles with the PR198 helper copied into the isolated app config and sourced inside the real Flatpak before executing its unchanged `/app/lib/floorp/floorp`. This verifies the helper/product interaction, **not** a full manifest build or deployment of the candidate launcher.

| Subject | Initial / home enabled / home removed | Marker | Shutdown |
|---|---|---|---|
| Official Flatpak | legacy profile / new XDG profile / original legacy profile; local profile/cache paths change with profile | present / missing / present | 3/3 in-app shutdown, forced=false, exit0 |
| Official Flatpak + PR198 helper | same app-data legacy profile and same local profile/cache directory throughout | present / present / present | 3/3 in-app shutdown, forced=false, exit0 |

The official profile remains on disk and is recovered after removing home permission. This is a profile-selection failure, not demonstrated data deletion. Candidate cache **path** stability was observed; cache-file content reuse and cache migration were not exhaustively validated.

`normal-shutdown-result.json` is the machine-readable aggregate; `official-normal-result.json` and `candidate-normal-result.json` retain the individual browser responses. Six PNGs show the actual new product windows, and six launch logs preserve warnings. Earlier harness attempts (wrong port JSON key and timed smoke launch) are kept separately; they are infrastructure/test-harness failures, not product results.

## Remaining conditions and disposition

- Bazzite44 with Bazaar/Flatseal and its complete desktop/portal system service configuration.
- A complete candidate Flatpak package built from PR198's manifest/launcher, rather than injecting only the exact helper into the official installed application.
- Mixed real-world legacy/XDG/host registries under actual package execution, normal shutdown of those exceptional startup cases, explicit override usability, and deeper cache reuse/migration validation. PR's unit tests cover fail-closed selection but do not qualify all GUI flows.
- Keep the existing Draft PR as canonical. Do not merge, release, mark Done, or remove the Bazzite/package-specific remaining-condition labels from this comparison alone.

Small shareable evidence: this report, `profile_normal_shutdown.py`, `normal-shutdown-result.json`, `unit-tests.log`, `flatpak-info.txt`, `test-sha256.txt`. Do not commit isolated browser HOME data, installation/sysroot binaries, or `sources.json` (contains retrieved account metadata).
