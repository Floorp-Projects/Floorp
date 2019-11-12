/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHyphenator.h"

#include "mozilla/Telemetry.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsIInputStream.h"
#include "nsIJARURI.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsUnicodeProperties.h"
#include "nsUTF8Utils.h"

#include "mapped_hyph.h"

static const void* GetItemPtrFromJarURI(nsIJARURI* aJAR, uint32_t* aLength) {
  // Try to get the jarfile's nsZipArchive, find the relevant item, and return
  // a pointer to its data provided it is stored uncompressed.
  nsCOMPtr<nsIURI> jarFile;
  if (NS_FAILED(aJAR->GetJARFile(getter_AddRefs(jarFile)))) {
    return nullptr;
  }
  nsCOMPtr<nsIFileURL> fileUrl = do_QueryInterface(jarFile);
  if (!fileUrl) {
    return nullptr;
  }
  nsCOMPtr<nsIFile> file;
  fileUrl->GetFile(getter_AddRefs(file));
  if (!file) {
    return nullptr;
  }
  RefPtr<nsZipArchive> archive = mozilla::Omnijar::GetReader(file);
  if (archive) {
    nsCString path;
    aJAR->GetJAREntry(path);
    nsZipItem* item = archive->GetItem(path.get());
    if (item && item->Compression() == 0 && item->Size() > 0) {
      // We do NOT own this data, but it won't go away until the omnijar
      // file is closed during shutdown.
      const uint8_t* data = archive->GetData(item);
      if (data) {
        *aLength = item->Size();
        return data;
      }
    }
  }
  return nullptr;
}

static const void* LoadResourceFromURI(nsIURI* aURI, uint32_t* aLength) {
  nsCOMPtr<nsIChannel> channel;
  if (NS_FAILED(NS_NewChannel(getter_AddRefs(channel), aURI,
                              nsContentUtils::GetSystemPrincipal(),
                              nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                              nsIContentPolicy::TYPE_OTHER))) {
    return nullptr;
  }
  nsCOMPtr<nsIInputStream> instream;
  if (NS_FAILED(channel->Open(getter_AddRefs(instream)))) {
    return nullptr;
  }
  // Check size, bail out if it is excessively large (the largest of the
  // hyphenation files currently shipped with Firefox is around 1MB
  // uncompressed).
  uint64_t available;
  if (NS_FAILED(instream->Available(&available)) || !available ||
      available > 16 * 1024 * 1024) {
    return nullptr;
  }
  char* buffer = static_cast<char*>(malloc(available));
  if (!buffer) {
    return nullptr;
  }
  uint32_t bytesRead = 0;
  if (NS_FAILED(instream->Read(buffer, available, &bytesRead)) ||
      bytesRead != available) {
    free(buffer);
    return nullptr;
  }
  *aLength = bytesRead;
  return buffer;
}

nsHyphenator::nsHyphenator(nsIURI* aURI, bool aHyphenateCapitalized)
    : mDict(nullptr),
      mDictSize(0),
      mOwnsDict(false),
      mHyphenateCapitalized(aHyphenateCapitalized) {
  Telemetry::AutoTimer<Telemetry::HYPHENATION_LOAD_TIME> telemetry;

  nsCOMPtr<nsIJARURI> jar = do_QueryInterface(aURI);
  if (jar) {
    // This gives us a raw pointer into the omnijar's data (if uncompressed);
    // we do not own it and must not attempt to free it!
    mDict = GetItemPtrFromJarURI(jar, &mDictSize);
    if (!mDict) {
      // Omnijar must be compressed: we need to decompress the item into our
      // own buffer. (Currently this is the case on Android.)
      // TODO: Allocate in shared memory for all content processes to use.
      mDict = LoadResourceFromURI(aURI, &mDictSize);
      mOwnsDict = true;
    }
    if (mDict) {
      // Reject the resource from omnijar if it fails to validate. (If this
      // happens, we will hit the MOZ_ASSERT_UNREACHABLE at the end of the
      // constructor, indicating the build is broken in some way.)
      if (!mapped_hyph_is_valid_hyphenator(static_cast<const uint8_t*>(mDict),
                                           mDictSize)) {
        if (mOwnsDict) {
          free(const_cast<void*>(mDict));
        }
        mDict = nullptr;
        mDictSize = 0;
      }
    }
  } else if (mozilla::net::SchemeIsFile(aURI)) {
    // Ask the Rust lib to mmap the file. In this case our mDictSize field
    // remains zero; mDict is not a pointer to the raw data but an opaque
    // reference to a Rust object, and can only be freed by passing it to
    // mapped_hyph_free_dictionary().
    nsAutoCString path;
    aURI->GetFilePath(path);
    mDict = mapped_hyph_load_dictionary(path.get());
  }

  if (!mDict) {
    // This should never happen, unless someone has included an invalid
    // hyphenation file that fails to load.
    MOZ_ASSERT_UNREACHABLE("invalid hyphenation resource?");
  }
}

nsHyphenator::~nsHyphenator() {
  if (mDict) {
    if (mDictSize) {
      if (mOwnsDict) {
        free(const_cast<void*>(mDict));
      }
    } else {
      mapped_hyph_free_dictionary((HyphDic*)mDict);
    }
  }
}

bool nsHyphenator::IsValid() { return (mDict != nullptr); }

