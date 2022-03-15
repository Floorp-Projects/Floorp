const { f } = await import("./file_script_module_frames_dynamic_shared.js");
f();

window.dispatchEvent(new Event("test_evaluated"));
