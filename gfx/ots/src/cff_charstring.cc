// Copyright (c) 2010-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A parser for the Type 2 Charstring Format.
// http://www.adobe.com/devnet/font/pdfs/5177.Type2.pdf

#include "cff_charstring.h"

#include <climits>
#include <cstdio>
#include <cstring>
#include <stack>
#include <string>
#include <utility>

#define TABLE_NAME "CFF"

namespace {

// Type 2 Charstring Implementation Limits. See Appendix. B in Adobe Technical
// Note #5177.
const int32_t kMaxSubrsCount = 65536;
const size_t kMaxCharStringLength = 65535;
const size_t kMaxNumberOfStemHints = 96;
const size_t kMaxSubrNesting = 10;

// |dummy_result| should be a huge positive integer so callsubr and callgsubr
// will fail with the dummy value.
const int32_t dummy_result = INT_MAX;

bool ExecuteCharString(ots::OpenTypeCFF& cff,
                       size_t call_depth,
                       const ots::CFFIndex& global_subrs_index,
                       const ots::CFFIndex& local_subrs_index,
                       ots::Buffer *cff_table,
                       ots::Buffer *char_string,
                       std::stack<int32_t> *argument_stack,
                       bool *out_found_endchar,
                       bool *out_found_width,
                       size_t *in_out_num_stems,
                       bool cff2);

bool ArgumentStackOverflows(std::stack<int32_t> *argument_stack, bool cff2) {
  if ((cff2 && argument_stack->size() > ots::kMaxCFF2ArgumentStack) ||
      (!cff2 && argument_stack->size() > ots::kMaxCFF1ArgumentStack)) {
    return true;
  }
  return false;
}

#ifdef DUMP_T2CHARSTRING
// Converts |op| to a string and returns it.
const char *CharStringOperatorToString(ots::CharStringOperator op) {
  switch (op) {
  case ots::kHStem:
    return "hstem";
  case ots::kVStem:
    return "vstem";
  case ots::kVMoveTo:
    return "vmoveto";
  case ots::kRLineTo:
    return "rlineto";
  case ots::kHLineTo:
    return "hlineto";
  case ots::kVLineTo:
    return "vlineto";
  case ots::kRRCurveTo:
    return "rrcurveto";
  case ots::kCallSubr:
    return "callsubr";
  case ots::kReturn:
    return "return";
  case ots::kEndChar:
    return "endchar";
  case ots::kVSIndex:
    return "vsindex";
  case ots::kBlend:
    return "blend";
  case ots::kHStemHm:
    return "hstemhm";
  case ots::kHintMask:
    return "hintmask";
  case ots::kCntrMask:
    return "cntrmask";
  case ots::kRMoveTo:
    return "rmoveto";
  case ots::kHMoveTo:
    return "hmoveto";
  case ots::kVStemHm:
    return "vstemhm";
  case ots::kRCurveLine:
    return "rcurveline";
  case ots::kRLineCurve:
    return "rlinecurve";
  case ots::kVVCurveTo:
    return "VVCurveTo";
  case ots::kHHCurveTo:
    return "hhcurveto";
  case ots::kCallGSubr:
    return "callgsubr";
  case ots::kVHCurveTo:
    return "vhcurveto";
  case ots::kHVCurveTo:
    return "HVCurveTo";
  case ots::kDotSection:
    return "dotsection";
  case ots::kAnd:
    return "and";
  case ots::kOr:
    return "or";
  case ots::kNot:
    return "not";
  case ots::kAbs:
    return "abs";
  case ots::kAdd:
    return "add";
  case ots::kSub:
    return "sub";
  case ots::kDiv:
    return "div";
  case ots::kNeg:
    return "neg";
  case ots::kEq:
    return "eq";
  case ots::kDrop:
    return "drop";
  case ots::kPut:
    return "put";
  case ots::kGet:
    return "get";
  case ots::kIfElse:
    return "ifelse";
  case ots::kRandom:
    return "random";
  case ots::kMul:
    return "mul";
  case ots::kSqrt:
    return "sqrt";
  case ots::kDup:
    return "dup";
  case ots::kExch:
    return "exch";
  case ots::kIndex:
    return "index";
  case ots::kRoll:
    return "roll";
  case ots::kHFlex:
    return "hflex";
  case ots::kFlex:
    return "flex";
  case ots::kHFlex1:
    return "hflex1";
  case ots::kFlex1:
    return "flex1";
  }

  return "UNKNOWN";
}
#endif

// Read one or more bytes from the |char_string| buffer and stores the number
// read on |out_number|. If the number read is an operator (ex 'vstem'), sets
// true on |out_is_operator|. Returns true if the function read a number.
bool ReadNextNumberFromCharString(ots::Buffer *char_string,
                                  int32_t *out_number,
                                  bool *out_is_operator) {
  uint8_t v = 0;
  if (!char_string->ReadU8(&v)) {
    return OTS_FAILURE();
  }
  *out_is_operator = false;

  // The conversion algorithm is described in Adobe Technical Note #5177, page
  // 13, Table 1.
  if (v <= 11) {
    *out_number = v;
    *out_is_operator = true;
  } else if (v == 12) {
    uint16_t result = (v << 8);
    if (!char_string->ReadU8(&v)) {
      return OTS_FAILURE();
    }
    result += v;
    *out_number = result;
    *out_is_operator = true;
  } else if (v <= 27) {
    // Special handling for v==19 and v==20 are implemented in
    // ExecuteCharStringOperator().
    *out_number = v;
    *out_is_operator = true;
  } else if (v == 28) {
    if (!char_string->ReadU8(&v)) {
      return OTS_FAILURE();
    }
    uint16_t result = (v << 8);
    if (!char_string->ReadU8(&v)) {
      return OTS_FAILURE();
    }
    result += v;
    *out_number = result;
  } else if (v <= 31) {
    *out_number = v;
    *out_is_operator = true;
  } else if (v <= 246) {
    *out_number = static_cast<int32_t>(v) - 139;
  } else if (v <= 250) {
    uint8_t w = 0;
    if (!char_string->ReadU8(&w)) {
      return OTS_FAILURE();
    }
    *out_number = ((static_cast<int32_t>(v) - 247) * 256) +
        static_cast<int32_t>(w) + 108;
  } else if (v <= 254) {
    uint8_t w = 0;
    if (!char_string->ReadU8(&w)) {
      return OTS_FAILURE();
    }
    *out_number = -((static_cast<int32_t>(v) - 251) * 256) -
        static_cast<int32_t>(w) - 108;
  } else if (v == 255) {
    // TODO(yusukes): We should not skip the 4 bytes. Note that when v is 255,
    // we should treat the following 4-bytes as a 16.16 fixed-point number
    // rather than 32bit signed int.
    if (!char_string->Skip(4)) {
      return OTS_FAILURE();
    }
    *out_number = dummy_result;
  } else {
    return OTS_FAILURE();
  }

  return true;
}

bool ValidCFF2Operator(int32_t op) {
  switch (op) {
  case ots::kReturn:
  case ots::kEndChar:
  case ots::kAbs:
  case ots::kAdd:
  case ots::kSub:
  case ots::kDiv:
  case ots::kNeg:
  case ots::kRandom:
  case ots::kMul:
  case ots::kSqrt:
  case ots::kDrop:
  case ots::kExch:
  case ots::kIndex:
  case ots::kRoll:
  case ots::kDup:
  case ots::kPut:
  case ots::kGet:
  case ots::kDotSection:
  case ots::kAnd:
  case ots::kOr:
  case ots::kNot:
  case ots::kEq:
  case ots::kIfElse:
    return false;
  }

  return true;
}

// Executes |op| and updates |argument_stack|. Returns true if the execution
// succeeds. If the |op| is kCallSubr or kCallGSubr, the function recursively
// calls ExecuteCharString() function. The arguments other than |op| and
// |argument_stack| are passed for that reason.
bool ExecuteCharStringOperator(ots::OpenTypeCFF& cff,
                               int32_t op,
                               size_t call_depth,
                               const ots::CFFIndex& global_subrs_index,
                               const ots::CFFIndex& local_subrs_index,
                               ots::Buffer *cff_table,
                               ots::Buffer *char_string,
                               std::stack<int32_t> *argument_stack,
                               bool *out_found_endchar,
                               bool *in_out_found_width,
                               size_t *in_out_num_stems,
                               bool *in_out_have_blend,
                               bool *in_out_have_visindex,
                               int32_t *in_out_vsindex,
                               bool cff2) {
  ots::Font* font = cff.GetFont();
  const size_t stack_size = argument_stack->size();

  if (cff2 && !ValidCFF2Operator(op)) {
    return OTS_FAILURE();
  }

  switch (op) {
  case ots::kCallSubr:
  case ots::kCallGSubr: {
    const ots::CFFIndex& subrs_index =
        (op == ots::kCallSubr ? local_subrs_index : global_subrs_index);

    if (stack_size < 1) {
      return OTS_FAILURE();
    }
    int32_t subr_number = argument_stack->top();
    argument_stack->pop();
    if (subr_number == dummy_result) {
      // For safety, we allow subr calls only with immediate subr numbers for
      // now. For example, we allow "123 callgsubr", but does not allow "100 12
      // add callgsubr". Please note that arithmetic and conditional operators
      // always push the |dummy_result| in this implementation.
      return OTS_FAILURE();
    }

    // See Adobe Technical Note #5176 (CFF), "16. Local/GlobalSubrs INDEXes."
    int32_t bias = 32768;
    if (subrs_index.count < 1240) {
      bias = 107;
    } else if (subrs_index.count < 33900) {
      bias = 1131;
    }
    subr_number += bias;

    // Sanity checks of |subr_number|.
    if (subr_number < 0) {
      return OTS_FAILURE();
    }
    if (subr_number >= kMaxSubrsCount) {
      return OTS_FAILURE();
    }
    if (subrs_index.offsets.size() <= static_cast<size_t>(subr_number + 1)) {
      return OTS_FAILURE();  // The number is out-of-bounds.
    }

    // Prepare ots::Buffer where we're going to jump.
    const size_t length =
      subrs_index.offsets[subr_number + 1] - subrs_index.offsets[subr_number];
    if (length > kMaxCharStringLength) {
      return OTS_FAILURE();
    }
    const size_t offset = subrs_index.offsets[subr_number];
    cff_table->set_offset(offset);
    if (!cff_table->Skip(length)) {
      return OTS_FAILURE();
    }
    ots::Buffer char_string_to_jump(cff_table->buffer() + offset, length);

    return ExecuteCharString(cff,
                             call_depth + 1,
                             global_subrs_index,
                             local_subrs_index,
                             cff_table,
                             &char_string_to_jump,
                             argument_stack,
                             out_found_endchar,
                             in_out_found_width,
                             in_out_num_stems,
                             cff2);
  }

  case ots::kReturn:
    return true;

  case ots::kEndChar:
    *out_found_endchar = true;
    *in_out_found_width = true;  // just in case.
    return true;

  case ots::kVSIndex: {
    if (!cff2) {
      return OTS_FAILURE();
    }
    if (stack_size != 1) {
      return OTS_FAILURE();
    }
    if (*in_out_have_blend || *in_out_have_visindex) {
      return OTS_FAILURE();
    }
    if (argument_stack->top() >= cff.region_index_count.size()) {
      return OTS_FAILURE();
    }
    *in_out_have_visindex = true;
    *in_out_vsindex = argument_stack->top();
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;
  }

  case ots::kBlend: {
    if (!cff2) {
      return OTS_FAILURE();
    }
    if (stack_size < 1) {
      return OTS_FAILURE();
    }
    if (*in_out_vsindex >= cff.region_index_count.size()) {
      return OTS_FAILURE();
    }
    uint16_t k = cff.region_index_count.at(*in_out_vsindex);
    uint16_t n = argument_stack->top();
    if (stack_size < n * (k + 1) + 1) {
      return OTS_FAILURE();
    }

    // Keep the 1st n operands on the stack for the next operator to use and
    // pop the rest. There can be multiple consecutive blend operator, so this
    // makes sure the operands of all of them are kept on the stack.
    while (argument_stack->size() > stack_size - ((n * k) + 1))
      argument_stack->pop();
    *in_out_have_blend = true;
    return true;
  }

  case ots::kHStem:
  case ots::kVStem:
  case ots::kHStemHm:
  case ots::kVStemHm: {
    bool successful = false;
    if (stack_size < 2) {
      return OTS_FAILURE();
    }
    if ((stack_size % 2) == 0) {
      successful = true;
    } else if ((!(*in_out_found_width)) && (((stack_size - 1) % 2) == 0)) {
      // The -1 is for "width" argument. For details, see Adobe Technical Note
      // #5177, page 16, note 4.
      successful = true;
    }
    (*in_out_num_stems) += (stack_size / 2);
    if ((*in_out_num_stems) > kMaxNumberOfStemHints) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    *in_out_found_width = true;  // always set true since "w" might be 0 byte.
    return successful ? true : OTS_FAILURE();
  }

  case ots::kRMoveTo: {
    bool successful = false;
    if (stack_size == 2) {
      successful = true;
    } else if ((!(*in_out_found_width)) && (stack_size - 1 == 2)) {
      successful = true;
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    *in_out_found_width = true;
    return successful ? true : OTS_FAILURE();
  }

  case ots::kVMoveTo:
  case ots::kHMoveTo: {
    bool successful = false;
    if (stack_size == 1) {
      successful = true;
    } else if ((!(*in_out_found_width)) && (stack_size - 1 == 1)) {
      successful = true;
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    *in_out_found_width = true;
    return successful ? true : OTS_FAILURE();
  }

  case ots::kHintMask:
  case ots::kCntrMask: {
    bool successful = false;
    if (stack_size == 0) {
      successful = true;
    } else if ((!(*in_out_found_width)) && (stack_size == 1)) {
      // A number for "width" is found.
      successful = true;
    } else if ((!(*in_out_found_width)) ||  // in this case, any sizes are ok.
               ((stack_size % 2) == 0)) {
      // The numbers are vstem definition.
      // See Adobe Technical Note #5177, page 24, hintmask.
      (*in_out_num_stems) += (stack_size / 2);
      if ((*in_out_num_stems) > kMaxNumberOfStemHints) {
        return OTS_FAILURE();
      }
      successful = true;
    }
    if (!successful) {
       return OTS_FAILURE();
    }

    if ((*in_out_num_stems) == 0) {
      return OTS_FAILURE();
    }
    const size_t mask_bytes = (*in_out_num_stems + 7) / 8;
    if (!char_string->Skip(mask_bytes)) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    *in_out_found_width = true;
    return true;
  }

  case ots::kRLineTo:
    if (!(*in_out_found_width)) {
      // The first stack-clearing operator should be one of hstem, hstemhm,
      // vstem, vstemhm, cntrmask, hintmask, hmoveto, vmoveto, rmoveto, or
      // endchar. For details, see Adobe Technical Note #5177, page 16, note 4.
      return OTS_FAILURE();
    }
    if (stack_size < 2) {
      return OTS_FAILURE();
    }
    if ((stack_size % 2) != 0) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;

  case ots::kHLineTo:
  case ots::kVLineTo:
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size < 1) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;

  case ots::kRRCurveTo:
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size < 6) {
      return OTS_FAILURE();
    }
    if ((stack_size % 6) != 0) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;

  case ots::kRCurveLine:
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size < 8) {
      return OTS_FAILURE();
    }
    if (((stack_size - 2) % 6) != 0) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;

  case ots::kRLineCurve:
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size < 8) {
      return OTS_FAILURE();
    }
    if (((stack_size - 6) % 2) != 0) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;

