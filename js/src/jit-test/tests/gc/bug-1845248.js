for (let v1 = 0; v1 < 10; v1++) { }

for (let v5 = 0; v5 < 5; v5++) {
    function f6(a7, a8, a9, a10) {
        try { a8(a10, a9); } catch (e) {}
        try { new a8(v5, a9, v5, a8); } catch (e) {}
        gc();

        const o15 = {
            __proto__: a7,
        };

        for (let v20 = 0; v20 < 150; v20++) { }
    }
    f6(f6, f6, f6);
}
