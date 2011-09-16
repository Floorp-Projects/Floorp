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
  tf->mFont = gfxFT2Font::GetOrMakeFont(NS_ConvertASCIItoUTF16(name), &style);
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
    tf->mFont->AddRef();

}

void
anp_typeface_unref(ANPTypeface* tf)
{
  LOG("%s\n", __PRETTY_FUNCTION__);
  if (tf->mFont)
    tf->mFont->Release();
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

