// SPDX-License-Identifier: MPL-2.0

import { isHubIdentifier } from "../HubPageDescriptorValidation.ts";
import type {
  FeaturePackageAuthoringMetadata,
  FeaturePackageCompatibility,
  FeaturePackageErrorCode,
  FeaturePackageManifest,
  FeaturePackagePolicy,
  FeaturePackagePolicySnapshot,
  ParsedFeaturePackageEnvelope,
  SignedFeaturePackageEnvelope,
  VerifiedFeaturePackage,
} from "./types.ts";

export const FEATURE_PACKAGE_MAX_BYTES = 1_048_576;
export const FEATURE_PACKAGE_SIGNING_MESSAGE_MAX_BYTES = 8_192;
export const FEATURE_PACKAGE_SOURCE_MAX_BYTES = 131_072;
export const FEATURE_PACKAGE_METADATA_MAX_BYTES = 8_192;
export const FEATURE_PACKAGE_POLICY_MAX_BYTES = 16_384;
export const FEATURE_PACKAGE_PKCS8_MAX_BYTES = 1_024;
export const FEATURE_PACKAGE_DOMAIN = "floorp-feature-package:v1\n";

const encoder = new TextEncoder();
const buildIdPattern = /^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$/;
const keyIdPattern = /^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$/;
const versionPattern = /^[A-Za-z0-9][A-Za-z0-9.-]{0,63}$/;
const capabilityPattern = /^[a-z][a-z0-9-]{0,63}$/;
const errorMessages: Record<FeaturePackageErrorCode, string> = {
  "invalid-envelope": "Package envelope is invalid.",
  "invalid-policy": "Trusted package policy is invalid.",
  "invalid-metadata": "Package authoring metadata is invalid.",
  "invalid-source": "Package source is invalid.",
  "invalid-signing-key": "Signing key is invalid.",
  "unsupported-crypto": "Required cryptography is unavailable.",
  "unsupported-version": "Package format version is unsupported.",
  "untrusted-key": "Package signing key is not trusted.",
  "invalid-signature": "Package signature is invalid.",
  "source-digest-mismatch": "Package source digest or length does not match.",
  "feature-mismatch": "Package feature does not match policy.",
  "host-incompatible": "Package host compatibility does not match policy.",
  "runtime-incompatible":
    "Package runtime compatibility does not match policy.",
  "capability-denied": "Package requests a capability not granted by policy.",
  "sequence-too-low": "Package release sequence is below policy minimum.",
  "size-limit": "Package input exceeds the allowed size.",
};

export class FeaturePackageError extends Error {
  constructor(readonly code: FeaturePackageErrorCode) {
    super(errorMessages[code]);
    this.name = "FeaturePackageError";
  }
}

function fail(code: FeaturePackageErrorCode): never {
  throw new FeaturePackageError(code);
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function isUint8Array(value: unknown): value is Uint8Array {
  try {
    return ArrayBuffer.isView(value) &&
      Object.prototype.toString.call(value) === "[object Uint8Array]";
  } catch {
    return false;
  }
}

function hasExactKeys(
  value: Record<string, unknown>,
  keys: readonly string[],
): boolean {
  try {
    const actual = Reflect.ownKeys(value);
    return actual.length === keys.length &&
      actual.every((key) => typeof key === "string" && keys.includes(key)) &&
      keys.every((key) => Object.hasOwn(value, key));
  } catch {
    return false;
  }
}

function isPositiveSafeInteger(value: unknown): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) && value > 0;
}

function asciiCompare(left: string, right: string): number {
  return left < right ? -1 : left > right ? 1 : 0;
}

function sortedUniqueStrings(
  value: unknown,
  maximum: number,
  pattern: RegExp,
  code: FeaturePackageErrorCode,
  requireSorted: boolean,
): string[] {
  if (!Array.isArray(value) || value.length > maximum) fail(code);
  const items: string[] = [];
  for (const item of value) {
    if (typeof item !== "string" || !pattern.test(item)) fail(code);
    items.push(item);
  }
  const sorted = [...items].sort(asciiCompare);
  if (new Set(items).size !== items.length) fail(code);
  if (requireSorted && items.some((item, index) => item !== sorted[index])) {
    fail(code);
  }
  return sorted;
}

