# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from unicodedata import combining

UNICODE_LIMIT = 0x110000

UNICODE_COMBINING_CLASS_NOT_REORDERED = 0
UNICODE_COMBINING_CLASS_KANA_VOICING = 8
UNICODE_COMBINING_CLASS_VIRAMA = 9


# Keep this function in sync with IsCombiningDiacritic in nsUnicodeProperties.h.
def is_combining_diacritic(char):
    return combining(char) not in (
        UNICODE_COMBINING_CLASS_NOT_REORDERED,
        UNICODE_COMBINING_CLASS_KANA_VOICING,
        UNICODE_COMBINING_CLASS_VIRAMA,
        91,
        129,
        130,
        132,
    )


# See gfxFontUtils.h for the SharedBitSet that we're creating a const instance of here.
BLOCK_SIZE = 32
BLOCK_SIZE_BITS = BLOCK_SIZE * 8


def main(header):
    blockIndex = []
    blocks = []

    # Figure out the contents of each 256-char block, and see if it is unique
    # or can share an already-allocated block.
    block = [0] * BLOCK_SIZE
    byte = 0
    bit = 0x01
    for char in range(UNICODE_LIMIT):
        if is_combining_diacritic(chr(char)):
            block[byte] |= bit
        bit <<= 1
        if bit == 0x100:
            bit = 0x01
            byte += 1
        if byte == BLOCK_SIZE:
            found = False
            for b in range(len(blocks)):
                if block == blocks[b]:
                    blockIndex.append(b)
                    found = True
                    break
            if not found:
                blockIndex.append(len(blocks))
                blocks.append(block)
            byte = 0
            block = [0] * BLOCK_SIZE

    # Strip trailing empty blocks from the index.
    while blockIndex[len(blockIndex) - 1] == 0:
        del blockIndex[len(blockIndex) - 1]

    # Write the SharedBitSet as data in a C++ header file.
    header.write("/* !GENERATED DATA -- DO NOT EDIT! */\n")
    header.write("/* (see is_combining_diacritic.py) */\n")
    header.write("\n")
    header.write("#include \"gfxFontUtils.h\"\n")
    header.write("\n")

    header.write("typedef struct {\n")
    header.write("  uint16_t mBlockIndexCount;\n")
    header.write("  uint16_t mBlockCount;\n")
    header.write("  uint16_t mBlockIndex[" + str(len(blockIndex)) + "];\n")
    header.write("  uint8_t mBlockData[" + str(len(blocks) * BLOCK_SIZE) + "];\n")
    header.write("} CombiningDiacriticsBitset_t;\n")
    header.write("\n")

    header.write("static const CombiningDiacriticsBitset_t COMBINING_DIACRITICS_BITSET_DATA = {\n")
    header.write("  " + str(len(blockIndex)) + ",\n")
    header.write("  " + str(len(blocks)) + ",\n")
    header.write("  {\n")
    for b in blockIndex:
        header.write("    " + str(b) + ",\n")
    header.write("  },\n")
    header.write("  {\n")
    for b in blocks:
        header.write("    ")
        for i in b:
            header.write(str(i) + ",")
        header.write("\n")
    header.write("  },\n")
    header.write("};\n")
    header.write("\n")
    header.write("static const SharedBitSet* sCombiningDiacriticsSet =\n")
    header.write("    reinterpret_cast<const SharedBitSet*>(&COMBINING_DIACRITICS_BITSET_DATA);\n")
    header.write("\n")
