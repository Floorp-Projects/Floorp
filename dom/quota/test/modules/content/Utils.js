/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

import { getUsageForOrigin } from "/tests/dom/quota/test/modules/StorageUtils.js";

export const Utils = {
  async getCachedOriginUsage() {
    const principal = SpecialPowers.wrap(document).nodePrincipal;
    const result = await getUsageForOrigin(principal, true);
    return result.usage;
  },
};
