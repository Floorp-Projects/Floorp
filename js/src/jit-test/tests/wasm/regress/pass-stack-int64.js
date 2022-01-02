var params = '';
var locals = '';
for (let i = 0; i < 20; i++) {
    params += '(param i64) ';
    locals += `(local.get ${i}) `;
}

wasmEvalText(`
(module
    (func
        ${params}
        (call 0 ${locals})
    )
)
`);
