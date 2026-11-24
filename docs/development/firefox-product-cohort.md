# Firefox product cohort (nonrequired shadow)

This expands the Floorp product-side work beyond the first 17-case evidence
contract. It records 289 original paths: 127 Tabs, 92 modern SessionStore, 70
legacy SessionStore. This is the union of all 288 previously selected paths and
the 283 distinct paths found in local execution logs. An older aggregate of 194
is not a pass list. No historical result is imported as a current pass.

The three original Mozilla manifests are pinned by SHA-256 at the exact Runtime
commit in the JSON, which must equal `source.commit` in
`floorp-runtime.lock.json`. The Mozilla mochitest harness receives the original
paths and uses the unmodified manifests, support files, expectations and
timeouts. Nothing copies or edits the upstream test files. Compiler CSP, leak
checks, and existing production protections are unchanged.

Run the plan generator with Python 3.11+ from a provisioned checkout:

```sh
python3 tools/qa/native-bridge/product_cohort.py \
  --source /absolute/matched-runtime-source \
  --support /absolute/extracted-common-and-mochitest-test-packages \
  --python /absolute/runtime-python \
  --floorp /absolute/assembled-product/floorp \
  --output /absolute/new-evidence-directory
```

It emits serial browser-chrome argv arrays (default 16 originals per batch). It
neither launches nor grants a check. An approved isolated supervisor must bind
the reviewed Floorp commit, actual assembled noraneko artifact, strict Runtime
and same-OBJDIR fixture/harness closure; provide its owned fresh
profile/display, network and teardown safeguards; compare active native
discovery to each planned batch; and keep original raw logs, actual process
exits, settings and leak results. DEV, STG and compiled product are separate
outcomes. Missing, inactive, unexpected, or unrun originals remain non-green;
never replace them with a smaller cohort. An ordinary PR result must be observed
before setting any required check. This changes no merge rule, public package,
or acceptance threshold. The existing 17-case receipt verifier remains
independent and cannot qualify this cohort.

Each job also emits the same invocation with stock `--dump-tests` for preflight.
Call `validate_discovery(job, document)` on that JSON before launching the
batch; wrong manifests, extra or missing cases, duplication, disabled cases and
non-pass expectations prevent admission. Batches never contain a single case:
this Runtime's original harness can remove the `disabled` entry when only one
test matches. A preflight mismatch is evidence to record, not a reason to
force-enable a test.

## Shadow workflow

`.github/workflows/firefox_product_gate.yml` runs check
`firefox-product-cohort (linux-x86_64)` on pull requests to `main`. It is not
required. Three jobs run in order, so no single job exceeds the hosted limit:

1. `product runtime build (linux-x86_64)` builds the locked Runtime with its
   release mozconfig (non-PGO), then `mach package` and the stock `test_archive`
   common, mochitest and condprof packages from the same OBJDIR
   (`product-build-runtime.sh`). `product_provenance.py
   runtime` records the
   locked commit and tree, `platform.ini`, `mozinfo.json`, the package digests
   and the staged OBJDIR copy of every planned manifest and test, and rejects
   any difference from the source.
2. `product assemble (linux-x86_64)` assembles packaged Floorp with noraneko
   around that exact Runtime package along the Linux release path of
   `package.yml` (`product-assemble.sh`).
3. `firefox-product-cohort (linux-x86_64)` fails unless both jobs succeeded,
   extracts the product and the three test packages, and installs the packaged
   `config/marionette_requirements.txt`. `product_provenance.py bind` checks the
   package digests, Gecko BuildID and SourceStamp, `libxul.so`, noraneko chrome,
   `mozinfo.json` and every planned manifest and test against the compiled
   identity. `run_product_cohort.py` then runs all planned batches serially with
   the packaged stock `mochitest/runtests.py` under Xvfb with a window manager,
   each with stock discovery preflight, a fresh mochitest profile and a check
   that no product process outlives the batch.

The verdict is PASS only when all 289 originals end with an expected pass and
every batch exits zero with `suite_end`, no unexpected subtest, crash, leakcheck
failure (the stock mozlog rule: missing totals, or bytes above the harness
threshold), unplanned test or surviving process. Sub-threshold leak totals are
kept in `summary.json` as warnings. Otherwise the job fails, and `summary.json`
lists every case as `pass`, `fail`, `crash`, `incomplete`, `unexecuted` or
`preflight_rejected`. This lane is the compiled product only; DEV
(`feles-build dev`) and STG (`feles-build stage`) are not exercised here.
