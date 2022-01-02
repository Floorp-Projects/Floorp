(new Int8Array(3)).join();
[Math.abs, Math.abs].forEach(x => {
    try {
        Int8Array.prototype.join.call(x);
    } catch(e) {}
});
