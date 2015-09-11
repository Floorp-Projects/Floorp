#!/usr/bin/python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Adapted from
# https://hg.mozilla.org/projects/htmlparser/file/0d906fb1ab90/generate-encoding-data.py

# indexes.json comes from 
# https://encoding.spec.whatwg.org/indexes.json
# i.e.
# https://github.com/whatwg/encoding/blob/ce4e83d0df5b5efec0697fc76e66699737e033a3/indexes.json

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


classFile = open("../ucvtw/nsBIG5Data.cpp", "w")
classFile.write('''/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * THIS IS A GENERATED FILE. PLEASE DO NOT EDIT.
 * Instead, please regenerate using intl/uconv/tools/gen-big5-data.py
 */

#include "nsBIG5Data.h"

static const char16_t kBig5LowBitsTable[] = {
''')

for (low, high) in ranges:
  for i in xrange(low, high):
    classFile.write('  0x%04X,\n' % (index[i] & 0xFFFF))

classFile.write('''};

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
  classFile.write('  0x%08X,\n' % accu)
  i += 32

classFile.write('''};

// static
char16_t
nsBIG5Data::LowBits(size_t aPointer)
{
''')

base = 0
for (low, high) in ranges:
  classFile.write('''  if (aPointer < %d) {
    return 0;
  }
  if (aPointer < %d) {
    return kBig5LowBitsTable[%d + (aPointer - %d)];
  }
''' % (low, high, base, low))
  base += (high - low)

classFile.write('''  return 0;
}

// static
bool
nsBIG5Data::IsAstral(size_t aPointer)
{
''')

base = 0
for (low, high) in astralRanges:
  if high - low == 1:
    classFile.write('''  if (aPointer < %d) {
    return false;
  }
  if (aPointer == %d) {
    return true;
  }
''' % (low, low))
  else:
    classFile.write('''  if (aPointer < %d) {
    return false;
  }
  if (aPointer < %d) {
    size_t index = %d + (aPointer - %d);
    return kBig5AstralnessTable[index >> 5] & (1 << (index & 0x1F));
  }
''' % (low, high, base, low))
  base += (high - low)

classFile.write('''  return false;
}

//static
size_t
nsBIG5Data::FindPointer(char16_t aLowBits, bool aIsAstral)
{
  if (!aIsAstral) {
    switch (aLowBits) {
''')

hkscsBound = (0xA1 - 0x81) * 157

preferLast = [
  0x2550,
  0x255E,
  0x2561,
  0x256A,
  0x5341,
  0x5345,
]

for codePoint in preferLast:
  # Python lists don't have .rindex() :-(
  for i in xrange(len(index) - 1, -1, -1):
    candidate = index[i]
    if candidate == codePoint:
       classFile.write('''      case 0x%04X:
        return %d;
''' % (codePoint, i))
       break

classFile.write('''      default:
        break;
    }
  }''')

base = 0
start = 0
for (low, high) in ranges:
  if low <= hkscsBound and hkscsBound < high:
    # This is the first range we don't ignore and the
    # range that contains the first non-HKSCS pointer.
    # Avoid searching HKSCS.
    start = base + hkscsBound - low
    break
  base += (high - low)

classFile.write('''
  for (size_t i = %d; i < MOZ_ARRAY_LENGTH(kBig5LowBitsTable); ++i) {
    if (kBig5LowBitsTable[i] == aLowBits) {
      size_t pointer;
      ''' % start)

base = 0
prevLow = 0
prevHigh = 0
prevBase = 0
writing = False
for (low, high) in ranges:
  if writing:
    classFile.write('''if (i < %d) {
        pointer = i + %d;
      } else ''' % ((prevBase + prevHigh - prevLow), (prevLow - prevBase)))
  prevLow = low
  prevHigh = high
  prevBase = base
  if high > hkscsBound:
    writing = True
  base += (high - low)

classFile.write('''{
        pointer = i + %d;
      }''' % (prevLow - prevBase))

classFile.write('''
      if (aIsAstral == IsAstral(pointer)) {
        return pointer;
      }
    }
  }
  return 0;
}
''')
classFile.close()
