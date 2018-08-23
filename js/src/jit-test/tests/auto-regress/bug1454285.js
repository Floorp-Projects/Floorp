// IteratorClose in for-await-of with block-scoped function.

// Non-local-jump without target.

(async function() {
    for await (let c of []) {
        function f() {};
        return;
    }
})();
(async function() {
    for await (let c of []) {
        function f() {};
        break;
    }
})();
(async function() {
    for await (let c of []) {
        function f() {};
        continue;
    }
})();

// Non-local-jump with target.

(async function() {
    for (let x of []) {
        x: for (let y of []) {
            for await (let c of []) {
                function f() {};
                return;
            }
        }
    }
})();
(async function() {
    for (let x of []) {
        x: for (let y of []) {
            for await (let c of []) {
                function f() {};
                break x;
            }
        }
    }
})();
(async function() {
    for (let x of []) {
        x: for (let y of []) {
            for await (let c of []) {
                function f() {};
                continue x;
            }
        }
    }
})();

(async function() {
    for await (let x of []) {
        x: for await (let y of []) {
            for await (let c of []) {
                function f() {};
                return;
            }
        }
    }
})();
(async function() {
    for await (let x of []) {
        x: for await (let y of []) {
            for await (let c of []) {
                function f() {};
                break x;
            }
        }
    }
})();
(async function() {
    for await (let x of []) {
        x: for await (let y of []) {
            for await (let c of []) {
                function f() {};
                continue x;
            }
        }
    }
})();
