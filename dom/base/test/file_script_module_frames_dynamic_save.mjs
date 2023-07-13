const { f } = await import("./file_script_module_frames_dynamic_shared.mjs");
f();

window.dispatchEvent(new Event("test_evaluated"));
