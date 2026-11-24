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
  --python /absolute/runtime-python \
  --floorp /absolute/assembled-product/floorp \
  --modules /absolute/same-objdir/testing-modules \
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
required. In one job it:

1. builds the locked Runtime with its release mozconfig (non-PGO) and keeps the
   compile OBJDIR (`product-build-runtime.sh`, followed by
   `mach package-tests`);
2. assembles packaged Floorp around that Runtime package along the Linux release
   path of `package.yml` (`product-assemble.sh`);
3. binds provenance (`product_provenance.py`): locked commit and tree, Gecko
   BuildID and SourceStamp, `libxul.so` digest, noraneko chrome, staged OBJDIR
   manifests and tests against the plan;
4. runs all planned batches serially under Xvfb with a window manager
   (`run_product_cohort.py`), each with stock discovery preflight, a fresh
   mochitest profile and a check that no product process outlives the batch.

The verdict is PASS only when all 289 originals end with an expected pass and
every batch exits zero with `suite_end`, no unexpected subtest, crash, leakcheck
failure (the stock mozlog rule: missing totals, or bytes above the harness
threshold), unplanned test or surviving process. Sub-threshold leak totals are
kept in `summary.json` as warnings. Otherwise the job fails, and `summary.json`
lists every case as `pass`, `fail`, `crash`, `incomplete`, `unexecuted` or
`preflight_rejected`. This lane is the compiled product only; DEV
(`feles-build dev`) and STG (`feles-build stage`) are not exercised here.
