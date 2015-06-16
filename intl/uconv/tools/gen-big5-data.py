#!/usr/bin/python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Adapted from
# https://hg.mozilla.org/projects/htmlparser/file/3ac10f9e8612/generate-encoding-data.py

# indexes.json comes from 
# https://encoding.spec.whatwg.org/indexes.json
# i.e.
# https://github.com/whatwg/encoding/blob/a5215d07106e250dfef34908b99b3e4a576be2f6/indexes.json

import json

indexes = json.load(open("indexes.json", "r"))

def nullToZero(codePoint):
  if not codePoint:
    codePoint = 0
  return codePoint

index = []

for codePoint in indexes["big5"]:
  index.append(nullToZero(codePoint))  

# There are four major gaps consisting of more than 4 consecutive invalid pointers
gaps = []
consecutive = 0
consecutiveStart = 0
offset = 0
for codePoint in index:
  if codePoint == 0:
    if consecutive == 0:
      consecutiveStart = offset
    consecutive +=1
  else:
    if consecutive > 4:
      gaps.append((consecutiveStart, consecutiveStart + consecutive))
    consecutive = 0
  offset += 1

def invertRanges(ranges, cap):
  inverted = []
  invertStart = 0
  for (start, end) in ranges:
    if start != 0:
      inverted.append((invertStart, start))
    invertStart = end
  inverted.append((invertStart, cap))
  return inverted

cap = len(index)
ranges = invertRanges(gaps, cap)

# Now compute a compressed lookup table for astralness

gaps = []
consecutive = 0
consecutiveStart = 0
offset = 0
for codePoint in index:
  if codePoint <= 0xFFFF:
    if consecutive == 0:
      consecutiveStart = offset
    consecutive +=1
  else:
    if consecutive > 40:
      gaps.append((consecutiveStart, consecutiveStart + consecutive))
    consecutive = 0
  offset += 1

astralRanges = invertRanges(gaps, cap)


includeFile = open("../ucvtw/nsBIG5DecoderData.h", "w")
includeFile.write('''/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * THIS IS A GENERATED FILE. PLEASE DO NOT EDIT.
 * Instead, please regenerate using intl/uconv/tools/gen-big5-data.py
 */

static const char16_t kBig5LowBitsTable[] = {
''')

for (low, high) in ranges:
  for i in xrange(low, high):
    includeFile.write('  0x%04X,\n' % (index[i] & 0xFFFF))

includeFile.write('''};

static const uint32_t kBig5AstralnessTable[] = {
''')

# An array of bool is inefficient per
# http://stackoverflow.com/questions/4049156/1-bit-per-bool-in-array-c

bits = []
for (low, high) in astralRanges:
  for i in xrange(low, high):
    bits.append(1 if index[i] > 0xFFFF else 0)
# pad length to multiple of 32
for i in xrange(32 - (len(bits) % 32)):
  bits.append(0)
i = 0
while i < len(bits):
  accu = 0
  for j in xrange(32):
    accu |= bits[i + j] << j
  includeFile.write('  0x%08X,\n' % accu)
  i += 32

includeFile.write('''};

// static
char16_t
nsBIG5ToUnicode::LowBits(size_t aPointer)
{
''')

base = 0
for (low, high) in ranges:
  includeFile.write('''  if (aPointer < %d) {
    return 0;
  }
  if (aPointer < %d) {
    return kBig5LowBitsTable[%d + (aPointer - %d)];
  }
''' % (low, high, base, low))
  base += (high - low)

includeFile.write('''  return 0;
}

// static
bool
nsBIG5ToUnicode::IsAstral(size_t aPointer)
{
''')

base = 0
for (low, high) in astralRanges:
  if high - low == 1:
    includeFile.write('''  if (aPointer < %d) {
    return false;
  }
  if (aPointer == %d) {
    return true;
  }
''' % (low, low))
  else:
    includeFile.write('''  if (aPointer < %d) {
    return false;
  }
  if (aPointer < %d) {
    size_t index = %d + (aPointer - %d);
    return kBig5AstralnessTable[index >> 5] & (1 << (index & 0x1F));
  }
''' % (low, high, base, low))
  base += (high - low)

includeFile.write('''  return false;
}
''')
includeFile.close()
