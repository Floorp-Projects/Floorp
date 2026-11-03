/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** Package authentication, separate from structural CRX header parsing.
 * CRX3 wire format: components/crx_file/crx3.proto in Chromium.
 * Developer signatures bind the original Chrome ID; they do not prove that
 * Google reviewed or distributed a package.
 */
import { extractZipFromCRX, parseCRXHeader } from "./CRXParser.sys.mts";
import { isChromeExtensionId, MAX_CRX_SIZE } from "./Constants.sys.mts";

export interface VerifiedCRX {
  readonly zipData: ArrayBuffer;
  readonly chromeId: string;
  readonly version: 2 | 3;
  readonly algorithm: "rsa-sha1" | "rsa-sha256" | "ecdsa-sha256";
  readonly publicKeySha256: string;
}

export class CRXVerificationError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "CRXVerificationError";
  }
}

const MAX_HEADER_SIZE = 16 * 1024 * 1024;
const MAX_PROOFS = 32;
const MAX_KEY_SIZE = 64 * 1024;
const SIGNATURE_CONTEXT = new TextEncoder().encode("CRX3 SignedData\x00");

interface Proof {
  publicKey: Uint8Array<ArrayBuffer>;
  signature: Uint8Array<ArrayBuffer>;
  algorithm: "rsa-sha256" | "ecdsa-sha256";
}

interface Field {
  number: number;
  bytes?: Uint8Array<ArrayBuffer>;
}

function fail(message: string): never {
  throw new CRXVerificationError(message);
}

/** Reject truncation, overflow, groups, and invalid protobuf field numbers. */
function* fields(bytes: Uint8Array<ArrayBuffer>): Generator<Field> {
  let offset = 0;
  const readUint32 = (): number => {
    let value = 0;
    for (let index = 0; index < 5; index++) {
      if (offset === bytes.length) fail("Truncated CRX3 protobuf varint");
      const byte = bytes[offset++];
      if (index === 4 && byte > 15) fail("Overflowing CRX3 protobuf varint");
      value += (byte & 127) * 2 ** (index * 7);
      if (byte < 128) return value;
    }
    return fail("Invalid CRX3 protobuf varint");
  };
  const advance = (length: number): void => {
    if (length > bytes.length - offset) fail("Truncated CRX3 protobuf field");
    offset += length;
  };
  while (offset < bytes.length) {
    const tag = readUint32();
    const number = tag >>> 3;
    if (!number) fail("Invalid CRX3 protobuf field number");
    const wireType = tag & 7;
    if (wireType === 2) {
      const length = readUint32();
      const start = offset;
      advance(length);
      yield { number, bytes: bytes.subarray(start, offset) };
    } else {
      if (wireType === 0) {
        for (let index = 0; ; index++) {
          if (offset === bytes.length) fail("Truncated CRX3 protobuf value");
          const byte = bytes[offset++];
          if (index === 9 && byte > 1) fail("Overflowing CRX3 protobuf value");
          if (byte < 128) break;
        }
      } else if (wireType === 1) {
        advance(8);
      } else if (wireType === 5) {
        advance(4);
      } else {
        fail("Unsupported CRX3 protobuf wire type");
      }
      yield { number };
    }
  }
}

function parseProof(
  bytes: Uint8Array<ArrayBuffer>,
  algorithm: Proof["algorithm"],
): Proof {
  let publicKey: Uint8Array<ArrayBuffer> | undefined;
  let signature: Uint8Array<ArrayBuffer> | undefined;
  for (const field of fields(bytes)) {
    if (field.number === 1) {
      if (publicKey || !field.bytes?.length)
        fail("Invalid CRX3 proof public key");
      publicKey = field.bytes;
    } else if (field.number === 2) {
      if (signature || !field.bytes?.length)
        fail("Invalid CRX3 proof signature");
      signature = field.bytes;
    }
  }
  if (
    !publicKey ||
    !signature ||
    publicKey.length > MAX_KEY_SIZE ||
    signature.length > MAX_KEY_SIZE
  )
    fail("Missing or oversized CRX3 proof");
  return { publicKey, signature, algorithm };
}

