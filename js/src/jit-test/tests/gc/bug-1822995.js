function f0(a1, a2, a3) {
    try {
        a2(a1, a2);
    } catch(e5) {
        const v7 = new Set();
        const v8 = v7.add();
        function f9(a10, a11) {
            a11.sameZoneAs = v8;
            return this;
        }
        f9(v8, f9).newGlobal(f9).blackRoot(a1);
    }
    return a2;
}
const v17 = this.wrapWithProto(f0, this);
const v19 = Proxy.revocable(f0, v17);
v19.proxy(v17, v17);
v19.proxy(v19, v17);
gc();
