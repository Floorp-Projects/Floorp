// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares the ScopedClipboardWriter class, a wrapper around
// the Clipboard class which simplifies writing data to the system clipboard.
// Upon deletion the class atomically writes all data to |clipboard_|,
// avoiding any potential race condition with other processes that are also
// writing to the system clipboard.
#ifndef BASE_SCOPED_CLIPBOARD_WRITER_H_
#define BASE_SCOPED_CLIPBOARD_WRITER_H_

#include <string>
#include <vector>

#include "base/clipboard.h"
#include "base/file_path.h"
#include "base/string16.h"

// This class is a wrapper for |Clipboard| that handles packing data
// into a Clipboard::ObjectMap.
// NB: You should probably NOT be using this class if you include
// webkit_glue.h. Use ScopedClipboardWriterGlue instead.
class ScopedClipboardWriter {
 public:
  // Create an instance that is a simple wrapper around clipboard.
  ScopedClipboardWriter(Clipboard* clipboard);

  ~ScopedClipboardWriter();

  // Converts |text| to UTF-8 and adds it to the clipboard.
  void WriteText(const string16& text);

  // Adds HTML to the clipboard.  The url parameter is optional, but especially
  // useful if the HTML fragment contains relative links.
  void WriteHTML(const string16& markup, const std::string& source_url);

  // Adds a bookmark to the clipboard.
  void WriteBookmark(const string16& bookmark_title,
                     const std::string& url);

  // Adds both a bookmark and an HTML hyperlink to the clipboard.  It is a
  // convenience wrapper around WriteBookmark and WriteHTML. |link_text| is
  // used as the bookmark title.
  void WriteHyperlink(const string16& link_text, const std::string& url);

  // Adds a file or group of files to the clipboard.
  void WriteFile(const FilePath& file);
  void WriteFiles(const std::vector<FilePath>& files);

  // Used by WebKit to determine whether WebKit wrote the clipboard last
  void WriteWebSmartPaste();

  // Adds a bitmap to the clipboard
  // Pixel format is assumed to be 32-bit BI_RGB.
  void WriteBitmapFromPixels(const void* pixels, const gfx::Size& size);

 protected:
  // We accumulate the data passed to the various targets in the |objects_|
  // vector, and pass it to Clipboard::WriteObjects() during object destruction.
  Clipboard::ObjectMap objects_;
  Clipboard* clipboard_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedClipboardWriter);
};

#endif  // BASE_SCOPED_CLIPBOARD_WRITER_H_
