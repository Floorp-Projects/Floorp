// SPDX-License-Identifier: MPL-2.0

/**
 * Deterministic three-way merge for Floorp Notes (cross-client contract).
 *
 * Mirrors the iOS implementation (floorp-ios firefox-ios/Floorp/FloorpNotesSync.swift)
 * so winners, conflict copies, and ordering are identical on both clients.
 * Kept free of browser/rpc dependencies so it is directly unit-testable.
 */

import type { Note } from "../types/note.ts";

export type { Note } from "../types/note.ts";

export interface NoteSnapshot {
  id: string;
  title: string;
  content: string;
  updatedAt: number;
}

export interface MergeResult {
  merged: Note[];
  hadConflicts: boolean;
  conflictCount: number;
}

export type NotesMergeErrorCode = "blank-note-id" | "duplicate-note-id";
export type NotesMergeSource = "base" | "local" | "remote";

export class NotesMergeError extends Error {
  constructor(
    readonly code: NotesMergeErrorCode,
    readonly source: NotesMergeSource,
    readonly noteID: string,
  ) {
    super(`${code} in ${source}: ${noteID}`);
    this.name = "NotesMergeError";
  }
}

type Resolution =
  | { kind: "none" }
  | { kind: "note"; note: Note }
  | { kind: "conflict"; winner: Note; loser: Note };

const CONFLICT_ID_PREFIX = "floorp-sync-conflict-";

/** Deterministic SHA-256 over bytes (synchronous, Web Crypto free). */
function sha256Bytes(input: Uint8Array): Uint8Array {
  // Compact SHA-256 (FIPS 180-4) implementation.
  const K = new Uint32Array([
    0x428a2f98,
    0x71374491,
    0xb5c0fbcf,
    0xe9b5dba5,
    0x3956c25b,
    0x59f111f1,
    0x923f82a4,
    0xab1c5ed5,
    0xd807aa98,
    0x12835b01,
    0x243185be,
    0x550c7dc3,
    0x72be5d74,
    0x80deb1fe,
    0x9bdc06a7,
    0xc19bf174,
    0xe49b69c1,
    0xefbe4786,
    0x0fc19dc6,
    0x240ca1cc,
    0x2de92c6f,
    0x4a7484aa,
    0x5cb0a9dc,
    0x76f988da,
    0x983e5152,
    0xa831c66d,
    0xb00327c8,
    0xbf597fc7,
    0xc6e00bf3,
    0xd5a79147,
    0x06ca6351,
    0x14292967,
    0x27b70a85,
    0x2e1b2138,
    0x4d2c6dfc,
    0x53380d13,
    0x650a7354,
    0x766a0abb,
    0x81c2c92e,
    0x92722c85,
    0xa2bfe8a1,
    0xa81a664b,
    0xc24b8b70,
    0xc76c51a3,
    0xd192e819,
    0xd6990624,
    0xf40e3585,
    0x106aa070,
    0x19a4c116,
    0x1e376c08,
    0x2748774c,
    0x34b0bcb5,
    0x391c0cb3,
    0x4ed8aa4a,
    0x5b9cca4f,
    0x682e6ff3,
    0x748f82ee,
    0x78a5636f,
    0x84c87814,
    0x8cc70208,
    0x90befffa,
    0xa4506ceb,
    0xbef9a3f7,
    0xc67178f2,
  ]);
  const H = new Uint32Array([
    0x6a09e667,
    0xbb67ae85,
    0x3c6ef372,
    0xa54ff53a,
    0x510e527f,
    0x9b05688c,
    0x1f83d9ab,
    0x5be0cd19,
  ]);
  const bitLength = input.byteLength * 8;
  const padded = new Uint8Array(
    Math.ceil((input.byteLength + 9) / 64) * 64,
  );
  padded.set(input);
  padded[input.byteLength] = 0x80;
  const view = new DataView(padded.buffer);
  view.setUint32(padded.byteLength - 8, Math.floor(bitLength / 0x100000000));
  view.setUint32(padded.byteLength - 4, bitLength >>> 0);

  const w = new Uint32Array(64);
  for (let offset = 0; offset < padded.byteLength; offset += 64) {
    for (let i = 0; i < 16; i++) {
      w[i] = view.getUint32(offset + i * 4);
    }
    for (let i = 16; i < 64; i++) {
      const s0 = ((w[i - 15] >>> 7) | (w[i - 15] << 25)) ^
        ((w[i - 15] >>> 18) | (w[i - 15] << 14)) ^ (w[i - 15] >>> 3);
      const s1 = ((w[i - 2] >>> 17) | (w[i - 2] << 15)) ^
        ((w[i - 2] >>> 19) | (w[i - 2] << 13)) ^ (w[i - 2] >>> 10);
      w[i] = (w[i - 16] + s0 + w[i - 7] + s1) >>> 0;
    }
    let [a, b, c, d, e, f, g, h] = H;
    for (let i = 0; i < 64; i++) {
      const S1 = ((e >>> 6) | (e << 26)) ^ ((e >>> 11) | (e << 21)) ^
        ((e >>> 25) | (e << 7));
      const ch = (e & f) ^ (~e & g);
      const temp1 = (h + S1 + ch + K[i] + w[i]) >>> 0;
      const S0 = ((a >>> 2) | (a << 30)) ^ ((a >>> 13) | (a << 19)) ^
        ((a >>> 22) | (a << 10));
      const maj = (a & b) ^ (a & c) ^ (b & c);
      const temp2 = (S0 + maj) >>> 0;
      h = g;
      g = f;
      f = e;
      e = (d + temp1) >>> 0;
      d = c;
      c = b;
      b = a;
      a = (temp1 + temp2) >>> 0;
    }
    H[0] = (H[0] + a) >>> 0;
    H[1] = (H[1] + b) >>> 0;
    H[2] = (H[2] + c) >>> 0;
    H[3] = (H[3] + d) >>> 0;
    H[4] = (H[4] + e) >>> 0;
    H[5] = (H[5] + f) >>> 0;
    H[6] = (H[6] + g) >>> 0;
    H[7] = (H[7] + h) >>> 0;
  }
  const digest = new Uint8Array(32);
  const digestView = new DataView(digest.buffer);
  for (let i = 0; i < 8; i++) {
    digestView.setUint32(i * 4, H[i]);
  }
  return digest;
}

