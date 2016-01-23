try {
    evalInWorker("evalInWorker('evalInWorker(\"assertEq(1, 1)\")')");
} catch(e) {
    assertEq(e.toString().includes("--no-threads"), true);
}