function chromeId(bytes: Uint8Array<ArrayBuffer>): string {
  return Array.from(bytes, (byte) =>
    String.fromCharCode(97 + (byte >> 4), 97 + (byte & 15)),
  ).join("");
}

async function keyIdentity(publicKey: Uint8Array<ArrayBuffer>) {
  const digest = new Uint8Array(
    await crypto.subtle.digest("SHA-256", publicKey),
  );
  return {
    chromeId: chromeId(digest.subarray(0, 16)),
    publicKeySha256: Array.from(digest, (byte) =>
      byte.toString(16).padStart(2, "0"),
    ).join(""),
  };
}

/** CRX3 carries DER ECDSA signatures; WebCrypto expects fixed-width r || s. */
function decodeECDSASignature(
  der: Uint8Array<ArrayBuffer>,
): Uint8Array<ArrayBuffer> {
  let offset = 0;
  // P-256 sequence/integers always have short DER lengths.
  if (
    der[offset++] !== 0x30 ||
    der[offset++] !== der.length - 2 ||
    der.length > 72
  )
    fail("Invalid CRX3 ECDSA signature encoding");
  const raw = new Uint8Array(64);
  for (let index = 0; index < 2; index++) {
    if (der[offset++] !== 2) fail("Invalid CRX3 ECDSA integer");
    const length = der[offset++];
    if (!length || length > 33 || length > der.length - offset) {
      fail("Invalid CRX3 ECDSA integer length");
    }
    const integer = der.subarray(offset, offset + length);
    if (
      integer[0] & 128 ||
      (length > 1 && integer[0] === 0 && !(integer[1] & 128))
    ) {
      fail("Invalid CRX3 ECDSA integer encoding");
    }
    const scalar = integer[0] === 0 ? integer.subarray(1) : integer;
    if (scalar.length > 32) fail("Oversized CRX3 ECDSA scalar");
    raw.set(scalar, index * 32 + 32 - scalar.length);
    offset += length;
  }
  if (offset !== der.length) fail("Trailing CRX3 ECDSA signature bytes");
  return raw;
}

async function verifyProof(
  publicKey: Uint8Array<ArrayBuffer>,
  signature: Uint8Array<ArrayBuffer>,
  data: ArrayBuffer,
  algorithm: VerifiedCRX["algorithm"],
): Promise<void> {
  try {
    let key: CryptoKey;
    let valid: boolean;
    if (algorithm === "ecdsa-sha256") {
      key = await crypto.subtle.importKey(
        "spki",
        publicKey,
        { name: "ECDSA", namedCurve: "P-256" },
        false,
        ["verify"],
      );
      valid = await crypto.subtle.verify(
        { name: "ECDSA", hash: "SHA-256" },
        key,
        decodeECDSASignature(signature),
        data,
      );
    } else {
      key = await crypto.subtle.importKey(
        "spki",
        publicKey,
        {
          name: "RSASSA-PKCS1-v1_5",
          hash: algorithm === "rsa-sha1" ? "SHA-1" : "SHA-256",
        },
        false,
        ["verify"],
      );
      valid = await crypto.subtle.verify(
        "RSASSA-PKCS1-v1_5",
        key,
        signature,
        data,
      );
    }
    if (!valid) fail("CRX package signature does not verify");
  } catch (error) {
    if (error instanceof CRXVerificationError) throw error;
    fail("Invalid CRX public key or signature");
  }
}