function utf8Bytes(value: string, code: FeaturePackageErrorCode): Uint8Array {
  const bytes = encoder.encode(value);
  let decoded: string;
  try {
    decoded = new TextDecoder("utf-8", { fatal: true, ignoreBOM: true }).decode(
      bytes,
    );
  } catch {
    fail(code);
  }
  if (decoded !== value) fail(code);
  return bytes;
}

function decodeBase64Url(
  value: unknown,
  expectedBytes: number,
  code: FeaturePackageErrorCode,
): Uint8Array {
  if (
    typeof value !== "string" ||
    value.length !== Math.ceil(expectedBytes * 8 / 6) ||
    !/^[A-Za-z0-9_-]+$/.test(value)
  ) fail(code);
  let decoded: Uint8Array;
  try {
    const base64 = value.replaceAll("-", "+").replaceAll("_", "/");
    const binary = atob(base64 + "=".repeat((4 - base64.length % 4) % 4));
    decoded = Uint8Array.from(binary, (character) => character.charCodeAt(0));
  } catch {
    fail(code);
  }
  if (decoded.length !== expectedBytes || encodeBase64Url(decoded) !== value) {
    fail(code);
  }
  return decoded;
}

function encodeBase64Url(bytes: Uint8Array): string {
  let binary = "";
  for (const byte of bytes) binary += String.fromCharCode(byte);
  return btoa(binary).replaceAll("+", "-").replaceAll("/", "_").replace(
    /=+$/,
    "",
  );
}

function bytesEqual(left: Uint8Array, right: Uint8Array): boolean {
  return left.length === right.length &&
    left.every((byte, index) => byte === right[index]);
}

function arrayBuffer(bytes: Uint8Array): ArrayBuffer {
  const buffer = new ArrayBuffer(bytes.byteLength);
  new Uint8Array(buffer).set(bytes);
  return buffer;
}

function canonicalCompatibility(value: FeaturePackageCompatibility): string {
  return JSON.stringify({
    hostSchema: value.hostSchema,
    hostBuildId: value.hostBuildId,
    runtimeBuildIds: value.runtimeBuildIds,
    capabilities: value.capabilities,
  });
}

function canonicalManifest(value: FeaturePackageManifest): string {
  return JSON.stringify({
    featureId: value.featureId,
    version: value.version,
    releaseSequence: value.releaseSequence,
    sourceBytes: value.sourceBytes,
    sourceSha256: value.sourceSha256,
    compatibility: JSON.parse(canonicalCompatibility(value.compatibility)),
  });
}

function canonicalHeader(
  keyId: string,
  manifest: FeaturePackageManifest,
): string {
  return JSON.stringify({
    formatVersion: 1,
    keyId,
    manifest: JSON.parse(canonicalManifest(manifest)),
  });
}

function canonicalEnvelope(
  keyId: string,
  manifest: FeaturePackageManifest,
  source: string,
  signature: string,
): string {
  return JSON.stringify({
    formatVersion: 1,
    keyId,
    manifest: JSON.parse(canonicalManifest(manifest)),
    source,
    signature,
  });
}