function hex(bytes: Uint8Array): string {
  return [...bytes].map((b) => b.toString(16).padStart(2, "0")).join("");
}

/** u64 big-endian bytes for a non-negative integer. */
function u64be(value: number): Uint8Array {
  const out = new Uint8Array(8);
  new DataView(out.buffer).setBigUint64(0, BigInt(Math.trunc(value)), false);
  return out;
}

/** Appends a u64-length-prefixed UTF-8 string (desktop wire layout). */
function appendPrefixed(target: number[], text: string): void {
  const bytes = new TextEncoder().encode(text);
  for (const b of u64be(bytes.byteLength)) target.push(b);
  for (const b of bytes) target.push(b);
}

/** Canonical bytes for a note; identical to the iOS contract. */
export function canonicalData(note: Note): Uint8Array {
  const bytes: number[] = [];
  appendPrefixed(bytes, note.id);
  appendPrefixed(bytes, note.title);
  appendPrefixed(bytes, note.content);
  for (const b of u64be(note.createdAt)) bytes.push(b);
  for (const b of u64be(note.updatedAt)) bytes.push(b);
  return new Uint8Array(bytes);
}

/** Deterministic ordering: updatedAt ascending, ties broken bytewise. */
export function precedes(local: Note, remote: Note): boolean {
  if (local.updatedAt !== remote.updatedAt) {
    return local.updatedAt < remote.updatedAt;
  }
  const lhs = canonicalData(local);
  const rhs = canonicalData(remote);
  const length = Math.min(lhs.byteLength, rhs.byteLength);
  for (let i = 0; i < length; i++) {
    if (lhs[i] !== rhs[i]) return lhs[i] < rhs[i];
  }
  return lhs.byteLength < rhs.byteLength;
}

function sameUserContent(local: Note, remote: Note): boolean {
  return local.title === remote.title && local.content === remote.content;
}

function conflictCopyID(losingNote: Note, probe: number): string {
  const bytes: number[] = [];
  appendPrefixed(bytes, losingNote.id);
  for (const b of canonicalData(losingNote)) bytes.push(b);
  if (probe > 0) appendPrefixed(bytes, String(probe));
  return CONFLICT_ID_PREFIX + hex(sha256Bytes(new Uint8Array(bytes)));
}

