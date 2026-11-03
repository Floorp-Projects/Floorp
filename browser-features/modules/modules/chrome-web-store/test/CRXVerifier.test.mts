// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { CRXVerificationError, verifyCRX } from "../CRXVerifier.sys.mts";
import { parseCRXHeader } from "../CRXParser.sys.mts";
import {
  concatBytes,
  createSignedCRX2,
  createSignedCRX3,
  CRX_ECDSA_TEST_ID,
  CRX_TEST_ID,
  protobufBytes,
  uint32,
} from "./helpers/CRXFixtures.mts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

const ZIP = new Uint8Array([0x50, 0x4b, 0x03, 0x04, 17, 23, 29, 31]);
const OTHER_ID = "a".repeat(32);

async function rejects(
  data: ArrayBuffer,
  expectedId = CRX_TEST_ID,
): Promise<void> {
  try {
    await verifyCRX(data, expectedId);
  } catch (error) {
    assert(
      error instanceof CRXVerificationError,
      "stable verification error type",
    );
    return;
  }
  throw new Error("Unauthenticated CRX must be rejected");
}

function replaceCRX3Header(data: ArrayBuffer, header: Uint8Array): ArrayBuffer {
  const parsed = parseCRXHeader(data);
  assert(parsed?.version === 3, "fixture is CRX3");
  return concatBytes(
    new Uint8Array(data, 0, 8),
    uint32(header.length),
    header,
    new Uint8Array(data, parsed.zipOffset),
  ).buffer;
}

