// |jit-test| skip-if: !('oomTest' in this)

oomTest(() => parseModule('import v from "mod";'));
fullcompartmentchecks(true);
