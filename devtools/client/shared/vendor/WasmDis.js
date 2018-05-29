"use strict";
var __extends = (this && this.__extends) || (function () {
    var extendStatics = Object.setPrototypeOf ||
        ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
        function (d, b) { for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p]; };
    return function (d, b) {
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
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
var WasmParser_1 = require("./WasmParser");
function typeToString(type) {
    switch (type) {
        case -1 /* i32 */: return 'i32';
        case -2 /* i64 */: return 'i64';
        case -3 /* f32 */: return 'f32';
        case -4 /* f64 */: return 'f64';
        case -16 /* anyfunc */: return 'anyfunc';
        default: throw new Error('Unexpected type');
    }
}
function formatFloat32(n) {
    if (n === 0)
        return (1 / n) < 0 ? '-0.0' : '0.0';
    if (isFinite(n))
        return n.toString();
    if (!isNaN(n))
        return n < 0 ? '-infinity' : 'infinity';
    var view = new DataView(new ArrayBuffer(8));
    view.setFloat32(0, n, true);
    var data = view.getInt32(0, true);
    var payload = data & 0x7FFFFF;
    var canonicalBits = 4194304; // 0x800..0
    if (data > 0 && payload === canonicalBits)
        return 'nan'; // canonical NaN;
    else if (payload === canonicalBits)
        return '-nan';
    return (data < 0 ? '-' : '+') + 'nan:0x' + payload.toString(16);
}
function formatFloat64(n) {
    if (n === 0)
        return (1 / n) < 0 ? '-0.0' : '0.0';
    if (isFinite(n))
        return n.toString();
    if (!isNaN(n))
        return n < 0 ? '-infinity' : 'infinity';
    var view = new DataView(new ArrayBuffer(8));
    view.setFloat64(0, n, true);
    var data1 = view.getUint32(0, true);
    var data2 = view.getInt32(4, true);
    var payload = data1 + (data2 & 0xFFFFF) * 4294967296;
    var canonicalBits = 524288 * 4294967296; // 0x800..0
    if (data2 > 0 && payload === canonicalBits)
        return 'nan'; // canonical NaN;
    else if (payload === canonicalBits)
        return '-nan';
    return (data2 < 0 ? '-' : '+') + 'nan:0x' + payload.toString(16);
}
function memoryAddressToString(address, code) {
    var defaultAlignFlags;
    switch (code) {
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
            defaultAlignFlags = 3;
            break;
        case 40 /* i32_load */:
        case 52 /* i64_load32_s */:
        case 53 /* i64_load32_u */:
        case 54 /* i32_store */:
        case 62 /* i64_store32 */:
        case 42 /* f32_load */:
        case 56 /* f32_store */:
        case 65024 /* atomic_wake */:
        case 65025 /* i32_atomic_wait */:
        case 65040 /* i32_atomic_load */:
        case 65046 /* i64_atomic_load32_u */:
        case 65047 /* i32_atomic_store */:
        case 65053 /* i64_atomic_store32 */:
        case 65054 /* i32_atomic_rmw_add */:
        case 65060 /* i64_atomic_rmw32_u_add */:
        case 65061 /* i32_atomic_rmw_sub */:
        case 65067 /* i64_atomic_rmw32_u_sub */:
        case 65068 /* i32_atomic_rmw_and */:
        case 65074 /* i64_atomic_rmw32_u_and */:
        case 65075 /* i32_atomic_rmw_or */:
        case 65081 /* i64_atomic_rmw32_u_or */:
        case 65082 /* i32_atomic_rmw_xor */:
        case 65088 /* i64_atomic_rmw32_u_xor */:
        case 65089 /* i32_atomic_rmw_xchg */:
        case 65095 /* i64_atomic_rmw32_u_xchg */:
        case 65096 /* i32_atomic_rmw_cmpxchg */:
        case 65102 /* i64_atomic_rmw32_u_cmpxchg */:
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
        case 65057 /* i32_atomic_rmw16_u_add */:
        case 65059 /* i64_atomic_rmw16_u_add */:
        case 65064 /* i32_atomic_rmw16_u_sub */:
        case 65066 /* i64_atomic_rmw16_u_sub */:
        case 65071 /* i32_atomic_rmw16_u_and */:
        case 65073 /* i64_atomic_rmw16_u_and */:
        case 65078 /* i32_atomic_rmw16_u_or */:
        case 65080 /* i64_atomic_rmw16_u_or */:
        case 65085 /* i32_atomic_rmw16_u_xor */:
        case 65087 /* i64_atomic_rmw16_u_xor */:
        case 65092 /* i32_atomic_rmw16_u_xchg */:
        case 65094 /* i64_atomic_rmw16_u_xchg */:
        case 65099 /* i32_atomic_rmw16_u_cmpxchg */:
        case 65101 /* i64_atomic_rmw16_u_cmpxchg */:
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
        case 65056 /* i32_atomic_rmw8_u_add */:
        case 65058 /* i64_atomic_rmw8_u_add */:
        case 65063 /* i32_atomic_rmw8_u_sub */:
        case 65065 /* i64_atomic_rmw8_u_sub */:
        case 65070 /* i32_atomic_rmw8_u_and */:
        case 65072 /* i64_atomic_rmw8_u_and */:
        case 65077 /* i32_atomic_rmw8_u_or */:
        case 65079 /* i64_atomic_rmw8_u_or */:
        case 65084 /* i32_atomic_rmw8_u_xor */:
        case 65086 /* i64_atomic_rmw8_u_xor */:
        case 65091 /* i32_atomic_rmw8_u_xchg */:
        case 65093 /* i64_atomic_rmw8_u_xchg */:
        case 65098 /* i32_atomic_rmw8_u_cmpxchg */:
        case 65100 /* i64_atomic_rmw8_u_cmpxchg */:
            defaultAlignFlags = 0;
            break;
    }
    if (address.flags == defaultAlignFlags)
        return !address.offset ? null : "offset=" + address.offset;
    if (!address.offset)
        return "align=" + (1 << address.flags);
    return "offset=" + (address.offset | 0) + " align=" + (1 << address.flags);
}
function globalTypeToString(type) {
    if (!type.mutability)
        return typeToString(type.contentType);
    return "(mut " + typeToString(type.contentType) + ")";
}
function limitsToString(limits) {
    return limits.initial + (limits.maximum !== undefined ? ' ' + limits.maximum : '');
}
var paddingCache = ['0', '00', '000'];
function formatHex(n, width) {
    var s = n.toString(16).toUpperCase();
    if (width === undefined || s.length >= width)
        return s;
    var paddingIndex = width - s.length - 1;
    while (paddingIndex >= paddingCache.length)
        paddingCache.push(paddingCache[paddingCache.length - 1] + '0');
    return paddingCache[paddingIndex] + s;
}
var IndentIncrement = '  ';
var operatorCodeNamesCache = null;
function getOperatorName(code) {
    if (!operatorCodeNamesCache) {
        operatorCodeNamesCache = Object.create(null);
        Object.keys(WasmParser_1.OperatorCodeNames).forEach(function (key) {
            var value = WasmParser_1.OperatorCodeNames[key];
            if (typeof value !== 'string')
                return;
            operatorCodeNamesCache[key] = value.replace(/^([if](32|64))_/, "$1.").replace(/_([if](32|64))$/, "\/$1");
        });
    }
    return operatorCodeNamesCache[code];
}
var DefaultNameResolver = /** @class */ (function () {
    function DefaultNameResolver() {
    }
    DefaultNameResolver.prototype.getTypeName = function (index, isRef) {
        return '$type' + index;
    };
    DefaultNameResolver.prototype.getTableName = function (index, isRef) {
        return '$table' + index;
    };
    DefaultNameResolver.prototype.getMemoryName = function (index, isRef) {
        // TODO '$memory' + index;
        return isRef ? '' + index : "(;" + index + ";)";
    };
    DefaultNameResolver.prototype.getGlobalName = function (index, isRef) {
        return '$global' + index;
    };
    DefaultNameResolver.prototype.getFunctionName = function (index, isImport, isRef) {
        return (isImport ? '$import' : '$func') + index;
    };
    DefaultNameResolver.prototype.getVariableName = function (funcIndex, index, isRef) {
        return '$var' + index;
    };
    DefaultNameResolver.prototype.getLabel = function (index) {
        return '$label' + index;
    };
    return DefaultNameResolver;
}());
exports.DefaultNameResolver = DefaultNameResolver;
var NumericNameResolver = /** @class */ (function () {
    function NumericNameResolver() {
    }
    NumericNameResolver.prototype.getTypeName = function (index, isRef) {
        return isRef ? '' + index : "(;" + index + ";)";
    };
    NumericNameResolver.prototype.getTableName = function (index, isRef) {
        return isRef ? '' + index : "(;" + index + ";)";
    };
    NumericNameResolver.prototype.getMemoryName = function (index, isRef) {
        return isRef ? '' + index : "(;" + index + ";)";
    };
    NumericNameResolver.prototype.getGlobalName = function (index, isRef) {
        return isRef ? '' + index : "(;" + index + ";)";
    };
    NumericNameResolver.prototype.getFunctionName = function (index, isImport, isRef) {
        return isRef ? '' + index : "(;" + index + ";)";
    };
    NumericNameResolver.prototype.getVariableName = function (funcIndex, index, isRef) {
        return isRef ? '' + index : "(;" + index + ";)";
    };
    NumericNameResolver.prototype.getLabel = function (index) {
        return null;
    };
    return NumericNameResolver;
}());
exports.NumericNameResolver = NumericNameResolver;
var LineBuffer = /** @class */ (function () {
    function LineBuffer() {
        this._firstPart = '';
        this._secondPart = '';
        this._thirdPart = '';
        this._count = 0;
    }
    Object.defineProperty(LineBuffer.prototype, "length", {
        get: function () {
            switch (this._count) {
                case 0:
                    return 0;
                case 1:
                    return this._firstPart.length;
                case 2:
                    return this._firstPart.length + this._secondPart.length;
                default:
                    return this._firstPart.length +
                        this._secondPart.length +
                        this._thirdPart.length;
            }
        },
        enumerable: true,
        configurable: true
    });
    LineBuffer.prototype.append = function (part) {
        switch (this._count) {
            case 0:
                this._firstPart = part;
                this._count = 1;
                break;
            case 1:
                this._secondPart = part;
                this._count = 2;
                break;
            case 2:
                this._thirdPart = part;
                this._count = 3;
                break;
            default:
                this._count = 1;
                this._firstPart = this._firstPart + this._secondPart +
                    this._thirdPart + part;
                break;
        }
    };
    LineBuffer.prototype.finalize = function () {
        switch (this._count) {
            case 0:
                return '';
            case 1:
                this._count = 0;
                return this._firstPart;
            case 2:
                this._count = 0;
                return this._firstPart + this._secondPart;
            default:
                this._count = 0;
                return this._firstPart + this._secondPart + this._thirdPart;
        }
    };
    return LineBuffer;
}());
var LabelMode;
(function (LabelMode) {
    LabelMode[LabelMode["Depth"] = 0] = "Depth";
    LabelMode[LabelMode["WhenUsed"] = 1] = "WhenUsed";
    LabelMode[LabelMode["Always"] = 2] = "Always";
})(LabelMode = exports.LabelMode || (exports.LabelMode = {}));
var WasmDisassembler = /** @class */ (function () {
    function WasmDisassembler() {
        this._lines = [];
        this._offsets = [];
        this._buffer = new LineBuffer();
        this._indent = null;
        this._indentLevel = 0;
        this._addOffsets = false;
        this._done = false;
        this._currentPosition = 0;
        this._nameResolver = new DefaultNameResolver();
        this._labelMode = LabelMode.WhenUsed;
        this._reset();
    }
    WasmDisassembler.prototype._reset = function () {
        this._types = [];
        this._funcIndex = 0;
        this._funcTypes = [];
        this._importCount = 0;
        this._globalCount = 0;
        this._memoryCount = 0;
        this._tableCount = 0;
        this._initExpression = [];
        this._backrefLabels = null;
        this._labelIndex = 0;
    };
    Object.defineProperty(WasmDisassembler.prototype, "addOffsets", {
        get: function () {
            return this._addOffsets;
        },
        set: function (value) {
            if (this._currentPosition)
                throw new Error('Cannot switch addOffsets during processing.');
            this._addOffsets = value;
        },
        enumerable: true,
        configurable: true
    });
    Object.defineProperty(WasmDisassembler.prototype, "labelMode", {
        get: function () {
            return this._labelMode;
        },
        set: function (value) {
            if (this._currentPosition)
                throw new Error('Cannot switch labelMode during processing.');
            this._labelMode = value;
        },
        enumerable: true,
        configurable: true
    });
    Object.defineProperty(WasmDisassembler.prototype, "nameResolver", {
        get: function () {
            return this._nameResolver;
        },
        set: function (resolver) {
            if (this._currentPosition)
                throw new Error('Cannot switch nameResolver during processing.');
            this._nameResolver = resolver;
        },
        enumerable: true,
        configurable: true
    });
    WasmDisassembler.prototype.appendBuffer = function (s) {
        this._buffer.append(s);
    };
    WasmDisassembler.prototype.newLine = function () {
        if (this.addOffsets)
            this._offsets.push(this._currentPosition);
        this._lines.push(this._buffer.finalize());
    };
    WasmDisassembler.prototype.printFuncType = function (typeIndex) {
        var type = this._types[typeIndex];
        if (type.form !== -32 /* func */)
            throw new Error('NYI other function form');
        if (type.params.length > 0) {
            this.appendBuffer(' (param');
            for (var i = 0; i < type.params.length; i++) {
                this.appendBuffer(' ');
                this.appendBuffer(typeToString(type.params[i]));
            }
            this.appendBuffer(')');
        }
        if (type.returns.length > 0) {
            this.appendBuffer(' (result');
            for (var i = 0; i < type.returns.length; i++) {
                this.appendBuffer(' ');
                this.appendBuffer(typeToString(type.returns[i]));
            }
            this.appendBuffer(')');
        }
    };
    WasmDisassembler.prototype.printString = function (b) {
        this.appendBuffer('\"');
        for (var i = 0; i < b.length; i++) {
            var byte = b[i];
            if (byte < 0x20 || byte >= 0x7F ||
                byte == /* " */ 0x22 || byte == /* \ */ 0x5c) {
                this.appendBuffer('\\' + (byte >> 4).toString(16) + (byte & 15).toString(16));
            }
            else {
                this.appendBuffer(String.fromCharCode(byte));
            }
        }
        this.appendBuffer('\"');
    };
    WasmDisassembler.prototype.useLabel = function (depth) {
        if (!this._backrefLabels) {
            return '' + depth;
        }
        var i = this._backrefLabels.length - depth - 1;
        if (i < 0) {
            return '' + depth;
        }
        var backrefLabel = this._backrefLabels[i];
        if (!backrefLabel.useLabel) {
            backrefLabel.useLabel = true;
            backrefLabel.label = this._nameResolver.getLabel(this._labelIndex);
            var line = this._lines[backrefLabel.line];
            this._lines[backrefLabel.line] = line.substring(0, backrefLabel.position) +
                ' ' + backrefLabel.label + line.substring(backrefLabel.position);
            this._labelIndex++;
        }
        return backrefLabel.label || '' + depth;
    };
    WasmDisassembler.prototype.printOperator = function (operator) {
        var code = operator.code;
        this.appendBuffer(getOperatorName(code));
        switch (code) {
            case 2 /* block */:
            case 3 /* loop */:
            case 4 /* if */:
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
                            this.appendBuffer(' ');
                            this.appendBuffer(backrefLabel_1.label);
                        }
                    }
                    this._backrefLabels.push(backrefLabel_1);
                }
                if (operator.blockType !== -64 /* empty_block_type */) {
                    this.appendBuffer(' (result ');
                    this.appendBuffer(typeToString(operator.blockType));
                    this.appendBuffer(')');
                }
                break;
            case 11 /* end */:
                if (this._labelMode === LabelMode.Depth) {
                    break;
                }
                var backrefLabel = this._backrefLabels.pop();
                if (backrefLabel.label) {
                    this.appendBuffer(' ');
                    this.appendBuffer(backrefLabel.label);
                }
                break;
            case 12 /* br */:
            case 13 /* br_if */:
                this.appendBuffer(' ');
                this.appendBuffer(this.useLabel(operator.brDepth));
                break;
            case 14 /* br_table */:
                for (var i = 0; i < operator.brTable.length; i++) {
                    this.appendBuffer(' ');
                    this.appendBuffer(this.useLabel(operator.brTable[i]));
                }
                break;
            case 16 /* call */:
                var funcName = this._nameResolver.getFunctionName(operator.funcIndex, operator.funcIndex < this._importCount, true);
                this.appendBuffer(" " + funcName);
                break;
            case 17 /* call_indirect */:
                var typeName = this._nameResolver.getTypeName(operator.typeIndex, true);
                this.appendBuffer(" " + typeName);
                break;
            case 32 /* get_local */:
            case 33 /* set_local */:
            case 34 /* tee_local */:
                var paramName = this._nameResolver.getVariableName(this._funcIndex, operator.localIndex, true);
                this.appendBuffer(" " + paramName);
                break;
            case 35 /* get_global */:
            case 36 /* set_global */:
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
            case 65024 /* atomic_wake */:
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
            case 65056 /* i32_atomic_rmw8_u_add */:
            case 65057 /* i32_atomic_rmw16_u_add */:
            case 65058 /* i64_atomic_rmw8_u_add */:
            case 65059 /* i64_atomic_rmw16_u_add */:
            case 65060 /* i64_atomic_rmw32_u_add */:
            case 65061 /* i32_atomic_rmw_sub */:
            case 65062 /* i64_atomic_rmw_sub */:
            case 65063 /* i32_atomic_rmw8_u_sub */:
            case 65064 /* i32_atomic_rmw16_u_sub */:
            case 65065 /* i64_atomic_rmw8_u_sub */:
            case 65066 /* i64_atomic_rmw16_u_sub */:
            case 65067 /* i64_atomic_rmw32_u_sub */:
            case 65068 /* i32_atomic_rmw_and */:
            case 65069 /* i64_atomic_rmw_and */:
            case 65070 /* i32_atomic_rmw8_u_and */:
            case 65071 /* i32_atomic_rmw16_u_and */:
            case 65072 /* i64_atomic_rmw8_u_and */:
            case 65073 /* i64_atomic_rmw16_u_and */:
            case 65074 /* i64_atomic_rmw32_u_and */:
            case 65075 /* i32_atomic_rmw_or */:
            case 65076 /* i64_atomic_rmw_or */:
            case 65077 /* i32_atomic_rmw8_u_or */:
            case 65078 /* i32_atomic_rmw16_u_or */:
            case 65079 /* i64_atomic_rmw8_u_or */:
            case 65080 /* i64_atomic_rmw16_u_or */:
            case 65081 /* i64_atomic_rmw32_u_or */:
            case 65082 /* i32_atomic_rmw_xor */:
            case 65083 /* i64_atomic_rmw_xor */:
            case 65084 /* i32_atomic_rmw8_u_xor */:
            case 65085 /* i32_atomic_rmw16_u_xor */:
            case 65086 /* i64_atomic_rmw8_u_xor */:
            case 65087 /* i64_atomic_rmw16_u_xor */:
            case 65088 /* i64_atomic_rmw32_u_xor */:
            case 65089 /* i32_atomic_rmw_xchg */:
            case 65090 /* i64_atomic_rmw_xchg */:
            case 65091 /* i32_atomic_rmw8_u_xchg */:
            case 65092 /* i32_atomic_rmw16_u_xchg */:
            case 65093 /* i64_atomic_rmw8_u_xchg */:
            case 65094 /* i64_atomic_rmw16_u_xchg */:
            case 65095 /* i64_atomic_rmw32_u_xchg */:
            case 65096 /* i32_atomic_rmw_cmpxchg */:
            case 65097 /* i64_atomic_rmw_cmpxchg */:
            case 65098 /* i32_atomic_rmw8_u_cmpxchg */:
            case 65099 /* i32_atomic_rmw16_u_cmpxchg */:
            case 65100 /* i64_atomic_rmw8_u_cmpxchg */:
            case 65101 /* i64_atomic_rmw16_u_cmpxchg */:
            case 65102 /* i64_atomic_rmw32_u_cmpxchg */:
                var memoryAddress = memoryAddressToString(operator.memoryAddress, operator.code);
                if (memoryAddress !== null) {
                    this.appendBuffer(' ');
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
                this.appendBuffer(" " + operator.literal.toDouble());
                break;
            case 67 /* f32_const */:
                this.appendBuffer(" " + formatFloat32(operator.literal));
                break;
            case 68 /* f64_const */:
                this.appendBuffer(" " + formatFloat64(operator.literal));
                break;
        }
    };
    WasmDisassembler.prototype.printImportSource = function (info) {
        this.printString(info.module);
        this.appendBuffer(' ');
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
                return line + ' ;; @' + position;
            });
        }
        lines.push(''); // we need '\n' after last line
        var result = lines.join('\n');
        this._lines.length = 0;
        this._offsets.length = 0;
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
            };
        }
        if (linesReady === this._lines.length) {
            var result_1 = {
                lines: this._lines,
                offsets: this._addOffsets ? this._offsets : undefined,
                done: this._done,
            };
            this._lines = [];
            if (this._addOffsets)
                this._offsets = [];
            return result_1;
        }
        var result = {
            lines: this._lines.splice(0, linesReady),
            offsets: this._addOffsets ? this._offsets.splice(0, linesReady) : undefined,
            done: false,
        };
        if (this._backrefLabels) {
            this._backrefLabels.forEach(function (backrefLabel) {
                backrefLabel.line -= linesReady;
            });
        }
        return result;
    };
    WasmDisassembler.prototype.disassembleChunk = function (reader, offsetInModule) {
        var _this = this;
        if (offsetInModule === void 0) { offsetInModule = 0; }
        if (this._done)
            throw new Error('Invalid state: disassembly process was already finished.');
        while (true) {
            this._currentPosition = reader.position + offsetInModule;
            if (!reader.read())
                return false;
            switch (reader.state) {
                case 2 /* END_WASM */:
                    this.appendBuffer(')');
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
                    this.appendBuffer('(module');
                    this.newLine();
                    break;
                case 4 /* END_SECTION */:
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
                            break; // reading known section;
                        default:
                            reader.skipSection();
                            break;
                    }
                    break;
                case 15 /* MEMORY_SECTION_ENTRY */:
                    var memoryInfo = reader.result;
                    var memoryName = this._nameResolver.getMemoryName(this._memoryCount++, false);
                    this.appendBuffer("  (memory " + memoryName + " ");
                    if (memoryInfo.shared) {
                        this.appendBuffer("(shared " + limitsToString(memoryInfo.limits) + ")");
                    }
                    else {
                        this.appendBuffer(limitsToString(memoryInfo.limits));
                    }
                    this.appendBuffer(')');
                    this.newLine();
                    break;
                case 14 /* TABLE_SECTION_ENTRY */:
                    var tableInfo = reader.result;
                    var tableName = this._nameResolver.getTableName(this._tableCount++, false);
                    this.appendBuffer("  (table " + tableName + " " + limitsToString(tableInfo.limits) + " " + typeToString(tableInfo.elementType) + ")");
                    this.newLine();
                    break;
                case 17 /* EXPORT_SECTION_ENTRY */:
                    var exportInfo = reader.result;
                    this.appendBuffer('  (export ');
                    this.printString(exportInfo.field);
                    this.appendBuffer(' ');
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
                        default:
                            throw new Error("Unsupported export " + exportInfo.kind);
                    }
                    this.appendBuffer(')');
                    this.newLine();
                    break;
                case 12 /* IMPORT_SECTION_ENTRY */:
                    var importInfo = reader.result;
                    this.appendBuffer('  (import ');
                    this.printImportSource(importInfo);
                    switch (importInfo.kind) {
                        case 0 /* Function */:
                            this._importCount++;
                            var funcName = this._nameResolver.getFunctionName(this._funcIndex++, true, false);
                            this.appendBuffer(" (func " + funcName);
                            this.printFuncType(importInfo.funcTypeIndex);
                            this.appendBuffer(')');
                            break;
                        case 1 /* Table */:
                            var tableImportInfo = importInfo.type;
                            var tableName = this._nameResolver.getTableName(this._tableCount++, false);
                            this.appendBuffer(" (table " + tableName + " " + limitsToString(tableImportInfo.limits) + " " + typeToString(tableImportInfo.elementType) + ")");
                            break;
                        case 2 /* Memory */:
                            var memoryImportInfo = importInfo.type;
                            var memoryName = this._nameResolver.getMemoryName(this._memoryCount++, false);
                            this.appendBuffer(" (memory " + memoryName + " ");
                            if (memoryImportInfo.shared) {
                                this.appendBuffer("(shared " + limitsToString(memoryImportInfo.limits) + ")");
                            }
                            else {
                                this.appendBuffer(limitsToString(memoryImportInfo.limits));
                            }
                            this.appendBuffer(')');
                            break;
                        case 3 /* Global */:
                            var globalImportInfo = importInfo.type;
                            var globalName = this._nameResolver.getGlobalName(this._globalCount++, false);
                            this.appendBuffer(" (global " + globalName + " " + globalTypeToString(globalImportInfo) + ")");
                            break;
                        default:
                            throw new Error("NYI other import types: " + importInfo.kind);
                    }
                    this.appendBuffer(')');
                    this.newLine();
                    break;
                case 33 /* BEGIN_ELEMENT_SECTION_ENTRY */:
                    var elementSegmentInfo = reader.result;
                    this.appendBuffer('  (elem ');
                    break;
                case 35 /* END_ELEMENT_SECTION_ENTRY */:
                    this.appendBuffer(')');
                    this.newLine();
                    break;
                case 34 /* ELEMENT_SECTION_ENTRY_BODY */:
                    var elementSegmentBody = reader.result;
                    elementSegmentBody.elements.forEach(function (funcIndex) {
                        var funcName = _this._nameResolver.getFunctionName(funcIndex, funcIndex < _this._importCount, true);
                        _this.appendBuffer(" " + funcName);
                    });
                    break;
                case 39 /* BEGIN_GLOBAL_SECTION_ENTRY */:
                    var globalInfo = reader.result;
                    var globalName = this._nameResolver.getGlobalName(this._globalCount++, false);
                    this.appendBuffer("  (global " + globalName + " " + globalTypeToString(globalInfo.type) + " ");
                    break;
                case 40 /* END_GLOBAL_SECTION_ENTRY */:
                    this.appendBuffer(')');
                    this.newLine();
                    break;
                case 11 /* TYPE_SECTION_ENTRY */:
                    var funcType = reader.result;
                    var typeIndex = this._types.length;
                    this._types.push(funcType);
                    var typeName = this._nameResolver.getTypeName(typeIndex, false);
                    this.appendBuffer("  (type " + typeName + " (func");
                    this.printFuncType(typeIndex);
                    this.appendBuffer('))');
                    this.newLine();
                    break;
                case 22 /* START_SECTION_ENTRY */:
                    var startEntry = reader.result;
                    var funcName = this._nameResolver.getFunctionName(startEntry.index, startEntry.index < this._importCount, true);
                    this.appendBuffer("  (start " + funcName + ")");
                    this.newLine();
                    break;
                case 36 /* BEGIN_DATA_SECTION_ENTRY */:
                    this.appendBuffer('  (data ');
                    break;
                case 37 /* DATA_SECTION_ENTRY_BODY */:
                    var body = reader.result;
                    this.newLine();
                    this.appendBuffer('    ');
                    this.printString(body.data);
                    this.newLine();
                    break;
                case 38 /* END_DATA_SECTION_ENTRY */:
                    this.appendBuffer('  )');
                    this.newLine();
                    break;
                case 25 /* BEGIN_INIT_EXPRESSION_BODY */:
                    break;
                case 26 /* INIT_EXPRESSION_OPERATOR */:
                    this._initExpression.push(reader.result);
                    break;
                case 27 /* END_INIT_EXPRESSION_BODY */:
                    this.appendBuffer('(');
                    // TODO fix printing when more that one operator is used.
                    this._initExpression.forEach(function (op, index) {
                        if (op.code === 11 /* end */) {
                            return; // do not print end
                        }
                        if (index > 0) {
                            _this.appendBuffer(' ');
                        }
                        _this.printOperator(op);
                    });
                    this.appendBuffer(')');
                    this._initExpression.length = 0;
                    break;
                case 13 /* FUNCTION_SECTION_ENTRY */:
                    this._funcTypes.push(reader.result.typeIndex);
                    break;
                case 28 /* BEGIN_FUNCTION_BODY */:
                    var func = reader.result;
                    var type = this._types[this._funcTypes[this._funcIndex - this._importCount]];
                    this.appendBuffer('  (func ');
                    this.appendBuffer(this._nameResolver.getFunctionName(this._funcIndex, false, false));
                    for (var i = 0; i < type.params.length; i++) {
                        var paramName = this._nameResolver.getVariableName(this._funcIndex, i, false);
                        this.appendBuffer(" (param " + paramName + " " + typeToString(type.params[i]) + ")");
                    }
                    for (var i = 0; i < type.returns.length; i++) {
                        this.appendBuffer(" (result " + typeToString(type.returns[i]) + ")");
                    }
                    this.newLine();
                    var localIndex = type.params.length;
                    if (func.locals.length > 0) {
                        this.appendBuffer('   ');
                        for (var _i = 0, _a = func.locals; _i < _a.length; _i++) {
                            var l = _a[_i];
                            for (var i = 0; i < l.count; i++) {
                                var paramName = this._nameResolver.getVariableName(this._funcIndex, localIndex++, false);
                                this.appendBuffer(" (local " + paramName + " " + typeToString(l.type) + ")");
                            }
                        }
                        this.newLine();
                    }
                    this._indent = '    ';
                    this._indentLevel = 0;
                    this._labelIndex = 0;
                    this._backrefLabels = this._labelMode === LabelMode.Depth ? null : [];
                    break;
                case 30 /* CODE_OPERATOR */:
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
                            this.increaseIndent();
                            break;
                    }
                    break;
                case 31 /* END_FUNCTION_BODY */:
                    this._funcIndex++;
                    this._backrefLabels = null;
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
    function NameSectionNameResolver(names) {
        var _this = _super.call(this) || this;
        _this._names = names;
        return _this;
    }
    NameSectionNameResolver.prototype.getFunctionName = function (index, isImport, isRef) {
        var name = this._names[index];
        if (!name)
            return "$" + UNKNOWN_FUNCTION_PREFIX + index;
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
        this._hasNames = false;
    }
    NameSectionReader.prototype.read = function (reader) {
        var _this = this;
        if (this._done)
            throw new Error('Invalid state: disassembly process was already finished.');
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
                    this._hasNames = false;
                    break;
                case 4 /* END_SECTION */:
                    break;
                case 3 /* BEGIN_SECTION */:
                    var sectionInfo = reader.result;
                    if (sectionInfo.id === 0 /* Custom */ &&
                        WasmParser_1.bytesToString(sectionInfo.name) === "name") {
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
                    if (nameInfo.type !== 1 /* Function */)
                        break;
                    var functionNameInfo = nameInfo;
                    functionNameInfo.names.forEach(function (naming) {
                        _this._functionNames[naming.index] = WasmParser_1.bytesToString(naming.name);
                    });
                    this._hasNames = true;
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
                !/[^0-9A-Za-z!#$%&'*+.:<=>?@^_`|~\/\-]/.test(name_1) &&
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
        return new NameSectionNameResolver(functionNames);
    };
    return NameSectionReader;
}());
exports.NameSectionReader = NameSectionReader;