const tests: TestCase[] = [
  {
    name: "CRX2 authenticates RSA SHA-1 payload and derives the Chrome ID",
    async fn() {
      const result = await verifyCRX(await createSignedCRX2(ZIP), CRX_TEST_ID);
      assertEquals(result.version, 2, "CRX2 version");
      assertEquals(result.algorithm, "rsa-sha1", "legacy signature algorithm");
      assertEquals(result.chromeId, CRX_TEST_ID, "ID derived from key");
      assertEquals(
        Array.from(new Uint8Array(result.zipData)).join(),
        ZIP.join(),
        "signed ZIP returned",
      );
      assert(
        /^[a-f0-9]{64}$/.test(result.publicKeySha256),
        "full key fingerprint",
      );
    },
  },
  {
    name: "CRX3 authenticates developer RSA SHA-256 proof",
    async fn() {
      const result = await verifyCRX(await createSignedCRX3(ZIP), CRX_TEST_ID);
      assertEquals(result.version, 3, "CRX3 version");
      assertEquals(result.algorithm, "rsa-sha256", "modern RSA signature");
      assertEquals(
        Array.from(new Uint8Array(result.zipData)).join(),
        ZIP.join(),
        "signed ZIP returned",
      );
      const legacy = await verifyCRX(await createSignedCRX2(ZIP), CRX_TEST_ID);
      assertEquals(
        result.publicKeySha256,
        legacy.publicKeySha256,
        "identity survives CRX format migration",
      );
    },
  },
  {
    name: "CRX3 authenticates P-256 ECDSA DER proof with WebCrypto",
    async fn() {
      const result = await verifyCRX(
        await createSignedCRX3(ZIP, { algorithm: "ecdsa" }),
        CRX_ECDSA_TEST_ID,
      );
      assertEquals(result.algorithm, "ecdsa-sha256", "ECDSA supported");
      assertEquals(
        result.chromeId,
        CRX_ECDSA_TEST_ID,
        "ECDSA developer identity",
      );
    },
  },
  {
    name: "CRX2 and CRX3 reject signed ZIP tampering",
    async fn() {
      for (const data of [
        await createSignedCRX2(ZIP),
        await createSignedCRX3(ZIP),
      ]) {
        const bytes = new Uint8Array(data);
        bytes[bytes.length - 1] ^= 1;
        await rejects(data);
      }
    },
  },
  {
    name: "CRX2 rejects a corrupted RSA signature",
    async fn() {
      const data = await createSignedCRX2(ZIP);
      const keyLength = new DataView(data).getUint32(8, true);
      new Uint8Array(data)[16 + keyLength] ^= 1;
      await rejects(data);
    },
  },
  {
    name: "CRX2 and CRX3 reject another requested ID",
    async fn() {
      await rejects(await createSignedCRX2(ZIP), OTHER_ID);
      await rejects(await createSignedCRX3(ZIP), OTHER_ID);
    },
  },
  {
    name: "CRX3 rejects an attacker key signing another extension ID",
    async fn() {
      await rejects(
        await createSignedCRX3(ZIP, { declaredChromeId: OTHER_ID }),
        OTHER_ID,
      );
    },
  },
  {
    name: "CRX3 requires the exact NUL-terminated signature context",
    async fn() {
      await rejects(
        await createSignedCRX3(ZIP, { signatureContext: "CRX3 SignedData" }),
      );
      await rejects(
        await createSignedCRX3(ZIP, {
          signatureContext: "CRX3 SignedData\x01",
        }),
      );
    },
  },
  {
    name: "CRX3 rejects changed signed header bytes",
    async fn() {
      const data = await createSignedCRX3(ZIP);
      const parsed = parseCRXHeader(data);
      assert(parsed !== null, "fixture header");
      // Last byte of header is the last byte of signed_header_data.crx_id.
      new Uint8Array(data)[parsed.zipOffset - 1] ^= 1;
      await rejects(data);
    },
  },
  {
    name: "CRX3 rejects duplicate signed headers and ID fields",
    async fn() {
      const signedHeader = protobufBytes(1, new Uint8Array(16));
      await rejects(
        await createSignedCRX3(ZIP, {
          extraHeader: protobufBytes(10000, signedHeader),
        }),
      );
      await rejects(
        await createSignedCRX3(ZIP, {
          signedHeader: concatBytes(signedHeader, signedHeader),
        }),
        OTHER_ID,
      );
    },
  },
  {
    name: "CRX3 rejects corrupted and non-DER ECDSA signatures",
    async fn() {
      for (const transformSignature of [
        (signature: Uint8Array<ArrayBuffer>) => {
          const bytes = signature.slice();
          bytes[bytes.length - 1] ^= 1;
          return bytes;
        },
        (signature: Uint8Array<ArrayBuffer>) => signature.subarray(1),
        (signature: Uint8Array<ArrayBuffer>) =>
          concatBytes(signature, new Uint8Array([0])),
      ])
        await rejects(
          await createSignedCRX3(ZIP, {
            algorithm: "ecdsa",
            transformSignature,
          }),
          CRX_ECDSA_TEST_ID,
        );
    },
  },
  {
    name: "CRX3 rejects missing developer proof instead of trusting its declared ID",
    async fn() {
      const data = await createSignedCRX3(ZIP);
      const parsed = parseCRXHeader(data);
      assert(parsed !== null, "fixture header");
      const header = new Uint8Array(data, parsed.zipOffset - 22, 22);
      await rejects(replaceCRX3Header(data, header));
    },
  },
  {
    name: "CRX3 rejects malformed protobuf fields and wire types",
    async fn() {
      for (const extraHeader of [
        new Uint8Array([0]), // field zero
        new Uint8Array([0x80]), // truncated tag
        new Uint8Array([0xff, 0xff, 0xff, 0xff, 0x10]), // tag overflow
        new Uint8Array([0x2a, 20, 1]), // bytes exceed header
        new Uint8Array([0x2a, 0xff, 0xff, 0xff, 0xff, 0x10]), // length overflow
        new Uint8Array([0x2b]), // unsupported group
        new Uint8Array([0x29, 1]), // truncated fixed64
        new Uint8Array([0x2d, 1]), // truncated fixed32
        new Uint8Array([0x28, 0x80]), // truncated uint64
        new Uint8Array([0x28, ...new Array<number>(10).fill(0xff)]), // uint64 overflow
      ])
        await rejects(await createSignedCRX3(ZIP, { extraHeader }));
    },
  },
  {
    name: "CRX3 skips future unknown protobuf fields without changing signed bytes",
    async fn() {
      const result = await verifyCRX(
        await createSignedCRX3(ZIP, {
          extraHeader: concatBytes(
            protobufBytes(5, new Uint8Array([1, 2])),
            new Uint8Array([
              0x30, 0x96, 1, 0x39, 1, 2, 3, 4, 5, 6, 7, 8, 0x45, 1, 2, 3, 4,
            ]),
          ),
        }),
        CRX_TEST_ID,
      );
      assertEquals(
        result.chromeId,
        CRX_TEST_ID,
        "unknown fields remain compatible",
      );
    },
  },
  {
    name: "CRX3 rejects invalid additional proofs",
    async fn() {
      const invalidProof = concatBytes(
        protobufBytes(1, new Uint8Array([1])),
        protobufBytes(2, new Uint8Array([2])),
      );
      await rejects(
        await createSignedCRX3(ZIP, {
          extraHeader: protobufBytes(3, invalidProof),
        }),
      );
    },
  },
  {
    name: "authentication snapshots caller data before asynchronous cryptography",
    async fn() {
      const data = await createSignedCRX3(ZIP);
      const pending = verifyCRX(data, CRX_TEST_ID);
      new Uint8Array(data).fill(0);
      const result = await pending;
      assertEquals(
        Array.from(new Uint8Array(result.zipData)).join(),
        ZIP.join(),
        "verified snapshot returned",
      );
    },
  },
  {
    name: "unsigned structural CRX and invalid Chrome IDs are rejected",
    async fn() {
      await rejects(
        concatBytes(
          new TextEncoder().encode("Cr24"),
          uint32(2),
          uint32(0),
          uint32(0),
          ZIP,
        ).buffer,
      );
      await rejects(await createSignedCRX2(ZIP), "z".repeat(32));
      await rejects(new ArrayBuffer(0));
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("CRXVerifier.test.mts", tests);
}
