# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import re
from collections import namedtuple
from unicodedata import category, combining, normalize

UNICODE_LIMIT = 0x110000
UNICODE_BMP_LIMIT = 0x10000

UNICODE_COMBINING_CLASS_NOT_REORDERED = 0
UNICODE_COMBINING_CLASS_KANA_VOICING = 8
UNICODE_COMBINING_CLASS_VIRAMA = 9

BaseCharMapping = namedtuple("BaseCharMapping", ("char", "base_char"))
BaseCharMappingBlock = namedtuple("BaseCharMappingBlock", ("first", "last", "offset"))


def is_in_bmp(char):
    return ord(char) < UNICODE_BMP_LIMIT


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


# Keep this function in sync with IsMathSymbol in nsUnicodeProperties.h.
def is_math_symbol(char):
    return category(char) == "Sm"


def crosses_bmp(char, base_char):
    if is_in_bmp(char) != is_in_bmp(base_char):
        # Mappings that would change the length of a UTF-16 string are not
        # currently supported.
        return True
    if not is_in_bmp(char):
        # Currently there are no mappings we care about outside of the basic
        # multilingual plane. However, if such a mapping is added to Unicode in
        # the future, this warning will appear at build time.
        print(
            "Warning: Skipping "
            + "{:#06x}".format(ord(char))
            + " → "
            + "{:#06x}".format(ord(base_char))
        )
        print(
            "base_chars.py and nsUnicodeProperties.cpp need to be rewritten to "
            "use uint32_t instead of uint16_t."
        )
        return True
    return False


def main(header, fallback_table):
    mappings = {}

    # Glean mappings from decompositions

    for char in range(UNICODE_BMP_LIMIT):
        char = chr(char)
        if is_combining_diacritic(char) or is_math_symbol(char):
            continue
        decomposition = normalize("NFD", char)
        if len(decomposition) < 2:
            continue
        base_char = decomposition[0]
        if crosses_bmp(char, base_char):
            continue
        next_char = decomposition[1]
        if not is_combining_diacritic(next_char):
            # Hangul syllables decompose but do not actually have diacritics.
            # This also excludes decompositions with the Japanese marks U+3099
            # and U+309A (COMBINING KATAKANA-HIRAGANA [SEMI-]VOICED SOUND
            # MARK), which we should not ignore for searching (bug 1624244).
            continue
        mappings[char] = base_char

    # Add mappings from the ASCII fallback table

    for line in open(fallback_table, encoding="UTF-8"):
        m = re.match("^(.) → (.+?) ;", line)
        if not m:
            continue
        char = m.group(1)
        decomposition = m.group(2)
        if len(decomposition) >= 3:
            if decomposition.startswith("'") and decomposition.endswith("'"):
                decomposition = decomposition[1:-1]
        if len(decomposition) >= 2:
            if decomposition.startswith("\\"):
                decomposition = decomposition[1:]
        if len(decomposition) > 1:
            continue
        if crosses_bmp(char, decomposition):
            continue
        mappings[char] = decomposition

    # Organize mappings into contiguous blocks

    mappings = sorted([BaseCharMapping(ord(k), ord(v)) for k, v in mappings.items()])
    blocks = []
    i = 0
    while i < len(mappings) - 1:
        offset = i
        first = mappings[i].char & 0xFF
        while (
            i < len(mappings) - 1 and mappings[i].char >> 8 == mappings[i + 1].char >> 8
        ):
            while (
                i < len(mappings) - 1
                and mappings[i].char >> 8 == mappings[i + 1].char >> 8
                and mappings[i + 1].char - mappings[i].char > 1
            ):
                char = mappings[i].char + 1
                mappings.insert(i + 1, BaseCharMapping(char, char))
                i += 1
            i += 1
        last = mappings[i].char & 0xFF
        blocks.append(BaseCharMappingBlock(first, last, offset))
        i += 1

    indexes = []
    for i, block in enumerate(blocks):
        while len(indexes) < mappings[block.offset].char >> 8:
            indexes.append(255)
        indexes.append(i)
    while len(indexes) < 256:
        indexes.append(255)

    # Write the mappings to a C header file

    header.write("struct BaseCharMappingBlock {\n")
    header.write("  uint8_t mFirst;\n")
    header.write("  uint8_t mLast;\n")
    header.write("  uint16_t mMappingStartOffset;\n")
    header.write("};\n")
    header.write("\n")
    header.write("static const uint16_t BASE_CHAR_MAPPING_LIST[] = {\n")
    for char, base_char in mappings:
        header.write(
            "  /* {:#06x}".format(char) + " */ " + "{:#06x}".format(base_char) + ","
        )
        if char != base_char:
            header.write(" /* " + chr(char) + " → " + chr(base_char) + " */")
        header.write("\n")
    header.write("};\n")
    header.write("\n")
    header.write(
        "static const struct BaseCharMappingBlock BASE_CHAR_MAPPING_BLOCKS[] = {\n"
    )
    for block in blocks:
        header.write(
            "  {"
            + "{:#04x}".format(block.first)
            + ", "
            + "{:#04x}".format(block.last)
            + ", "
            + str(block.offset).rjust(4)
            + "}, // "
            + "{:#04x}".format(mappings[block.offset].char >> 8)
            + "xx\n"
        )
    header.write("};\n")
    header.write("\n")
    header.write("static const uint8_t BASE_CHAR_MAPPING_BLOCK_INDEX[256] = {\n")
    for i, index in enumerate(indexes):
        header.write(
            "  " + str(index).rjust(3) + ", // " + "{:#04x}".format(i) + "xx\n"
        )
    header.write("};\n")