  case ots::kVVCurveTo:
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size < 4) {
      return OTS_FAILURE();
    }
    if (((stack_size % 4) != 0) &&
        (((stack_size - 1) % 4) != 0)) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;

  case ots::kHHCurveTo: {
    bool successful = false;
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size < 4) {
      return OTS_FAILURE();
    }
    if ((stack_size % 4) == 0) {
      // {dxa dxb dyb dxc}+
      successful = true;
    } else if (((stack_size - 1) % 4) == 0) {
      // dy1? {dxa dxb dyb dxc}+
      successful = true;
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return successful ? true : OTS_FAILURE();
  }

  case ots::kVHCurveTo:
  case ots::kHVCurveTo: {
    bool successful = false;
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size < 4) {
      return OTS_FAILURE();
    }
    if (((stack_size - 4) % 8) == 0) {
      // dx1 dx2 dy2 dy3 {dya dxb dyb dxc dxd dxe dye dyf}*
      successful = true;
    } else if ((stack_size >= 5) &&
               ((stack_size - 5) % 8) == 0) {
      // dx1 dx2 dy2 dy3 {dya dxb dyb dxc dxd dxe dye dyf}* dxf
      successful = true;
    } else if ((stack_size >= 8) &&
               ((stack_size - 8) % 8) == 0) {
      // {dxa dxb dyb dyc dyd dxe dye dxf}+
      successful = true;
    } else if ((stack_size >= 9) &&
               ((stack_size - 9) % 8) == 0) {
      // {dxa dxb dyb dyc dyd dxe dye dxf}+ dyf?
      successful = true;
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return successful ? true : OTS_FAILURE();
  }

  case ots::kDotSection:
    // Deprecated operator but harmless, we probably should drop it some how.
    if (stack_size != 0) {
      return OTS_FAILURE();
    }
    return true;

  case ots::kAnd:
  case ots::kOr:
  case ots::kEq:
  case ots::kAdd:
  case ots::kSub:
    if (stack_size < 2) {
      return OTS_FAILURE();
    }
    argument_stack->pop();
    argument_stack->pop();
    argument_stack->push(dummy_result);
    // TODO(yusukes): Implement this. We should push a real value for all
    // arithmetic and conditional operations.
    return true;

  case ots::kNot:
  case ots::kAbs:
  case ots::kNeg:
    if (stack_size < 1) {
      return OTS_FAILURE();
    }
    argument_stack->pop();
    argument_stack->push(dummy_result);
    // TODO(yusukes): Implement this. We should push a real value for all
    // arithmetic and conditional operations.
    return true;

  case ots::kDiv:
    // TODO(yusukes): Should detect div-by-zero errors.
    if (stack_size < 2) {
      return OTS_FAILURE();
    }
    argument_stack->pop();
    argument_stack->pop();
    argument_stack->push(dummy_result);
    // TODO(yusukes): Implement this. We should push a real value for all
    // arithmetic and conditional operations.
    return true;

  case ots::kDrop:
    if (stack_size < 1) {
      return OTS_FAILURE();
    }
    argument_stack->pop();
    return true;

  case ots::kPut:
  case ots::kGet:
  case ots::kIndex:
    // For now, just call OTS_FAILURE since there is no way to check whether the
    // index argument, |i|, is out-of-bounds or not. Fortunately, no OpenType
    // fonts I have (except malicious ones!) use the operators.
    // TODO(yusukes): Implement them in a secure way.
    return OTS_FAILURE();

  case ots::kRoll:
    // Likewise, just call OTS_FAILURE for kRoll since there is no way to check
    // whether |N| is smaller than the current stack depth or not.
    // TODO(yusukes): Implement them in a secure way.
    return OTS_FAILURE();

  case ots::kRandom:
    // For now, we don't handle the 'random' operator since the operator makes
    // it hard to analyze hinting code statically.
    return OTS_FAILURE();

  case ots::kIfElse:
    if (stack_size < 4) {
      return OTS_FAILURE();
    }
    argument_stack->pop();
    argument_stack->pop();
    argument_stack->pop();
    argument_stack->pop();
    argument_stack->push(dummy_result);
    // TODO(yusukes): Implement this. We should push a real value for all
    // arithmetic and conditional operations.
    return true;

  case ots::kMul:
    // TODO(yusukes): Should detect overflows.
    if (stack_size < 2) {
      return OTS_FAILURE();
    }
    argument_stack->pop();
    argument_stack->pop();
    argument_stack->push(dummy_result);
    // TODO(yusukes): Implement this. We should push a real value for all
    // arithmetic and conditional operations.
    return true;

  case ots::kSqrt:
    // TODO(yusukes): Should check if the argument is negative.
    if (stack_size < 1) {
      return OTS_FAILURE();
    }
    argument_stack->pop();
    argument_stack->push(dummy_result);
    // TODO(yusukes): Implement this. We should push a real value for all
    // arithmetic and conditional operations.
    return true;

  case ots::kDup:
    if (stack_size < 1) {
      return OTS_FAILURE();
    }
    argument_stack->pop();
    argument_stack->push(dummy_result);
    argument_stack->push(dummy_result);
    if (ArgumentStackOverflows(argument_stack, cff2)) {
      return OTS_FAILURE();
    }
    // TODO(yusukes): Implement this. We should push a real value for all
    // arithmetic and conditional operations.
    return true;

  case ots::kExch:
    if (stack_size < 2) {
      return OTS_FAILURE();
    }
    argument_stack->pop();
    argument_stack->pop();
    argument_stack->push(dummy_result);
    argument_stack->push(dummy_result);
    // TODO(yusukes): Implement this. We should push a real value for all
    // arithmetic and conditional operations.
    return true;

  case ots::kHFlex:
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size != 7) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;

  case ots::kFlex:
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size != 13) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;

  case ots::kHFlex1:
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size != 9) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;

  case ots::kFlex1:
    if (!(*in_out_found_width)) {
      return OTS_FAILURE();
    }
    if (stack_size != 11) {
      return OTS_FAILURE();
    }
    while (!argument_stack->empty())
      argument_stack->pop();
    return true;
  }

  return OTS_FAILURE_MSG("Undefined operator: %d (0x%x)", op, op);
}

