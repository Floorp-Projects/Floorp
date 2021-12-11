gczeal(14, 1);
let { exports } = wasmEvalText(`(module
    (global $externref (import "glob" "externref") externref)
    (func (export "get") (result externref) global.get $externref)
)`, {
    glob: {
        externref: { sentinel: "lol" },
    }
});
assertEq(exports.get().sentinel, "lol");
