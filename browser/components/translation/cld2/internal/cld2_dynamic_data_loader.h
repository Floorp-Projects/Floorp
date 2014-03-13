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

#ifndef CLD2_INTERNAL_CLD2_DYNAMIC_DATA_LOADER_H_
#define CLD2_INTERNAL_CLD2_DYNAMIC_DATA_LOADER_H_

#include "scoreonescriptspan.h"
#include "cld2_dynamic_data.h"

namespace CLD2DynamicDataLoader {

// Read a header from the specified file and return it.
// The header returned is dynamically allocated; you must 'delete' the array
// of TableHeaders as well as the returned FileHeader* when done.
CLD2DynamicData::FileHeader* loadHeader(const char* fileName);

// Load data directly into a ScoringTables structure using a private, read-only
// mmap and return the newly-allocated structure.
// The out-parameter "mmapAddressOut" is a pointer to a void*; the starting
// address of the mmap()'d block will be written here.
// The out-parameter "mmapLengthOut" is a pointer to an int; the length of the
// mmap()'d block will be written here.
// It is up to the caller to delete
CLD2::ScoringTables* loadDataFile(const char* fileName,
  void** mmapAddressOut, int* mmapLengthOut);

// Given pointers to the data from a previous invocation of loadDataFile,
// unloads the data safely - freeing and deleting any malloc'd/new'd objects.
// When this method returns, the mmap has been deleted, as have all the scoring
// tables; the pointers passed in are all zeroed, such that:
//   *scoringTables == NULL
//   *mmapAddress == NULL
//   mmapLength == NULL
// This is the only safe way to unload data that was previously loaded, as there
// is an unfortunate mixture of new and malloc involved in building the
// in-memory represtation of the data.
void unloadData(CLD2::ScoringTables** scoringTables,
  void** mmapAddress, int* mmapLength);

} // End namespace CLD2DynamicDataExtractor
#endif  // CLD2_INTERNAL_CLD2_DYNAMIC_DATA_EXTRACTOR_H_