// Executes |char_string| and updates |argument_stack|.
//
// call_depth: The current call depth. Initial value is zero.
// global_subrs_index: Global subroutines.
// local_subrs_index: Local subroutines for the current glyph.
// cff_table: A whole CFF table which contains all global and local subroutines.
// char_string: A charstring we'll execute. |char_string| can be a main routine
//              in CharString INDEX, or a subroutine in GlobalSubr/LocalSubr.
// argument_stack: The stack which an operator in |char_string| operates.
// out_found_endchar: true is set if |char_string| contains 'endchar'.
// in_out_found_width: true is set if |char_string| contains 'width' byte (which
//                     is 0 or 1 byte.)
// in_out_num_stems: total number of hstems and vstems processed so far.
bool ExecuteCharString(ots::OpenTypeCFF& cff,
                       size_t call_depth,
                       const ots::CFFIndex& global_subrs_index,
                       const ots::CFFIndex& local_subrs_index,
                       ots::Buffer *cff_table,
                       ots::Buffer *char_string,
                       std::stack<int32_t> *argument_stack,
                       bool *out_found_endchar,
                       bool *in_out_found_width,
                       size_t *in_out_num_stems,
                       bool cff2) {
  if (call_depth > kMaxSubrNesting) {
    return OTS_FAILURE();
  }
  *out_found_endchar = false;

  bool in_out_have_blend = false, in_out_have_visindex = false;
  int32_t in_out_vsindex = 0;
  const size_t length = char_string->length();
  while (char_string->offset() < length) {
    int32_t operator_or_operand = 0;
    bool is_operator = false;
    if (!ReadNextNumberFromCharString(char_string,
                                           &operator_or_operand,
                                           &is_operator)) {
      return OTS_FAILURE();
    }

#ifdef DUMP_T2CHARSTRING
    /*
      You can dump all operators and operands (except mask bytes for hintmask
      and cntrmask) by the following code:
    */

      if (!is_operator) {
        std::fprintf(stderr, "%d ", operator_or_operand);
      } else {
        std::fprintf(stderr, "%s\n",
           CharStringOperatorToString(
               ots::CharStringOperator(operator_or_operand))
           );
      }
#endif

    if (!is_operator) {
      argument_stack->push(operator_or_operand);
      if (ArgumentStackOverflows(argument_stack, cff2)) {
        return OTS_FAILURE();
      }
      continue;
    }

    // An operator is found. Execute it.
    if (!ExecuteCharStringOperator(cff,
                                   operator_or_operand,
                                   call_depth,
                                   global_subrs_index,
                                   local_subrs_index,
                                   cff_table,
                                   char_string,
                                   argument_stack,
                                   out_found_endchar,
                                   in_out_found_width,
                                   in_out_num_stems,
                                   &in_out_have_blend,
                                   &in_out_have_visindex,
                                   &in_out_vsindex,
                                   cff2)) {
      return OTS_FAILURE();
    }
    if (*out_found_endchar) {
      return true;
    }
    if (operator_or_operand == ots::kReturn) {
      return true;
    }
  }

  // No endchar operator is found.
  if (cff2)
    return true;
  return OTS_FAILURE();
}

