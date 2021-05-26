"use strict";
var __extends = (this && this.__extends) || (function () {
    var extendStatics = function (d, b) {
        extendStatics = Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
            function (d, b) { for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p]; };
        return extendStatics(d, b);
    };
    return function (d, b) {
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.DevToolsNameGenerator = exports.DevToolsNameResolver = exports.NameSectionReader = exports.WasmDisassembler = exports.LabelMode = exports.NumericNameResolver = exports.DefaultNameResolver = void 0;
/* Copyright 2016 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
var WasmParser_js_1 = require("./WasmParser.js");
var NAME_SECTION_NAME = "name";
var INVALID_NAME_SYMBOLS_REGEX = /[^0-9A-Za-z!#$%&'*+.:<=>?@^_`|~\/\-]/;
var INVALID_NAME_SYMBOLS_REGEX_GLOBAL = new RegExp(INVALID_NAME_SYMBOLS_REGEX.source, "g");
function formatFloat32(n) {
    if (n === 0)
        return 1 / n < 0 ? "-0.0" : "0.0";
    if (isFinite(n))
        return n.toString();
    if (!isNaN(n))
        return n < 0 ? "-inf" : "inf";
    var view = new DataView(new ArrayBuffer(8));
    view.setFloat32(0, n, true);
    var data = view.getInt32(0, true);
    var payload = data & 0x7fffff;
    var canonicalBits = 4194304; // 0x800..0
    if (data > 0 && payload === canonicalBits)
        return "nan";
    // canonical NaN;
    else if (payload === canonicalBits)
        return "-nan";
    return (data < 0 ? "-" : "+") + "nan:0x" + payload.toString(16);
}
function formatFloat64(n) {
    if (n === 0)
        return 1 / n < 0 ? "-0.0" : "0.0";
    if (isFinite(n))
        return n.toString();
    if (!isNaN(n))
        return n < 0 ? "-inf" : "inf";
    var view = new DataView(new ArrayBuffer(8));
    view.setFloat64(0, n, true);
    var data1 = view.getUint32(0, true);
    var data2 = view.getInt32(4, true);
    var payload = data1 + (data2 & 0xfffff) * 4294967296;
    var canonicalBits = 524288 * 4294967296; // 0x800..0
    if (data2 > 0 && payload === canonicalBits)
        return "nan";
    // canonical NaN;
    else if (payload === canonicalBits)
        return "-nan";
    return (data2 < 0 ? "-" : "+") + "nan:0x" + payload.toString(16);
}
function formatI32Array(bytes, count) {
    var dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    var result = [];
    for (var i = 0; i < count; i++)
        result.push("0x" + formatHex(dv.getInt32(i << 2, true), 8));
    return result.join(" ");
}
function formatI8Array(bytes, count) {
    var dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    var result = [];
    for (var i = 0; i < count; i++)
        result.push("" + dv.getInt8(i));
    return result.join(" ");
}
function memoryAddressToString(address, code) {
    var defaultAlignFlags;
    switch (code) {
        case 64768 /* v128_load */:
        case 64769 /* i16x8_load8x8_s */:
        case 64770 /* i16x8_load8x8_u */:
        case 64771 /* i32x4_load16x4_s */:
        case 64772 /* i32x4_load16x4_u */:
        case 64773 /* i64x2_load32x2_s */:
        case 64774 /* i64x2_load32x2_u */:
        case 64775 /* v8x16_load_splat */:
        case 64776 /* v16x8_load_splat */:
        case 64777 /* v32x4_load_splat */:
        case 64778 /* v64x2_load_splat */:
        case 64779 /* v128_store */:
            defaultAlignFlags = 4;
            break;
        case 41 /* i64_load */:
        case 55 /* i64_store */:
        case 43 /* f64_load */:
        case 57 /* f64_store */:
        case 65026 /* i64_atomic_wait */:
        case 65041 /* i64_atomic_load */:
        case 65048 /* i64_atomic_store */:
        case 65055 /* i64_atomic_rmw_add */:
        case 65062 /* i64_atomic_rmw_sub */:
        case 65069 /* i64_atomic_rmw_and */:
        case 65076 /* i64_atomic_rmw_or */:
        case 65083 /* i64_atomic_rmw_xor */:
        case 65090 /* i64_atomic_rmw_xchg */:
        case 65097 /* i64_atomic_rmw_cmpxchg */:
        case 64861 /* v128_load64_zero */:
            defaultAlignFlags = 3;
            break;
        case 40 /* i32_load */:
        case 52 /* i64_load32_s */:
        case 53 /* i64_load32_u */:
        case 54 /* i32_store */:
        case 62 /* i64_store32 */:
        case 42 /* f32_load */:
        case 56 /* f32_store */:
        case 65024 /* atomic_notify */:
        case 65025 /* i32_atomic_wait */:
        case 65040 /* i32_atomic_load */:
        case 65046 /* i64_atomic_load32_u */:
        case 65047 /* i32_atomic_store */:
        case 65053 /* i64_atomic_store32 */:
        case 65054 /* i32_atomic_rmw_add */:
        case 65060 /* i64_atomic_rmw32_add_u */:
        case 65061 /* i32_atomic_rmw_sub */:
        case 65067 /* i64_atomic_rmw32_sub_u */:
        case 65068 /* i32_atomic_rmw_and */:
        case 65074 /* i64_atomic_rmw32_and_u */:
        case 65075 /* i32_atomic_rmw_or */:
        case 65081 /* i64_atomic_rmw32_or_u */:
        case 65082 /* i32_atomic_rmw_xor */:
        case 65088 /* i64_atomic_rmw32_xor_u */:
        case 65089 /* i32_atomic_rmw_xchg */:
        case 65095 /* i64_atomic_rmw32_xchg_u */:
        case 65096 /* i32_atomic_rmw_cmpxchg */:
        case 65102 /* i64_atomic_rmw32_cmpxchg_u */:
        case 64860 /* v128_load32_zero */:
            defaultAlignFlags = 2;
            break;
        case 46 /* i32_load16_s */:
        case 47 /* i32_load16_u */:
        case 50 /* i64_load16_s */:
        case 51 /* i64_load16_u */:
        case 59 /* i32_store16 */:
        case 61 /* i64_store16 */:
        case 65043 /* i32_atomic_load16_u */:
        case 65045 /* i64_atomic_load16_u */:
        case 65050 /* i32_atomic_store16 */:
        case 65052 /* i64_atomic_store16 */:
        case 65057 /* i32_atomic_rmw16_add_u */:
        case 65059 /* i64_atomic_rmw16_add_u */:
        case 65064 /* i32_atomic_rmw16_sub_u */:
        case 65066 /* i64_atomic_rmw16_sub_u */:
        case 65071 /* i32_atomic_rmw16_and_u */:
        case 65073 /* i64_atomic_rmw16_and_u */:
        case 65078 /* i32_atomic_rmw16_or_u */:
        case 65080 /* i64_atomic_rmw16_or_u */:
        case 65085 /* i32_atomic_rmw16_xor_u */:
        case 65087 /* i64_atomic_rmw16_xor_u */:
        case 65092 /* i32_atomic_rmw16_xchg_u */:
        case 65094 /* i64_atomic_rmw16_xchg_u */:
        case 65099 /* i32_atomic_rmw16_cmpxchg_u */:
        case 65101 /* i64_atomic_rmw16_cmpxchg_u */:
            defaultAlignFlags = 1;
            break;
        case 44 /* i32_load8_s */:
        case 45 /* i32_load8_u */:
        case 48 /* i64_load8_s */:
        case 49 /* i64_load8_u */:
        case 58 /* i32_store8 */:
        case 60 /* i64_store8 */:
        case 65042 /* i32_atomic_load8_u */:
        case 65044 /* i64_atomic_load8_u */:
        case 65049 /* i32_atomic_store8 */:
        case 65051 /* i64_atomic_store8 */:
        case 65056 /* i32_atomic_rmw8_add_u */:
        case 65058 /* i64_atomic_rmw8_add_u */:
        case 65063 /* i32_atomic_rmw8_sub_u */:
        case 65065 /* i64_atomic_rmw8_sub_u */:
        case 65070 /* i32_atomic_rmw8_and_u */:
        case 65072 /* i64_atomic_rmw8_and_u */:
        case 65077 /* i32_atomic_rmw8_or_u */:
        case 65079 /* i64_atomic_rmw8_or_u */:
        case 65084 /* i32_atomic_rmw8_xor_u */:
        case 65086 /* i64_atomic_rmw8_xor_u */:
        case 65091 /* i32_atomic_rmw8_xchg_u */:
        case 65093 /* i64_atomic_rmw8_xchg_u */:
        case 65098 /* i32_atomic_rmw8_cmpxchg_u */:
        case 65100 /* i64_atomic_rmw8_cmpxchg_u */:
            defaultAlignFlags = 0;
            break;
    }
    if (address.flags == defaultAlignFlags)
        // hide default flags
        return !address.offset ? null : "offset=" + address.offset;
    if (!address.offset)
        // hide default offset
        return "align=" + (1 << address.flags);
    return "offset=" + (address.offset | 0) + " align=" + (1 << address.flags);
}
function limitsToString(limits) {
    return (limits.initial + (limits.maximum !== undefined ? " " + limits.maximum : ""));
}
var paddingCache = ["0", "00", "000"];
function formatHex(n, width) {
    var s = (n >>> 0).toString(16).toUpperCase();
    if (width === undefined || s.length >= width)
        return s;
    var paddingIndex = width - s.length - 1;
    while (paddingIndex >= paddingCache.length)
        paddingCache.push(paddingCache[paddingCache.length - 1] + "0");
    return paddingCache[paddingIndex] + s;
}
var IndentIncrement = "  ";
function isValidName(name) {
    return !INVALID_NAME_SYMBOLS_REGEX.test(name);
}
var DefaultNameResolver = /** @class */ (function () {
    function DefaultNameResolver() {
    }
    DefaultNameResolver.prototype.getTypeName = function (index, isRef) {
        return "$type" + index;
    };
    DefaultNameResolver.prototype.getTableName = function (index, isRef) {
        return "$table" + index;
    };
    DefaultNameResolver.prototype.getMemoryName = function (index, isRef) {
        return "$memory" + index;
    };
    DefaultNameResolver.prototype.getGlobalName = function (index, isRef) {
        return "$global" + index;
    };
    DefaultNameResolver.prototype.getElementName = function (index, isRef) {
        return "$elem" + index;
    };
    DefaultNameResolver.prototype.getEventName = function (index, isRef) {
        return "$event" + index;
    };
    DefaultNameResolver.prototype.getFunctionName = function (index, isImport, isRef) {
        return (isImport ? "$import" : "$func") + index;
    };
    DefaultNameResolver.prototype.getVariableName = function (funcIndex, index, isRef) {
        return "$var" + index;
    };
    DefaultNameResolver.prototype.getFieldName = function (typeIndex, index, isRef) {
        return "$field" + index;
    };
    DefaultNameResolver.prototype.getLabel = function (index) {
        return "$label" + index;
    };
    return DefaultNameResolver;
}());
exports.DefaultNameResolver = DefaultNameResolver;
var EMPTY_STRING_ARRAY = [];
var DevToolsExportMetadata = /** @class */ (function () {
    function DevToolsExportMetadata(functionExportNames, globalExportNames, memoryExportNames, tableExportNames, eventExportNames) {
        this._functionExportNames = functionExportNames;
        this._globalExportNames = globalExportNames;
        this._memoryExportNames = memoryExportNames;
        this._tableExportNames = tableExportNames;
        this._eventExportNames = eventExportNames;
    }
    DevToolsExportMetadata.prototype.getFunctionExportNames = function (index) {
        var _a;
        return (_a = this._functionExportNames[index]) !== null && _a !== void 0 ? _a : EMPTY_STRING_ARRAY;
    };
    DevToolsExportMetadata.prototype.getGlobalExportNames = function (index) {
        var _a;
        return (_a = this._globalExportNames[index]) !== null && _a !== void 0 ? _a : EMPTY_STRING_ARRAY;
    };
    DevToolsExportMetadata.prototype.getMemoryExportNames = function (index) {
        var _a;
        return (_a = this._memoryExportNames[index]) !== null && _a !== void 0 ? _a : EMPTY_STRING_ARRAY;
    };
    DevToolsExportMetadata.prototype.getTableExportNames = function (index) {
        var _a;
        return (_a = this._tableExportNames[index]) !== null && _a !== void 0 ? _a : EMPTY_STRING_ARRAY;
    };
    DevToolsExportMetadata.prototype.getEventExportNames = function (index) {
        var _a;
        return (_a = this._eventExportNames[index]) !== null && _a !== void 0 ? _a : EMPTY_STRING_ARRAY;
    };
    return DevToolsExportMetadata;
}());
var NumericNameResolver = /** @class */ (function () {
    function NumericNameResolver() {
    }
    NumericNameResolver.prototype.getTypeName = function (index, isRef) {
        return isRef ? "" + index : "(;" + index + ";)";
    };
    NumericNameResolver.prototype.getTableName = function (index, isRef) {
        return isRef ? "" + index : "(;" + index + ";)";
    };
    NumericNameResolver.prototype.getMemoryName = function (index, isRef) {
        return isRef ? "" + index : "(;" + index + ";)";
    };
    NumericNameResolver.prototype.getGlobalName = function (index, isRef) {
        return isRef ? "" + index : "(;" + index + ";)";
    };
    NumericNameResolver.prototype.getElementName = function (index, isRef) {
        return isRef ? "" + index : "(;" + index + ";)";
    };
    NumericNameResolver.prototype.getEventName = function (index, isRef) {
        return isRef ? "" + index : "(;" + index + ";)";
    };
    NumericNameResolver.prototype.getFunctionName = function (index, isImport, isRef) {
        return isRef ? "" + index : "(;" + index + ";)";
    };
    NumericNameResolver.prototype.getVariableName = function (funcIndex, index, isRef) {
        return isRef ? "" + index : "(;" + index + ";)";
    };
    NumericNameResolver.prototype.getFieldName = function (typeIndex, index, isRef) {
        return isRef ? "" : index + ("(;" + index + ";)");
    };
    NumericNameResolver.prototype.getLabel = function (index) {
        return null;
    };
    return NumericNameResolver;
}());
exports.NumericNameResolver = NumericNameResolver;
var LabelMode;
(function (LabelMode) {
    LabelMode[LabelMode["Depth"] = 0] = "Depth";
    LabelMode[LabelMode["WhenUsed"] = 1] = "WhenUsed";
    LabelMode[LabelMode["Always"] = 2] = "Always";
})(LabelMode = exports.LabelMode || (exports.LabelMode = {}));
var WasmDisassembler = /** @class */ (function () {
    function WasmDisassembler() {
        this._skipTypes = true;
        this._exportMetadata = null;
        this._lines = [];
        this._offsets = [];
        this._buffer = "";
        this._indent = null;
        this._indentLevel = 0;
        this._addOffsets = false;
        this._done = false;
        this._currentPosition = 0;
        this._nameResolver = new DefaultNameResolver();
        this._labelMode = LabelMode.WhenUsed;
        this._functionBodyOffsets = [];
        this._currentFunctionBodyOffset = 0;
        this._currentSectionId = -1 /* Unknown */;
        this._logFirstInstruction = false;
        this._reset();
    }
    WasmDisassembler.prototype._reset = function () {
        this._types = [];
        this._funcIndex = 0;
        this._funcTypes = [];
        this._importCount = 0;
        this._globalCount = 0;
        this._memoryCount = 0;
        this._eventCount = 0;
        this._tableCount = 0;
        this._elementCount = 0;
        this._expression = [];
        this._backrefLabels = null;
        this._labelIndex = 0;
    };
    Object.defineProperty(WasmDisassembler.prototype, "addOffsets", {
        get: function () {
            return this._addOffsets;
        },
        set: function (value) {
            if (this._currentPosition)
                throw new Error("Cannot switch addOffsets during processing.");
            this._addOffsets = value;
        },
        enumerable: false,
        configurable: true
    });
    Object.defineProperty(WasmDisassembler.prototype, "skipTypes", {
        get: function () {
            return this._skipTypes;
        },
        set: function (skipTypes) {
            if (this._currentPosition)
                throw new Error("Cannot switch skipTypes during processing.");
            this._skipTypes = skipTypes;
        },
        enumerable: false,
        configurable: true
    });
    Object.defineProperty(WasmDisassembler.prototype, "labelMode", {
        get: function () {
            return this._labelMode;
        },
        set: function (value) {
            if (this._currentPosition)
                throw new Error("Cannot switch labelMode during processing.");
            this._labelMode = value;
        },
        enumerable: false,
        configurable: true
    });
    Object.defineProperty(WasmDisassembler.prototype, "exportMetadata", {
        get: function () {
            return this._exportMetadata;
        },
        set: function (exportMetadata) {
            if (this._currentPosition)
                throw new Error("Cannot switch exportMetadata during processing.");
            this._exportMetadata = exportMetadata;
        },
        enumerable: false,
        configurable: true
    });
    Object.defineProperty(WasmDisassembler.prototype, "nameResolver", {
        get: function () {
            return this._nameResolver;
        },
        set: function (resolver) {
            if (this._currentPosition)
                throw new Error("Cannot switch nameResolver during processing.");
            this._nameResolver = resolver;
        },
        enumerable: false,
        configurable: true
    });
    WasmDisassembler.prototype.appendBuffer = function (s) {
        this._buffer += s;
    };
    WasmDisassembler.prototype.newLine = function () {
        if (this.addOffsets)
            this._offsets.push(this._currentPosition);
        this._lines.push(this._buffer);
        this._buffer = "";
    };
    WasmDisassembler.prototype.logStartOfFunctionBodyOffset = function () {
        if (this.addOffsets) {
            this._currentFunctionBodyOffset = this._currentPosition;
        }
    };
    WasmDisassembler.prototype.logEndOfFunctionBodyOffset = function () {
        if (this.addOffsets) {
            this._functionBodyOffsets.push({
                start: this._currentFunctionBodyOffset,
                end: this._currentPosition,
            });
        }
    };
    WasmDisassembler.prototype.typeIndexToString = function (typeIndex) {
        if (typeIndex >= 0)
            return this._nameResolver.getTypeName(typeIndex, true);
        switch (typeIndex) {
            case -16 /* funcref */:
                return "func";
            case -17 /* externref */:
                return "extern";
            case -18 /* anyref */:
                return "any";
            case -19 /* eqref */:
                return "eq";
            case -22 /* i31ref */:
                return "i31";
            case -25 /* dataref */:
                return "data";
        }
    };
    WasmDisassembler.prototype.typeToString = function (type) {
        switch (type.kind) {
            case -1 /* i32 */:
                return "i32";
            case -2 /* i64 */:
                return "i64";
            case -3 /* f32 */:
                return "f32";
            case -4 /* f64 */:
                return "f64";
            case -5 /* v128 */:
                return "v128";
            case -6 /* i8 */:
                return "i8";
            case -7 /* i16 */:
                return "i16";
            case -16 /* funcref */:
                return "funcref";
            case -17 /* externref */:
                return "externref";
            case -18 /* anyref */:
                return "anyref";
            case -19 /* eqref */:
                return "eqref";
            case -22 /* i31ref */:
                return "i31ref";
            case -25 /* dataref */:
                return "dataref";
            case -21 /* ref */:
                return "(ref " + this.typeIndexToString(type.index) + ")";
            case -20 /* optref */:
                return "(ref null " + this.typeIndexToString(type.index) + ")";
            case -24 /* rtt */:
                return "(rtt " + this.typeIndexToString(type.index) + ")";
            case -23 /* rtt_d */:
                return "(rtt " + type.depth + " " + this.typeIndexToString(type.index) + ")";
            default:
                throw new Error("Unexpected type " + JSON.stringify(type));
        }
    };
    WasmDisassembler.prototype.maybeMut = function (type, mutability) {
        return mutability ? "(mut " + type + ")" : type;
    };
    WasmDisassembler.prototype.globalTypeToString = function (type) {
        var typeStr = this.typeToString(type.contentType);
        return this.maybeMut(typeStr, !!type.mutability);
    };
    WasmDisassembler.prototype.printFuncType = function (typeIndex) {
        var type = this._types[typeIndex];
        if (type.params.length > 0) {
            this.appendBuffer(" (param");
            for (var i = 0; i < type.params.length; i++) {
                this.appendBuffer(" ");
                this.appendBuffer(this.typeToString(type.params[i]));
            }
            this.appendBuffer(")");
        }
        if (type.returns.length > 0) {
            this.appendBuffer(" (result");
            for (var i = 0; i < type.returns.length; i++) {
                this.appendBuffer(" ");
                this.appendBuffer(this.typeToString(type.returns[i]));
            }
            this.appendBuffer(")");
        }
    };
    WasmDisassembler.prototype.printStructType = function (typeIndex) {
        var type = this._types[typeIndex];
        if (type.fields.length === 0)
            return;
        for (var i = 0; i < type.fields.length; i++) {
            var fieldType = this.maybeMut(this.typeToString(type.fields[i]), type.mutabilities[i]);
            var fieldName = this._nameResolver.getFieldName(typeIndex, i, false);
            this.appendBuffer(" (field " + fieldName + " " + fieldType + ")");
        }
    };
    WasmDisassembler.prototype.printArrayType = function (typeIndex) {
        var type = this._types[typeIndex];
        this.appendBuffer(" (field ");
        this.appendBuffer(this.maybeMut(this.typeToString(type.elementType), type.mutability));
    };
    WasmDisassembler.prototype.printBlockType = function (type) {
        if (type.kind === -64 /* empty_block_type */) {
            return;
        }
        if (type.kind === 0 /* unspecified */) {
            return this.printFuncType(type.index);
        }
        this.appendBuffer(" (result ");
        this.appendBuffer(this.typeToString(type));
        this.appendBuffer(")");
    };
    WasmDisassembler.prototype.printString = function (b) {
        this.appendBuffer('"');
        for (var i = 0; i < b.length; i++) {
            var byte = b[i];
            if (byte < 0x20 ||
                byte >= 0x7f ||
                byte == /* " */ 0x22 ||
                byte == /* \ */ 0x5c) {
                this.appendBuffer("\\" + (byte >> 4).toString(16) + (byte & 15).toString(16));
            }
            else {
                this.appendBuffer(String.fromCharCode(byte));
            }
        }
        this.appendBuffer('"');
    };
    WasmDisassembler.prototype.printExpression = function (expression) {
        for (var _i = 0, expression_1 = expression; _i < expression_1.length; _i++) {
            var operator = expression_1[_i];
            this.appendBuffer("(");
            this.printOperator(operator);
            this.appendBuffer(")");
        }
    };
    // extraDepthOffset is used by "delegate" instructions.
    WasmDisassembler.prototype.useLabel = function (depth, extraDepthOffset) {
        if (extraDepthOffset === void 0) { extraDepthOffset = 0; }
        if (!this._backrefLabels) {
            return "" + depth;
        }
        var i = this._backrefLabels.length - depth - 1 - extraDepthOffset;
        if (i < 0) {
            return "" + depth;
        }
        var backrefLabel = this._backrefLabels[i];
        if (!backrefLabel.useLabel) {
            backrefLabel.useLabel = true;
            backrefLabel.label = this._nameResolver.getLabel(this._labelIndex);
            var line = this._lines[backrefLabel.line];
            this._lines[backrefLabel.line] =
                line.substring(0, backrefLabel.position) +
                    " " +
                    backrefLabel.label +
                    line.substring(backrefLabel.position);
            this._labelIndex++;
        }
        return backrefLabel.label || "" + depth;
    };
    WasmDisassembler.prototype.printOperator = function (operator) {
        var code = operator.code;
        this.appendBuffer(WasmParser_js_1.OperatorCodeNames[code]);
        switch (code) {
            case 2 /* block */:
            case 3 /* loop */:
            case 4 /* if */:
            case 6 /* try */:
                if (this._labelMode !== LabelMode.Depth) {
                    var backrefLabel_1 = {
                        line: this._lines.length,
                        position: this._buffer.length,
                        useLabel: false,
                        label: null,
                    };
                    if (this._labelMode === LabelMode.Always) {
                        backrefLabel_1.useLabel = true;
                        backrefLabel_1.label = this._nameResolver.getLabel(this._labelIndex++);
                        if (backrefLabel_1.label) {
                            this.appendBuffer(" ");
                            this.appendBuffer(backrefLabel_1.label);
                        }
                    }
                    this._backrefLabels.push(backrefLabel_1);
                }
                this.printBlockType(operator.blockType);
                break;
            case 11 /* end */:
                if (this._labelMode === LabelMode.Depth) {
                    break;
                }
                var backrefLabel = this._backrefLabels.pop();
                if (backrefLabel.label) {
                    this.appendBuffer(" ");
                    this.appendBuffer(backrefLabel.label);
                }
                break;
            case 12 /* br */:
            case 13 /* br_if */:
            case 212 /* br_on_null */:
            case 64322 /* br_on_cast */:
            case 64352 /* br_on_func */:
            case 64353 /* br_on_data */:
            case 64354 /* br_on_i31 */:
                this.appendBuffer(" ");
                this.appendBuffer(this.useLabel(operator.brDepth));
                break;
            case 14 /* br_table */:
                for (var i = 0; i < operator.brTable.length; i++) {
                    this.appendBuffer(" ");
                    this.appendBuffer(this.useLabel(operator.brTable[i]));
                }
                break;
            case 9 /* rethrow */:
                this.appendBuffer(" ");
                this.appendBuffer(this.useLabel(operator.relativeDepth));
                break;
            case 24 /* delegate */:
                this.appendBuffer(" ");
                this.appendBuffer(this.useLabel(operator.relativeDepth, 1));
                break;
            case 7 /* catch */:
            case 8 /* throw */:
                var eventName = this._nameResolver.getEventName(operator.eventIndex, true);
                this.appendBuffer(" " + eventName);
                break;
            case 208 /* ref_null */:
                this.appendBuffer(" ");
                this.appendBuffer(this.typeIndexToString(operator.refType));
                break;
            case 16 /* call */:
            case 18 /* return_call */:
            case 210 /* ref_func */:
                var funcName = this._nameResolver.getFunctionName(operator.funcIndex, operator.funcIndex < this._importCount, true);
                this.appendBuffer(" " + funcName);
                break;
            case 17 /* call_indirect */:
            case 19 /* return_call_indirect */:
                this.printFuncType(operator.typeIndex);
                break;
            case 32 /* local_get */:
            case 33 /* local_set */:
            case 34 /* local_tee */:
                var paramName = this._nameResolver.getVariableName(this._funcIndex, operator.localIndex, true);
                this.appendBuffer(" " + paramName);
                break;
            case 35 /* global_get */:
            case 36 /* global_set */:
                var globalName = this._nameResolver.getGlobalName(operator.globalIndex, true);
                this.appendBuffer(" " + globalName);
                break;
            case 40 /* i32_load */:
            case 41 /* i64_load */:
            case 42 /* f32_load */:
            case 43 /* f64_load */:
            case 44 /* i32_load8_s */:
            case 45 /* i32_load8_u */:
            case 46 /* i32_load16_s */:
            case 47 /* i32_load16_u */:
            case 48 /* i64_load8_s */:
            case 49 /* i64_load8_u */:
            case 50 /* i64_load16_s */:
            case 51 /* i64_load16_u */:
            case 52 /* i64_load32_s */:
            case 53 /* i64_load32_u */:
            case 54 /* i32_store */:
            case 55 /* i64_store */:
            case 56 /* f32_store */:
            case 57 /* f64_store */:
            case 58 /* i32_store8 */:
            case 59 /* i32_store16 */:
            case 60 /* i64_store8 */:
            case 61 /* i64_store16 */:
            case 62 /* i64_store32 */:
            case 65024 /* atomic_notify */:
            case 65025 /* i32_atomic_wait */:
            case 65026 /* i64_atomic_wait */:
            case 65040 /* i32_atomic_load */:
            case 65041 /* i64_atomic_load */:
            case 65042 /* i32_atomic_load8_u */:
            case 65043 /* i32_atomic_load16_u */:
            case 65044 /* i64_atomic_load8_u */:
            case 65045 /* i64_atomic_load16_u */:
            case 65046 /* i64_atomic_load32_u */:
            case 65047 /* i32_atomic_store */:
            case 65048 /* i64_atomic_store */:
            case 65049 /* i32_atomic_store8 */:
            case 65050 /* i32_atomic_store16 */:
            case 65051 /* i64_atomic_store8 */:
            case 65052 /* i64_atomic_store16 */:
            case 65053 /* i64_atomic_store32 */:
            case 65054 /* i32_atomic_rmw_add */:
            case 65055 /* i64_atomic_rmw_add */:
            case 65056 /* i32_atomic_rmw8_add_u */:
            case 65057 /* i32_atomic_rmw16_add_u */:
            case 65058 /* i64_atomic_rmw8_add_u */:
            case 65059 /* i64_atomic_rmw16_add_u */:
            case 65060 /* i64_atomic_rmw32_add_u */:
            case 65061 /* i32_atomic_rmw_sub */:
            case 65062 /* i64_atomic_rmw_sub */:
            case 65063 /* i32_atomic_rmw8_sub_u */:
            case 65064 /* i32_atomic_rmw16_sub_u */:
            case 65065 /* i64_atomic_rmw8_sub_u */:
            case 65066 /* i64_atomic_rmw16_sub_u */:
            case 65067 /* i64_atomic_rmw32_sub_u */:
            case 65068 /* i32_atomic_rmw_and */:
            case 65069 /* i64_atomic_rmw_and */:
            case 65070 /* i32_atomic_rmw8_and_u */:
            case 65071 /* i32_atomic_rmw16_and_u */:
            case 65072 /* i64_atomic_rmw8_and_u */:
            case 65073 /* i64_atomic_rmw16_and_u */:
            case 65074 /* i64_atomic_rmw32_and_u */:
            case 65075 /* i32_atomic_rmw_or */:
            case 65076 /* i64_atomic_rmw_or */:
            case 65077 /* i32_atomic_rmw8_or_u */:
            case 65078 /* i32_atomic_rmw16_or_u */:
            case 65079 /* i64_atomic_rmw8_or_u */:
            case 65080 /* i64_atomic_rmw16_or_u */:
            case 65081 /* i64_atomic_rmw32_or_u */:
            case 65082 /* i32_atomic_rmw_xor */:
            case 65083 /* i64_atomic_rmw_xor */:
            case 65084 /* i32_atomic_rmw8_xor_u */:
            case 65085 /* i32_atomic_rmw16_xor_u */:
            case 65086 /* i64_atomic_rmw8_xor_u */:
            case 65087 /* i64_atomic_rmw16_xor_u */:
            case 65088 /* i64_atomic_rmw32_xor_u */:
            case 65089 /* i32_atomic_rmw_xchg */:
            case 65090 /* i64_atomic_rmw_xchg */:
            case 65091 /* i32_atomic_rmw8_xchg_u */:
            case 65092 /* i32_atomic_rmw16_xchg_u */:
            case 65093 /* i64_atomic_rmw8_xchg_u */:
            case 65094 /* i64_atomic_rmw16_xchg_u */:
            case 65095 /* i64_atomic_rmw32_xchg_u */:
            case 65096 /* i32_atomic_rmw_cmpxchg */:
            case 65097 /* i64_atomic_rmw_cmpxchg */:
            case 65098 /* i32_atomic_rmw8_cmpxchg_u */:
            case 65099 /* i32_atomic_rmw16_cmpxchg_u */:
            case 65100 /* i64_atomic_rmw8_cmpxchg_u */:
            case 65101 /* i64_atomic_rmw16_cmpxchg_u */:
            case 65102 /* i64_atomic_rmw32_cmpxchg_u */:
            case 64768 /* v128_load */:
            case 64769 /* i16x8_load8x8_s */:
            case 64770 /* i16x8_load8x8_u */:
            case 64771 /* i32x4_load16x4_s */:
            case 64772 /* i32x4_load16x4_u */:
            case 64773 /* i64x2_load32x2_s */:
            case 64774 /* i64x2_load32x2_u */:
            case 64775 /* v8x16_load_splat */:
            case 64776 /* v16x8_load_splat */:
            case 64777 /* v32x4_load_splat */:
            case 64778 /* v64x2_load_splat */:
            case 64779 /* v128_store */:
            case 64860 /* v128_load32_zero */:
            case 64861 /* v128_load64_zero */:
                var memoryAddress = memoryAddressToString(operator.memoryAddress, operator.code);
                if (memoryAddress !== null) {
                    this.appendBuffer(" ");
                    this.appendBuffer(memoryAddress);
                }
                break;
            case 63 /* current_memory */:
            case 64 /* grow_memory */:
                break;
            case 65 /* i32_const */:
                this.appendBuffer(" " + operator.literal.toString());
                break;
            case 66 /* i64_const */:
                this.appendBuffer(" " + operator.literal.toString());
                break;
            case 67 /* f32_const */:
                this.appendBuffer(" " + formatFloat32(operator.literal));
                break;
            case 68 /* f64_const */:
                this.appendBuffer(" " + formatFloat64(operator.literal));
                break;
            case 64780 /* v128_const */:
                this.appendBuffer(" i32x4 " + formatI32Array(operator.literal, 4));
                break;
            case 64781 /* i8x16_shuffle */:
                this.appendBuffer(" " + formatI8Array(operator.lines, 16));
                break;
            case 64789 /* i8x16_extract_lane_s */:
            case 64790 /* i8x16_extract_lane_u */:
            case 64791 /* i8x16_replace_lane */:
            case 64792 /* i16x8_extract_lane_s */:
            case 64793 /* i16x8_extract_lane_u */:
            case 64794 /* i16x8_replace_lane */:
            case 64795 /* i32x4_extract_lane */:
            case 64796 /* i32x4_replace_lane */:
            case 64799 /* f32x4_extract_lane */:
            case 64800 /* f32x4_replace_lane */:
            case 64797 /* i64x2_extract_lane */:
            case 64798 /* i64x2_replace_lane */:
            case 64801 /* f64x2_extract_lane */:
            case 64802 /* f64x2_replace_lane */:
                this.appendBuffer(" " + operator.lineIndex);
                break;
            case 64520 /* memory_init */:
            case 64521 /* data_drop */:
                this.appendBuffer(" " + operator.segmentIndex);
                break;
            case 64525 /* elem_drop */:
                var elementName = this._nameResolver.getElementName(operator.segmentIndex, true);
                this.appendBuffer(" " + elementName);
                break;
            case 38 /* table_set */:
            case 37 /* table_get */:
            case 64529 /* table_fill */: {
                var tableName = this._nameResolver.getTableName(operator.tableIndex, true);
                this.appendBuffer(" " + tableName);
                break;
            }
            case 64526 /* table_copy */: {
                // Table index might be omitted and defaults to 0.
                if (operator.tableIndex !== 0 || operator.destinationIndex !== 0) {
                    var tableName = this._nameResolver.getTableName(operator.tableIndex, true);
                    var destinationName = this._nameResolver.getTableName(operator.destinationIndex, true);
                    this.appendBuffer(" " + destinationName + " " + tableName);
                }
                break;
            }
            case 64524 /* table_init */: {
                // Table index might be omitted and defaults to 0.
                if (operator.tableIndex !== 0) {
                    var tableName = this._nameResolver.getTableName(operator.tableIndex, true);
                    this.appendBuffer(" " + tableName);
                }
                var elementName_1 = this._nameResolver.getElementName(operator.segmentIndex, true);
                this.appendBuffer(" " + elementName_1);
                break;
            }
            case 64259 /* struct_get */:
            case 64260 /* struct_get_s */:
            case 64261 /* struct_get_u */:
            case 64262 /* struct_set */: {
                var refType = this._nameResolver.getTypeName(operator.refType, true);
                var fieldName = this._nameResolver.getFieldName(operator.refType, operator.fieldIndex, true);
                this.appendBuffer(" " + refType + " " + fieldName);
                break;
            }
            case 64304 /* rtt_canon */:
            case 64305 /* rtt_sub */:
            case 64258 /* struct_new_default_with_rtt */:
            case 64257 /* struct_new_with_rtt */:
            case 64274 /* array_new_default_with_rtt */:
            case 64273 /* array_new_with_rtt */:
            case 64275 /* array_get */:
            case 64276 /* array_get_s */:
            case 64277 /* array_get_u */:
            case 64278 /* array_set */:
            case 64279 /* array_len */: {
                var refType = this._nameResolver.getTypeName(operator.refType, true);
                this.appendBuffer(" " + refType);
                break;
            }
        }
    };
    WasmDisassembler.prototype.printImportSource = function (info) {
        this.printString(info.module);
        this.appendBuffer(" ");
        this.printString(info.field);
    };
    WasmDisassembler.prototype.increaseIndent = function () {
        this._indent += IndentIncrement;
        this._indentLevel++;
    };
    WasmDisassembler.prototype.decreaseIndent = function () {
        this._indent = this._indent.slice(0, -IndentIncrement.length);
        this._indentLevel--;
    };
    WasmDisassembler.prototype.disassemble = function (reader) {
        var _this = this;
        var done = this.disassembleChunk(reader);
        if (!done)
            return null;
        var lines = this._lines;
        if (this._addOffsets) {
            lines = lines.map(function (line, index) {
                var position = formatHex(_this._offsets[index], 4);
                return line + " ;; @" + position;
            });
        }
        lines.push(""); // we need '\n' after last line
        var result = lines.join("\n");
        this._lines.length = 0;
        this._offsets.length = 0;
        this._functionBodyOffsets.length = 0;
        return result;
    };
    WasmDisassembler.prototype.getResult = function () {
        var linesReady = this._lines.length;
        if (this._backrefLabels && this._labelMode === LabelMode.WhenUsed) {
            this._backrefLabels.some(function (backrefLabel) {
                if (backrefLabel.useLabel)
                    return false;
                linesReady = backrefLabel.line;
                return true;
            });
        }
        if (linesReady === 0) {
            return {
                lines: [],
                offsets: this._addOffsets ? [] : undefined,
                done: this._done,
                functionBodyOffsets: this._addOffsets ? [] : undefined,
            };
        }
        if (linesReady === this._lines.length) {
            var result_1 = {
                lines: this._lines,
                offsets: this._addOffsets ? this._offsets : undefined,
                done: this._done,
                functionBodyOffsets: this._addOffsets
                    ? this._functionBodyOffsets
                    : undefined,
            };
            this._lines = [];
            if (this._addOffsets) {
                this._offsets = [];
                this._functionBodyOffsets = [];
            }
            return result_1;
        }
        var result = {
            lines: this._lines.splice(0, linesReady),
            offsets: this._addOffsets
                ? this._offsets.splice(0, linesReady)
                : undefined,
            done: false,
            functionBodyOffsets: this._addOffsets
                ? this._functionBodyOffsets
                : undefined,
        };
        if (this._backrefLabels) {
            this._backrefLabels.forEach(function (backrefLabel) {
                backrefLabel.line -= linesReady;
            });
        }
        return result;
    };
    WasmDisassembler.prototype.disassembleChunk = function (reader, offsetInModule) {
        if (offsetInModule === void 0) { offsetInModule = 0; }
        if (this._done)
            throw new Error("Invalid state: disassembly process was already finished.");
        while (true) {
            this._currentPosition = reader.position + offsetInModule;
            if (!reader.read())
                return false;
            switch (reader.state) {
                case 2 /* END_WASM */:
                    this.appendBuffer(")");
                    this.newLine();
                    this._reset();
                    if (!reader.hasMoreBytes()) {
                        this._done = true;
                        return true;
                    }
                    break;
                case -1 /* ERROR */:
                    throw reader.error;
                case 1 /* BEGIN_WASM */:
                    this.appendBuffer("(module");
                    this.newLine();
                    break;
                case 4 /* END_SECTION */:
                    this._currentSectionId = -1 /* Unknown */;
                    break;
                case 3 /* BEGIN_SECTION */:
                    var sectionInfo = reader.result;
                    switch (sectionInfo.id) {
                        case 1 /* Type */:
                        case 2 /* Import */:
                        case 7 /* Export */:
                        case 6 /* Global */:
                        case 3 /* Function */:
                        case 8 /* Start */:
                        case 10 /* Code */:
                        case 5 /* Memory */:
                        case 11 /* Data */:
                        case 4 /* Table */:
                        case 9 /* Element */:
                        case 13 /* Event */:
                            this._currentSectionId = sectionInfo.id;
                            break; // reading known section;
                        default:
                            reader.skipSection();
                            break;
                    }
                    break;
                case 15 /* MEMORY_SECTION_ENTRY */:
                    var memoryInfo = reader.result;
                    var memoryIndex = this._memoryCount++;
                    var memoryName = this._nameResolver.getMemoryName(memoryIndex, false);
                    this.appendBuffer("  (memory " + memoryName);
                    if (this._exportMetadata !== null) {
                        for (var _i = 0, _a = this._exportMetadata.getMemoryExportNames(memoryIndex); _i < _a.length; _i++) {
                            var exportName = _a[_i];
                            this.appendBuffer(" (export \"" + exportName + "\")");
                        }
                    }
                    this.appendBuffer(" " + limitsToString(memoryInfo.limits));
                    if (memoryInfo.shared) {
                        this.appendBuffer(" shared");
                    }
                    this.appendBuffer(")");
                    this.newLine();
                    break;
                case 23 /* EVENT_SECTION_ENTRY */:
                    var eventInfo = reader.result;
                    var eventIndex = this._eventCount++;
                    var eventName = this._nameResolver.getEventName(eventIndex, false);
                    this.appendBuffer("  (event " + eventName);
                    if (this._exportMetadata !== null) {
                        for (var _b = 0, _c = this._exportMetadata.getEventExportNames(eventIndex); _b < _c.length; _b++) {
                            var exportName = _c[_b];
                            this.appendBuffer(" (export \"" + exportName + "\")");
                        }
                    }
                    this.printFuncType(eventInfo.typeIndex);
                    this.appendBuffer(")");
                    this.newLine();
                    break;
                case 14 /* TABLE_SECTION_ENTRY */:
                    var tableInfo = reader.result;
                    var tableIndex = this._tableCount++;
                    var tableName = this._nameResolver.getTableName(tableIndex, false);
                    this.appendBuffer("  (table " + tableName);
                    if (this._exportMetadata !== null) {
                        for (var _d = 0, _e = this._exportMetadata.getTableExportNames(tableIndex); _d < _e.length; _d++) {
                            var exportName = _e[_d];
                            this.appendBuffer(" (export \"" + exportName + "\")");
                        }
                    }
                    this.appendBuffer(" " + limitsToString(tableInfo.limits) + " " + this.typeToString(tableInfo.elementType) + ")");
                    this.newLine();
                    break;
                case 17 /* EXPORT_SECTION_ENTRY */:
                    // Skip printing exports here when we have export metadata
                    // which we can use to print export information inline.
                    if (this._exportMetadata === null) {
                        var exportInfo = reader.result;
                        this.appendBuffer("  (export ");
                        this.printString(exportInfo.field);
                        this.appendBuffer(" ");
                        switch (exportInfo.kind) {
                            case 0 /* Function */:
                                var funcName = this._nameResolver.getFunctionName(exportInfo.index, exportInfo.index < this._importCount, true);
                                this.appendBuffer("(func " + funcName + ")");
                                break;
                            case 1 /* Table */:
                                var tableName = this._nameResolver.getTableName(exportInfo.index, true);
                                this.appendBuffer("(table " + tableName + ")");
                                break;
                            case 2 /* Memory */:
                                var memoryName = this._nameResolver.getMemoryName(exportInfo.index, true);
                                this.appendBuffer("(memory " + memoryName + ")");
                                break;
                            case 3 /* Global */:
                                var globalName = this._nameResolver.getGlobalName(exportInfo.index, true);
                                this.appendBuffer("(global " + globalName + ")");
                                break;
                            case 4 /* Event */:
                                var eventName = this._nameResolver.getEventName(exportInfo.index, true);
                                this.appendBuffer("(event " + eventName + ")");
                                break;
                            default:
                                throw new Error("Unsupported export " + exportInfo.kind);
                        }
                        this.appendBuffer(")");
                        this.newLine();
                    }
                    break;
                case 12 /* IMPORT_SECTION_ENTRY */:
                    var importInfo = reader.result;
                    switch (importInfo.kind) {
                        case 0 /* Function */:
                            this._importCount++;
                            var funcIndex = this._funcIndex++;
                            var funcName = this._nameResolver.getFunctionName(funcIndex, true, false);
                            this.appendBuffer("  (func " + funcName);
                            if (this._exportMetadata !== null) {
                                for (var _f = 0, _g = this._exportMetadata.getFunctionExportNames(funcIndex); _f < _g.length; _f++) {
                                    var exportName = _g[_f];
                                    this.appendBuffer(" (export \"" + exportName + "\")");
                                }
                            }
                            this.appendBuffer(" (import ");
                            this.printImportSource(importInfo);
                            this.appendBuffer(")");
                            this.printFuncType(importInfo.funcTypeIndex);
                            this.appendBuffer(")");
                            break;
                        case 3 /* Global */:
                            var globalImportInfo = importInfo.type;
                            var globalIndex = this._globalCount++;
                            var globalName = this._nameResolver.getGlobalName(globalIndex, false);
                            this.appendBuffer("  (global " + globalName);
                            if (this._exportMetadata !== null) {
                                for (var _h = 0, _j = this._exportMetadata.getGlobalExportNames(globalIndex); _h < _j.length; _h++) {
                                    var exportName = _j[_h];
                                    this.appendBuffer(" (export \"" + exportName + "\")");
                                }
                            }
                            this.appendBuffer(" (import ");
                            this.printImportSource(importInfo);
                            this.appendBuffer(") " + this.globalTypeToString(globalImportInfo) + ")");
                            break;
                        case 2 /* Memory */:
                            var memoryImportInfo = importInfo.type;
                            var memoryIndex = this._memoryCount++;
                            var memoryName = this._nameResolver.getMemoryName(memoryIndex, false);
                            this.appendBuffer("  (memory " + memoryName);
                            if (this._exportMetadata !== null) {
                                for (var _k = 0, _l = this._exportMetadata.getMemoryExportNames(memoryIndex); _k < _l.length; _k++) {
                                    var exportName = _l[_k];
                                    this.appendBuffer(" (export \"" + exportName + "\")");
                                }
                            }
                            this.appendBuffer(" (import ");
                            this.printImportSource(importInfo);
                            this.appendBuffer(") " + limitsToString(memoryImportInfo.limits));
                            if (memoryImportInfo.shared) {
                                this.appendBuffer(" shared");
                            }
                            this.appendBuffer(")");
                            break;
                        case 1 /* Table */:
                            var tableImportInfo = importInfo.type;
                            var tableIndex = this._tableCount++;
                            var tableName = this._nameResolver.getTableName(tableIndex, false);
                            this.appendBuffer("  (table " + tableName);
                            if (this._exportMetadata !== null) {
                                for (var _m = 0, _o = this._exportMetadata.getTableExportNames(tableIndex); _m < _o.length; _m++) {
                                    var exportName = _o[_m];
                                    this.appendBuffer(" (export \"" + exportName + "\")");
                                }
                            }
                            this.appendBuffer(" (import ");
                            this.printImportSource(importInfo);
                            this.appendBuffer(") " + limitsToString(tableImportInfo.limits) + " " + this.typeToString(tableImportInfo.elementType) + ")");
                            break;
                        case 4 /* Event */:
                            var eventImportInfo = importInfo.type;
                            var eventIndex = this._eventCount++;
                            var eventName = this._nameResolver.getEventName(eventIndex, false);
                            this.appendBuffer("  (event " + eventName);
                            if (this._exportMetadata !== null) {
                                for (var _p = 0, _q = this._exportMetadata.getEventExportNames(eventIndex); _p < _q.length; _p++) {
                                    var exportName = _q[_p];
                                    this.appendBuffer(" (export \"" + exportName + "\")");
                                }
                            }
                            this.appendBuffer(" (import ");
                            this.printImportSource(importInfo);
                            this.appendBuffer(")");
                            this.printFuncType(eventImportInfo.typeIndex);
                            this.appendBuffer(")");
                            break;
                        default:
                            throw new Error("NYI other import types: " + importInfo.kind);
                    }
                    this.newLine();
                    break;
                case 33 /* BEGIN_ELEMENT_SECTION_ENTRY */:
                    var elementSegment = reader.result;
                    var elementIndex = this._elementCount++;
                    var elementName = this._nameResolver.getElementName(elementIndex, false);
                    this.appendBuffer("  (elem " + elementName);
                    switch (elementSegment.mode) {
                        case 0 /* Active */:
                            if (elementSegment.tableIndex !== 0) {
                                var tableName_1 = this._nameResolver.getTableName(elementSegment.tableIndex, false);
                                this.appendBuffer(" (table " + tableName_1 + ")");
                            }
                            break;
                        case 1 /* Passive */:
                            break;
                        case 2 /* Declarative */:
                            this.appendBuffer(" declare");
                            break;
                    }
                    break;
                case 35 /* END_ELEMENT_SECTION_ENTRY */:
                    this.appendBuffer(")");
                    this.newLine();
                    break;
                case 34 /* ELEMENT_SECTION_ENTRY_BODY */:
                    var elementSegmentBody = reader.result;
                    this.appendBuffer(" " + this.typeToString(elementSegmentBody.elementType));
                    break;
                case 39 /* BEGIN_GLOBAL_SECTION_ENTRY */:
                    var globalInfo = reader.result;
                    var globalIndex = this._globalCount++;
                    var globalName = this._nameResolver.getGlobalName(globalIndex, false);
                    this.appendBuffer("  (global " + globalName);
                    if (this._exportMetadata !== null) {
                        for (var _r = 0, _s = this._exportMetadata.getGlobalExportNames(globalIndex); _r < _s.length; _r++) {
                            var exportName = _s[_r];
                            this.appendBuffer(" (export \"" + exportName + "\")");
                        }
                    }
                    this.appendBuffer(" " + this.globalTypeToString(globalInfo.type));
                    break;
                case 40 /* END_GLOBAL_SECTION_ENTRY */:
                    this.appendBuffer(")");
                    this.newLine();
                    break;
                case 11 /* TYPE_SECTION_ENTRY */:
                    var typeEntry = reader.result;
                    var typeIndex = this._types.length;
                    this._types.push(typeEntry);
                    if (!this._skipTypes) {
                        var typeName = this._nameResolver.getTypeName(typeIndex, false);
                        if (typeEntry.form === -32 /* func */) {
                            this.appendBuffer("  (type " + typeName + " (func");
                            this.printFuncType(typeIndex);
                            this.appendBuffer("))");
                        }
                        else if (typeEntry.form === -33 /* struct */) {
                            this.appendBuffer("  (type " + typeName + " (struct");
                            this.printStructType(typeIndex);
                            this.appendBuffer("))");
                        }
                        else if (typeEntry.form === -34 /* array */) {
                            this.appendBuffer("  (type " + typeName + " (array");
                            this.printArrayType(typeIndex);
                            this.appendBuffer("))");
                        }
                        else {
                            throw new Error("Unknown type form: " + typeEntry.form);
                        }
                        this.newLine();
                    }
                    break;
                case 22 /* START_SECTION_ENTRY */:
                    var startEntry = reader.result;
                    var funcName = this._nameResolver.getFunctionName(startEntry.index, startEntry.index < this._importCount, true);
                    this.appendBuffer("  (start " + funcName + ")");
                    this.newLine();
                    break;
                case 36 /* BEGIN_DATA_SECTION_ENTRY */:
                    this.appendBuffer("  (data");
                    break;
                case 37 /* DATA_SECTION_ENTRY_BODY */:
                    var body = reader.result;
                    this.appendBuffer(" ");
                    this.printString(body.data);
                    break;
                case 38 /* END_DATA_SECTION_ENTRY */:
                    this.appendBuffer(")");
                    this.newLine();
                    break;
                case 25 /* BEGIN_INIT_EXPRESSION_BODY */:
                case 44 /* BEGIN_OFFSET_EXPRESSION_BODY */:
                    this._expression = [];
                    break;
                case 26 /* INIT_EXPRESSION_OPERATOR */:
                case 45 /* OFFSET_EXPRESSION_OPERATOR */:
                    var operator = reader.result;
                    if (operator.code !== 11 /* end */) {
                        this._expression.push(operator);
                    }
                    break;
                case 46 /* END_OFFSET_EXPRESSION_BODY */:
                    if (this._expression.length > 1) {
                        this.appendBuffer(" (offset ");
                        this.printExpression(this._expression);
                        this.appendBuffer(")");
                    }
                    else {
                        this.appendBuffer(" ");
                        this.printExpression(this._expression);
                    }
                    this._expression = [];
                    break;
                case 27 /* END_INIT_EXPRESSION_BODY */:
                    if (this._expression.length > 1 &&
                        this._currentSectionId === 9 /* Element */) {
                        this.appendBuffer(" (item ");
                        this.printExpression(this._expression);
                        this.appendBuffer(")");
                    }
                    else {
                        this.appendBuffer(" ");
                        this.printExpression(this._expression);
                    }
                    this._expression = [];
                    break;
                case 13 /* FUNCTION_SECTION_ENTRY */:
                    this._funcTypes.push(reader.result.typeIndex);
                    break;
                case 28 /* BEGIN_FUNCTION_BODY */:
                    var func = reader.result;
                    var type = this._types[this._funcTypes[this._funcIndex - this._importCount]];
                    this.appendBuffer("  (func ");
                    this.appendBuffer(this._nameResolver.getFunctionName(this._funcIndex, false, false));
                    if (this._exportMetadata !== null) {
                        for (var _t = 0, _u = this._exportMetadata.getFunctionExportNames(this._funcIndex); _t < _u.length; _t++) {
                            var exportName = _u[_t];
                            this.appendBuffer(" (export \"" + exportName + "\")");
                        }
                    }
                    for (var i = 0; i < type.params.length; i++) {
                        var paramName = this._nameResolver.getVariableName(this._funcIndex, i, false);
                        this.appendBuffer(" (param " + paramName + " " + this.typeToString(type.params[i]) + ")");
                    }
                    for (var i = 0; i < type.returns.length; i++) {
                        this.appendBuffer(" (result " + this.typeToString(type.returns[i]) + ")");
                    }
                    this.newLine();
                    var localIndex = type.params.length;
                    if (func.locals.length > 0) {
                        this.appendBuffer("   ");
                        for (var _v = 0, _w = func.locals; _v < _w.length; _v++) {
                            var l = _w[_v];
                            for (var i = 0; i < l.count; i++) {
                                var paramName = this._nameResolver.getVariableName(this._funcIndex, localIndex++, false);
                                this.appendBuffer(" (local " + paramName + " " + this.typeToString(l.type) + ")");
                            }
                        }
                        this.newLine();
                    }
                    this._indent = "    ";
                    this._indentLevel = 0;
                    this._labelIndex = 0;
                    this._backrefLabels = this._labelMode === LabelMode.Depth ? null : [];
                    this._logFirstInstruction = true;
                    break;
                case 30 /* CODE_OPERATOR */:
                    if (this._logFirstInstruction) {
                        this.logStartOfFunctionBodyOffset();
                        this._logFirstInstruction = false;
                    }
                    var operator = reader.result;
                    if (operator.code == 11 /* end */ && this._indentLevel == 0) {
                        // reached of the function, closing function body
                        this.appendBuffer("  )");
                        this.newLine();
                        break;
                    }
                    switch (operator.code) {
                        case 11 /* end */:
                        case 5 /* else */:
                        case 7 /* catch */:
                        case 25 /* catch_all */:
                        case 10 /* unwind */:
                        case 24 /* delegate */:
                            this.decreaseIndent();
                            break;
                    }
                    this.appendBuffer(this._indent);
                    this.printOperator(operator);
                    this.newLine();
                    switch (operator.code) {
                        case 4 /* if */:
                        case 2 /* block */:
                        case 3 /* loop */:
                        case 5 /* else */:
                        case 6 /* try */:
                        case 7 /* catch */:
                        case 25 /* catch_all */:
                        case 10 /* unwind */:
                            this.increaseIndent();
                            break;
                    }
                    break;
                case 31 /* END_FUNCTION_BODY */:
                    this._funcIndex++;
                    this._backrefLabels = null;
                    this.logEndOfFunctionBodyOffset();
                    // See case BinaryReaderState.CODE_OPERATOR for closing of body
                    break;
                default:
                    throw new Error("Expectected state: " + reader.state);
            }
        }
    };
    return WasmDisassembler;
}());
exports.WasmDisassembler = WasmDisassembler;
var UNKNOWN_FUNCTION_PREFIX = "unknown";
var NameSectionNameResolver = /** @class */ (function (_super) {
    __extends(NameSectionNameResolver, _super);
    function NameSectionNameResolver(functionNames, localNames, eventNames, typeNames, tableNames, memoryNames, globalNames, fieldNames) {
        var _this = _super.call(this) || this;
        _this._functionNames = functionNames;
        _this._localNames = localNames;
        _this._eventNames = eventNames;
        _this._typeNames = typeNames;
        _this._tableNames = tableNames;
        _this._memoryNames = memoryNames;
        _this._globalNames = globalNames;
        _this._fieldNames = fieldNames;
        return _this;
    }
    NameSectionNameResolver.prototype.getTypeName = function (index, isRef) {
        var name = this._typeNames[index];
        if (!name)
            return _super.prototype.getTypeName.call(this, index, isRef);
        return isRef ? "$" + name : "$" + name + " (;" + index + ";)";
    };
    NameSectionNameResolver.prototype.getTableName = function (index, isRef) {
        var name = this._tableNames[index];
        if (!name)
            return _super.prototype.getTableName.call(this, index, isRef);
        return isRef ? "$" + name : "$" + name + " (;" + index + ";)";
    };
    NameSectionNameResolver.prototype.getMemoryName = function (index, isRef) {
        var name = this._memoryNames[index];
        if (!name)
            return _super.prototype.getMemoryName.call(this, index, isRef);
        return isRef ? "$" + name : "$" + name + " (;" + index + ";)";
    };
    NameSectionNameResolver.prototype.getGlobalName = function (index, isRef) {
        var name = this._globalNames[index];
        if (!name)
            return _super.prototype.getGlobalName.call(this, index, isRef);
        return isRef ? "$" + name : "$" + name + " (;" + index + ";)";
    };
    NameSectionNameResolver.prototype.getEventName = function (index, isRef) {
        var name = this._eventNames[index];
        if (!name)
            return _super.prototype.getEventName.call(this, index, isRef);
        return isRef ? "$" + name : "$" + name + " (;" + index + ";)";
    };
    NameSectionNameResolver.prototype.getFunctionName = function (index, isImport, isRef) {
        var name = this._functionNames[index];
        if (!name)
            return "$" + UNKNOWN_FUNCTION_PREFIX + index;
        return isRef ? "$" + name : "$" + name + " (;" + index + ";)";
    };
    NameSectionNameResolver.prototype.getVariableName = function (funcIndex, index, isRef) {
        var name = this._localNames[funcIndex] && this._localNames[funcIndex][index];
        if (!name)
            return _super.prototype.getVariableName.call(this, funcIndex, index, isRef);
        return isRef ? "$" + name : "$" + name + " (;" + index + ";)";
    };
    NameSectionNameResolver.prototype.getFieldName = function (typeIndex, index, isRef) {
        var name = this._fieldNames[typeIndex] && this._fieldNames[typeIndex][index];
        if (!name)
            return _super.prototype.getFieldName.call(this, typeIndex, index, isRef);
        return isRef ? "$" + name : "$" + name + " (;" + index + ";)";
    };
    return NameSectionNameResolver;
}(DefaultNameResolver));
var NameSectionReader = /** @class */ (function () {
    function NameSectionReader() {
        this._done = false;
        this._functionsCount = 0;
        this._functionImportsCount = 0;
        this._functionNames = null;
        this._functionLocalNames = null;
        this._eventNames = null;
        this._typeNames = null;
        this._tableNames = null;
        this._memoryNames = null;
        this._globalNames = null;
        this._fieldNames = null;
        this._hasNames = false;
    }
    NameSectionReader.prototype.read = function (reader) {
        var _this = this;
        if (this._done)
            throw new Error("Invalid state: disassembly process was already finished.");
        while (true) {
            if (!reader.read())
                return false;
            switch (reader.state) {
                case 2 /* END_WASM */:
                    if (!reader.hasMoreBytes()) {
                        this._done = true;
                        return true;
                    }
                    break;
                case -1 /* ERROR */:
                    throw reader.error;
                case 1 /* BEGIN_WASM */:
                    this._functionsCount = 0;
                    this._functionImportsCount = 0;
                    this._functionNames = [];
                    this._functionLocalNames = [];
                    this._eventNames = [];
                    this._typeNames = [];
                    this._tableNames = [];
                    this._memoryNames = [];
                    this._globalNames = [];
                    this._fieldNames = [];
                    this._hasNames = false;
                    break;
                case 4 /* END_SECTION */:
                    break;
                case 3 /* BEGIN_SECTION */:
                    var sectionInfo = reader.result;
                    if (sectionInfo.id === 0 /* Custom */ &&
                        WasmParser_js_1.bytesToString(sectionInfo.name) === NAME_SECTION_NAME) {
                        break;
                    }
                    if (sectionInfo.id === 3 /* Function */ ||
                        sectionInfo.id === 2 /* Import */) {
                        break;
                    }
                    reader.skipSection();
                    break;
                case 12 /* IMPORT_SECTION_ENTRY */:
                    var importInfo = reader.result;
                    if (importInfo.kind === 0 /* Function */)
                        this._functionImportsCount++;
                    break;
                case 13 /* FUNCTION_SECTION_ENTRY */:
                    this._functionsCount++;
                    break;
                case 19 /* NAME_SECTION_ENTRY */:
                    var nameInfo = reader.result;
                    if (nameInfo.type === 1 /* Function */) {
                        var names = nameInfo.names;
                        names.forEach(function (_a) {
                            var index = _a.index, name = _a.name;
                            _this._functionNames[index] = WasmParser_js_1.bytesToString(name);
                        });
                        this._hasNames = true;
                    }
                    else if (nameInfo.type === 2 /* Local */) {
                        var funcs = nameInfo.funcs;
                        funcs.forEach(function (_a) {
                            var index = _a.index, locals = _a.locals;
                            var localNames = (_this._functionLocalNames[index] = []);
                            locals.forEach(function (_a) {
                                var index = _a.index, name = _a.name;
                                localNames[index] = WasmParser_js_1.bytesToString(name);
                            });
                        });
                        this._hasNames = true;
                    }
                    else if (nameInfo.type === 3 /* Event */) {
                        var names = nameInfo.names;
                        names.forEach(function (_a) {
                            var index = _a.index, name = _a.name;
                            _this._eventNames[index] = WasmParser_js_1.bytesToString(name);
                        });
                        this._hasNames = true;
                    }
                    else if (nameInfo.type === 4 /* Type */) {
                        var names = nameInfo.names;
                        names.forEach(function (_a) {
                            var index = _a.index, name = _a.name;
                            _this._typeNames[index] = WasmParser_js_1.bytesToString(name);
                        });
                        this._hasNames = true;
                    }
                    else if (nameInfo.type === 5 /* Table */) {
                        var names = nameInfo.names;
                        names.forEach(function (_a) {
                            var index = _a.index, name = _a.name;
                            _this._tableNames[index] = WasmParser_js_1.bytesToString(name);
                        });
                        this._hasNames = true;
                    }
                    else if (nameInfo.type === 6 /* Memory */) {
                        var names = nameInfo.names;
                        names.forEach(function (_a) {
                            var index = _a.index, name = _a.name;
                            _this._memoryNames[index] = WasmParser_js_1.bytesToString(name);
                        });
                        this._hasNames = true;
                    }
                    else if (nameInfo.type === 7 /* Global */) {
                        var names = nameInfo.names;
                        names.forEach(function (_a) {
                            var index = _a.index, name = _a.name;
                            _this._globalNames[index] = WasmParser_js_1.bytesToString(name);
                        });
                        this._hasNames = true;
                    }
                    else if (nameInfo.type === 10 /* Field */) {
                        var types = nameInfo.types;
                        types.forEach(function (_a) {
                            var index = _a.index, fields = _a.fields;
                            var fieldNames = (_this._fieldNames[index] = []);
                            fields.forEach(function (_a) {
                                var index = _a.index, name = _a.name;
                                fieldNames[index] = WasmParser_js_1.bytesToString(name);
                            });
                        });
                    }
                    break;
                default:
                    throw new Error("Expectected state: " + reader.state);
            }
        }
    };
    NameSectionReader.prototype.hasValidNames = function () {
        return this._hasNames;
    };
    NameSectionReader.prototype.getNameResolver = function () {
        if (!this.hasValidNames())
            throw new Error("Has no valid name section");
        // Fix bad names.
        var functionNamesLength = this._functionImportsCount + this._functionsCount;
        var functionNames = this._functionNames.slice(0, functionNamesLength);
        var usedNameAt = Object.create(null);
        for (var i = 0; i < functionNames.length; i++) {
            var name_1 = functionNames[i];
            if (!name_1)
                continue;
            var goodName = !(name_1 in usedNameAt) &&
                isValidName(name_1) &&
                name_1.indexOf(UNKNOWN_FUNCTION_PREFIX) !== 0;
            if (!goodName) {
                if (usedNameAt[name_1] >= 0) {
                    // Remove all non-unique names.
                    functionNames[usedNameAt[name_1]] = null;
                    usedNameAt[name_1] = -1;
                }
                functionNames[i] = null;
                continue;
            }
            usedNameAt[name_1] = i;
        }
        return new NameSectionNameResolver(functionNames, this._functionLocalNames, this._eventNames, this._typeNames, this._tableNames, this._memoryNames, this._globalNames, this._fieldNames);
    };
    return NameSectionReader;
}());
exports.NameSectionReader = NameSectionReader;
var DevToolsNameResolver = /** @class */ (function (_super) {
    __extends(DevToolsNameResolver, _super);
    function DevToolsNameResolver(functionNames, localNames, eventNames, typeNames, tableNames, memoryNames, globalNames, fieldNames) {
        return _super.call(this, functionNames, localNames, eventNames, typeNames, tableNames, memoryNames, globalNames, fieldNames) || this;
    }
    DevToolsNameResolver.prototype.getFunctionName = function (index, isImport, isRef) {
        var name = this._functionNames[index];
        if (!name)
            return isImport ? "$import" + index : "$func" + index;
        return isRef ? "$" + name : "$" + name + " (;" + index + ";)";
    };
    return DevToolsNameResolver;
}(NameSectionNameResolver));
exports.DevToolsNameResolver = DevToolsNameResolver;
var DevToolsNameGenerator = /** @class */ (function () {
    function DevToolsNameGenerator() {
        this._done = false;
        this._functionImportsCount = 0;
        this._memoryImportsCount = 0;
        this._tableImportsCount = 0;
        this._globalImportsCount = 0;
        this._eventImportsCount = 0;
        this._functionNames = null;
        this._functionLocalNames = null;
        this._eventNames = null;
        this._memoryNames = null;
        this._typeNames = null;
        this._tableNames = null;
        this._globalNames = null;
        this._fieldNames = null;
        this._functionExportNames = null;
        this._globalExportNames = null;
        this._memoryExportNames = null;
        this._tableExportNames = null;
        this._eventExportNames = null;
    }
    DevToolsNameGenerator.prototype._addExportName = function (exportNames, index, name) {
        var names = exportNames[index];
        if (names) {
            names.push(name);
        }
        else {
            exportNames[index] = [name];
        }
    };
    DevToolsNameGenerator.prototype._setName = function (names, index, name, isNameSectionName) {
        if (!name)
            return;
        if (isNameSectionName) {
            if (!isValidName(name))
                return;
            names[index] = name;
        }
        else if (!names[index]) {
            names[index] = name.replace(INVALID_NAME_SYMBOLS_REGEX_GLOBAL, "_");
        }
    };
    DevToolsNameGenerator.prototype.read = function (reader) {
        var _this = this;
        if (this._done)
            throw new Error("Invalid state: disassembly process was already finished.");
        while (true) {
            if (!reader.read())
                return false;
            switch (reader.state) {
                case 2 /* END_WASM */:
                    if (!reader.hasMoreBytes()) {
                        this._done = true;
                        return true;
                    }
                    break;
                case -1 /* ERROR */:
                    throw reader.error;
                case 1 /* BEGIN_WASM */:
                    this._functionImportsCount = 0;
                    this._memoryImportsCount = 0;
                    this._tableImportsCount = 0;
                    this._globalImportsCount = 0;
                    this._eventImportsCount = 0;
                    this._functionNames = [];
                    this._functionLocalNames = [];
                    this._eventNames = [];
                    this._memoryNames = [];
                    this._typeNames = [];
                    this._tableNames = [];
                    this._globalNames = [];
                    this._fieldNames = [];
                    this._functionExportNames = [];
                    this._globalExportNames = [];
                    this._memoryExportNames = [];
                    this._tableExportNames = [];
                    this._eventExportNames = [];
                    break;
                case 4 /* END_SECTION */:
                    break;
                case 3 /* BEGIN_SECTION */:
                    var sectionInfo = reader.result;
                    if (sectionInfo.id === 0 /* Custom */ &&
                        WasmParser_js_1.bytesToString(sectionInfo.name) === NAME_SECTION_NAME) {
                        break;
                    }
                    switch (sectionInfo.id) {
                        case 2 /* Import */:
                        case 7 /* Export */:
                            break; // reading known section;
                        default:
                            reader.skipSection();
                            break;
                    }
                    break;
                case 12 /* IMPORT_SECTION_ENTRY */:
                    var importInfo = reader.result;
                    var importName = WasmParser_js_1.bytesToString(importInfo.module) + "." + WasmParser_js_1.bytesToString(importInfo.field);
                    switch (importInfo.kind) {
                        case 0 /* Function */:
                            this._setName(this._functionNames, this._functionImportsCount++, importName, false);
                            break;
                        case 1 /* Table */:
                            this._setName(this._tableNames, this._tableImportsCount++, importName, false);
                            break;
                        case 2 /* Memory */:
                            this._setName(this._memoryNames, this._memoryImportsCount++, importName, false);
                            break;
                        case 3 /* Global */:
                            this._setName(this._globalNames, this._globalImportsCount++, importName, false);
                            break;
                        case 4 /* Event */:
                            this._setName(this._eventNames, this._eventImportsCount++, importName, false);
                        default:
                            throw new Error("Unsupported export " + importInfo.kind);
                    }
                    break;
                case 19 /* NAME_SECTION_ENTRY */:
                    var nameInfo = reader.result;
                    if (nameInfo.type === 1 /* Function */) {
                        var names = nameInfo.names;
                        names.forEach(function (_a) {
                            var index = _a.index, name = _a.name;
                            _this._setName(_this._functionNames, index, WasmParser_js_1.bytesToString(name), true);
                        });
                    }
                    else if (nameInfo.type === 2 /* Local */) {
                        var funcs = nameInfo.funcs;
                        funcs.forEach(function (_a) {
                            var index = _a.index, locals = _a.locals;
                            var localNames = (_this._functionLocalNames[index] = []);
                            locals.forEach(function (_a) {
                                var index = _a.index, name = _a.name;
                                localNames[index] = WasmParser_js_1.bytesToString(name);
                            });
                        });
                    }
                    else if (nameInfo.type === 3 /* Event */) {
                        var names = nameInfo.names;
                        names.forEach(function (_a) {
                            var index = _a.index, name = _a.name;
                            _this._setName(_this._eventNames, index, WasmParser_js_1.bytesToString(name), true);
                        });
                    }
                    else if (nameInfo.type === 4 /* Type */) {
                        var names = nameInfo.names;
                        names.forEach(function (_a) {
                            var index = _a.index, name = _a.name;
                            _this._setName(_this._typeNames, index, WasmParser_js_1.bytesToString(name), true);
                        });
                    }
                    else if (nameInfo.type === 5 /* Table */) {
                        var names = nameInfo.names;
                        names.forEach(function (_a) {
                            var index = _a.index, name = _a.name;
                            _this._setName(_this._tableNames, index, WasmParser_js_1.bytesToString(name), true);
                        });
                    }
                    else if (nameInfo.type === 6 /* Memory */) {
                        var names = nameInfo.names;
                        names.forEach(function (_a) {
                            var index = _a.index, name = _a.name;
                            _this._setName(_this._memoryNames, index, WasmParser_js_1.bytesToString(name), true);
                        });
                    }
                    else if (nameInfo.type === 7 /* Global */) {
                        var names = nameInfo.names;
                        names.forEach(function (_a) {
                            var index = _a.index, name = _a.name;
                            _this._setName(_this._globalNames, index, WasmParser_js_1.bytesToString(name), true);
                        });
                    }
                    else if (nameInfo.type === 10 /* Field */) {
                        var types = nameInfo.types;
                        types.forEach(function (_a) {
                            var index = _a.index, fields = _a.fields;
                            var fieldNames = (_this._fieldNames[index] = []);
                            fields.forEach(function (_a) {
                                var index = _a.index, name = _a.name;
                                fieldNames[index] = WasmParser_js_1.bytesToString(name);
                            });
                        });
                    }
                    break;
                case 17 /* EXPORT_SECTION_ENTRY */:
                    var exportInfo = reader.result;
                    var exportName = WasmParser_js_1.bytesToString(exportInfo.field);
                    switch (exportInfo.kind) {
                        case 0 /* Function */:
                            this._addExportName(this._functionExportNames, exportInfo.index, exportName);
                            this._setName(this._functionNames, exportInfo.index, exportName, false);
                            break;
                        case 3 /* Global */:
                            this._addExportName(this._globalExportNames, exportInfo.index, exportName);
                            this._setName(this._globalNames, exportInfo.index, exportName, false);
                            break;
                        case 2 /* Memory */:
                            this._addExportName(this._memoryExportNames, exportInfo.index, exportName);
                            this._setName(this._memoryNames, exportInfo.index, exportName, false);
                            break;
                        case 1 /* Table */:
                            this._addExportName(this._tableExportNames, exportInfo.index, exportName);
                            this._setName(this._tableNames, exportInfo.index, exportName, false);
                            break;
                        case 4 /* Event */:
                            this._addExportName(this._eventExportNames, exportInfo.index, exportName);
                            this._setName(this._eventNames, exportInfo.index, exportName, false);
                            break;
                        default:
                            throw new Error("Unsupported export " + exportInfo.kind);
                    }
                    break;
                default:
                    throw new Error("Expectected state: " + reader.state);
            }
        }
    };
    DevToolsNameGenerator.prototype.getExportMetadata = function () {
        return new DevToolsExportMetadata(this._functionExportNames, this._globalExportNames, this._memoryExportNames, this._tableExportNames, this._eventExportNames);
    };
    DevToolsNameGenerator.prototype.getNameResolver = function () {
        return new DevToolsNameResolver(this._functionNames, this._functionLocalNames, this._eventNames, this._typeNames, this._tableNames, this._memoryNames, this._globalNames, this._fieldNames);
    };
    return DevToolsNameGenerator;
}());
exports.DevToolsNameGenerator = DevToolsNameGenerator;