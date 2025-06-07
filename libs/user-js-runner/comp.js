import {
  setBoolPref,
  setIntPref,
  setStringPref,
} from "local:component/noraneko";
function user_pref(prefName, prefValue) {
  if (prefValue === true || prefValue === false) {
    setBoolPref(prefName, prefValue);
  } else if (!Number.isNaN(prefValue)) {
    setIntPref(prefName, prefValue);
  } else {
    setStringPref(prefName, prefValue);
  }
}
export function run(src) {
  // biome-ignore lint/security/noGlobalEval: This is transpiled to wasm and must ran with time-limit
  eval(src);
}
