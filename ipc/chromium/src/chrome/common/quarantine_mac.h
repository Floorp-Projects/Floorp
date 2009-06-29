// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_QUARANTINE_MAC_H_
#define CHROME_COMMON_QUARANTINE_MAC_H_

class FilePath;
class GURL;

namespace quarantine_mac {

// Adds quarantine metadata to the file, assuming it has already been
// quarantined by the OS.
// |source| should be the source URL for the download, and |referrer| should be
// the URL the user initiated the download from.
void AddQuarantineMetadataToFile(const FilePath& file, const GURL& source,
                                 const GURL& referrer);

}  // namespace quarantine_mac

#endif  // CHROME_COMMON_QUARANTINE_MAC_H_
