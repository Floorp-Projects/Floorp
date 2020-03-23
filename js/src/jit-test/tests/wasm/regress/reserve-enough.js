// Bug 1281131 - be sure to reserve enough stack space

wasmEvalText(
`(module
  (func $func0
   ${loopy(100)}
   (nop)))`);

function loopy(n) {
    if (n == 0)
	return "(nop)";
    return `(block $out${n} (loop $in${n} ${loopy(n-1)}))`;
}