function snapshotPolicy(
  value: FeaturePackagePolicy,
): FeaturePackagePolicySnapshot {
  if (
    !isRecord(value) || !hasExactKeys(value, [
      "featureId",
      "hostSchema",
      "hostBuildId",
      "runtimeBuildId",
      "allowedCapabilities",
      "minimumReleaseSequence",
      "trustedKeys",
    ])
  ) fail("invalid-policy");
  const featureId = value.featureId;
  const hostSchema = value.hostSchema;
  const hostBuildId = value.hostBuildId;
  const runtimeBuildId = value.runtimeBuildId;
  const minimumReleaseSequence = value.minimumReleaseSequence;
  if (
    !isHubIdentifier(featureId) || !isPositiveSafeInteger(hostSchema) ||
    typeof hostBuildId !== "string" || !buildIdPattern.test(hostBuildId) ||
    typeof runtimeBuildId !== "string" ||
    !buildIdPattern.test(runtimeBuildId) ||
    typeof minimumReleaseSequence !== "number" ||
    !Number.isSafeInteger(minimumReleaseSequence) || minimumReleaseSequence < 0
  ) fail("invalid-policy");
  const allowedCapabilities = sortedUniqueStrings(
    value.allowedCapabilities,
    16,
    capabilityPattern,
    "invalid-policy",
    false,
  );
  const keyValues = value.trustedKeys;
  if (!Array.isArray(keyValues) || keyValues.length > 16) {
    fail("invalid-policy");
  }
  const trustedKeys: { keyId: string; publicKey: Uint8Array }[] = [];
  const keyIds = new Set<string>();
  for (const candidate of keyValues) {
    if (
      !isRecord(candidate) ||
      !hasExactKeys(candidate, ["keyId", "publicKey"])
    ) fail("invalid-policy");
    const keyId = candidate.keyId;
    if (
      typeof keyId !== "string" || !keyIdPattern.test(keyId) ||
      keyIds.has(keyId)
    ) {
      fail("invalid-policy");
    }
    keyIds.add(keyId);
    trustedKeys.push({
      keyId,
      publicKey: decodeBase64Url(candidate.publicKey, 32, "invalid-policy"),
    });
  }
  return {
    featureId,
    hostSchema,
    hostBuildId,
    runtimeBuildId,
    allowedCapabilities,
    minimumReleaseSequence,
    trustedKeys,
  };
}

function parseCompatibility(value: unknown): FeaturePackageCompatibility {
  if (
    !isRecord(value) || !hasExactKeys(value, [
      "hostSchema",
      "hostBuildId",
      "runtimeBuildIds",
      "capabilities",
    ])
  ) fail("invalid-envelope");
  const hostSchema = value.hostSchema;
  const hostBuildId = value.hostBuildId;
  if (
    !isPositiveSafeInteger(hostSchema) || typeof hostBuildId !== "string" ||
    !buildIdPattern.test(hostBuildId)
  ) fail("invalid-envelope");
  const runtimeBuildIds = sortedUniqueStrings(
    value.runtimeBuildIds,
    16,
    buildIdPattern,
    "invalid-envelope",
    true,
  );
  if (runtimeBuildIds.length === 0) fail("invalid-envelope");
  const capabilities = sortedUniqueStrings(
    value.capabilities,
    16,
    capabilityPattern,
    "invalid-envelope",
    true,
  );
  return Object.freeze({
    hostSchema,
    hostBuildId,
    runtimeBuildIds: Object.freeze(runtimeBuildIds),
    capabilities: Object.freeze(capabilities),
  });
}

function parseManifest(value: unknown): FeaturePackageManifest {
  if (
    !isRecord(value) || !hasExactKeys(value, [
      "featureId",
      "version",
      "releaseSequence",
      "sourceBytes",
      "sourceSha256",
      "compatibility",
    ])
  ) fail("invalid-envelope");
  const featureId = value.featureId;
  const version = value.version;
  const releaseSequence = value.releaseSequence;
  const sourceBytes = value.sourceBytes;
  const sourceSha256 = value.sourceSha256;
  if (
    !isHubIdentifier(featureId) || typeof version !== "string" ||
    !versionPattern.test(version) || !isPositiveSafeInteger(releaseSequence) ||
    !isPositiveSafeInteger(sourceBytes) || typeof sourceSha256 !== "string" ||
    !/^[a-f0-9]{64}$/.test(sourceSha256)
  ) fail("invalid-envelope");
  return Object.freeze({
    featureId,
    version,
    releaseSequence,
    sourceBytes,
    sourceSha256,
    compatibility: parseCompatibility(value.compatibility),
  });
}

