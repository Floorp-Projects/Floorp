export const name = "c";

import { name as aName } from "./es6module_cycle_a.js";

export let loaded = true;

export function getValueFromA() {
  return aName;
}
