// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GFX_TEXT_ELIDER_H_
#define CHROME_COMMON_GFX_TEXT_ELIDER_H_

#include <unicode/coll.h>
#include <unicode/uchar.h>

#include "app/gfx/chrome_font.h"
#include "base/basictypes.h"
#include "base/string16.h"

class FilePath;
class GURL;

namespace url_parse {
struct Parsed;
}

// TODO(port): this file should deal in string16s rather than wstrings.
namespace gfx {

// A function to get URL string from a GURL that will be suitable for display
// to the user. The parsing of the URL may change because various parts of the
// string will change lengths. The new parsing will be placed in the given out
// parameter. |prefix_end| is set to the end of the prefix (spec and separator
// characters before host).
// |languages|, |new_parsed|, and |prefix_end| may all be empty or NULL.
std::wstring GetCleanStringFromUrl(const GURL& url,
                                   const std::wstring& languages,
                                   url_parse::Parsed* new_parsed,
                                   size_t* prefix_end);

// This function takes a GURL object and elides it. It returns a string
// which composed of parts from subdomain, domain, path, filename and query.
// A "..." is added automatically at the end if the elided string is bigger
// than the available pixel width. For available pixel width = 0, empty
// string is returned. |languages| is a comma separted list of ISO 639
// language codes and is used to determine what characters are understood
// by a user. It should come from |prefs::kAcceptLanguages|.
//
// Note: in RTL locales, if the URL returned by this function is going to be
// displayed in the UI, then it is likely that the string needs to be marked
// as an LTR string (using l10n_util::WrapStringWithLTRFormatting()) so that it
// is displayed properly in an RTL context. Please refer to
// http://crbug.com/6487 for more information.
std::wstring ElideUrl(const GURL& url,
                      const ChromeFont& font,
                      int available_pixel_width,
                      const std::wstring& languages);

std::wstring ElideText(const std::wstring& text,
                       const ChromeFont& font,
                       int available_pixel_width);

// Elide a filename to fit a given pixel width, with an emphasis on not hiding
// the extension unless we have to. If filename contains a path, the path will
// be removed if filename doesn't fit into available_pixel_width.
std::wstring ElideFilename(const FilePath& filename,
                           const ChromeFont& font,
                           int available_pixel_width);

// SortedDisplayURL maintains a string from a URL suitable for display to the
// use. SortedDisplayURL also provides a function used for comparing two
// SortedDisplayURLs for use in visually ordering the SortedDisplayURLs.
//
// SortedDisplayURL is relatively cheap and supports value semantics.
class SortedDisplayURL {
 public:
  SortedDisplayURL(const GURL& url, const std::wstring& languages);
  SortedDisplayURL() {}

  // Compares this SortedDisplayURL to |url| using |collator|. Returns a value
  // < 0, = 1 or > 0 as to whether this url is less then, equal to or greater
  // than the supplied url.
  int Compare(const SortedDisplayURL& other, Collator* collator) const;

  // Returns the display string for the URL.
  const string16& display_url() const { return display_url_; }

 private:
  // Returns everything after the host. This is used by Compare if the hosts
  // match.
  string16 AfterHost() const;

  // Host name minus 'www.'. Used by Compare.
  string16 sort_host_;

  // End of the prefix (spec and separator) in display_url_.
  size_t prefix_end_;

  string16 display_url_;
};

} // namespace gfx.

#endif  // CHROME_COMMON_GFX_TEXT_ELIDER_H_
