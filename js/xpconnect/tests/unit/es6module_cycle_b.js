export const name = "b";

import { name as cName } from "./es6module_cycle_c.js";

export let loaded = true;

export function getValueFromC() {
  return cName;
}
