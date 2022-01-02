//|jit-test| error:Error

const options = {
    fileName: "compileToStencil-DATA.js",
    lineNumber: 1,
    forceFullParse: true,
};

compileAndDelazifyAllToStencil(`(x => x)(1)`, options);
