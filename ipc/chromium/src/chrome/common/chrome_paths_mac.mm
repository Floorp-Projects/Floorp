// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/common/chrome_paths_internal.h"

#import <Cocoa/Cocoa.h>

#import "base/base_paths.h"
#import "base/file_path.h"
#import "base/logging.h"
#import "base/path_service.h"

namespace chrome {

bool GetDefaultUserDataDirectory(FilePath* result) {
  bool success = false;
  NSArray* dirs =
      NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory,
                                          NSUserDomainMask, YES);
  if ([dirs count] && result) {
    NSString* base = [dirs objectAtIndex:0];
#if defined(GOOGLE_CHROME_BUILD)
    base = [base stringByAppendingPathComponent:@"Google"];
    NSString* tail = @"Chrome";
#else
    NSString* tail = @"Chromium";
#endif
    NSString* path = [base stringByAppendingPathComponent:tail];
    *result = FilePath([path fileSystemRepresentation]);
    success = true;
  }
  return success;
}

bool GetUserDocumentsDirectory(FilePath* result) {
  bool success = false;
  NSArray* docArray =
      NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                          NSUserDomainMask,
                                          YES);
  if ([docArray count] && result) {
    *result = FilePath([[docArray objectAtIndex:0] fileSystemRepresentation]);
    success = true;
  }
  return success;
}

bool GetUserDownloadsDirectory(FilePath* result) {
  bool success = false;
  NSArray* docArray =
      NSSearchPathForDirectoriesInDomains(NSDownloadsDirectory,
                                          NSUserDomainMask,
                                          YES);
  if ([docArray count] && result) {
    *result = FilePath([[docArray objectAtIndex:0] fileSystemRepresentation]);
    success = true;
  }
  return success;
}

bool GetUserDesktop(FilePath* result) {
  bool success = false;
  NSArray* docArray =
      NSSearchPathForDirectoriesInDomains(NSDesktopDirectory,
                                          NSUserDomainMask,
                                          YES);
  if ([docArray count] && result) {
    *result = FilePath([[docArray objectAtIndex:0] fileSystemRepresentation]);
    success = true;
  }
  return success;
}

}  // namespace chrome