/** Authenticate an immutable snapshot before exposing ZIP data to conversion. */
export async function verifyCRX(
  data: ArrayBuffer,
  expectedChromeId: string,
): Promise<VerifiedCRX> {
  if (!isChromeExtensionId(expectedChromeId))
    fail("Invalid expected Chrome extension ID");
  if (data.byteLength > MAX_CRX_SIZE) fail("CRX package exceeds size limit");
  const snapshot = data.slice(0);
  const header = parseCRXHeader(snapshot);
  const zipData = extractZipFromCRX(snapshot);
  if (!header || !zipData) fail("Invalid CRX package structure");
  if (header.zipOffset > MAX_HEADER_SIZE) fail("CRX header exceeds size limit");
  if (header.version === 2) {
    const publicKeyLength = header.publicKeyLength ?? 0;
    const signatureLength = header.signatureLength ?? 0;
    if (
      !publicKeyLength ||
      !signatureLength ||
      publicKeyLength > MAX_KEY_SIZE ||
      signatureLength > MAX_KEY_SIZE
    )
      fail("Missing or oversized CRX2 proof");
    const publicKey = new Uint8Array(snapshot, 16, publicKeyLength);
    const signature = new Uint8Array(
      snapshot,
      16 + publicKeyLength,
      signatureLength,
    );
    const identity = await keyIdentity(publicKey);
    if (identity.chromeId !== expectedChromeId)
      fail("CRX public key does not match Chrome extension ID");
    await verifyProof(publicKey, signature, zipData, "rsa-sha1");
    return { ...identity, version: 2, algorithm: "rsa-sha1", zipData };
  }

  const proofs: Proof[] = [];
  let signedHeader: Uint8Array<ArrayBuffer> | undefined;
  for (const field of fields(
    new Uint8Array(snapshot, 12, header.headerLength),
  )) {
    if (field.number === 2 || field.number === 3) {
      if (!field.bytes || proofs.length === MAX_PROOFS)
        fail("Invalid CRX3 proof list");
      proofs.push(
        parseProof(
          field.bytes,
          field.number === 2 ? "rsa-sha256" : "ecdsa-sha256",
        ),
      );
    } else if (field.number === 10000) {
      if (signedHeader || !field.bytes) fail("Invalid CRX3 signed header");
      signedHeader = field.bytes;
    }
  }
  if (!signedHeader || !proofs.length)
    fail("Missing CRX3 signed header or proof");
  let declaredId: Uint8Array<ArrayBuffer> | undefined;
  for (const field of fields(signedHeader)) {
    if (field.number === 1) {
      if (declaredId || field.bytes?.length !== 16)
        fail("Invalid CRX3 signed Chrome ID");
      declaredId = field.bytes;
    }
  }
  if (!declaredId || chromeId(declaredId) !== expectedChromeId) {
    fail("CRX3 signed header does not match Chrome extension ID");
  }
  const signedData = new Uint8Array(
    SIGNATURE_CONTEXT.length + 4 + signedHeader.length + zipData.byteLength,
  );
  signedData.set(SIGNATURE_CONTEXT);
  new DataView(signedData.buffer).setUint32(
    SIGNATURE_CONTEXT.length,
    signedHeader.length,
    true,
  );
  signedData.set(signedHeader, SIGNATURE_CONTEXT.length + 4);
  signedData.set(
    new Uint8Array(zipData),
    SIGNATURE_CONTEXT.length + 4 + signedHeader.length,
  );
  let developer:
    | { publicKeySha256: string; algorithm: Proof["algorithm"] }
    | undefined;
  for (const proof of proofs) {
    const identity = await keyIdentity(proof.publicKey);
    await verifyProof(
      proof.publicKey,
      proof.signature,
      signedData.buffer,
      proof.algorithm,
    );
    if (identity.chromeId === expectedChromeId) {
      developer = {
        publicKeySha256: identity.publicKeySha256,
        algorithm: proof.algorithm,
      };
    }
  }
  if (!developer)
    fail("CRX3 developer public key does not match Chrome extension ID");
  return { ...developer, chromeId: expectedChromeId, version: 3, zipData };
}
