# Complete-product UI comparison

This runner never substitutes product resources. Authenticate each complete
package separately and retain its source SHA, workflow/artifact metadata and
archive digest beside these UI records. Invoke inside a dedicated DBus session:

```sh
dbus-run-session -- python3 full_ui.py \
  --mode baseline --product /absolute/baseline/floorp \
  --out /absolute/evidence/baseline-ui

dbus-run-session -- python3 full_ui.py \
  --mode candidate --product /absolute/integration/floorp \
  --migration-from /absolute/evidence/baseline-ui/migration-seed \
  --out /absolute/evidence/integration-ui
```

Every output directory must be new. The baseline profile is fresh. Its migration
seed is cloned only after real visible Hub and native Settings controls produce
the mismatch and the product quits normally. The only user.js override selects
the Marionette endpoint. No design/native settings are seeded. Before each
normal quit the selected page is navigated to about:blank; startup snapshots
record selected URLs and every tab URL before this runner navigates to Hub.
Migration/baseline-restart startup asserts that no tab has restored Hub. Check
those URLs instead of assuming that session restoration omitted background Hub.

Default `--migration-direction horizontal` naturally creates nativefalse /
storedvertical. Repeat both commands with `--migration-direction vertical` and
different output directories (`baseline-reverse-ui`, `integration-reverse-ui`),
passing the reverse baseline's migration seed to the reverse candidate. That
direction selects Hub Horizontal first, then clicks native Vertical, naturally
creating nativetrue / storedhorizontal. The candidate cold-start expectation is
therefore native true / storedvertical. Neither direction directly seeds prefs.

Controls use synthetic WebDriver ElementClick, including the real shadow-DOM
buttons in Firefox's layout picker. This is Xvfb X11 with normal DBus, not
physical input, KDE, Wayland, GPU or original macOS qualification. The runner
owns only display102, port2914, its explicit product process and dedicated
profile. It checks sandbox6, executable/profile identity, and product file
hashes before/after; no protection is disabled.

The result records execution completion separately from expected outcome
checks. Candidate checks include startup reconciliation, both live directions,
settled new-window identity/layout, Workspace switches and multirow persistence
through normal restart. Multirow coverage uses few tabs and does not establish
overflowing multi-row geometry. It does not claim to reproduce the separate
two-visible-window stale pending-save UI race; actual-source API tests cover
the stale-style save contract separately.

`profile/`, `migration-seed/`, `home/` and `runtime/` are local execution data.
Never publish profiles or user data. Publish the script, result JSON,
snapshots, screenshots and non-secret logs after review.

## Actual shipped-module API contract

Run separately and sequentially with the UI runner (same owned display/port):

```sh
dbus-run-session -- python3 shipped_api.py \
  --product /absolute/integration/floorp \
  --out /absolute/evidence/integration-shipped-api
```

`shipped_api.py` conservatively discovers the emitted getter/save exports from
the product's own `omni.ja`, records the asset digest and import URI, and imports
that exact module from the real Hub content origin. It never evaluates a
locally compiled substitute implementation. The actual getter supplies a stale
settings object; a real external Services preference write models a second
window changing horizontal↔multirow, and the shipped save function applies an
unrelated favicon change. Both original design/native values are restored at
the end. This is a dedicated-profile API contract test; it is not a claim that
the timing of two visible Hub forms' autosave race has been reproduced.
