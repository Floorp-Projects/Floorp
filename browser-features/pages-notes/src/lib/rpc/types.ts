import type { NRSettingsParentFunctions } from "../../../../modules/common/defines.ts";

export type NotesPrefFunctions = Pick<
  NRSettingsParentFunctions,
  | "getBoolPref"
  | "getIntPref"
  | "getStringPref"
  | "setBoolPref"
  | "setIntPref"
  | "setStringPref"
>;
