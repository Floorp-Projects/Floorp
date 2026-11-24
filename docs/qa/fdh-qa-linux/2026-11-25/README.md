# FDH Linux container QA target — live runs (KAN-144, Linux side)

Date: 2026-11-25. Target: the Debian 13 QA host (x86_64, 4 cores, 15 GB RAM,
Docker 29.6, cgroup v2), driven from the Windows dev PC over SSH with
`tools/src/fdh_qa_linux.ts` and the live scripts in `tools/integration/`.
Payload: `floorp-linux-x86_64.tar.xz`, Floorp 12.21.1@159.0.1 (BuildID
20261105184721), SHA-256
`584e4afd9865df3a5867b145ed75b7c3051a0a26d173da240056a4b355bfcad9`.

Image: `fdh-qa-linux:0027e4d3b9d3` (Debian trixie-slim, Xvfb + openbox, user
`qa`), tagged by the SHA-256 of the Dockerfile and entrypoint in the module; 854
MB on the host. Container per task: `--memory 2560m --memory-swap 2560m`,
`--cpus 2`, `--cap-drop ALL`, `--security-opt no-new-privileges`,
`--network none`, `--init`; Marionette is not published.

The host address is not part of these reports.

## Runs

| File                          | Script                      | What it shows                                                                                                                                                                                                                                                                     |
| ----------------------------- | --------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `kan-144-linux-1-report.json` | `fdh-qa-linux-live.ts`      | Image missing, so built with `--build-image` (about 2.5 min); preflight ready; container created; upload (108 MB) + hash check + extract; launch on the Xvfb display with a fresh profile; Marionette probe (`Floorp`, `Linux`, 1 window); screenshot; container removed. Exit 0. |
| `kan-144-linux-1-linux.png`   | —                           | Screenshot from that run (Marionette `WebDriver:TakeScreenshot`, chrome context). SHA-256 `1a39a68eae33de8fc24fc9d5d1d669f5e0ca52ceabfe9a37f580308bdec685dd`.                                                                                                                     |
| `lock-check.json`             | `fdh-qa-linux-lock-live.ts` | Container-name lock: task A creates the container and re-acquires as a no-op; task B is refused (`container-held`) for acquire and cleanup; preflight shows A as holder; A's cleanup removes the container. `passed: true`, exit 0.                                               |

After the runs the host had no `fdh-qa-*` container and no `/tmp/fdh-qa-*`
directory; only the image remains.

## Limits

- Xvfb + openbox only: no real desktop session (KDE/GNOME, Wayland), no GPU
  driver, no distro integration (portals, IME, system fonts beyond DejaVu and
  Noto CJK). The Debian host's own seat0 desktop is not used.
- One container per target name at a time.
- `--network none` by default; pages that need the network need
  `"network": "bridge"` in the target config.
- The SSH account on the host needs Docker access (root here).
- Not QA qualification, release evidence or a QA3 replacement.
