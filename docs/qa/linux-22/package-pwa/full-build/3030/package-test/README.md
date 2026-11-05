# Full workflow-package comparison controller (prepared, not yet executed)

`compare_full_packages.py` compares the untouched baseline workflow package from main984509d2 with the untouched PR3030 package from e55152c5. It never rewrites a product manifest, substitutes a frontend bundle or edits package files. It inventories all package files before and after. Four fresh profiles (baseline/candidate × ja/es-CL) independently install the existing official signed language pack using normal AddonManager, quit normally and restart in the requested language. RequestedLocales, actual app locale, post-restart active language pack and signedState2 are all required; a startup language fallback cannot pass.

A focused controller runs the complete new LegacyBranding test module plus unchanged repository runTests/assertEquals harness in the actual browser chrome. This remains separate from top-level colocated CLI orchestration. It also records the actual brand, geometry/client-list-validated native icon pixels, normal product exits and native menu screenshots. A candidate row passes only if brand, test, known Floorp icon and translated UI labels all match. Baseline is expected to retain Firefox branding, fail the new invariant and advertise the known Firefox icon under the controlled theme. Content sandbox6 and no sandbox-disable variable are mandatory.

Use a new output directory, an independent D-Bus session and the prepared prefix dependencies:

```
dbus-run-session -- python3 compare_full_packages.py \
  --baseline /workspace/qa-linux-full-build/products/baseline/floorp \
  --candidate /workspace/qa-linux-full-build/products/3030/floorp \
  --provenance /path/to/verified-package-provenance.json \
  --out /path/to/new-output-directory
```

Provenance JSON contains `baseline` and `candidate` records. Each must contain `packageKind: "workflow-generated-linux-package"` and its exact sourceSha. Include the authentic workflow run/artifact ID, API artifact digest, downloaded ZIP digest and enclosed package digest from the retrieval records. The candidate extraction script already emits this record as package-provenance.json; root supplies the baseline record after its independent retrieval. Artifact ZIP digests must be verified before extraction. The controller does not replace this authentic download verification.

The default :94/port2893 are independent of earlier QA desktops. All input is synthetic. The caller must verify the requested slot is free. A dedicated known GTK icon theme and Noto font are installed only inside the new test HOME. Any missing _NET_WM_ICON or startup/profile/locale failure is recorded as a failed comparison, not converted into success. No release/signing/native-recompilation qualification follows from artifact-mode packaging alone.

At preparation time only Python syntax and --help were checked. No full-package execution result is claimed yet. Exclude generated test HOME/profile/runtime directories from QA evidence commits.
