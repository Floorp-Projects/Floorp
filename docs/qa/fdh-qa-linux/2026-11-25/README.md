# FDH Linux container QA target — live runs (KAN-144, Linux side)

Date: 2026-11-25. Target: the Debian 13 QA host (x86_64, 4 cores, 15 GB RAM,
Docker 29.6, cgroup v2), driven from the Windows dev PC over SSH with
`tools/src/fdh_qa_linux.ts` and the live scripts in `tools/integration/`.
Payload: `floorp-linux-x86_64.tar.xz`, Floorp 12.21.1@159.0.1 (BuildID
20261105184721), SHA-256
`584e4afd9865df3a5867b145ed75b7c3051a0a26d173da240056a4b355bfcad9`.

Image: `fdh-qa-linux:41fc5d48b6ce` (base `debian:trixie-slim` pinned by digest,
built without `--pull`, Xvfb + openbox, user `qa`), tagged by the SHA-256 of the
Dockerfile and entrypoint in the module; 854 MB on the host. Container per task:
`--memory 2560m --memory-swap 2560m`, `--cpus 2`, `--pids-limit 2048`,
`--read-only` root with tmpfs `/home/qa` (1 GiB) and `/tmp` (256 MiB),
`--cap-drop ALL`, `--security-opt no-new-privileges`, `--network none`,
`--init`; Marionette is not published.

The host address is not part of these reports.

## Runs (at the code in this PR, after review)

| File                          | Script                      | What it shows                                                                                                                                                                                                                                                                                                                                             |
| ----------------------------- | --------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `kan-144-linux-3-report.json` | `fdh-qa-linux-live.ts`      | Image already present (built in the previous run of this PR); preflight ready; container created; upload (108 MB) + hash check + extract into the tmpfs home; launch on the Xvfb display with a fresh profile; Marionette probe (`Floorp`, `Linux`, 1 window, `security.sandbox.content.level` 6); screenshot; container removed. `passed: true`, exit 0. |
| `kan-144-linux-3-linux.png`   | —                           | Screenshot from that run (Marionette `WebDriver:TakeScreenshot`, chrome context). SHA-256 `1a39a68eae33de8fc24fc9d5d1d669f5e0ca52ceabfe9a37f580308bdec685dd`.                                                                                                                                                                                             |
| `lock-check.json`             | `fdh-qa-linux-lock-live.ts` | Container-name lock: task A creates the container and re-acquires as a no-op; task B is refused (`container-held`) for acquire and cleanup; preflight shows A as holder; A's cleanup removes the container. `passed: true`, exit 0.                                                                                                                       |

After the runs the host had no `fdh-qa-*` container, no `/tmp/fdh-qa-*`
directory and no new `debian` tag; only the image remains. The image from the
first revision of this PR (`fdh-qa-linux:0027e4d3b9d3`) was removed.

## Limits

- Xvfb + openbox only: no real desktop session (KDE/GNOME, Wayland), no session
  D-Bus, no GPU driver, no distro integration (portals, IME, system fonts beyond
  DejaVu and Noto CJK). The Debian host's own seat0 desktop is not used.
- Sandbox fidelity: Docker's default seccomp profile blocks unprivileged user
  namespaces, so Floorp's namespace sandbox layer cannot start in the container.
  The probe reports the configured content-sandbox level (6), not the effective
  one; runs here do not exercise the desktop sandbox setup.
- The base is pinned, but packages come from the Debian archive at build time,
  so two builds of one tag can differ in package versions.
- The tmpfs home holds the payload, the extracted app and the profile, so a
  build that unpacks to more than about 1 GiB does not fit, and the tmpfs mounts
  count against the container's memory limit (hence `memoryMb` of at least
  2048).
- One container per target name at a time. A changed image definition leaves the
  previous image on the host; remove old ones with
  `docker images --filter label=fdh.qa.image=1` and `docker image rm <tag>`.
- `--network none` by default. With `"network": "bridge"` the browser under test
  can reach other containers on the default bridge and ports published on the
  host; use it only for pages that need the network.
- The SSH account on the host needs Docker access (root here).
- Not QA qualification, release evidence or a QA3 replacement.
