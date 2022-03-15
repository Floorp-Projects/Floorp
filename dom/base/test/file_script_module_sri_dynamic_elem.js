const { f } = await import("./file_script_module_sri_dynamic_elem_imported.js");
f();

window.dispatchEvent(new Event("test_evaluated"));
