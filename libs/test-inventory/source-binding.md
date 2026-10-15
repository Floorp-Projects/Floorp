# Development test entry source inputs

The ordinary colocated runner binds the original inventory hash to the bytes
provided to Vite for each browser test entry. This is a development harness
contract, not whole-product or executable-byte attestation.

The inventory reads each source through one ordinary file descriptor, with
nonblocking open and mandatory safe positive descriptor/path identities. It
limits actual reads to 2 MiB per source and 32 MiB overall, checks
descriptor/path identity and size/time changes, and hashes the returned buffer.
Size checks on an earlier `lstat` do not authorize an unbounded later
`readFile`. Unsupported nonblocking open or absent/zero/unsafe device/inode
identifiers refuse this source-binding path. This is not general Windows/macOS
filesystem qualification.

The actual Deno 2.9.6 Linux SDK accepted `O_NONBLOCK` on a FIFO but ignored
`O_NOFOLLOW` on a final symlink. The original failed flag probe is preserved.
The implementation therefore does not claim a no-follow open: it requires an
ordinary FD with exactly the pre-read path identity before reading any bytes,
then rechecks the path/FD after reading. The Vite hook additionally requires the
exact logical entry path to resolve to that same project entry, so an alias or
leaf symlink cannot redirect entry A to another authorized entry B. These checks
do not establish atomic path-snapshot or hostile same-UID protection.

Before fresh browser/server startup, the collector creates a new append-only
`nora-tests-source-<runId>.json` in its existing test profile. It includes every
discovered browser entry's hash, including unselected entries that an eager
pages import may load. The run ID is a closed filename component, and its exact
manifest digest and expiry are bound into the original one-shot control. The
manifest remains as evidence; later runs never replace or retag it.

Only `feles-build test` explicitly starts the main 5181 server with
`--mode
test`. Its first pre-load plugin binds the original control/manifest
once. Before returning a browser test module, it resolves the exact project
source path, reads a bounded buffer from one descriptor, checks the expected
digest, and returns code decoded from that same buffer. UTF-8 decoding is fatal;
a BOM is decoded deterministically while the digest still includes its raw
bytes. Unknown browser entries and unsupported queries fail instead of falling
through. An explicit reserved export records the run, manifest, entry ID and
actual source hash.

The driver receives a run-bound marker from the same plugin. The browser checks
that marker against its original consumed control **before** eager pages
discovery. Missing/mismatched driver evidence refuses ordinary execution. The
host then requires each result's entry ID, original source hash, run and
manifest digest to match the original plan. A missing proof, another run's cache
or an unexecuted/missing result cannot grant a successful suite.

The test server remains bound to its first run. It does not retag already
compiled modules for a new run; cached modules retain the old proof. HMR is
disabled only for this test server, and changed sources on subsequent loads are
rejected. A source-bound collection requires fresh browser/server startup;
ordinary development and production builds do not enable this plugin. A fresh
server is required after changing a plan or running again.

Restart bootstrap can receive only an unmarked driver. Its existing original
control, exact sentinel, matching prefs and short lease must still authorize
zero automatic suites with `NOT_EXECUTED`. That server cannot subsequently
become an ordinary QA source server or serve test entries. Bootstrap is not
source-binding or execution credit.

With `--inventory <path>`, the original inventory remains a pre-execution plan
with executed=0. `<path>.source.json` preserves the exact original source
manifest, and `<path>.executed-source.json` records observed result proofs and
binding issues. The existing browser CI retains those two exact metadata files
with its original logs. No profile prefs, credentials, screenshots or product
payloads are added to that artifact.

The binding covers the selected test **entry source inputs** and any observed
unselected eager entry inputs. Later SWC/Solid/Vite transforms, transformed JS
bytes, test harness/compiler integrity, transitive modules, and the original
Mozilla files imported by a generated wrapper's `marker.load()` remain separate
requirements. The eight wrapper entries do not constitute atomic proof of those
original Mozilla tests. Concurrent privileged modification, network-filesystem
durability and kernel-blocked I/O are not qualified by this local-file contract.
OS-specific negative behavior must be tested on that OS; a Linux host result is
not Windows/macOS qualification.

All browser entries remain packaged not-onboarded, product `UNVERIFIED` and
publication unauthorized. The fixed 109-case source plan, Runtime lock,
Fluent/Flasco, signing, manual approval and publisher gates retain their
existing scope. This does not create an Agent action API or qualify a worker
that could attach to a personal browser profile.
