import { f } from "./file_script_module_frames_import_shared.js";
f();

window.dispatchEvent(new Event("test_evaluated"));
