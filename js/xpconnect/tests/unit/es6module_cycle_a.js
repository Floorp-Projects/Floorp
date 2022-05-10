export const name = "a";

import { name as bName } from "./es6module_cycle_b.js";

export let loaded = true;

export function getValueFromB() {
  return bName;
}
