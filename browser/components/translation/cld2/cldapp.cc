/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "public/compact_lang_det.h"

extern "C" {

using namespace CLD2;

bool g_is_reliable;

const char* detectLangCode(const char* src) {
  return LanguageCode(DetectLanguage(src, strlen(src),
                                     true /* is_plain_text */,
                                     &g_is_reliable));
}

bool lastResultReliable(void) {
  return g_is_reliable;
}

}