// Selects a set of subroutings for |glyph_index| from |cff| and sets it on
// |out_local_subrs_to_use|. Returns true on success.
bool SelectLocalSubr(const ots::OpenTypeCFF& cff,
                     uint16_t glyph_index,  // 0-origin
                     const ots::CFFIndex **out_local_subrs_to_use) {
  bool cff2 = (cff.major == 2);
  *out_local_subrs_to_use = NULL;

  // First, find local subrs from |local_subrs_per_font|.
  if ((cff.fd_select.size() > 0) &&
      (!cff.local_subrs_per_font.empty())) {
    // Look up FDArray index for the glyph.
    const auto& iter = cff.fd_select.find(glyph_index);
    if (iter == cff.fd_select.end()) {
      return OTS_FAILURE();
    }
    const auto fd_index = iter->second;
    if (fd_index >= cff.local_subrs_per_font.size()) {
      return OTS_FAILURE();
    }
    *out_local_subrs_to_use = cff.local_subrs_per_font.at(fd_index);
  } else if (cff.local_subrs) {
    // Second, try to use |local_subrs|. Most Latin fonts don't have FDSelect
    // entries. If The font has a local subrs index associated with the Top
    // DICT (not FDArrays), use it.
    *out_local_subrs_to_use = cff.local_subrs;
  } else if (cff2 && cff.local_subrs_per_font.size() == 1) {
    *out_local_subrs_to_use = cff.local_subrs_per_font.at(0);
  } else {
    // Just return NULL.
    *out_local_subrs_to_use = NULL;
  }

  return true;
}

}  // namespace

