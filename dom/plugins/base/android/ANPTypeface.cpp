/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Android NPAPI support code
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Doug Turner <dougt@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "assert.h"
#include "ANPBase.h"
#include <android/log.h>
#include "gfxAndroidPlatform.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_typeface_##name

ANPTypeface*
anp_typeface_createFromName(const char name[], ANPTypefaceStyle aStyle)
{
  LOG("%s - %s\n", __PRETTY_FUNCTION__, name);

  gfxFontStyle style (aStyle == kItalic_ANPTypefaceStyle ? FONT_STYLE_ITALIC :
                      FONT_STYLE_NORMAL,
                      NS_FONT_STRETCH_NORMAL,
                      aStyle == kBold_ANPTypefaceStyle ? 700 : 400,
                      16.0,
                      NS_NewPermanentAtom(NS_LITERAL_STRING("en")),
                      0.0,
                      PR_FALSE, PR_FALSE,
                      NS_LITERAL_STRING(""),
                      NS_LITERAL_STRING(""));
  ANPTypeface* tf = new ANPTypeface;
  gfxAndroidPlatform * p = (gfxAndroidPlatform*)gfxPlatform::GetPlatform();
  nsRefPtr<gfxFont> font = gfxFT2Font::GetOrMakeFont(NS_ConvertASCIItoUTF16(name), &style);
  font.forget(&tf->mFont);
  if (tf->mFont) {
    ++tf->mRefCnt;
  }
  return tf;
}

ANPTypeface*
anp_typeface_createFromTypeface(const ANPTypeface* family,
                                ANPTypefaceStyle)
{
  NOT_IMPLEMENTED();
  return 0;
}

int32_t
anp_typeface_getRefCount(const ANPTypeface*)
{
  NOT_IMPLEMENTED();
  return 0;
}

void
anp_typeface_ref(ANPTypeface* tf)
{
  LOG("%s\n", __PRETTY_FUNCTION__);
  if (tf->mFont)
    ++tf->mRefCnt;

}

void
anp_typeface_unref(ANPTypeface* tf)
{
  LOG("%s\n", __PRETTY_FUNCTION__);
  if (tf->mFont)
    --tf->mRefCnt;
  if (tf->mRefCnt.get() == 0) {
    NS_IF_RELEASE(tf->mFont);
  }
}

ANPTypefaceStyle
anp_typeface_getStyle(const ANPTypeface* ft)
{
  LOG("%s\n", __PRETTY_FUNCTION__);
  return kBold_ANPTypefaceStyle;
}

int32_t
anp_typeface_getFontPath(const ANPTypeface*, char path[], int32_t length,
                         int32_t* index)
{
  NOT_IMPLEMENTED();
  return 0;
}

static const char* gFontDir;
#define FONT_DIR_SUFFIX     "/fonts/"

const char*
anp_typeface_getFontDirectoryPath()
{
  LOG("%s\n", __PRETTY_FUNCTION__);
  if (NULL == gFontDir) {
    const char* root = getenv("ANDROID_ROOT");
    size_t len = strlen(root);
    char* storage = (char*)malloc(len + sizeof(FONT_DIR_SUFFIX));
    if (NULL == storage) {
      return NULL;
    }
    memcpy(storage, root, len);
    memcpy(storage + len, FONT_DIR_SUFFIX, sizeof(FONT_DIR_SUFFIX));
    // save this assignment for last, so that if multiple threads call us
    // (which should never happen), we never return an incomplete global.
    // At worst, we would allocate storage for the path twice.
    gFontDir = storage;
  }

  return 0;
}

void InitTypeFaceInterface(ANPTypefaceInterfaceV0 *i) {
  _assert(i->inSize == sizeof(*i));
  ASSIGN(i, createFromName);
  ASSIGN(i, createFromTypeface);
  ASSIGN(i, getRefCount);
  ASSIGN(i, ref);
  ASSIGN(i, unref);
  ASSIGN(i, getStyle);
  ASSIGN(i, getFontPath);
  ASSIGN(i, getFontDirectoryPath);
}

