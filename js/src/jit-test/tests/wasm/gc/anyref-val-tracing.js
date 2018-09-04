if (!wasmGcEnabled()) {
    quit(0);
}

gczeal(14, 1);
let { exports } = wasmEvalText(`(module
    (global $anyref (import "glob" "anyref") anyref)
    (func (export "get") (result anyref) get_global $anyref)
)`, {
    glob: {
        anyref: { sentinel: "lol" },
    }
});
assertEq(exports.get().sentinel, "lol");