namespace ots {

bool ValidateCFFCharStrings(
    ots::OpenTypeCFF& cff,
    const CFFIndex& global_subrs_index,
    Buffer* cff_table) {
  const CFFIndex& char_strings_index = *(cff.charstrings_index);
  if (char_strings_index.offsets.size() == 0) {
    return OTS_FAILURE();  // no charstring.
  }

  bool cff2 = (cff.major == 2);
  // For each glyph, validate the corresponding charstring.
  for (unsigned i = 1; i < char_strings_index.offsets.size(); ++i) {
    // Prepare a Buffer object, |char_string|, which contains the charstring
    // for the |i|-th glyph.
    const size_t length =
      char_strings_index.offsets[i] - char_strings_index.offsets[i - 1];
    if (length > kMaxCharStringLength) {
      return OTS_FAILURE();
    }
    const size_t offset = char_strings_index.offsets[i - 1];
    cff_table->set_offset(offset);
    if (!cff_table->Skip(length)) {
      return OTS_FAILURE();
    }
    Buffer char_string(cff_table->buffer() + offset, length);

    // Get a local subrs for the glyph.
    const unsigned glyph_index = i - 1;  // index in the map is 0-origin.
    const CFFIndex *local_subrs_to_use = NULL;
    if (!SelectLocalSubr(cff,
                         glyph_index,
                         &local_subrs_to_use)) {
      return OTS_FAILURE();
    }
    // If |local_subrs_to_use| is still NULL, use an empty one.
    CFFIndex default_empty_subrs;
    if (!local_subrs_to_use){
      local_subrs_to_use = &default_empty_subrs;
    }

    // Check a charstring for the |i|-th glyph.
    std::stack<int32_t> argument_stack;
    bool found_endchar = false;
    // CFF2 CharString has no value for width, so we start with true here to
    // error out if width is found.
    bool found_width = cff2;
    size_t num_stems = 0;
    if (!ExecuteCharString(cff,
                           0 /* initial call_depth is zero */,
                           global_subrs_index, *local_subrs_to_use,
                           cff_table, &char_string, &argument_stack,
                           &found_endchar, &found_width, &num_stems,
                           cff2)) {
      return OTS_FAILURE();
    }
    if (!cff2 && !found_endchar) {
      return OTS_FAILURE();
    }
  }
  return true;
}

}  // namespace ots

#undef TABLE_NAME
