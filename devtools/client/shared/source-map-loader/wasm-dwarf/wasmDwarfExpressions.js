/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint camelcase: 0*/
/* eslint-disable no-inline-comments */

"use strict";

class Value {
  val;

  constructor(val) {
    this.val = val;
  }
  toString() {
    return `${this.val}`;
  }
}

const Int32Formatter = {
  fromAddr(addr) {
    return `(new DataView(memory0.buffer).getInt32(${addr}, true))`;
  },
  fromValue(value) {
    return `${value.val}`;
  },
};

const Uint32Formatter = {
  fromAddr(addr) {
    return `(new DataView(memory0.buffer).getUint32(${addr}, true))`;
  },
  fromValue(value) {
    return `(${value.val} >>> 0)`;
  },
};

function createPieceFormatter(bytes) {
  let getter;
  switch (bytes) {
    case 0:
    case 1:
      getter = "getUint8";
      break;
    case 2:
      getter = "getUint16";
      break;
    case 3:
    case 4:
    default:
      // FIXME 64-bit
      getter = "getUint32";
      break;
  }
  const mask = (1 << (8 * bytes)) - 1;
  return {
    fromAddr(addr) {
      return `(new DataView(memory0.buffer).${getter}(${addr}, true))`;
    },
    fromValue(value) {
      return `((${value.val} & ${mask}) >>> 0)`;
    },
  };
}

// eslint-disable-next-line complexity
function toJS(buf, typeFormatter, frame_base = "fp()") {
  const readU8 = function () {
    return buf[i++];
  };
  const readS8 = function () {
    return (readU8() << 24) >> 24;
  };
  const readU16 = function () {
    const w = buf[i] | (buf[i + 1] << 8);
    i += 2;
    return w;
  };
  const readS16 = function () {
    return (readU16() << 16) >> 16;
  };
  const readS32 = function () {
    const w =
      buf[i] | (buf[i + 1] << 8) | (buf[i + 2] << 16) | (buf[i + 3] << 24);
    i += 4;
    return w;
  };
  const readU32 = function () {
    return readS32() >>> 0;
  };
  const readU = function () {
    let n = 0,
      shift = 0,
      b;
    while ((b = readU8()) & 0x80) {
      n |= (b & 0x7f) << shift;
      shift += 7;
    }
    return n | (b << shift);
  };
  const readS = function () {
    let n = 0,
      shift = 0,
      b;
    while ((b = readU8()) & 0x80) {
      n |= (b & 0x7f) << shift;
      shift += 7;
    }
    n |= b << shift;
    shift += 7;
    return shift > 32 ? (n << (32 - shift)) >> (32 - shift) : n;
  };
  const popValue = function (formatter) {
    const loc = stack.pop();
    if (loc instanceof Value) {
      return formatter.fromValue(loc);
    }
    return formatter.fromAddr(loc);
  };
  let i = 0,
    a,
    b;
  const stack = [frame_base];
  while (i < buf.length) {
    const code = buf[i++];
    switch (code) {
      case 0x03 /* DW_OP_addr */:
        stack.push(Uint32Formatter.fromAddr(readU32()));
        break;
      case 0x08 /* DW_OP_const1u 0x08 1 1-byte constant */:
        stack.push(readU8());
        break;
      case 0x09 /* DW_OP_const1s 0x09 1 1-byte constant */:
        stack.push(readS8());
        break;
      case 0x0a /* DW_OP_const2u 0x0a 1 2-byte constant */:
        stack.push(readU16());
        break;
      case 0x0b /* DW_OP_const2s 0x0b 1 2-byte constant */:
        stack.push(readS16());
        break;
      case 0x0c /* DW_OP_const2u 0x0a 1 2-byte constant */:
        stack.push(readU32());
        break;
      case 0x0d /* DW_OP_const2s 0x0b 1 2-byte constant */:
        stack.push(readS32());
        break;
      case 0x10 /* DW_OP_constu 0x10 1 ULEB128 constant */:
        stack.push(readU());
        break;
      case 0x11 /* DW_OP_const2s 0x0b 1 2-byte constant */:
        stack.push(readS());
        break;

      case 0x1c /* DW_OP_minus */:
        b = stack.pop();
        a = stack.pop();
        stack.push(`${a} - ${b}`);
        break;

      case 0x22 /* DW_OP_plus */:
        b = stack.pop();
        a = stack.pop();
        stack.push(`${a} + ${b}`);
        break;

      case 0x23 /* DW_OP_plus_uconst */:
        b = readU();
        a = stack.pop();
        stack.push(`${a} + ${b}`);
        break;

      case 0x30 /* DW_OP_lit0 */:
      case 0x31:
      case 0x32:
      case 0x33:
      case 0x34:
      case 0x35:
      case 0x36:
      case 0x37:
      case 0x38:
      case 0x39:
      case 0x3a:
      case 0x3b:
      case 0x3c:
      case 0x3d:
      case 0x3e:
      case 0x3f:
      case 0x40:
      case 0x41:
      case 0x42:
      case 0x43:
      case 0x44:
      case 0x45:
      case 0x46:
      case 0x47:
      case 0x48:
      case 0x49:
      case 0x4a:
      case 0x4b:
      case 0x4c:
      case 0x4d:
      case 0x4e:
      case 0x4f:
        stack.push(`${code - 0x30}`);
        break;

      case 0x93 /* DW_OP_piece */: {
        a = readU();
        const formatter = createPieceFormatter(a);
        stack.push(popValue(formatter));
        break;
      }

      case 0x9f /* DW_OP_stack_value */:
        stack.push(new Value(stack.pop()));
        break;

      case 0xf6 /* WASM ext (old, FIXME phase out) */:
      case 0xed /* WASM ext */:
        b = readU();
        a = readS();
        switch (b) {
          case 0:
            stack.push(`var${a}`);
            break;
          case 1:
            stack.push(`global${a}`);
            break;
          default:
            stack.push(`ti${b}(${a})`);
            break;
        }
        break;

      default:
        // Unknown encoding, baling out
        return null;
    }
  }
  // FIXME use real DWARF type information
  return popValue(typeFormatter);
}

function decodeExpr(expr) {
  if (expr.includes("//")) {
    expr = expr.slice(0, expr.indexOf("//")).trim();
  }
  const code = new Uint8Array(expr.length >> 1);
  for (let i = 0; i < code.length; i++) {
    code[i] = parseInt(expr.substr(i << 1, 2), 16);
  }
  const typeFormatter = Int32Formatter;
  return toJS(code, typeFormatter) || `dwarf("${expr}")`;
}

module.exports = {
  decodeExpr,
};