function orderChanged(
  candidate: string[],
  base: string[],
  availableIDs: Set<string>,
): boolean {
  const baseIDs = new Set(base);
  const candidateBaseOrder = candidate.filter((id) =>
    baseIDs.has(id) && availableIDs.has(id)
  );
  const candidateBaseIDs = new Set(candidateBaseOrder);
  const comparableBaseOrder = base.filter((id) =>
    candidateBaseIDs.has(id) && availableIDs.has(id)
  );
  return candidateBaseOrder.join("\u0000") !==
    comparableBaseOrder.join("\u0000");
}

function appendOrder(
  source: string[],
  availableIDs: Set<string>,
  conflictIDByOriginal: Map<string, string>,
  orderedIDs: string[],
  appendedIDs: Set<string>,
): void {
  for (const id of source) {
    if (!availableIDs.has(id)) continue;
    if (!appendedIDs.has(id)) {
      appendedIDs.add(id);
      orderedIDs.push(id);
    }
    const conflictID = conflictIDByOriginal.get(id);
    if (
      conflictID && availableIDs.has(conflictID) && !appendedIDs.has(conflictID)
    ) {
      appendedIDs.add(conflictID);
      orderedIDs.push(conflictID);
    }
  }
}

/**
 * Three-way merge implementing the approved cross-client contract
 * (docs/floorp-notes-sync-architecture.md). Deterministic: the winner is the
 * note that does not precede the other (updatedAt ascending, bytewise tie
 * break), and conflict copies get a canonical `floorp-sync-conflict-` ID so
 * iOS and Desktop derive identical results.
 */
export function mergeNotesThreeWay(
  local: Note[],
  remote: Note[],
  base: Record<string, NoteSnapshot>,
): MergeResult {
  validateNoteIDs("local", local.map((note) => note.id));
  validateNoteIDs("remote", remote.map((note) => note.id));
  validateNoteIDs("base", Object.keys(base));

  const localMap = new Map<string, Note>();
  for (const n of local) localMap.set(n.id, n);
  const remoteMap = new Map<string, Note>();
  for (const n of remote) remoteMap.set(n.id, n);

  const hasChanged = (note: Note, baseSnap?: NoteSnapshot): boolean => {
    if (!baseSnap) return true;
    return note.title !== baseSnap.title || note.content !== baseSnap.content;
  };

  const resolve = (
    baseN: NoteSnapshot | undefined,
    localN: Note | undefined,
    remoteN: Note | undefined,
  ): Resolution => {
    if (!baseN) {
      if (!localN && !remoteN) return { kind: "none" };
      if (localN && !remoteN) return { kind: "note", note: localN };
      if (!localN && remoteN) return { kind: "note", note: remoteN };
      return resolveConcurrent(localN!, remoteN!);
    }
    if (!localN && !remoteN) return { kind: "none" };
    if (localN && !remoteN) {
      return hasChanged(localN, baseN)
        ? { kind: "note", note: localN }
        : { kind: "none" };
    }
    if (!localN && remoteN) {
      return hasChanged(remoteN, baseN)
        ? { kind: "note", note: remoteN }
        : { kind: "none" };
    }
    const localChanged = hasChanged(localN!, baseN);
    const remoteChanged = hasChanged(remoteN!, baseN);
    if (!localChanged && !remoteChanged) {
      return { kind: "note", note: remoteN! };
    }
    if (localChanged && !remoteChanged) return { kind: "note", note: localN! };
    if (!localChanged && remoteChanged) return { kind: "note", note: remoteN! };
    return resolveConcurrent(localN!, remoteN!);
  };

  const resolveConcurrent = (localN: Note, remoteN: Note): Resolution => {
    if (sameUserContent(localN, remoteN)) return { kind: "note", note: localN };
    if (precedes(localN, remoteN)) {
      return { kind: "conflict", winner: remoteN, loser: localN };
    }
    return { kind: "conflict", winner: localN, loser: remoteN };
  };

  const originalIDs = [
    ...localMap.keys(),
    ...remoteMap.keys(),
    ...Object.keys(base),
  ].filter((id, index, array) => array.indexOf(id) === index);

  const resolutionsByID = new Map<string, Resolution>();
  for (const id of originalIDs) {
    resolutionsByID.set(
      id,
      resolve(base[id], localMap.get(id), remoteMap.get(id)),
    );
  }

  const mergedByID = new Map<string, Note>();
  const conflictIDByOriginal = new Map<string, string>();
  const generatedConflictCopies = new Map<string, Note>();
  const existingIDs = new Set<string>(originalIDs);
  let hadConflicts = false;

  for (const id of originalIDs) {
    const resolution = resolutionsByID.get(id);
    if (!resolution) continue;
    if (resolution.kind === "none") continue;
    if (resolution.kind === "note") {
      mergedByID.set(id, resolution.note);
      continue;
    }
    hadConflicts = true;
    mergedByID.set(id, resolution.winner);
    const copy = availableConflictCopy(
      resolution.loser,
      existingIDs,
      resolutionsByID,
      generatedConflictCopies,
    );
    conflictIDByOriginal.set(id, copy.note.id);
    if (copy.shouldInsert) {
      mergedByID.set(copy.note.id, copy.note);
      generatedConflictCopies.set(copy.note.id, copy.note);
    }
  }

  const availableIDs = new Set(mergedByID.keys());
  const localOrderChanged = orderChanged(
    [...localMap.keys()],
    Object.keys(base),
    availableIDs,
  );
  const remoteOrderChanged = orderChanged(
    [...remoteMap.keys()],
    Object.keys(base),
    availableIDs,
  );
  const primaryOrder = !localOrderChanged && remoteOrderChanged
    ? [...remoteMap.keys()]
    : [...localMap.keys()];
  const secondaryOrder = !localOrderChanged && remoteOrderChanged
    ? [...localMap.keys()]
    : [...remoteMap.keys()];

  const orderedIDs: string[] = [];
  const appendedIDs = new Set<string>();
  appendOrder(
    primaryOrder,
    availableIDs,
    conflictIDByOriginal,
    orderedIDs,
    appendedIDs,
  );
  appendOrder(
    secondaryOrder,
    availableIDs,
    conflictIDByOriginal,
    orderedIDs,
    appendedIDs,
  );
  appendOrder(
    Object.keys(base),
    availableIDs,
    conflictIDByOriginal,
    orderedIDs,
    appendedIDs,
  );
  for (const id of [...availableIDs].sort()) {
    if (!appendedIDs.has(id)) {
      appendedIDs.add(id);
      orderedIDs.push(id);
    }
  }

  const merged = orderedIDs
    .filter((id) => mergedByID.has(id))
    .map((id) => mergedByID.get(id)!);

  console.info(
    `[Floorp Notes] Three-way merge result: ${merged.length} notes, hadConflicts=${hadConflicts}`,
  );
  return {
    merged,
    hadConflicts,
    conflictCount: conflictIDByOriginal.size,
  };
}

