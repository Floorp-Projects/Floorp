// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <windows.h>
#include <ImageHlp.h>
#include <psapi.h>

#include "base/image_util.h"
#include "base/process_util.h"

// imagehlp.dll appears to ship in all win versions after Win95.
// nsylvain verified it is present in win2k.
// Using #pragma comment for dependency, instead of LoadLibrary/GetProcAddress.
#pragma comment(lib, "imagehlp.lib")

namespace image_util {

// ImageMetrics
ImageMetrics::ImageMetrics(HANDLE process) : process_(process) {
}

ImageMetrics::~ImageMetrics() {
}

bool ImageMetrics::GetDllImageSectionData(const std::string& loaded_dll_name,
                                          ImageSectionsData* section_sizes) {
  // Get a handle to the loaded DLL
  HMODULE the_dll = GetModuleHandleA(loaded_dll_name.c_str());
  char full_filename[MAX_PATH];
  // Get image path
  if (GetModuleFileNameExA(process_, the_dll, full_filename, MAX_PATH)) {
    return GetImageSectionSizes(full_filename, section_sizes);
  }
  return false;
}

bool ImageMetrics::GetProcessImageSectionData(ImageSectionsData*
                                              section_sizes) {
  char exe_path[MAX_PATH];
  // Get image path
  if (GetModuleFileNameExA(process_, NULL, exe_path, MAX_PATH)) {
    return GetImageSectionSizes(exe_path, section_sizes);
  }
  return false;
}

// private
bool ImageMetrics::GetImageSectionSizes(char* qualified_path,
                                        ImageSectionsData* result) {
  LOADED_IMAGE li;
  // TODO (timsteele): There is no unicode version for MapAndLoad, hence
  // why ansi functions are used in this class. Should we try and rewrite
  // this call ourselves to be safe?
  if (MapAndLoad(qualified_path, 0, &li, FALSE, TRUE)) {
    IMAGE_SECTION_HEADER* section_header = li.Sections;
    for (unsigned i = 0; i < li.NumberOfSections; i++, section_header++) {
      std::string name(reinterpret_cast<char*>(section_header->Name));
      ImageSectionData data(name, section_header->Misc.VirtualSize ?
          section_header->Misc.VirtualSize :
          section_header->SizeOfRawData);
      // copy into result
      result->push_back(data);
    }
  } else {
    // map and load failed
    return false;
  }
  UnMapAndLoad(&li);
  return true;
}

} // namespace image_util