nsresult nsHyphenator::Hyphenate(const nsAString& aString,
                                 nsTArray<bool>& aHyphens) {
  if (!aHyphens.SetLength(aString.Length(), mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  memset(aHyphens.Elements(), false, aHyphens.Length() * sizeof(bool));

  bool inWord = false;
  uint32_t wordStart = 0, wordLimit = 0;
  uint32_t chLen;
  for (uint32_t i = 0; i < aString.Length(); i += chLen) {
    uint32_t ch = aString[i];
    chLen = 1;

    if (NS_IS_HIGH_SURROGATE(ch)) {
      if (i + 1 < aString.Length() && NS_IS_LOW_SURROGATE(aString[i + 1])) {
        ch = SURROGATE_TO_UCS4(ch, aString[i + 1]);
        chLen = 2;
      } else {
        NS_WARNING("unpaired surrogate found during hyphenation");
      }
    }

    nsUGenCategory cat = mozilla::unicode::GetGenCategory(ch);
    if (cat == nsUGenCategory::kLetter || cat == nsUGenCategory::kMark) {
      if (!inWord) {
        inWord = true;
        wordStart = i;
      }
      wordLimit = i + chLen;
      if (i + chLen < aString.Length()) {
        continue;
      }
    }

    if (inWord) {
      HyphenateWord(aString, wordStart, wordLimit, aHyphens);
      inWord = false;
    }
  }

  return NS_OK;
}

void nsHyphenator::HyphenateWord(const nsAString& aString, uint32_t aStart,
                                 uint32_t aLimit, nsTArray<bool>& aHyphens) {
  // Convert word from aStart and aLimit in aString to utf-8 for mapped_hyph,
  // lowercasing it as we go so that it will match the (lowercased) patterns
  // (bug 1105644).
  nsAutoCString utf8;
  const char16_t* cur = aString.BeginReading() + aStart;
  const char16_t* end = aString.BeginReading() + aLimit;
  bool firstLetter = true;
  while (cur < end) {
    uint32_t ch = *cur++;

    if (NS_IS_HIGH_SURROGATE(ch)) {
      if (cur < end && NS_IS_LOW_SURROGATE(*cur)) {
        ch = SURROGATE_TO_UCS4(ch, *cur++);
      } else {
        return;  // unpaired surrogate: bail out, don't hyphenate broken text
      }
    } else if (NS_IS_LOW_SURROGATE(ch)) {
      return;  // unpaired surrogate
    }

    // XXX What about language-specific casing? Consider Turkish I/i...
    // In practice, it looks like the current patterns will not be
    // affected by this, as they treat dotted and undotted i similarly.
    uint32_t origCh = ch;
    ch = ToLowerCase(ch);

    if (ch != origCh) {
      // Avoid hyphenating capitalized words (bug 1550532) unless explicitly
      // allowed by prefs for the language in use.
      // Also never auto-hyphenate a word that has internal caps, as it may
      // well be an all-caps acronym or a quirky name like iTunes.
      if (!mHyphenateCapitalized || !firstLetter) {
        return;
      }
    }
    firstLetter = false;

    if (ch < 0x80) {  // U+0000 - U+007F
      utf8.Append(ch);
    } else if (ch < 0x0800) {  // U+0100 - U+07FF
      utf8.Append(0xC0 | (ch >> 6));
      utf8.Append(0x80 | (0x003F & ch));
    } else if (ch < 0x10000) {  // U+0800 - U+D7FF,U+E000 - U+FFFF
      utf8.Append(0xE0 | (ch >> 12));
      utf8.Append(0x80 | (0x003F & (ch >> 6)));
      utf8.Append(0x80 | (0x003F & ch));
    } else {
      utf8.Append(0xF0 | (ch >> 18));
      utf8.Append(0x80 | (0x003F & (ch >> 12)));
      utf8.Append(0x80 | (0x003F & (ch >> 6)));
      utf8.Append(0x80 | (0x003F & ch));
    }
  }

  AutoTArray<uint8_t, 200> hyphenValues;
  hyphenValues.SetLength(utf8.Length());
  int32_t result;
  if (mDictSize > 0) {
    result = mapped_hyph_find_hyphen_values_raw(
        static_cast<const uint8_t*>(mDict), mDictSize, utf8.BeginReading(),
        utf8.Length(), hyphenValues.Elements(), hyphenValues.Length());
  } else {
    result = mapped_hyph_find_hyphen_values_dic(
        static_cast<const HyphDic*>(mDict), utf8.BeginReading(), utf8.Length(),
        hyphenValues.Elements(), hyphenValues.Length());
  }
  if (result > 0) {
    // We need to convert UTF-8 indexing as used by the hyphenation lib into
    // UTF-16 indexing of the aHyphens[] array for Gecko.
    uint32_t utf16index = 0;
    for (uint32_t utf8index = 0; utf8index < utf8.Length();) {
      // We know utf8 is valid, so we only need to look at the first byte of
      // each character to determine its length and the corresponding UTF-16
      // length to add to utf16index.
      const uint8_t leadByte = utf8[utf8index];
      if (leadByte < 0x80) {
        utf8index += 1;
      } else if (leadByte < 0xE0) {
        utf8index += 2;
      } else if (leadByte < 0xF0) {
        utf8index += 3;
      } else {
        utf8index += 4;
      }
      // The hyphenation value of interest is the one for the last code unit
      // of the utf-8 character, and is recorded on the last code unit of the
      // utf-16 character (in the case of a surrogate pair).
      utf16index += leadByte >= 0xF0 ? 2 : 1;
      if (utf16index > 0 && (hyphenValues[utf8index - 1] & 0x01)) {
        aHyphens[aStart + utf16index - 1] = true;
      }
    }
  }
}