function parseEnvelope(bytes: Uint8Array): ParsedFeaturePackageEnvelope {
  if (bytes.length === 0 || bytes.length > FEATURE_PACKAGE_MAX_BYTES) {
    fail("size-limit");
  }
  if (bytes[0] === 0xef && bytes[1] === 0xbb && bytes[2] === 0xbf) {
    fail("invalid-envelope");
  }
  let parsed: unknown;
  try {
    parsed = JSON.parse(
      new TextDecoder("utf-8", { fatal: true }).decode(bytes),
    );
  } catch {
    fail("invalid-envelope");
  }
  if (
    !isRecord(parsed) || !hasExactKeys(parsed, [
      "formatVersion",
      "keyId",
      "manifest",
      "source",
      "signature",
    ])
  ) fail("invalid-envelope");
  if (parsed.formatVersion !== 1) fail("unsupported-version");
  const keyId = parsed.keyId;
  const source = parsed.source;
  if (typeof keyId !== "string" || !keyIdPattern.test(keyId)) {
    fail("invalid-envelope");
  }
  if (typeof source !== "string") fail("invalid-source");
  if (source.length > FEATURE_PACKAGE_SOURCE_MAX_BYTES) fail("size-limit");
  const sourceBytes = utf8Bytes(source, "invalid-source");
  if (
    sourceBytes.length === 0 ||
    sourceBytes.length > FEATURE_PACKAGE_SOURCE_MAX_BYTES
  ) {
    fail(
      sourceBytes.length > FEATURE_PACKAGE_SOURCE_MAX_BYTES
        ? "size-limit"
        : "invalid-source",
    );
  }
  const manifest = parseManifest(parsed.manifest);
  const signature = parsed.signature;
  if (typeof signature !== "string") fail("invalid-envelope");
  const signatureBytes = decodeBase64Url(
    signature,
    64,
    "invalid-envelope",
  );
  const envelope: SignedFeaturePackageEnvelope = {
    formatVersion: 1,
    keyId,
    manifest,
    source,
    signature,
  };
  const canonicalText = canonicalEnvelope(
    keyId,
    manifest,
    source,
    signature,
  );
  const canonicalBytes = encoder.encode(canonicalText);
  if (!bytesEqual(canonicalBytes, bytes)) fail("invalid-envelope");
  const signingText = FEATURE_PACKAGE_DOMAIN + canonicalHeader(keyId, manifest);
  const signingBytes = encoder.encode(signingText);
  if (signingBytes.length > FEATURE_PACKAGE_SIGNING_MESSAGE_MAX_BYTES) {
    fail("size-limit");
  }
  return {
    envelope,
    sourceBytes,
    signatureBytes,
    canonicalBytes,
    signingBytes,
    manifest,
  };
}

function snapshotAuthoringMetadata(
  value: FeaturePackageAuthoringMetadata,
): FeaturePackageAuthoringMetadata {
  if (
    !isRecord(value) || !hasExactKeys(value, [
      "featureId",
      "version",
      "releaseSequence",
      "compatibility",
    ])
  ) fail("invalid-metadata");
  const featureId = value.featureId;
  const version = value.version;
  const releaseSequence = value.releaseSequence;
  if (
    !isHubIdentifier(featureId) || typeof version !== "string" ||
    !versionPattern.test(version) || !isPositiveSafeInteger(releaseSequence)
  ) fail("invalid-metadata");
  const sourceCompatibility = value.compatibility;
  if (
    !isRecord(sourceCompatibility) || !hasExactKeys(sourceCompatibility, [
      "hostSchema",
      "hostBuildId",
      "runtimeBuildIds",
      "capabilities",
    ])
  ) fail("invalid-metadata");
  const hostSchema = sourceCompatibility.hostSchema;
  const hostBuildId = sourceCompatibility.hostBuildId;
  if (
    !isPositiveSafeInteger(hostSchema) || typeof hostBuildId !== "string" ||
    !buildIdPattern.test(hostBuildId)
  ) fail("invalid-metadata");
  const runtimeBuildIds = sortedUniqueStrings(
    sourceCompatibility.runtimeBuildIds,
    16,
    buildIdPattern,
    "invalid-metadata",
    false,
  );
  if (runtimeBuildIds.length === 0) fail("invalid-metadata");
  const capabilities = sortedUniqueStrings(
    sourceCompatibility.capabilities,
    16,
    capabilityPattern,
    "invalid-metadata",
    false,
  );
  const compatibility: FeaturePackageCompatibility = Object.freeze({
    hostSchema,
    hostBuildId,
    runtimeBuildIds: Object.freeze(runtimeBuildIds),
    capabilities: Object.freeze(capabilities),
  });
  const metadata = Object.freeze({
    featureId,
    version,
    releaseSequence,
    compatibility,
  });
  if (
    encoder.encode(JSON.stringify(metadata)).length >
      FEATURE_PACKAGE_METADATA_MAX_BYTES
  ) {
    fail("size-limit");
  }
  return metadata;
}

