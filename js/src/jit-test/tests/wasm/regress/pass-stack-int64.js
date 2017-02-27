var params = '';
var locals = '';
for (let i = 0; i < 20; i++) {
    params += '(param i64) ';
    locals += `(get_local ${i}) `;
}

wasmEvalText(`
(module
    (func
        ${params}
        (call 0 ${locals})
    )
)
`);
