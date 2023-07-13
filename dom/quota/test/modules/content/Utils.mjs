/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This file expectes the SpecialPowers to be available in the scope
// it is loaded into.
/* global SpecialPowers */

import { getUsageForOrigin } from "./StorageUtils.mjs";

export const Utils = {
  async getCachedOriginUsage() {
    const principal = SpecialPowers.wrap(document).nodePrincipal;
    const result = await getUsageForOrigin(principal, true);
    return result.usage;
  },
};
