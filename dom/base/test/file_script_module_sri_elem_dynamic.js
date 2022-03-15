const { f } = await import("./file_script_module_sri_elem_dynamic_imported.js");
f();

window.dispatchEvent(new Event("test_evaluated"));
