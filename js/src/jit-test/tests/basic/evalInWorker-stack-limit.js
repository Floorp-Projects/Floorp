try {
    evalInWorker(`
        function f() { f(); }
        try { f(); } catch(e) {}
    `);
} catch(e) {
    assertEq(e.toString().includes("--no-threads"), true);
}
