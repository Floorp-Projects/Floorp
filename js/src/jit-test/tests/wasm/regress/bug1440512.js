// |jit-test| skip-if: !wasmDebugSupport()
var g = newGlobal({newCompartment: true});
var dbg = new g.Debugger(this);
var dbg = new Debugger;
var kWasmH0 = 0;
var kWasmH1 = 0x61;
var kWasmH2 = 0x73;
var kWasmH3 = 0x6d;
var kWasmV0 = 0x1;
var kWasmV1 = 0;
var kWasmV2 = 0;
var kWasmV3 = 0;
let kTypeSectionCode = 1; // Function signature declarations
let kImportSectionCode = 2; // Import declarations
let kFunctionSectionCode = 3; // Function declarations
let kExportSectionCode = 7; // Exports
let kCodeSectionCode = 10; // Function code
let kWasmFunctionTypeForm = 0x60;
let kWasmI32 = 0x7f;
let kExternalFunction = 0;
let kSig_i_i = makeSig([kWasmI32], [kWasmI32]);
function makeSig(params, results) {
    return {
        params: params,
        results: results
    };
}
let kExprEnd = 0x0b;
let kExprGetLocal = 0x20;
class Binary extends Array {
    emit_u8(val) {
        this.push(val);
    }
    emit_u32v(val) {
        while (true) {
            let v = val & 0xff;
            val = val >>> 7;
                this.push(v);
                break;
        }
    }
    emit_bytes(data) {
        for (let i = 0; i < data.length; i++) {
            this.push(data[i] & 0xff);
        }
    }
    emit_string(string) {
        let string_utf8 = unescape(encodeURIComponent(string));
        this.emit_u32v(string_utf8.length);
        for (let i = 0; i < string_utf8.length; i++) {
            this.emit_u8(string_utf8.charCodeAt(i));
        }
    }
    emit_header() {
        this.push(kWasmH0, kWasmH1, kWasmH2, kWasmH3, kWasmV0, kWasmV1, kWasmV2, kWasmV3);
    }
    emit_section(section_code, content_generator) {
        this.emit_u8(section_code);
        let section = new Binary;
        content_generator(section);
        this.emit_u32v(section.length);
        for (var i = 0; i < section.length; i++) this.push(section[i]);
    }
}
class WasmFunctionBuilder {
    constructor(module, name, type_index) {
        this.module = module;
    }
    exportAs(name) {
        this.module.addExport(name, this.index);
    }
    addBody(body) {
        this.body = body.slice();
        this.body.push(kExprEnd);
        return this;
    }
}
class WasmModuleBuilder {
    constructor() {
        this.types = [];
        this.imports = [];
        this.exports = [];
        this.functions = [];
    }
    addType(type) {
        this.types.push(type);
    }
    addFunction(name, type) {
        let type_index = (typeof type) == "number" ? type : this.addType(type);
        let func = new WasmFunctionBuilder(this, name, type_index);
        this.functions.push(func);
        return func;
    }
    addImport(module = "", name, type) {
        this.imports.push({
            module: module,
            name: name,
            kind: kExternalFunction,
        });
    }
    addExport(name, index) {
        this.exports.push({
            name: name,
        });
    }
    toArray(debug = false) {
        let binary = new Binary;
        let wasm = this;
        binary.emit_header();
        if (wasm.types.length > 0) {
            binary.emit_section(kTypeSectionCode, section => {
                section.emit_u32v(wasm.types.length);
                for (let type of wasm.types) {
                    section.emit_u8(kWasmFunctionTypeForm);
                    section.emit_u32v(type.params.length);
                    for (let param of type.params) {
                        section.emit_u8(param);
                    }
                    section.emit_u32v(type.results.length);
                    for (let result of type.results) {
                        section.emit_u8(result);
                    }
                }
            });
            binary.emit_section(kImportSectionCode, section => {
                section.emit_u32v(wasm.imports.length);
                for (let imp of wasm.imports) {
                    section.emit_string(imp.module);
                    section.emit_string(imp.name || '');
                    section.emit_u8(imp.kind);
                    if (imp.kind == kExternalFunction) {
                        section.emit_u32v(imp.type);
                    }
                }
            });
            binary.emit_section(kFunctionSectionCode, section => {
                section.emit_u32v(wasm.functions.length);
                for (let func of wasm.functions) {
                    section.emit_u32v(func.type_index);
                }
            });
        }
        var mem_export = (wasm.memory !== undefined && wasm.memory.exp);
        var exports_count = wasm.exports.length + (mem_export ? 1 : 0);
        if (exports_count > 0) {
            binary.emit_section(kExportSectionCode, section => {
                section.emit_u32v(exports_count);
                for (let exp of wasm.exports) {
                    section.emit_string(exp.name);
                    section.emit_u8(exp.kind);
                    section.emit_u8(0);
                }
            });
        }
        if (wasm.start_index !== undefined) {
        }
        if (wasm.functions.length > 0) {
            binary.emit_section(kCodeSectionCode, section => {
                section.emit_u32v(wasm.functions.length);
                for (let func of wasm.functions) {
                    let local_decls = [];
                    let header = new Binary;
                    header.emit_u32v(local_decls.length);
                    section.emit_u32v(header.length + func.body.length);
                    section.emit_bytes(header);
                    section.emit_bytes(func.body);
                }
            });
        }
        let num_function_names = 0;
        let num_functions_with_local_names = 0;
        if (num_function_names > 0 || num_functions_with_local_names > 0 || wasm.name !== undefined) {
            binary.emit_section(kUnknownSectionCode, section => {
                if (num_function_names > 0) {
                }
            });
        }
        return binary;
    }
    toBuffer(debug = false) {
        let bytes = this.toArray(debug);
        let buffer = new ArrayBuffer(bytes.length);
        let view = new Uint8Array(buffer);
        for (let i = 0; i < bytes.length; i++) {
            let val = bytes[i];
            view[i] = val | 0;
        }
        return buffer;
    }
    toModule(debug = false) {
        return new WebAssembly.Module(this.toBuffer(debug));
    }
}
const verifyHeap = gc;
function testProperties(obj) {
    for (let i = 0; i < 3; i++) {
        verifyHeap();
    }
}
(function ExportedFunctionTest() {
    var builder = new WasmModuleBuilder();
    builder.addFunction("exp", kSig_i_i).addBody([
        kExprGetLocal, 0,
    ]).exportAs("exp");
    let module1 = builder.toModule();
    let instance1 = new WebAssembly.Instance(module1);
    let g = instance1.exports.exp;
    testProperties(g);
    builder.addImport("imp", "func", kSig_i_i);
    let module2 = builder.toModule();
    let instance2 = new WebAssembly.Instance(module2, {
        imp: {
            func: g
        }
    });
})();