function validateNoteIDs(source: NotesMergeSource, noteIDs: string[]): void {
  const seen = new Set<string>();
  for (const noteID of noteIDs) {
    if (noteID.trim().length === 0) {
      throw new NotesMergeError("blank-note-id", source, noteID);
    }
    if (seen.has(noteID)) {
      throw new NotesMergeError("duplicate-note-id", source, noteID);
    }
    seen.add(noteID);
  }
}

function sameWireNote(lhs: Note, rhs: Note): boolean {
  return lhs.id === rhs.id &&
    lhs.title === rhs.title &&
    lhs.content === rhs.content &&
    lhs.createdAt === rhs.createdAt &&
    lhs.updatedAt === rhs.updatedAt;
}

function availableConflictCopy(
  losingNote: Note,
  existingIDs: Set<string>,
  originalResolutions: Map<string, Resolution>,
  generatedConflictCopies: Map<string, Note>,
): { note: Note; shouldInsert: boolean } {
  for (let probe = 0; probe <= 1_000; probe++) {
    const candidate: Note = {
      id: conflictCopyID(losingNote, probe),
      title: losingNote.title ? `${losingNote.title} (Conflict)` : "(Conflict)",
      content: losingNote.content,
      createdAt: losingNote.createdAt,
      updatedAt: losingNote.updatedAt,
    };
    if (!existingIDs.has(candidate.id)) {
      existingIDs.add(candidate.id);
      return { note: candidate, shouldInsert: true };
    }
    const generated = generatedConflictCopies.get(candidate.id);
    if (generated && sameWireNote(generated, candidate)) {
      return { note: generated, shouldInsert: false };
    }
    const originalResolution = originalResolutions.get(candidate.id);
    if (originalResolution) {
      const existing = originalResolution.kind === "note"
        ? originalResolution.note
        : originalResolution.kind === "conflict"
        ? originalResolution.winner
        : undefined;
      if (existing && sameWireNote(existing, candidate)) {
        return { note: existing, shouldInsert: false };
      }
    }
  }
  throw new Error(`conflict ID exhausted for note ${losingNote.id}`);
}
