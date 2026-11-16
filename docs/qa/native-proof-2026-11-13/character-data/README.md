# Historical CharacterData UTF-8 proof (2026-11-13)

This is an inert, reviewable archive, **not an active packaging patch** or a
current-Runtime fix. The patch and browser regression source are byte-for-byte
copies of the completed historical proof. They are original
Floorp/Mozilla-derived source under the repository MPL-2.0 license. No bundled
third-party assets, native binaries, profiles or raw logs are included.

## Scope and provenance

Floorp `984509d2053be7bae9027133bda7259cd706b208` (12.21.1), Runtime
`8965ffc346d426ccdf919bd18585e4d4a0c21654`, macOS ARM64 native Debug with tests
enabled and optimization disabled. This was a locally built production-UI
product, not an official signed release.

The native patch makes an empty
`CharacterDataBuffer::AppendTo(nsACString&, fallible_t)` a successful no-op
before a null-backed substring can reach Rust Latin1 conversion. It adds empty,
cleared, and nonempty UTF-8 gtests. The browser regression checks empty
stylesheet text, nonempty insertion, clearing, and reinsertion without network
access.

Historical evidence was inspected for publication: native build completed in
65m50s; gtest reported 20 tests, zero failures; browser regression had four
passing assertions, PASS test end and process exit 0. `evidence.json` records
SHA-256 hashes of the exact committed (LF) bytes of the artifacts and retained
local evidence, not publicly reproducible logs. On 2026-11-16,
`git apply --check` succeeded against exact copies of the two files from the
stated Runtime commit. No new native build or browser run was performed.

## Reproduction requirements

Use an isolated checkout of that exact Runtime base and its matching
Floorp/source/test configuration. First run `git apply --check` with the
archived patch, then apply it and perform a **native rebuild**. Artifact-only
packaging cannot change XUL and does not deliver this fix. Rebuild/repackage the
paired product and matching test assets before testing.

Run the Runtime gtest harness for `CharacterDataBufferTest.*` with the rebuilt
binary. For the browser test, place `browser_empty_style_text_utf8.js` in
`browser/components/tabbrowser/test/browser/tabs/`, register it in that
directory's native browser manifest in the isolated checkout, regenerate the
test install and run the Mozilla browser-chrome harness against the rebuilt
product with a fresh temporary profile. Follow that checkout's
`mach ... --help`; no user profile should be used. Require all assertions and a
clean harness exit, not just `test_end PASS`.

This archive intentionally does not modify a Runtime lockfile, manifest,
workflow, or the automatically applied `.github/patches` directory. A
current-Runtime integration needs applicability review, a native rebuild and
fresh gtest/browser proof. The fix is not known to be present in any later
Runtime: the two target files are unchanged in a later Runtime commit
(`741c4537`), so the defect is likely still present. Integration is tracked in
[#3157](https://github.com/Floorp-Projects/Floorp/issues/3157). Historical
results do not establish that current main passes or that the entire Firefox
cohort is green.

Related:
[native cohort evidence #3151](https://github.com/Floorp-Projects/Floorp/pull/3151),
[isolated UI source fixes #3152](https://github.com/Floorp-Projects/Floorp/pull/3152),
and
[native-product harness #3138](https://github.com/Floorp-Projects/Floorp/pull/3138).
Unresolved modal/profile icon CSP changes and restore/split-panel candidates are
not part of this archive.
