import { f } from "./file_script_module_frames_import_shared.mjs";

f();

window.dispatchEvent(new Event("test_evaluated"));
