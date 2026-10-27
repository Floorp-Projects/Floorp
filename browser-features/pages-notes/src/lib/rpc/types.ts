import type { NRSettingsParentFunctions } from "../../../../modules/common/defines.ts";
import type {
  NotesStorageFunctions,
  NotesStorageService,
} from "../../../../modules/common/notes-storage-types.ts";

export type NotesPrefFunctions =
  & Pick<
    NRSettingsParentFunctions,
    | "getBoolPref"
    | "getIntPref"
    | "getStringPref"
    | "setBoolPref"
    | "setIntPref"
    | "setStringPref"
  >
  & NotesStorageFunctions;

export interface NotesStorageModule {
  FloorpNotesStorage: NotesStorageService;
}
