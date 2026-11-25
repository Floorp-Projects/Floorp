# Local signed feature package format v1

M3a adds a bounded local package admission primitive. It does not activate
packages or replace FeatureHost's pinned catalog. A successful verification
result is descriptive data, not an execution-authority token; future activation
must reverify persisted package bytes against current core policy and
FeatureHost journal constraints.

## Wire format

The following envelope is schematic; its placeholder values do not form a
verifiable package.

The UTF-8 envelope is compact `JSON.stringify` output with exactly this property
order:

```json
{
  "formatVersion": 1,
  "keyId": "local-key",
  "manifest": {
    "featureId": "hub-panel-menu",
    "version": "v1",
    "releaseSequence": 1,
    "sourceBytes": 12,
    "sourceSha256": "<64 lowercase hex>",
    "compatibility": {
      "hostSchema": 2,
      "hostBuildId": "<build>",
      "runtimeBuildIds": ["<runtime-build>"],
      "capabilities": []
    }
  },
  "source": "<exact source string>",
  "signature": "<unpadded base64url Ed25519 signature>"
}
```

The signature is Ed25519 over UTF-8 `floorp-feature-package:v1\n` followed by
compact canonical JSON of `{formatVersion:1,keyId,manifest}`. Property order is
fixed by the closed schema. Arrays on the wire are unique and sorted by ASCII
order. Strings use ECMAScript JSON escaping without Unicode normalization. The
signed manifest contains the exact UTF-8 source byte count and SHA-256; source
text is never parsed, imported, evaluated, or persisted by the verifier.

The envelope is limited to 1 MiB, the signing message to 8 KiB, and source to
1..131072 UTF-8 bytes. Metadata is limited to 8 KiB, policy to 16 KiB, and
PKCS#8 input to 1 KiB. Public keys and signatures use canonical unpadded
base64url encodings of 32 and 64 bytes respectively.

Feature IDs use the existing lowercase ASCII hub identifier grammar and are
1..48 characters. Versions match `[A-Za-z0-9][A-Za-z0-9.-]{0,63}` and are opaque
labels. Key IDs match `[A-Za-z0-9][A-Za-z0-9._-]{0,63}`. Release sequences and
host schemas are positive safe integers; `sourceBytes` is the positive UTF-8
byte count, and `sourceSha256` is 64 lowercase hexadecimal characters. Host and
runtime BuildIDs match `[A-Za-z0-9][A-Za-z0-9._-]{0,127}`. Compatibility has
1..16 unique runtime BuildIDs and 0..16 unique capabilities matching
`[a-z][a-z0-9-]{0,63}`. Wire arrays must be ASCII-sorted; authoring arrays are
sorted by the signer.

The trusted policy is caller-supplied and has exactly `featureId`, `hostSchema`,
`hostBuildId`, `runtimeBuildId`, `allowedCapabilities`,
`minimumReleaseSequence`, and `trustedKeys`. The sequence floor is an inclusive
input check, not persisted rollback protection. There may be 0..16 trusted keys
with distinct key IDs; each entry contains only `keyId` and a canonical
base64url `publicKey`. A missing key denies the package. Verification errors
expose bounded reason codes and fixed messages, not package contents.

`verifySignedFeaturePackage(bytes, policy)` returns detached, deeply immutable
metadata, the source string, key ID, and whole-envelope SHA-256. The verifier
does not evaluate source or write state. Callers must not treat that result as
an execution-authority token.

## Local use

Create an authoring metadata JSON file with only `featureId`, `version`,
`releaseSequence`, and `compatibility`, plus a source file and a private Ed25519
PKCS#8 file owned by the caller:

```sh
deno run -A tools/feature-package.ts pack \
  --metadata metadata.json --source feature.js --key-id local-key \
  --key-pkcs8 signing-key.pk8 --out feature-package.json
deno run -A tools/feature-package.ts verify \
  --package feature-package.json --policy local-policy.json
```

The authoring metadata JSON contains exactly `featureId`, `version`,
`releaseSequence`, and `compatibility`. The local policy JSON contains the exact
trusted policy fields described above. `pack` writes only a newly created output
file; `verify` prints a small result without source text or private key
material. The CLI result is local policy verification, not Floorp production
approval. The core API reports fixed `FeaturePackageError` codes and messages;
the CLI maps failures to its own fixed error codes and messages without printing
underlying exception details.

M3a's verifier and CLI alone do not configure production trust, persist a
sequence high-water mark, provide durable rollback admission, reverify a
persistent cache, deliver key rotation or revocation, or integrate with the
production loader. Those are later design increments. No network or production
keys are involved; the existing pinned FeatureHost experiment remains active.

## Verified package cache

M3b stores exact canonical envelope bytes below the current profile in the
dedicated `floorp-signed-package-cache-v1` namespace. A cache instance is bound
to one validated feature ID; its test factory accepts only a lowercase UUID.
Package filenames are the 64-character lowercase SHA-256 of the complete
envelope. `put` verifies before writing, publishes through the fixed
`candidate.tmp` staging file with create-only/flush and no-overwrite move, then
bounded-reads and re-verifies the final file. `read` re-verifies on every call
against that call's detached current-policy snapshot; cached results and package
files do not grant trust.

Each feature cache is limited to 32 envelopes and 1 MiB per envelope. New
packages are rejected when the bounded incremental directory scan finds an
unknown entry, unsafe file type, or exhausted capacity. The cache never evicts,
executes source, modifies FeatureHost catalogs or journals, or persists policy
or trust.

Each root has one in-process owner reserved before initialization. Opening,
open, and closing roots reject duplicate owners. The cache admits at most 16
running-plus-queued operations; failed operations return their slots. Accepted
operations reserve their FIFO position before synchronously snapshotting caller
inputs and policy; storage and cryptographic work begin when that position
reaches the head. close() stops admission synchronously and drains accepted work
before releasing its owner. Storage checks controlled ancestors and entries for
static links and non-directory types, uses create-only staging and a
no-overwrite move, and re-verifies every read under the supplied current policy.

These checks do not prevent same-user replacement races, provide generic
multiprocess locking, or guarantee full power-loss durability. Dynamic
activation, durable rollback/replay state, eviction and loader integration
remain deferred to later increments.

## Native M3d transport and engine composition acceptance

The native acceptance driver covers package transport and experimental signed
M3c engine composition. It requires Linux, OpenSSL 3, NSS `certutil`, Deno, and
an already-built browser containing `FeaturePackageDownloader.sys.mjs`. It
refuses an existing profile and requires a new absolute
`FLOORP_TEST_PROFILE_DIR` for each run. Only the public ephemeral CA is
installed into that owned profile with NSS trust `C,,`.

```bash
FLOORP_TEST_PROFILE_DIR="$HOME/.cache/floorp-m3d-acceptance-$(date +%s)" deno run -A tools/src/m3d_package_acquisition_acceptance.ts
```

The run also covers three experimental composition cases that join acquisition
to the signed M3c facade: `download-lifecycle-inert`,
`download-lifecycle-transactions` and `close-during-download`. They use
`createSignedFeatureHostForTests` with `excludeWindowsForTests`, so they
exercise the lifecycle engine only: no window attachment, UI surface or
production startup path is covered or claimed. Each case owns a fresh UUID
journal and cache root under the owned profile, requires proven facade cleanup,
and removes only its own roots.

The composition cases exercise the signed M3c engine through a test-only facade.
Production loader integration and automatic startup remain deferred. The
downloader diagnostics gate is entered explicitly per observation because this
launch is not a test-mode startup.
