// |jit-test| skip-if: !wasmGcEnabled()

function linkGlobals(typeSection, exportInit, linkType) {
    let {global} = wasmEvalText(`(module
        ${typeSection}
        (global (export "global") ${linkType} ${exportInit})
    )`).exports;

    wasmEvalText(`(module
        ${typeSection}
        (import "" "global"
            (global ${linkType})
        )
    )`, {"": {global}});
}

function linkTables(typeSection, exportInit, linkType) {
    let {table} = wasmEvalText(`(module
        ${typeSection}
        (table (export "table") ${linkType} (elem (${exportInit})))
    )`).exports;

    wasmEvalText(`(module
        ${typeSection}
        (import "" "table"
            (table 0 1 ${linkType})
        )
    )`, {"": {table}});
}

function linkFuncs(typeSection, exportInit, linkType) {
    let {func} = wasmEvalText(`(module
        ${typeSection}
        (func
            (export "func")
            (param ${linkType})
            (result ${linkType})
            unreachable
        )
    )`).exports;

    wasmEvalText(`(module
        ${typeSection}
        (import "" "func"
            (func (param ${linkType}) (result ${linkType}))
        )
    )`, {"": {func}});
}

const TESTS = [
    [
        "(type (struct (field i32)))",
        "ref.null 0",
        "(ref null 0)",
    ],
    [
        "(type (array i32))",
        "ref.null 0",
        "(ref null 0)",
    ],
    [
        "(type (struct (field i32))) (type (struct (field (ref 0))))",
        "ref.null 1",
        "(ref null 1)",
    ]
];

for (let test of TESTS) {
    linkGlobals(...test);
    linkTables(...test);
    linkFuncs(...test);
}
