/***************************************************************************************************

  Zyan Disassembler Library (Zydis)

  Original Author : Florian Bernd

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

***************************************************************************************************/

#ifndef ZYDIS_INTERNAL_DECODERDATA_H
#define ZYDIS_INTERNAL_DECODERDATA_H

#include "zydis/Zycore/Defines.h"
#include "zydis/Zydis/DecoderTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

// MSVC does not like types other than (un-)signed int for bit-fields
#ifdef ZYAN_MSVC
#   pragma warning(push)
#   pragma warning(disable:4214)
#endif

#pragma pack(push, 1)

/* ---------------------------------------------------------------------------------------------- */
/* Decoder tree                                                                                   */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Defines the `ZydisDecoderTreeNodeType` data-type.
 */
typedef ZyanU8 ZydisDecoderTreeNodeType;

/**
 * @brief   Values that represent zydis decoder tree node types.
 */
enum ZydisDecoderTreeNodeTypes
{
    ZYDIS_NODETYPE_INVALID                  = 0x00,
    /**
     * @brief   Reference to an instruction-definition.
     */
    ZYDIS_NODETYPE_DEFINITION_MASK          = 0x80,
    /**
     * @brief   Reference to an XOP-map filter.
     */
    ZYDIS_NODETYPE_FILTER_XOP               = 0x01,
    /**
     * @brief   Reference to an VEX-map filter.
     */
    ZYDIS_NODETYPE_FILTER_VEX               = 0x02,
    /**
     * @brief   Reference to an EVEX/MVEX-map filter.
     */
    ZYDIS_NODETYPE_FILTER_EMVEX             = 0x03,
    /**
     * @brief   Reference to an opcode filter.
     */
    ZYDIS_NODETYPE_FILTER_OPCODE            = 0x04,
    /**
     * @brief   Reference to an instruction-mode filter.
     */
    ZYDIS_NODETYPE_FILTER_MODE              = 0x05,
    /**
     * @brief   Reference to an compacted instruction-mode filter.
     */
    ZYDIS_NODETYPE_FILTER_MODE_COMPACT      = 0x06,
    /**
     * @brief   Reference to a ModRM.mod filter.
     */
    ZYDIS_NODETYPE_FILTER_MODRM_MOD         = 0x07,
    /**
     * @brief   Reference to a compacted ModRM.mod filter.
     */
    ZYDIS_NODETYPE_FILTER_MODRM_MOD_COMPACT = 0x08,
    /**
     * @brief   Reference to a ModRM.reg filter.
     */
    ZYDIS_NODETYPE_FILTER_MODRM_REG         = 0x09,
    /**
     * @brief   Reference to a ModRM.rm filter.
     */
    ZYDIS_NODETYPE_FILTER_MODRM_RM          = 0x0A,
    /**
     * @brief   Reference to a PrefixGroup1 filter.
     */
    ZYDIS_NODETYPE_FILTER_PREFIX_GROUP1     = 0x0B,
    /**
     * @brief   Reference to a mandatory-prefix filter.
     */
    ZYDIS_NODETYPE_FILTER_MANDATORY_PREFIX  = 0x0C,
    /**
     * @brief   Reference to an operand-size filter.
     */
    ZYDIS_NODETYPE_FILTER_OPERAND_SIZE      = 0x0D,
    /**
     * @brief   Reference to an address-size filter.
     */
    ZYDIS_NODETYPE_FILTER_ADDRESS_SIZE      = 0x0E,
    /**
     * @brief   Reference to a vector-length filter.
     */
    ZYDIS_NODETYPE_FILTER_VECTOR_LENGTH     = 0x0F,
    /**
     * @brief   Reference to an REX/VEX/EVEX.W filter.
     */
    ZYDIS_NODETYPE_FILTER_REX_W             = 0x10,
    /**
     * @brief   Reference to an REX/VEX/EVEX.B filter.
     */
    ZYDIS_NODETYPE_FILTER_REX_B             = 0x11,
    /**
     * @brief   Reference to an EVEX.b filter.
     */
    ZYDIS_NODETYPE_FILTER_EVEX_B            = 0x12,
    /**
     * @brief   Reference to an MVEX.E filter.
     */
    ZYDIS_NODETYPE_FILTER_MVEX_E            = 0x13,
    /**
     * @brief   Reference to a AMD-mode filter.
     */
    ZYDIS_NODETYPE_FILTER_MODE_AMD          = 0x14,
    /**
     * @brief   Reference to a KNC-mode filter.
     */
    ZYDIS_NODETYPE_FILTER_MODE_KNC          = 0x15,
    /**
     * @brief   Reference to a MPX-mode filter.
     */
    ZYDIS_NODETYPE_FILTER_MODE_MPX          = 0x16,
    /**
     * @brief   Reference to a CET-mode filter.
     */
    ZYDIS_NODETYPE_FILTER_MODE_CET          = 0x17,
    /**
     * @brief   Reference to a LZCNT-mode filter.
     */
    ZYDIS_NODETYPE_FILTER_MODE_LZCNT        = 0x18,
    /**
     * @brief   Reference to a TZCNT-mode filter.
     */
    ZYDIS_NODETYPE_FILTER_MODE_TZCNT        = 0x19,
    /**
     * @brief   Reference to a WBNOINVD-mode filter.
     */
    ZYDIS_NODETYPE_FILTER_MODE_WBNOINVD     = 0x1A,
    /**
     * @brief   Reference to a CLDEMOTE-mode filter.
     */
    ZYDIS_NODETYPE_FILTER_MODE_CLDEMOTE     = 0x1B
};

