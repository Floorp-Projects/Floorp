// Copyright 2014 Google Inc. All Rights Reserved.                                                  
//                                                                                                  
// Licensed under the Apache License, Version 2.0 (the "License");                                  
// you may not use this file except in compliance with the License.                                 
// You may obtain a copy of the License at                                                          
//                                                                                                  
//     http://www.apache.org/licenses/LICENSE-2.0                                                   
//                                                                                                  
// Unless required by applicable law or agreed to in writing, software                              
// distributed under the License is distributed on an "AS IS" BASIS,                                
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.                         
// See the License for the specific language governing permissions and                              
// limitations under the License.

#ifndef CLD2_INTERNAL_CLD2_DYNAMIC_DATA_H_
#define CLD2_INTERNAL_CLD2_DYNAMIC_DATA_H_

#include "integral_types.h"
#include "cld2tablesummary.h"
#include "utf8statetable.h"
#include "scoreonescriptspan.h"

/*
  There are two primary parts to a CLD2 dynamic data file:
    1. A header, wherein trivial data, block lengths and block offsets are kept
    2. A data block, wherein the large binary blocks are kept

  By reading the header, an application can determine the offsets and lengths of
  all the data blocks for all tables. Offsets in the header are expressed
  relative to the first byte of the file, inclusive of the header itself; thus,
  any offset whose value is less than the length of the header is invalid.

  Any offset whose value is zero indicates a field that is null in the
  underlying CLD2 data; a real example of this is the fast_state field of the
  UTF8PropObj, which may be null.

  The size of the header can be precalculated by calling calculateHeaderSize(),
  which will indicate the exact size of the header for a data file that contains
  a given number of CLD2TableSummary objects.

  Notes on endianness:
  The data format is only suitable for little-endian machines. For big-endian
  systems, a tedious transformation would need to be made first to reverse the
  byte order of significant portions of the binary - not just the lengths, but
  also some of the underlying table data.

  Note on 32/64 bit:
  The data format is agnostic to 32/64 bit pointers. All the offsets within the 
  data blob itself are 32-bit values relative to the start of the file, and the
  file should certainly never be gigabytes in size!
  When the file is ultimately read by the loading code and mmap()'d, new
  pointers are generated at whatever size the system uses, initialized to the
  start of the mmap, and incremented by the 32-bit offset. This should be safe
  regardless of 32- or 64-bit architectures.

  --------------------------------------------------------------------
  FIELD
  --------------------------------------------------------------------
  DATA_FILE_MARKER (no null terminator)
  total file size (sanity check, uint32)
  --------------------------------------------------------------------
  UTF8PropObj: const uint32 state0
  UTF8PropObj: const uint32 state0_size
  UTF8PropObj: const uint32 total_size
  UTF8PropObj: const int max_expand
  UTF8PropObj: const int entry_shift (coerced to 32 bits)
  UTF8PropObj: const int bytes_per_entry (coerced to 32 bits)
  UTF8PropObj: const uint32 losub
  UTF8PropObj: const uint32 hiadd
  offset of UTF8PropObj: const uint8* state_table
  length of UTF8PropObj: const uint8* state_table
  offset of UTF8PropObj: const RemapEntry* remap_base (4-byte struct)
  length of UTF8PropObj: const RemapEntry* remap_base (4-byte struct)
  offset of UTF8PropObj: const uint8* remap_string
  length of UTF8PropObj: const uint8* remap_string
  offset of UTF8PropObj: const uint8* fast_state
  length of UTF8PropObj: const uint8* fast_state
  --------------------------------------------------------------------
  start of const short kAvgDeltaOctaScore[]
  length of const short kAvgDeltaOctaScore[]
  --------------------------------------------------------------------
  number of CLD2TableSummary objects encoded (n)
  [Table 1]: CLD2TableSummary: uint32 kCLDTableSizeOne
  [Table 1]: CLD2TableSummary: uint32 kCLDTableSize
  [Table 1]: CLD2TableSummary: uint32 kCLDTableKeyMask
  [Table 1]: CLD2TableSummary: uint32 kCLDTableBuildDate
  [Table 1]: offset of CLD2TableSummary: const IndirectProbBucket4* kCLDTable
  [Table 1]: length of CLD2TableSummary: const IndirectProbBucket4* kCLDTable
  [Table 1]: offset of CLD2TableSummary: const uint32* kCLDTableInd
  [Table 1]: length of CLD2TableSummary: const uint32* kCLDTableInd
  [Table 1]: offset of CLD2TableSummary: const char* kRecognizedLangScripts
  [Table 1]: length of CLD2TableSummary: const char* kRecognizedLangScripts + 1
  .
  .
  .
  [Table n]: CLD2TableSummary: uint32 kCLDTableSizeOne
  [Table n]: CLD2TableSummary: uint32 kCLDTableSize
  [Table n]: CLD2TableSummary: uint32 kCLDTableKeyMask
  [Table n]: CLD2TableSummary: uint32 kCLDTableBuildDate
  [Table n]: offset of CLD2TableSummary: const IndirectProbBucket4* kCLDTable
  [Table n]: length of CLD2TableSummary: const IndirectProbBucket4* kCLDTable
  [Table n]: offset of CLD2TableSummary: const uint32* kCLDTableInd
  [Table n]: length of CLD2TableSummary: const uint32* kCLDTableInd
  [Table n]: offset of CLD2TableSummary: const char* kRecognizedLangScripts
  [Table n]: length of CLD2TableSummary: const char* kRecognizedLangScripts + 1
  --------------------------------------------------------------------


  Immediately after the header fields comes the data block. The data block has
  the following content, in this order (note that padding is applied in order to
  keep lookups word-aligned):

  UTF8PropObj: const uint8* state_table
  UTF8PropObj: const RemapEntry* remap_base (4-byte struct)
  UTF8PropObj: const uint8* remap_string
  UTF8PropObj: const uint8* fast_state
  const short kAvgDeltaOctaScore[]
  [Table 1]: CLD2TableSummary: const IndirectProbBucket4* kCLDTable
  [Table 1]: CLD2TableSummary: const uint32* kCLDTableInd
  [Table 1]: CLD2TableSummary: const char* kRecognizedLangScripts (with null terminator)
  .
  .
  .
  [Table n]: CLD2TableSummary: const IndirectProbBucket4* kCLDTable
  [Table n]: CLD2TableSummary: const uint32* kCLDTableInd
  [Table n]: CLD2TableSummary: const char* kRecognizedLangScripts (with null terminator)


  It is STRONGLY recommended that the chunks within the data block be kept
  128-bit aligned for efficiency reasons, although the code will work without
  such alignment: the main lookup tables have randomly-accessed groups of four
  4-byte entries, and these must be 16-byte aligned to avoid the performance
  cost of multiple cache misses per group.
*/
namespace CLD2DynamicData {

static const char* DATA_FILE_MARKER = "cld2_data_file00";
static const int DATA_FILE_MARKER_LENGTH = 16; // Keep aligned to 128 bits

// Nicer version of memcmp that shows the offset at which bytes differ
bool mem_compare(const void* data1, const void* data2, const int length);

// Enable or disable debugging; 0 to disable, 1 to enable
void setDebug(int debug);

// Lower-level structure for individual tables. There are n table headers in
// a given file header.
typedef struct {
  CLD2::uint32 kCLDTableSizeOne;
  CLD2::uint32 kCLDTableSize;
  CLD2::uint32 kCLDTableKeyMask;
  CLD2::uint32 kCLDTableBuildDate;
  CLD2::uint32 startOf_kCLDTable;
  CLD2::uint32 lengthOf_kCLDTable;
  CLD2::uint32 startOf_kCLDTableInd;
  CLD2::uint32 lengthOf_kCLDTableInd;
  CLD2::uint32 startOf_kRecognizedLangScripts;
  CLD2::uint32 lengthOf_kRecognizedLangScripts;
} TableHeader;


// Top-level structure for a CLD2 Data File Header.
// Contains all the primitive fields for the header as well as an array of
// headers for the individual tables.
typedef struct {
  // Marker fields help recognize and verify the data file
  char sanityString[DATA_FILE_MARKER_LENGTH];
  CLD2::uint32 totalFileSizeBytes;

  // UTF8 primitives
  CLD2::uint32 utf8PropObj_state0;
  CLD2::uint32 utf8PropObj_state0_size;
  CLD2::uint32 utf8PropObj_total_size;
  CLD2::uint32 utf8PropObj_max_expand;
  CLD2::uint32 utf8PropObj_entry_shift;
  CLD2::uint32 utf8PropObj_bytes_per_entry;
  CLD2::uint32 utf8PropObj_losub;
  CLD2::uint32 utf8PropObj_hiadd;
  CLD2::uint32 startOf_utf8PropObj_state_table;
  CLD2::uint32 lengthOf_utf8PropObj_state_table;
  CLD2::uint32 startOf_utf8PropObj_remap_base;
  CLD2::uint32 lengthOf_utf8PropObj_remap_base;
  CLD2::uint32 startOf_utf8PropObj_remap_string;
  CLD2::uint32 lengthOf_utf8PropObj_remap_string;
  CLD2::uint32 startOf_utf8PropObj_fast_state;
  CLD2::uint32 lengthOf_utf8PropObj_fast_state;

  // Average delta-octa-score bits
  CLD2::uint32 startOf_kAvgDeltaOctaScore;
  CLD2::uint32 lengthOf_kAvgDeltaOctaScore;

  // Table bits
  CLD2::uint32 numTablesEncoded;
  TableHeader* tableHeaders;
} FileHeader;

// Calculate the exact size of a header that encodes the specified number of
// tables. This can be used to reserve space within the data file,
// calculate offsets, and so on.
CLD2::uint32 calculateHeaderSize(CLD2::uint32 numTables);

// Dump a given header to stdout as a human-readable string.
void dumpHeader(FileHeader* header);

// Verify that a given pair of scoring tables match precisely
// If there is a problem, returns an error message; otherwise, the empty string.
bool verify(const CLD2::ScoringTables* realData, const CLD2::ScoringTables* loadedData);

// Return true iff the program is running in little-endian mode.
bool isLittleEndian();

// Return true iff the core size assumptions are ok on this platform.
bool coreAssumptionsOk();

} // End namespace CLD2DynamicData
#endif  // CLD2_INTERNAL_CLD2_DYNAMIC_DATA_H_
