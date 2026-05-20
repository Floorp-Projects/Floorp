// SPDX-License-Identifier: MPL-2.0

/**
 * NMA Public API (stub)
 *
 * Currently exports types and state only. Core I/O (Sigstore verification,
 * archive loading, hash computation) and the loader orchestrator are not yet
 * ported — they depend on @freedomofpress/sigstore-browser which is not yet
 * a Floorp dependency.
 *
 * Next steps to complete the NMA port:
 *   1. Add @freedomofpress/sigstore-browser to Floorp deno.json imports
 *   2. Port nma/core.ts  (FS/Network/Crypto IO)
 *   3. Port nma/loader.ts (orchestrator: initializeNMALoader, hotswap, etc.)
 *   4. Wire initNMASystem() call into bridge/loader-features/loader/index.ts
 */

export * from "./types.ts";
export * from "./state.ts";
