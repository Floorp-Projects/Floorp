// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the ScopedClipboardWriter class. Documentation on its
// purpose can be found in base/scoped_clipboard_writer.h. Documentation on the
// format of the parameters for each clipboard target can be found in
// base/clipboard.h.
#include "base/scoped_clipboard_writer.h"

#include "base/string_util.h"

ScopedClipboardWriter::ScopedClipboardWriter(Clipboard* clipboard)
    : clipboard_(clipboard) {
}

ScopedClipboardWriter::~ScopedClipboardWriter() {
  if (!objects_.empty() && clipboard_)
    clipboard_->WriteObjects(objects_);
}

void ScopedClipboardWriter::WriteText(const string16& text) {
  if (text.empty())
    return;

  std::string utf8_text = UTF16ToUTF8(text);

  Clipboard::ObjectMapParams parameters;
  parameters.push_back(Clipboard::ObjectMapParam(utf8_text.begin(),
                                                 utf8_text.end()));
  objects_[Clipboard::CBF_TEXT] = parameters;
}

void ScopedClipboardWriter::WriteHTML(const string16& markup,
                                      const std::string& source_url) {
  if (markup.empty())
    return;

  std::string utf8_markup = UTF16ToUTF8(markup);

  Clipboard::ObjectMapParams parameters;
  parameters.push_back(
      Clipboard::ObjectMapParam(utf8_markup.begin(),
                                utf8_markup.end()));
  if (!source_url.empty()) {
    parameters.push_back(Clipboard::ObjectMapParam(source_url.begin(),
                                                   source_url.end()));
  }

  objects_[Clipboard::CBF_HTML] = parameters;
}

void ScopedClipboardWriter::WriteBookmark(const string16& bookmark_title,
                                          const std::string& url) {
  if (bookmark_title.empty() || url.empty())
    return;

  std::string utf8_markup = UTF16ToUTF8(bookmark_title);

  Clipboard::ObjectMapParams parameters;
  parameters.push_back(Clipboard::ObjectMapParam(utf8_markup.begin(),
                                                 utf8_markup.end()));
  parameters.push_back(Clipboard::ObjectMapParam(url.begin(), url.end()));
  objects_[Clipboard::CBF_BOOKMARK] = parameters;
}

void ScopedClipboardWriter::WriteHyperlink(const string16& link_text,
                                           const std::string& url) {
  if (link_text.empty() || url.empty())
    return;

  std::string utf8_markup = UTF16ToUTF8(link_text);

  Clipboard::ObjectMapParams parameters;
  parameters.push_back(Clipboard::ObjectMapParam(utf8_markup.begin(),
                                                 utf8_markup.end()));
  parameters.push_back(Clipboard::ObjectMapParam(url.begin(), url.end()));
  objects_[Clipboard::CBF_LINK] = parameters;
}

void ScopedClipboardWriter::WriteFile(const FilePath& file) {
  WriteFiles(std::vector<FilePath>(1, file));
}

// Save the filenames as a string separated by nulls and terminated with an
// extra null.
void ScopedClipboardWriter::WriteFiles(const std::vector<FilePath>& files) {
  if (files.empty())
    return;

  Clipboard::ObjectMapParam parameter;

  for (std::vector<FilePath>::const_iterator iter = files.begin();
       iter != files.end(); ++iter) {
    FilePath filepath = *iter;
    FilePath::StringType filename = filepath.value();

    size_t data_length = filename.length() * sizeof(FilePath::CharType);
    const char* data = reinterpret_cast<const char*>(filename.data());
    const char* data_end = data + data_length;

    for (const char* ch = data; ch < data_end; ++ch)
      parameter.push_back(*ch);

    // NUL-terminate the string.
    for (size_t i = 0; i < sizeof(FilePath::CharType); ++i)
      parameter.push_back('\0');
  }

  // NUL-terminate the string list.
  for (size_t i = 0; i < sizeof(FilePath::CharType); ++i)
    parameter.push_back('\0');

  Clipboard::ObjectMapParams parameters;
  parameters.push_back(parameter);
  objects_[Clipboard::CBF_FILES] = parameters;
}

void ScopedClipboardWriter::WriteWebSmartPaste() {
  objects_[Clipboard::CBF_WEBKIT] = Clipboard::ObjectMapParams();
}

void ScopedClipboardWriter::WriteBitmapFromPixels(const void* pixels,
                                                  const gfx::Size& size) {
  Clipboard::ObjectMapParam pixels_parameter, size_parameter;
  const char* pixels_data = reinterpret_cast<const char*>(pixels);
  size_t pixels_length = 4 * size.width() * size.height();
  for (size_t i = 0; i < pixels_length; i++)
    pixels_parameter.push_back(pixels_data[i]);

  const char* size_data = reinterpret_cast<const char*>(&size);
  size_t size_length = sizeof(gfx::Size);
  for (size_t i = 0; i < size_length; i++)
    size_parameter.push_back(size_data[i]);

  Clipboard::ObjectMapParams parameters;
  parameters.push_back(pixels_parameter);
  parameters.push_back(size_parameter);
  objects_[Clipboard::CBF_BITMAP] = parameters;
}