function subtleCrypto(): SubtleCrypto {
  try {
    const subtle = globalThis.crypto?.subtle;
    if (!subtle) fail("unsupported-crypto");
    return subtle;
  } catch {
    fail("unsupported-crypto");
  }
}

function hex(bytes: Uint8Array): string {
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0")).join(
    "",
  );
}

export async function signFeaturePackage(
  metadataInput: FeaturePackageAuthoringMetadata,
  sourceInput: string,
  keyIdInput: string,
  privateKey: CryptoKey,
): Promise<Uint8Array> {
  let metadata: FeaturePackageAuthoringMetadata;
  try {
    metadata = snapshotAuthoringMetadata(metadataInput);
  } catch (error: unknown) {
    if (error instanceof FeaturePackageError) throw error;
    fail("invalid-metadata");
  }
  const keyId = keyIdInput;
  if (typeof keyId !== "string" || !keyIdPattern.test(keyId)) {
    fail("invalid-metadata");
  }
  if (typeof sourceInput !== "string") fail("invalid-source");
  const source = sourceInput;
  if (source.length > FEATURE_PACKAGE_SOURCE_MAX_BYTES) fail("size-limit");
  const sourceBytes = utf8Bytes(source, "invalid-source");
  if (
    sourceBytes.length === 0 ||
    sourceBytes.length > FEATURE_PACKAGE_SOURCE_MAX_BYTES
  ) {
    fail(
      sourceBytes.length > FEATURE_PACKAGE_SOURCE_MAX_BYTES
        ? "size-limit"
        : "invalid-source",
    );
  }
  let validSigningKey = false;
  try {
    validSigningKey = !!privateKey && privateKey.type === "private" &&
      privateKey.algorithm.name === "Ed25519" &&
      privateKey.usages.includes("sign");
  } catch {
    fail("invalid-signing-key");
  }
  if (!validSigningKey) fail("invalid-signing-key");
  const subtle = subtleCrypto();
  const manifest: FeaturePackageManifest = Object.freeze({
    featureId: metadata.featureId,
    version: metadata.version,
    releaseSequence: metadata.releaseSequence,
    sourceBytes: sourceBytes.length,
    sourceSha256: "",
    compatibility: metadata.compatibility,
  });
  let sourceDigest: ArrayBuffer;
  try {
    sourceDigest = await subtle.digest("SHA-256", arrayBuffer(sourceBytes));
  } catch {
    fail("unsupported-crypto");
  }
  const signedManifest: FeaturePackageManifest = Object.freeze({
    ...manifest,
    sourceSha256: hex(new Uint8Array(sourceDigest)),
  });
  const signingBytes = encoder.encode(
    FEATURE_PACKAGE_DOMAIN + canonicalHeader(keyId, signedManifest),
  );
  if (signingBytes.length > FEATURE_PACKAGE_SIGNING_MESSAGE_MAX_BYTES) {
    fail("size-limit");
  }
  let signature: ArrayBuffer;
  try {
    signature = await subtle.sign(
      { name: "Ed25519" },
      privateKey,
      arrayBuffer(signingBytes),
    );
  } catch {
    fail("invalid-signing-key");
  }
  const signatureText = encodeBase64Url(new Uint8Array(signature));
  const envelopeText = canonicalEnvelope(
    keyId,
    signedManifest,
    source,
    signatureText,
  );
  const envelopeBytes = encoder.encode(envelopeText);
  if (envelopeBytes.length > FEATURE_PACKAGE_MAX_BYTES) fail("size-limit");
  return envelopeBytes;
}

