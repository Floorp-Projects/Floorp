// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/quarantine_mac.h"

#include <ApplicationServices/ApplicationServices.h>
#import <Foundation/Foundation.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "googleurl/src/gurl.h"

namespace quarantine_mac {

// The OS will automatically quarantine files due to the
// LSFileQuarantineEnabled entry in our Info.plist, but it knows relatively
// little about the files. We add more information about the download to
// improve the UI shown by the OS when the users tries to open the file.
void AddQuarantineMetadataToFile(const FilePath& file, const GURL& source,
                                 const GURL& referrer) {
  FSRef file_ref;
  if (!mac_util::FSRefFromPath(file.value(), &file_ref))
    return;

  NSMutableDictionary* quarantine_properties = nil;
  CFTypeRef quarantine_properties_base = NULL;
  if (::LSCopyItemAttribute(&file_ref, kLSRolesAll, kLSItemQuarantineProperties,
                            &quarantine_properties_base) == noErr) {
    if (::CFGetTypeID(quarantine_properties_base) ==
        ::CFDictionaryGetTypeID()) {
      // Quarantine properties will already exist if LSFileQuarantineEnabled
      // is on and the file doesn't match an exclusion.
      quarantine_properties =
          [[(NSDictionary*)quarantine_properties_base mutableCopy] autorelease];
    } else {
      LOG(WARNING) << "kLSItemQuarantineProperties is not a dictionary on file "
                   << file.value();
    }
    ::CFRelease(quarantine_properties_base);
  }

  if (!quarantine_properties) {
    // If there are no quarantine properties, then the file isn't quarantined
    // (e.g., because the user has set up exclusions for certain file types).
    // We don't want to add any metadata, because that will cause the file to
    // be quarantined against the user's wishes.
    return;
  }

  // kLSQuarantineAgentNameKey, kLSQuarantineAgentBundleIdentifierKey, and
  // kLSQuarantineTimeStampKey are set for us (see LSQuarantine.h), so we only
  // need to set the values that the OS can't infer.

  if (![quarantine_properties valueForKey:(NSString*)kLSQuarantineTypeKey]) {
    CFStringRef type = (source.SchemeIs("http") || source.SchemeIs("https"))
                       ? kLSQuarantineTypeWebDownload
                       : kLSQuarantineTypeOtherDownload;
    [quarantine_properties setValue:(NSString*)type
                             forKey:(NSString*)kLSQuarantineTypeKey];
  }

  if (![quarantine_properties
          valueForKey:(NSString*)kLSQuarantineOriginURLKey] &&
      referrer.is_valid()) {
    NSString* referrer_url =
        [NSString stringWithUTF8String:referrer.spec().c_str()];
    [quarantine_properties setValue:referrer_url
                             forKey:(NSString*)kLSQuarantineOriginURLKey];
  }

  if (![quarantine_properties valueForKey:(NSString*)kLSQuarantineDataURLKey] &&
      source.is_valid()) {
    NSString* origin_url =
        [NSString stringWithUTF8String:source.spec().c_str()];
    [quarantine_properties setValue:origin_url
                             forKey:(NSString*)kLSQuarantineDataURLKey];
  }

  OSStatus os_error = ::LSSetItemAttribute(&file_ref, kLSRolesAll,
                                           kLSItemQuarantineProperties,
                                           quarantine_properties);
  if (os_error != noErr) {
    LOG(WARNING) << "Unable to set quarantine attributes on file "
                 << file.value();
  }
}

}  // namespace quarantine_mac
