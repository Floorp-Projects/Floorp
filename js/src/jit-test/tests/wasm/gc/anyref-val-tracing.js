// |jit-test| skip-if: !wasmReftypesEnabled()

gczeal(14, 1);
let { exports } = wasmEvalText(`(module
    (global $anyref (import "glob" "externref") anyref)
    (func (export "get") (result anyref) global.get $anyref)
)`, {
    glob: {
        externref: { sentinel: "lol" },
    }
});
assertEq(exports.get().sentinel, "lol");
