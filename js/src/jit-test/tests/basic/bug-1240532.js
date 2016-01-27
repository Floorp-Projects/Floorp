if (helperThreadCount() > 0) {
    evalInWorker("try { newGlobal({principal : 5}); } catch (e) {}");
}