export async function verifySignedFeaturePackage(
  inputBytes: Uint8Array,
  policyInput: FeaturePackagePolicy,
): Promise<VerifiedFeaturePackage> {
  if (!isUint8Array(inputBytes)) fail("invalid-envelope");
  let packageBytes: Uint8Array;
  try {
    const inputByteLength: unknown = inputBytes.byteLength;
    if (
      typeof inputByteLength !== "number" ||
      !Number.isSafeInteger(inputByteLength) || inputByteLength < 0
    ) fail("invalid-envelope");
    if (inputByteLength > FEATURE_PACKAGE_MAX_BYTES) fail("size-limit");
    packageBytes = new Uint8Array(inputByteLength);
    packageBytes.set(inputBytes);
  } catch (error: unknown) {
    if (error instanceof FeaturePackageError) throw error;
    fail("invalid-envelope");
  }
  let policy: FeaturePackagePolicySnapshot;
  try {
    policy = snapshotPolicy(policyInput);
  } catch (error: unknown) {
    if (error instanceof FeaturePackageError) throw error;
    fail("invalid-policy");
  }
  const parsed = parseEnvelope(packageBytes);
  const { envelope, manifest } = parsed;
  if (manifest.featureId !== policy.featureId) fail("feature-mismatch");
  if (
    manifest.compatibility.hostSchema !== policy.hostSchema ||
    manifest.compatibility.hostBuildId !== policy.hostBuildId
  ) fail("host-incompatible");
  if (!manifest.compatibility.runtimeBuildIds.includes(policy.runtimeBuildId)) {
    fail("runtime-incompatible");
  }
  if (
    manifest.compatibility.capabilities.some((capability) =>
      !policy.allowedCapabilities.includes(capability)
    )
  ) fail("capability-denied");
  if (manifest.releaseSequence < policy.minimumReleaseSequence) {
    fail("sequence-too-low");
  }
  const trustedKey = policy.trustedKeys.find((key) =>
    key.keyId === envelope.keyId
  );
  if (!trustedKey) fail("untrusted-key");
  const subtle = subtleCrypto();
  let sourceDigest: ArrayBuffer;
  let packageDigest: ArrayBuffer;
  let publicKey: CryptoKey;
  try {
    [sourceDigest, packageDigest, publicKey] = await Promise.all([
      subtle.digest("SHA-256", arrayBuffer(parsed.sourceBytes)),
      subtle.digest("SHA-256", arrayBuffer(parsed.canonicalBytes)),
      subtle.importKey(
        "raw",
        arrayBuffer(trustedKey.publicKey),
        { name: "Ed25519" },
        false,
        ["verify"],
      ),
    ]);
  } catch {
    fail("unsupported-crypto");
  }
  if (
    manifest.sourceBytes !== parsed.sourceBytes.length ||
    manifest.sourceSha256 !== hex(new Uint8Array(sourceDigest))
  ) {
    fail("source-digest-mismatch");
  }
  let valid: boolean;
  try {
    valid = await subtle.verify(
      { name: "Ed25519" },
      publicKey,
      arrayBuffer(parsed.signatureBytes),
      arrayBuffer(parsed.signingBytes),
    );
  } catch {
    fail("unsupported-crypto");
  }
  if (!valid) fail("invalid-signature");
  return Object.freeze({
    formatVersion: 1,
    keyId: envelope.keyId,
    manifest,
    source: envelope.source,
    packageSha256: hex(new Uint8Array(packageDigest)),
  });
}

export function parseFeaturePackagePolicyJson(
  bytes: Uint8Array,
): FeaturePackagePolicy {
  if (bytes.length > 16_384) fail("size-limit");
  try {
    const value: unknown = JSON.parse(
      new TextDecoder("utf-8", { fatal: true }).decode(bytes),
    );
    if (!isRecord(value)) fail("invalid-policy");
    return value as unknown as FeaturePackagePolicy;
  } catch (error: unknown) {
    if (error instanceof FeaturePackageError) throw error;
    return fail("invalid-policy");
  }
}