/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Defines the `ZydisDecoderTreeNodeValue` data-type.
 */
typedef ZyanU16 ZydisDecoderTreeNodeValue;

/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Defines the `ZydisDecoderTreeNode` struct.
 */
typedef struct ZydisDecoderTreeNode_
{
    ZydisDecoderTreeNodeType type;
    ZydisDecoderTreeNodeValue value;
} ZydisDecoderTreeNode;

/* ---------------------------------------------------------------------------------------------- */

#pragma pack(pop)

#ifdef ZYAN_MSVC
#   pragma warning(pop)
#endif

/* ---------------------------------------------------------------------------------------------- */
/* Physical instruction encoding info                                                             */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Defines the `ZydisInstructionEncodingFlags` data-type.
 */
typedef ZyanU8 ZydisInstructionEncodingFlags;

/**
 * @brief   The instruction has an optional modrm byte.
 */
#define ZYDIS_INSTR_ENC_FLAG_HAS_MODRM      0x01

/**
 * @brief   The instruction has an optional displacement value.
 */
#define ZYDIS_INSTR_ENC_FLAG_HAS_DISP       0x02

/**
 * @brief   The instruction has an optional immediate value.
 */
#define ZYDIS_INSTR_ENC_FLAG_HAS_IMM0       0x04

/**
 * @brief   The instruction has a second optional immediate value.
 */
#define ZYDIS_INSTR_ENC_FLAG_HAS_IMM1       0x08

/**
 * @brief   The instruction ignores the value of `modrm.mod` and always assumes `modrm.mod == 3`
 *          ("reg, reg" - form).
 *
 *          Instructions with this flag can't have a SIB byte or a displacement value.
 */
#define ZYDIS_INSTR_ENC_FLAG_FORCE_REG_FORM 0x10

/**
 * @brief   Defines the `ZydisInstructionEncodingInfo` struct.
 */
typedef struct ZydisInstructionEncodingInfo_
{
    /**
     * @brief   Contains flags with information about the physical instruction-encoding.
     */
    ZydisInstructionEncodingFlags flags;
    /**
     * @brief   Displacement info.
     */
    struct
    {
        /**
         * @brief   The size of the displacement value.
         */
        ZyanU8 size[3];
    } disp;
    /**
     * @brief   Immediate info.
     */
    struct
    {
        /**
         * @brief   The size of the immediate value.
         */
        ZyanU8 size[3];
        /**
         * @brief   Signals, if the value is signed.
         */
        ZyanBool is_signed;
        /**
         * @brief   Signals, if the value is a relative offset.
         */
        ZyanBool is_relative;
    } imm[2];
} ZydisInstructionEncodingInfo;

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Functions                                                                                      */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Decoder tree                                                                                   */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Returns the root node of the instruction tree.
 *
 * @return  The root node of the instruction tree.
 */
ZYDIS_NO_EXPORT const ZydisDecoderTreeNode* ZydisDecoderTreeGetRootNode(void);

/**
 * @brief   Returns the child node of `parent` specified by `index`.
 *
 * @param   parent  The parent node.
 * @param   index   The index of the child node to retrieve.
 *
 * @return  The specified child node.
 */
ZYDIS_NO_EXPORT const ZydisDecoderTreeNode* ZydisDecoderTreeGetChildNode(
    const ZydisDecoderTreeNode* parent, ZyanU16 index);

/**
 * @brief   Returns information about optional instruction parts (like modrm, displacement or
 *          immediates) for the instruction that is linked to the given `node`.
 *
 * @param   node    The instruction definition node.
 * @param   info    A pointer to the `ZydisInstructionParts` struct.
 */
ZYDIS_NO_EXPORT void ZydisGetInstructionEncodingInfo(const ZydisDecoderTreeNode* node,
    const ZydisInstructionEncodingInfo** info);

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* ZYDIS_INTERNAL_DECODERDATA_H */
