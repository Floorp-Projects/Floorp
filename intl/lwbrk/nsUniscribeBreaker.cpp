/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsComplexBreaker.h"

#include <windows.h>

#include <usp10.h>

#include "nsUTF8Utils.h"
#include "nsString.h"
#include "nsTArray.h"

#if defined(MOZ_SANDBOX)
#  include "mozilla/WindowsProcessMitigations.h"
#  include "mozilla/SandboxSettings.h"
#  include "mozilla/sandboxTarget.h"
#  include "nsXULAppAPI.h"

#  if defined(MOZ_DEBUG)
#    include "mozilla/StaticPrefs_intl.h"
#  endif
#endif

using namespace mozilla;

#if defined(MOZ_SANDBOX)
static bool UseBrokeredLineBreaking() {
  // If win32k lockdown is enabled we can't use Uniscribe in this process. Also
  // if the sandbox is above a certain level we can't load the required DLLs
  // without other intervention. Given that it looks like we are likely to have
  // win32k lockdown enabled first, using the brokered call for people testing
  // this case also makes most sense.
  static bool sUseBrokeredLineBreaking =
      IsWin32kLockedDown() ||
      (XRE_IsContentProcess() && GetEffectiveContentSandboxLevel() >= 20);

  return sUseBrokeredLineBreaking;
}
#endif

void NS_GetComplexLineBreaks(const char16_t* aText, uint32_t aLength,
                             uint8_t* aBreakBefore) {
  NS_ASSERTION(aText, "aText shouldn't be null");

#if defined(MOZ_SANDBOX)
  if (UseBrokeredLineBreaking()) {
    // We can't use Uniscribe, so use a brokered call. Use of Uniscribe will be
    // replaced in bug 1684927.
    char16ptr_t text = aText;
    if (!SandboxTarget::Instance()->GetComplexLineBreaks(text, aLength,
                                                         aBreakBefore)) {
      NS_WARNING("Brokered line break failed, breaks might be incorrect.");
    }

    return;
  }
#endif

  int outItems = 0;
  HRESULT result;
  AutoTArray<SCRIPT_ITEM, 64> items;
  char16ptr_t text = aText;

  memset(aBreakBefore, false, aLength);

  items.AppendElements(64);

  do {
    result = ScriptItemize(text, aLength, items.Length(), nullptr, nullptr,
                           items.Elements(), &outItems);

    if (result == E_OUTOFMEMORY) {
      // XXX(Bug 1631371) Check if this should use a fallible operation as it
      // pretended earlier.
      items.AppendElements(items.Length());
    }
  } while (result == E_OUTOFMEMORY);

  for (int iItem = 0; iItem < outItems; ++iItem) {
    uint32_t endOffset =
        (iItem + 1 == outItems ? aLength : items[iItem + 1].iCharPos);
    uint32_t startOffset = items[iItem].iCharPos;
    AutoTArray<SCRIPT_LOGATTR, 64> sla;

    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    sla.AppendElements(endOffset - startOffset);

    if (ScriptBreak(text + startOffset, endOffset - startOffset,
                    &items[iItem].a, sla.Elements()) < 0)
      return;

    // We don't want to set a potential break position at the start of text;
    // that's the responsibility of a higher level.
    for (uint32_t j = startOffset ? 0 : 1; j + startOffset < endOffset; ++j) {
      aBreakBefore[j + startOffset] = sla[j].fSoftBreak;
    }
  }

#if defined(MOZ_DEBUG) && defined(MOZ_SANDBOX)
  // When tests are enabled and pref is set, we compare the line breaks returned
  // from the Uniscribe breaker in the content process, with the ones returned
  // from the brokered call to the parent. If they differ we crash so we can
  // test using a crashtest.
  if (!StaticPrefs::intl_compare_against_brokered_complex_line_breaks() ||
      !XRE_IsContentProcess()) {
    return;
  }

  nsTArray<uint8_t> brokeredBreaks(aLength);
  brokeredBreaks.AppendElements(aLength);
  if (!SandboxTarget::Instance()->GetComplexLineBreaks(
          text, aLength, brokeredBreaks.Elements())) {
    MOZ_CRASH("Brokered GetComplexLineBreaks failed.");
  }

  bool mismatch = false;
  for (uint32_t i = 0; i < aLength; ++i) {
    if (aBreakBefore[i] != brokeredBreaks[i]) {
      mismatch = true;
      break;
    }
  }

  if (mismatch) {
    nsCString line("uniscribe: ");
    // The logging here doesn't handle surrogates, but we only have tests using
    // Thai currently, which is BMP-only.
    for (uint32_t i = 0; i < aLength; ++i) {
      if (aBreakBefore[i]) line.Append('#');
      line.Append(NS_ConvertUTF16toUTF8(aText + i, 1).get());
    }
    printf_stderr("%s\n", line.get());
    line.Assign("brokered : ");
    for (uint32_t i = 0; i < aLength; ++i) {
      if (brokeredBreaks[i]) line.Append('#');
      line.Append(NS_ConvertUTF16toUTF8(aText + i, 1).get());
    }
    printf_stderr("%s\n", line.get());
    MOZ_CRASH("Brokered breaks did not match.");
  }
#endif
}
