function f() {
    for (var i = 0; i < 12; i++) {
        try {
            eval("");

            void x;
        } catch (e) { }
    }
}

f();

let x;
