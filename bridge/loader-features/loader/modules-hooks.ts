// SPDX-License-Identifier: MPL-2.0

/**
 * Module hooks — backward-compatible re-export shim.
 *
 * Delegates to io/hooks.ts. The `_`-prefixed names are kept here because
 * they were part of the original public surface (internal-but-exported).
 * Do not add logic here.
 */

export { onModuleLoaded } from "./io/mod.ts";

// Functions below are prefixed with `_` to indicate they are internal to the
// loader subsystem. Exported only because index.ts called them directly;
// treat them as implementation details, not public API.
export {
  registerModuleLoadState as _registerModuleLoadState,
  rejectOtherLoadStates as _rejectOtherLoadStates,
} from "./io/mod.ts";
