// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file/namespace contains utility functions for gathering
// information about PE (Portable Executable) headers within
// images (dll's / exe's )

#ifndef BASE_IMAGE_UTIL_H_
#define BASE_IMAGE_UTIL_H_

#include <windows.h>
#include <vector>

#include "base/basictypes.h"

namespace image_util {

// Contains both the PE section name (.text, .reloc etc) and its size.
struct ImageSectionData {
  ImageSectionData (const std::string& section_name, size_t section_size)
      : name (section_name),
        size_in_bytes(section_size) {
  }

  std::string name;
  size_t size_in_bytes;
};

typedef std::vector<ImageSectionData> ImageSectionsData;

// Provides image statistics for modules of a specified process, or for the
// specified process' own executable file. To use, invoke CreateImageMetrics()
// to get an instance for a specified process, then access the information via
// methods.
class ImageMetrics {
 public:
  // Creates an ImageMetrics instance for given process owned by
  // the caller.
  explicit ImageMetrics(HANDLE process);
  ~ImageMetrics();

  // Fills a vector of ImageSectionsData containing name/size info
  // for every section found in the specified dll's PE section table.
  // The DLL must be loaded by the process associated with this ImageMetrics
  // instance.
  bool GetDllImageSectionData(const std::string& loaded_dll_name,
                              ImageSectionsData* section_sizes);

  // Fills a vector if ImageSectionsData containing name/size info
  // for every section found in the executable file of the process
  // associated with this ImageMetrics instance.
  bool GetProcessImageSectionData(ImageSectionsData* section_sizes);

 private:
  // Helper for GetDllImageSectionData and GetProcessImageSectionData
  bool GetImageSectionSizes(char* qualified_path, ImageSectionsData* result);

  HANDLE process_;

  DISALLOW_COPY_AND_ASSIGN(ImageMetrics);
};

}  // namespace image_util

#endif  // BASE_IMAGE_UTIL_H_
