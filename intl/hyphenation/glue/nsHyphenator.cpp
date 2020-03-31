/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHyphenator.h"

#include "mozilla/dom/ContentChild.h"
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

using namespace mozilla;

void DefaultDelete<const HyphDic>::operator()(const HyphDic* aHyph) const {
  mapped_hyph_free_dictionary(const_cast<HyphDic*>(aHyph));
}

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
  RefPtr<nsZipArchive> archive = Omnijar::GetReader(file);
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

already_AddRefed<ipc::SharedMemoryBasic> GetHyphDictFromParent(
    nsIURI* aURI, uint32_t* aLength) {
  MOZ_ASSERT(!XRE_IsParentProcess());
  ipc::SharedMemoryBasic::Handle handle = ipc::SharedMemoryBasic::NULLHandle();
  uint32_t size;
  MOZ_ASSERT(aURI);
  if (!dom::ContentChild::GetSingleton()->SendGetHyphDict(aURI, &handle,
                                                          &size)) {
    return nullptr;
  }
  RefPtr<ipc::SharedMemoryBasic> shm = new ipc::SharedMemoryBasic();
  if (!shm->IsHandleValid(handle)) {
    return nullptr;
  }
  if (!shm->SetHandle(handle, ipc::SharedMemoryBasic::RightsReadOnly)) {
    return nullptr;
  }
  if (!shm->Map(size)) {
    return nullptr;
  }
  char* addr = static_cast<char*>(shm->memory());
  if (!addr) {
    return nullptr;
  }
  *aLength = size;
  return shm.forget();
}

static already_AddRefed<ipc::SharedMemoryBasic> LoadInShmemFromURI(
    nsIURI* aURI, uint32_t* aLength) {
  MOZ_ASSERT(XRE_IsParentProcess());
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

  // The shm-related calls here are not expected to fail, but if they do,
  // we'll just return null (as if the resource was unavailable) and proceed
  // without hyphenation.
  RefPtr<ipc::SharedMemoryBasic> shm = new ipc::SharedMemoryBasic();
  if (!shm->Create(available)) {
    return nullptr;
  }
  if (!shm->Map(available)) {
    return nullptr;
  }
  char* buffer = static_cast<char*>(shm->memory());
  if (!buffer) {
    return nullptr;
  }

  uint32_t bytesRead = 0;
  if (NS_FAILED(instream->Read(buffer, available, &bytesRead)) ||
      bytesRead != available) {
    return nullptr;
  }
  *aLength = bytesRead;
  return shm.forget();
}

nsHyphenator::nsHyphenator(nsIURI* aURI, bool aHyphenateCapitalized)
    : mDict(static_cast<const void*>(nullptr)),
      mDictSize(0),
      mHyphenateCapitalized(aHyphenateCapitalized) {
  nsCOMPtr<nsIJARURI> jar = do_QueryInterface(aURI);
  if (jar) {
    // This gives us a raw pointer into the omnijar's data (if uncompressed);
    // we do not own it and must not attempt to free it!
    const void* ptr = GetItemPtrFromJarURI(jar, &mDictSize);
    if (ptr) {
      if (mapped_hyph_is_valid_hyphenator(static_cast<const uint8_t*>(ptr),
                                          mDictSize)) {
        mDict = AsVariant(ptr);
        return;
      }
    } else {
      // Omnijar must be compressed (currently this is the case on Android).
      // If we're the parent process, decompress the resource into a shmem
      // buffer; if we're a child, send a request to the parent for the
      // shared-memory copy (which it will load if not already available).
      RefPtr<ipc::SharedMemoryBasic> shm;
      if (XRE_IsParentProcess()) {
        shm = LoadInShmemFromURI(aURI, &mDictSize);
        if (shm && mapped_hyph_is_valid_hyphenator(
                       static_cast<const uint8_t*>(shm->memory()), mDictSize)) {
          mDict = AsVariant(shm);
          return;
        }
      } else {
        shm = GetHyphDictFromParent(aURI, &mDictSize);
        if (shm) {
          // We don't need to validate mDict because the parent process
          // will have done so.
          mDict = AsVariant(shm);
          return;
        }
      }
    }
  }

  if (net::SchemeIsFile(aURI)) {
    // Ask the Rust lib to mmap the file. In this case our mDictSize field
    // remains zero; mDict is not a pointer to the raw data but an opaque
    // reference to a Rust object, and can only be freed by passing it to
    // mapped_hyph_free_dictionary().
    nsAutoCString path;
    aURI->GetFilePath(path);
    UniquePtr<const HyphDic> dic(mapped_hyph_load_dictionary(path.get()));
    if (dic) {
      mDict = AsVariant(std::move(dic));
      return;
    }
  }

  MOZ_ASSERT_UNREACHABLE("invalid hyphenation resource?");
}

bool nsHyphenator::IsValid() {
  return mDict.match(
      [](const void*& ptr) { return ptr != nullptr; },
      [](RefPtr<ipc::SharedMemoryBasic>& shm) { return shm != nullptr; },
      [](mozilla::UniquePtr<const HyphDic>& hyph) { return hyph != nullptr; });
}

nsresult nsHyphenator::Hyphenate(const nsAString& aString,
                                 nsTArray<bool>& aHyphens) {
  if (!aHyphens.SetLength(aString.Length(), fallible)) {
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

    nsUGenCategory cat = unicode::GetGenCategory(ch);
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
  int32_t result = mDict.match(
      [&](const void*& ptr) {
        return mapped_hyph_find_hyphen_values_raw(
            static_cast<const uint8_t*>(ptr), mDictSize, utf8.BeginReading(),
            utf8.Length(), hyphenValues.Elements(), hyphenValues.Length());
      },
      [&](RefPtr<mozilla::ipc::SharedMemoryBasic>& shm) {
        return mapped_hyph_find_hyphen_values_raw(
            static_cast<const uint8_t*>(shm->memory()), mDictSize,
            utf8.BeginReading(), utf8.Length(), hyphenValues.Elements(),
            hyphenValues.Length());
      },
      [&](mozilla::UniquePtr<const HyphDic>& hyph) {
        return mapped_hyph_find_hyphen_values_dic(
            hyph.get(), utf8.BeginReading(), utf8.Length(),
            hyphenValues.Elements(), hyphenValues.Length());
      });
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

void nsHyphenator::ShareToProcess(base::ProcessId aPid,
                                  ipc::SharedMemoryBasic::Handle* aOutHandle,
                                  uint32_t* aOutSize) {
  // If the resource is invalid, or if we fail to share it to the child
  // process, we'll just bail out and continue without hyphenation; no need
  // for this to be a fatal error.
  if (!mDict.is<RefPtr<mozilla::ipc::SharedMemoryBasic>>()) {
    return;
  }
  if (!mDict.as<RefPtr<mozilla::ipc::SharedMemoryBasic>>()->ShareToProcess(
          aPid, aOutHandle)) {
    return;
  }
  *aOutSize = mDictSize;
}
