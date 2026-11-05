# Reused KAN-95 Linux39 evidence (no reexecution)

Recovered all68 committed files (1,517,856 bytes), including41PNG, from fullcommit
`2c0aeaa8f70ed4876af0fa1d5a421497a5c1b9f0`, path
`qa-linux-additional/browser-ui/KAN-95`. Every recovered Git blob hash matches
the immutable GitHub tree; SHA256 inventory is in retrieval-provenance.json.

Source audit was29a3eb23145da90706d8a94441373278521cc737. Official current
Floorp12.21.1/Gecko159.0.1/BuildID20261103153232 and Firefox159.0.1 were
measured under Debian13.6/Xvfb101/Openbox/X11/softwareWebRender. Their binary
hashes are recorded, but a frontend sourceSHA is not inferred from official
binaries. Existing full rebuild control1b8f3503 was unrelated PR2934, not3052.

39layout rows: default6/Proton8/Lepton9/Fluerial8/Firefox8. Floorp tab hitbox
starts0px inside toolbar; selected background4px (Firefox6px). Lepton and
Fluerial therefore have measured4px historical current-Linux baselines.
Normal/maximized and menubar/titlebar configurations are recorded. Explicit
EWMHatoms and Gecko maximization agree. Native CSD was unavailable in the first
minimal session; truthful process-only XDG_CURRENT_DESKTOP=openbox enabled it
with zero frameextents. This was not a Windows-media override.

5OS XTest synthetic pointer controls: Floorp/Firefox client-top and center
clicks work with actual native CSD/maximization; initial server-decoration
controls have19px OS frame, so absolute screenY0 reaches titlebar instead.
These are not physical input or a Windows result.

Fluerial read-only actual CSSOM inspection shows a loaded root supplement
selector matching the selected background and!important4px fallback. The
base0px rule alone is not its effective design. Existing June17 declaration
comment intentionally restores topgap; do not merge this into Lepton's newly
confirmed token-rename regression.

Prior39 does not record browser.uidensity and has no3density/Photon/actual
vertical matrix. It cannot substitute for those new3052 complete-candidate
acceptance rows. Original report's “no new defect isolated” was the earlier
limited conclusion, before the root Windows3version evidence; it does not
disprove that later confirmed Lepton regression.

No test, browser, CI, dispatch, source modification, Jira or PR write occurred
during retrieval. Privacy scan found no personal-metadata keys or signed URLs.
Original Windows10/native titlebar/window configuration and physicalinput/GPU
remain unqualified. Reuse original report.md for exact scripts/environment and
measurement-summary.json for all39rows/5controls; do not overwrite raw evidence.
