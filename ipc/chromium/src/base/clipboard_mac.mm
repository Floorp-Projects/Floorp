// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/clipboard.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"

namespace {

// Would be nice if this were in UTCoreTypes.h, but it isn't
const NSString* kUTTypeURLName = @"public.url-name";

// Tells us if WebKit was the last to write to the pasteboard. There's no
// actual data associated with this type.
const NSString *kWebSmartPastePboardType = @"NeXT smart paste pasteboard type";

NSPasteboard* GetPasteboard() {
  // The pasteboard should not be nil in a UI session, but this handy DCHECK
  // can help track down problems if someone tries using clipboard code outside
  // of a UI session.
  NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
  DCHECK(pasteboard);
  return pasteboard;
}

}  // namespace

Clipboard::Clipboard() {
}

Clipboard::~Clipboard() {
}

void Clipboard::WriteObjects(const ObjectMap& objects) {
  NSPasteboard* pb = GetPasteboard();
  [pb declareTypes:[NSArray array] owner:nil];

  for (ObjectMap::const_iterator iter = objects.begin();
       iter != objects.end(); ++iter) {
    DispatchObject(static_cast<ObjectType>(iter->first), iter->second);
  }

}

void Clipboard::WriteText(const char* text_data, size_t text_len) {
  std::string text_str(text_data, text_len);
  NSString *text = base::SysUTF8ToNSString(text_str);
  NSPasteboard* pb = GetPasteboard();
  [pb addTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
  [pb setString:text forType:NSStringPboardType];
}

void Clipboard::WriteHTML(const char* markup_data,
                          size_t markup_len,
                          const char* url_data,
                          size_t url_len) {
  std::string html_fragment_str(markup_data, markup_len);
  NSString *html_fragment = base::SysUTF8ToNSString(html_fragment_str);

  // TODO(avi): url_data?
  NSPasteboard* pb = GetPasteboard();
  [pb addTypes:[NSArray arrayWithObject:NSHTMLPboardType] owner:nil];
  [pb setString:html_fragment forType:NSHTMLPboardType];
}

void Clipboard::WriteBookmark(const char* title_data,
                              size_t title_len,
                              const char* url_data,
                              size_t url_len) {
  WriteHyperlink(title_data, title_len, url_data, url_len);
}

void Clipboard::WriteHyperlink(const char* title_data,
                               size_t title_len,
                               const char* url_data,
                               size_t url_len) {
  std::string title_str(title_data, title_len);
  NSString *title =  base::SysUTF8ToNSString(title_str);
  std::string url_str(url_data, url_len);
  NSString *url =  base::SysUTF8ToNSString(url_str);

  // TODO(playmobil): In the Windows version of this function, an HTML
  // representation of the bookmark is also added to the clipboard, to support
  // drag and drop of web shortcuts.  I don't think we need to do this on the
  // Mac, but we should double check later on.
  NSURL* nsurl = [NSURL URLWithString:url];

  NSPasteboard* pb = GetPasteboard();
  // passing UTIs into the pasteboard methods is valid >= 10.5
  [pb addTypes:[NSArray arrayWithObjects:NSURLPboardType,
                                         kUTTypeURLName,
                                         nil]
         owner:nil];
  [nsurl writeToPasteboard:pb];
  [pb setString:title forType:kUTTypeURLName];
}

void Clipboard::WriteFiles(const char* file_data, size_t file_len) {
  NSMutableArray* fileList = [NSMutableArray arrayWithCapacity:1];

  // Offset of current filename from start of file_data array.
  size_t current_filename_offset = 0;

  // file_data is double null terminated (see table at top of clipboard.h).
  // So this loop can ignore the second null terminator, thus file_len - 1.
  // TODO(playmobil): If we need a loop like this on other platforms then split
  // this out into a common function that outputs a std::vector<const char*>.
  for (size_t i = 0; i < file_len - 1; ++i) {
    if (file_data[i] == '\0') {
      const char* filename = &file_data[current_filename_offset];
      [fileList addObject:[NSString stringWithUTF8String:filename]];

      current_filename_offset = i + 1;
      continue;
    }
  }

  NSPasteboard* pb = GetPasteboard();
  [pb addTypes:[NSArray arrayWithObject:NSFilenamesPboardType] owner:nil];
  [pb setPropertyList:fileList forType:NSFilenamesPboardType];
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void Clipboard::WriteWebSmartPaste() {
  NSPasteboard* pb = GetPasteboard();
  NSString* format = base::SysUTF8ToNSString(GetWebKitSmartPasteFormatType());
  [pb addTypes:[NSArray arrayWithObject:format] owner:nil];
  [pb setData:nil forType:format];
}

bool Clipboard::IsFormatAvailable(const Clipboard::FormatType& format) const {
  NSString* format_ns = base::SysUTF8ToNSString(format);

  NSPasteboard* pb = GetPasteboard();
  NSArray* types = [pb types];

  return [types containsObject:format_ns];
}

void Clipboard::ReadText(string16* result) const {
  NSPasteboard* pb = GetPasteboard();
  NSString* contents = [pb stringForType:NSStringPboardType];

  UTF8ToUTF16([contents UTF8String],
              [contents lengthOfBytesUsingEncoding:NSUTF8StringEncoding],
              result);
}

void Clipboard::ReadAsciiText(std::string* result) const {
  NSPasteboard* pb = GetPasteboard();
  NSString* contents = [pb stringForType:NSStringPboardType];

  if (!contents)
    result->clear();
  else
    result->assign([contents UTF8String]);
}

void Clipboard::ReadHTML(string16* markup, std::string* src_url) const {
  if (markup) {
    NSPasteboard* pb = GetPasteboard();
    NSArray *supportedTypes = [NSArray arrayWithObjects:NSHTMLPboardType,
                                                        NSStringPboardType,
                                                        nil];
    NSString *bestType = [pb availableTypeFromArray:supportedTypes];
    NSString* contents = [pb stringForType:bestType];
    UTF8ToUTF16([contents UTF8String],
                [contents lengthOfBytesUsingEncoding:NSUTF8StringEncoding],
                markup);
  }

  // TODO(avi): src_url?
  if (src_url)
    src_url->clear();
}

void Clipboard::ReadBookmark(string16* title, std::string* url) const {
  NSPasteboard* pb = GetPasteboard();

  if (title) {
    NSString* contents = [pb stringForType:kUTTypeURLName];
    UTF8ToUTF16([contents UTF8String],
                [contents lengthOfBytesUsingEncoding:NSUTF8StringEncoding],
                title);
  }

  if (url) {
    NSString* url_string = [[NSURL URLFromPasteboard:pb] absoluteString];
    if (!url_string)
      url->clear();
    else
      url->assign([url_string UTF8String]);
  }
}

void Clipboard::ReadFile(FilePath* file) const {
  if (!file) {
    NOTREACHED();
    return;
  }

  *file = FilePath();
  std::vector<FilePath> files;
  ReadFiles(&files);

  // Take the first file, if available.
  if (!files.empty())
    *file = files[0];
}

void Clipboard::ReadFiles(std::vector<FilePath>* files) const {
  if (!files) {
    NOTREACHED();
    return;
  }

  files->clear();

  NSPasteboard* pb = GetPasteboard();
  NSArray* fileList = [pb propertyListForType:NSFilenamesPboardType];

  for (unsigned int i = 0; i < [fileList count]; ++i) {
    std::string file = [[fileList objectAtIndex:i] UTF8String];
    files->push_back(FilePath(file));
  }
}

// static
Clipboard::FormatType Clipboard::GetUrlFormatType() {
  static const std::string type = base::SysNSStringToUTF8(NSURLPboardType);
  return type;
}

// static
Clipboard::FormatType Clipboard::GetUrlWFormatType() {
  static const std::string type = base::SysNSStringToUTF8(NSURLPboardType);
  return type;
}

// static
Clipboard::FormatType Clipboard::GetPlainTextFormatType() {
  static const std::string type = base::SysNSStringToUTF8(NSStringPboardType);
  return type;
}

// static
Clipboard::FormatType Clipboard::GetPlainTextWFormatType() {
  static const std::string type = base::SysNSStringToUTF8(NSStringPboardType);
  return type;
}

// static
Clipboard::FormatType Clipboard::GetFilenameFormatType() {
  static const std::string type =
      base::SysNSStringToUTF8(NSFilenamesPboardType);
  return type;
}

// static
Clipboard::FormatType Clipboard::GetFilenameWFormatType() {
  static const std::string type =
      base::SysNSStringToUTF8(NSFilenamesPboardType);
  return type;
}

// static
Clipboard::FormatType Clipboard::GetHtmlFormatType() {
  static const std::string type = base::SysNSStringToUTF8(NSHTMLPboardType);
  return type;
}

// static
Clipboard::FormatType Clipboard::GetWebKitSmartPasteFormatType() {
  static const std::string type =
      base::SysNSStringToUTF8(kWebSmartPastePboardType);
  return type;
}
