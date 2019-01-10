// |jit-test| allow-oom; skip-if: !('oomTest' in this)

oomTest(async function() {}, { keepFailing: true });
