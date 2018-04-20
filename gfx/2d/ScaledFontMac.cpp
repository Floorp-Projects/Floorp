/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontMac.h"
#include "UnscaledFontMac.h"
#include "mozilla/webrender/WebRenderTypes.h"
#ifdef USE_SKIA
#include "PathSkia.h"
#include "skia/include/core/SkPaint.h"
#include "skia/include/core/SkPath.h"
#include "skia/include/ports/SkTypeface_mac.h"
#endif
#include <vector>
#include <dlfcn.h>
#ifdef MOZ_WIDGET_UIKIT
#include <CoreFoundation/CoreFoundation.h>
#endif
#include "nsCocoaFeatures.h"
#include "mozilla/gfx/Logging.h"

#ifdef MOZ_WIDGET_COCOA
// prototype for private API
extern "C" {
CGPathRef CGFontGetGlyphPath(CGFontRef fontRef, CGAffineTransform *textTransform, int unknown, CGGlyph glyph);
};
#endif

#ifdef USE_CAIRO_SCALED_FONT
#include "cairo-quartz.h"
#endif

namespace mozilla {
namespace gfx {

// Simple helper class to automatically release a CFObject when it goes out
// of scope.
template<class T>
class AutoRelease
{
public:
  explicit AutoRelease(T aObject)
    : mObject(aObject)
  {
  }

  ~AutoRelease()
  {
    if (mObject) {
      CFRelease(mObject);
    }
  }

  operator T()
  {
    return mObject;
  }

  T forget()
  {
    T obj = mObject;
    mObject = nullptr;
    return obj;
  }

private:
  T mObject;
};

ScaledFontMac::CTFontDrawGlyphsFuncT* ScaledFontMac::CTFontDrawGlyphsPtr = nullptr;
bool ScaledFontMac::sSymbolLookupDone = false;

// Helper to create a CTFont from a CGFont, copying any variations that were
// set on the original CGFont.
static CTFontRef
CreateCTFontFromCGFontWithVariations(CGFontRef aCGFont, CGFloat aSize,
                                     bool aInstalledFont)
{
    // Avoid calling potentially buggy variation APIs on pre-Sierra macOS
    // versions (see bug 1331683).
    //
    // And on HighSierra, CTFontCreateWithGraphicsFont properly carries over
    // variation settings from the CGFont to CTFont, so we don't need to do
    // the extra work here -- and this seems to avoid Core Text crashiness
    // seen in bug 1454094.
    //
    // However, for installed fonts it seems we DO need to copy the variations
    // explicitly even on 10.13, otherwise fonts fail to render (as in bug
    // 1455494) when non-default values are used. Fortunately, the crash
    // mentioned above occurs with data fonts, not (AFAICT) with system-
    // installed fonts.
    //
    // So we only need to do this "the hard way" on Sierra, and for installed
    // fonts on HighSierra+; otherwise, just let the standard CTFont function
    // do its thing.
    //
    // NOTE in case this ever needs further adjustment: there is similar logic
    // in four places in the tree (sadly):
    //    CreateCTFontFromCGFontWithVariations in gfxMacFont.cpp
    //    CreateCTFontFromCGFontWithVariations in ScaledFontMac.cpp
    //    CreateCTFontFromCGFontWithVariations in cairo-quartz-font.c
    //    ctfont_create_exact_copy in SkFontHost_mac.cpp

    CTFontRef ctFont;
    if (nsCocoaFeatures::OnSierraExactly() ||
        (aInstalledFont && nsCocoaFeatures::OnHighSierraOrLater())) {
        CFDictionaryRef vars = CGFontCopyVariations(aCGFont);
        if (vars) {
            CFDictionaryRef varAttr =
                CFDictionaryCreate(nullptr,
                                   (const void**)&kCTFontVariationAttribute,
                                   (const void**)&vars, 1,
                                   &kCFTypeDictionaryKeyCallBacks,
                                   &kCFTypeDictionaryValueCallBacks);
            CFRelease(vars);

            CTFontDescriptorRef varDesc =
                CTFontDescriptorCreateWithAttributes(varAttr);
            CFRelease(varAttr);

            ctFont = CTFontCreateWithGraphicsFont(aCGFont, aSize, nullptr,
                                                  varDesc);
            CFRelease(varDesc);
        } else {
            ctFont = CTFontCreateWithGraphicsFont(aCGFont, aSize, nullptr,
                                                  nullptr);
        }
    } else {
        ctFont = CTFontCreateWithGraphicsFont(aCGFont, aSize, nullptr,
                                              nullptr);
    }
    return ctFont;
}

ScaledFontMac::ScaledFontMac(CGFontRef aFont,
                             const RefPtr<UnscaledFont>& aUnscaledFont,
                             Float aSize,
                             bool aOwnsFont,
                             const Color &aFontSmoothingBackgroundColor,
                             bool aUseFontSmoothing,
                             bool aApplySyntheticBold)
  : ScaledFontBase(aUnscaledFont, aSize)
  , mFont(aFont)
  , mFontSmoothingBackgroundColor(aFontSmoothingBackgroundColor)
  , mUseFontSmoothing(aUseFontSmoothing)
  , mApplySyntheticBold(aApplySyntheticBold)
{
  if (!sSymbolLookupDone) {
    CTFontDrawGlyphsPtr =
      (CTFontDrawGlyphsFuncT*)dlsym(RTLD_DEFAULT, "CTFontDrawGlyphs");
    sSymbolLookupDone = true;
  }

  if (!aOwnsFont) {
    // XXX: should we be taking a reference
    CGFontRetain(aFont);
  }

  if (CTFontDrawGlyphsPtr != nullptr) {
    // only create mCTFont if we're going to be using the CTFontDrawGlyphs API
    auto unscaledMac = static_cast<UnscaledFontMac*>(aUnscaledFont.get());
    bool dataFont = unscaledMac->IsDataFont();
    mCTFont = CreateCTFontFromCGFontWithVariations(aFont, aSize, !dataFont);
  } else {
    mCTFont = nullptr;
  }
}

ScaledFontMac::~ScaledFontMac()
{
  if (mCTFont) {
    CFRelease(mCTFont);
  }
  CGFontRelease(mFont);
}

#ifdef USE_SKIA
SkTypeface* ScaledFontMac::GetSkTypeface()
{
  if (!mTypeface) {
    if (mCTFont) {
      mTypeface = SkCreateTypefaceFromCTFont(mCTFont);
    } else {
      auto unscaledMac = static_cast<UnscaledFontMac*>(GetUnscaledFont().get());
      bool dataFont = unscaledMac->IsDataFont();
      CTFontRef fontFace =
        CreateCTFontFromCGFontWithVariations(mFont, mSize, !dataFont);
      mTypeface = SkCreateTypefaceFromCTFont(fontFace);
      CFRelease(fontFace);
    }
  }
  return mTypeface;
}
#endif

// private API here are the public options on OS X
// CTFontCreatePathForGlyph
// ATSUGlyphGetCubicPaths
// we've used this in cairo sucessfully for some time.
// Note: cairo dlsyms it. We could do that but maybe it's
// safe just to use?

already_AddRefed<Path>
ScaledFontMac::GetPathForGlyphs(const GlyphBuffer &aBuffer, const DrawTarget *aTarget)
{
  return ScaledFontBase::GetPathForGlyphs(aBuffer, aTarget);
}

uint32_t
CalcTableChecksum(const uint32_t *tableStart, uint32_t length, bool skipChecksumAdjust = false)
{
    uint32_t sum = 0L;
    const uint32_t *table = tableStart;
    const uint32_t *end = table + length / sizeof(uint32_t);
    while (table < end) {
        if (skipChecksumAdjust && (table - tableStart) == 2) {
            table++;
        } else {
            sum += CFSwapInt32BigToHost(*table++);
        }
    }

    // The length is not 4-byte aligned, but we still must process the remaining bytes.
    if (length & 3) {
        // Pad with zero before adding to the checksum.
        uint32_t last = 0;
        memcpy(&last, end, length & 3);
        sum += CFSwapInt32BigToHost(last);
    }

    return sum;
}

struct TableRecord {
    uint32_t tag;
    uint32_t checkSum;
    uint32_t offset;
    uint32_t length;
    CFDataRef data;
};

int maxPow2LessThan(int a)
{
    int x = 1;
    int shift = 0;
    while ((x<<(shift+1)) < a) {
        shift++;
    }
    return shift;
}

struct writeBuf
{
    explicit writeBuf(int size)
    {
        this->data = new unsigned char [size];
        this->offset = 0;
    }
    ~writeBuf() {
        delete[] this->data;
    }

    template <class T>
    void writeElement(T a)
    {
        *reinterpret_cast<T*>(&this->data[this->offset]) = a;
        this->offset += sizeof(T);
    }

    void writeMem(const void *data, unsigned long length)
    {
        memcpy(&this->data[this->offset], data, length);
        this->offset += length;
    }

    void align()
    {
        while (this->offset & 3) {
            this->data[this->offset] = 0;
            this->offset++;
        }
    }

    unsigned char *data;
    int offset;
};

bool
UnscaledFontMac::GetFontFileData(FontFileDataOutput aDataCallback, void *aBaton)
{
    // We'll reconstruct a TTF font from the tables we can get from the CGFont
    CFArrayRef tags = CGFontCopyTableTags(mFont);
    CFIndex count = CFArrayGetCount(tags);

    TableRecord *records = new TableRecord[count];
    uint32_t offset = 0;
    offset += sizeof(uint32_t)*3;
    offset += sizeof(uint32_t)*4*count;
    bool CFF = false;
    for (CFIndex i = 0; i<count; i++) {
        uint32_t tag = (uint32_t)(uintptr_t)CFArrayGetValueAtIndex(tags, i);
        if (tag == 0x43464620) // 'CFF '
            CFF = true;
        CFDataRef data = CGFontCopyTableForTag(mFont, tag);
        records[i].tag = tag;
        records[i].offset = offset;
        records[i].data = data;
        records[i].length = CFDataGetLength(data);
        bool skipChecksumAdjust = (tag == 0x68656164); // 'head'
        records[i].checkSum = CalcTableChecksum(reinterpret_cast<const uint32_t*>(CFDataGetBytePtr(data)),
                                                records[i].length, skipChecksumAdjust);
        offset += records[i].length;
        // 32 bit align the tables
        offset = (offset + 3) & ~3;
    }
    CFRelease(tags);

    struct writeBuf buf(offset);
    // write header/offset table
    if (CFF) {
      buf.writeElement(CFSwapInt32HostToBig(0x4f54544f));
    } else {
      buf.writeElement(CFSwapInt32HostToBig(0x00010000));
    }
    buf.writeElement(CFSwapInt16HostToBig(count));
    buf.writeElement(CFSwapInt16HostToBig((1<<maxPow2LessThan(count))*16));
    buf.writeElement(CFSwapInt16HostToBig(maxPow2LessThan(count)));
    buf.writeElement(CFSwapInt16HostToBig(count*16-((1<<maxPow2LessThan(count))*16)));

    // write table record entries
    for (CFIndex i = 0; i<count; i++) {
        buf.writeElement(CFSwapInt32HostToBig(records[i].tag));
        buf.writeElement(CFSwapInt32HostToBig(records[i].checkSum));
        buf.writeElement(CFSwapInt32HostToBig(records[i].offset));
        buf.writeElement(CFSwapInt32HostToBig(records[i].length));
    }

    // write tables
    int checkSumAdjustmentOffset = 0;
    for (CFIndex i = 0; i<count; i++) {
        if (records[i].tag == 0x68656164) {
            checkSumAdjustmentOffset = buf.offset + 2*4;
        }
        buf.writeMem(CFDataGetBytePtr(records[i].data), CFDataGetLength(records[i].data));
        buf.align();
        CFRelease(records[i].data);
    }
    delete[] records;

    // clear the checksumAdjust field before checksumming the whole font
    memset(&buf.data[checkSumAdjustmentOffset], 0, sizeof(uint32_t));
    uint32_t fontChecksum = CFSwapInt32HostToBig(0xb1b0afba - CalcTableChecksum(reinterpret_cast<const uint32_t*>(buf.data), offset));
    // set checkSumAdjust to the computed checksum
    memcpy(&buf.data[checkSumAdjustmentOffset], &fontChecksum, sizeof(fontChecksum));

    // we always use an index of 0
    aDataCallback(buf.data, buf.offset, 0, aBaton);

    return true;
}

bool
UnscaledFontMac::GetWRFontDescriptor(WRFontDescriptorOutput aCb, void* aBaton)
{
  if (mIsDataFont) {
    return false;
  }

  CFStringRef psname = CGFontCopyPostScriptName(mFont);
  if (!psname) {
    return false;
  }

  char buf[256];
  const char* cstr = CFStringGetCStringPtr(psname, kCFStringEncodingUTF8);
  if (!cstr) {
    if (!CFStringGetCString(psname, buf, sizeof(buf), kCFStringEncodingUTF8)) {
      return false;
    }
    cstr = buf;
  }

  aCb(reinterpret_cast<const uint8_t*>(cstr), strlen(cstr), 0, aBaton);
  return true;
}

static void
CollectVariationsFromDictionary(const void* aKey, const void* aValue, void* aContext)
{
  auto keyPtr = static_cast<const CFTypeRef>(aKey);
  auto valuePtr = static_cast<const CFTypeRef>(aValue);
  auto outVariations = static_cast<std::vector<FontVariation>*>(aContext);
  if (CFGetTypeID(keyPtr) == CFNumberGetTypeID() &&
      CFGetTypeID(valuePtr) == CFNumberGetTypeID()) {
    uint64_t t;
    double v;
    if (CFNumberGetValue(static_cast<CFNumberRef>(keyPtr), kCFNumberSInt64Type, &t) &&
        CFNumberGetValue(static_cast<CFNumberRef>(valuePtr), kCFNumberDoubleType, &v)) {
      outVariations->push_back(FontVariation{uint32_t(t), float(v)});
    }
  }
}

static bool
GetVariationsForCTFont(CTFontRef aCTFont, std::vector<FontVariation>* aOutVariations)
{
  // Avoid calling potentially buggy variation APIs on pre-Sierra macOS
  // versions (see bug 1331683)
  if (!nsCocoaFeatures::OnSierraOrLater()) {
    return true;
  }
  if (!aCTFont) {
    return true;
  }
  AutoRelease<CFDictionaryRef> dict(CTFontCopyVariation(aCTFont));
  CFIndex count = dict ? CFDictionaryGetCount(dict) : 0;
  if (count > 0) {
    aOutVariations->reserve(count);
    CFDictionaryApplyFunction(dict, CollectVariationsFromDictionary, aOutVariations);
  }
  return true;
}

bool
ScaledFontMac::GetFontInstanceData(FontInstanceDataOutput aCb, void* aBaton)
{
  // Collect any variation settings that were incorporated into the CTFont.
  std::vector<FontVariation> variations;
  if (!GetVariationsForCTFont(mCTFont, &variations)) {
    return false;
  }
  aCb(nullptr, 0, variations.data(), variations.size(), aBaton);
  return true;
}

bool
ScaledFontMac::GetWRFontInstanceOptions(Maybe<wr::FontInstanceOptions>* aOutOptions,
                                        Maybe<wr::FontInstancePlatformOptions>* aOutPlatformOptions,
                                        std::vector<FontVariation>* aOutVariations)
{
  GetVariationsForCTFont(mCTFont, aOutVariations);

  wr::FontInstanceOptions options;
  options.render_mode = wr::FontRenderMode::Subpixel;
  options.subpx_dir = wr::SubpixelDirection::Horizontal;
  options.flags = 0;
  if (mUseFontSmoothing) {
    options.flags |= wr::FontInstanceFlags::FONT_SMOOTHING;
  }
  if (mApplySyntheticBold) {
    options.flags |= wr::FontInstanceFlags::SYNTHETIC_BOLD;
  }
  options.bg_color = wr::ToColorU(mFontSmoothingBackgroundColor);
  *aOutOptions = Some(options);
  return true;
}

static CFDictionaryRef
CreateVariationDictionaryOrNull(CGFontRef aCGFont, uint32_t aVariationCount,
                                const FontVariation* aVariations)
{
  // Avoid calling potentially buggy variation APIs on pre-Sierra macOS
  // versions (see bug 1331683)
  if (!nsCocoaFeatures::OnSierraOrLater()) {
    return nullptr;
  }

  AutoRelease<CTFontRef>
    ctFont(CTFontCreateWithGraphicsFont(aCGFont, 0, nullptr, nullptr));
  AutoRelease<CFArrayRef> axes(CTFontCopyVariationAxes(ctFont));
  if (!axes) {
    return nullptr;
  }

  CFIndex axisCount = CFArrayGetCount(axes);
  AutoRelease<CFMutableDictionaryRef>
    dict(CFDictionaryCreateMutable(kCFAllocatorDefault, axisCount,
                                   &kCFTypeDictionaryKeyCallBacks,
                                   &kCFTypeDictionaryValueCallBacks));

  // Number of variation settings passed in the aVariations parameter.
  // This will typically be a very low value, so we just linear-search them.
  bool allDefaultValues = true;

  for (CFIndex i = 0; i < axisCount; ++i) {
    // We sanity-check the axis info found in the CTFont, and bail out
    // (returning null) if it doesn't have the expected types.
    CFTypeRef axisInfo = CFArrayGetValueAtIndex(axes, i);
    if (CFDictionaryGetTypeID() != CFGetTypeID(axisInfo)) {
      return nullptr;
    }
    CFDictionaryRef axis = static_cast<CFDictionaryRef>(axisInfo);

    CFTypeRef axisTag =
        CFDictionaryGetValue(axis, kCTFontVariationAxisIdentifierKey);
    if (!axisTag || CFGetTypeID(axisTag) != CFNumberGetTypeID()) {
      return nullptr;
    }
    int64_t tagLong;
    if (!CFNumberGetValue(static_cast<CFNumberRef>(axisTag),
                          kCFNumberSInt64Type, &tagLong)) {
      return nullptr;
    }

    CFTypeRef axisName =
      CFDictionaryGetValue(axis, kCTFontVariationAxisNameKey);
    if (!axisName || CFGetTypeID(axisName) != CFStringGetTypeID()) {
      return nullptr;
    }

    // Clamp axis values to the supported range.
    CFTypeRef min = CFDictionaryGetValue(axis, kCTFontVariationAxisMinimumValueKey);
    CFTypeRef max = CFDictionaryGetValue(axis, kCTFontVariationAxisMaximumValueKey);
    CFTypeRef def = CFDictionaryGetValue(axis, kCTFontVariationAxisDefaultValueKey);
    if (!min || CFGetTypeID(min) != CFNumberGetTypeID() ||
        !max || CFGetTypeID(max) != CFNumberGetTypeID() ||
        !def || CFGetTypeID(def) != CFNumberGetTypeID()) {
      return nullptr;
    }
    double minDouble;
    double maxDouble;
    double defDouble;
    if (!CFNumberGetValue(static_cast<CFNumberRef>(min), kCFNumberDoubleType,
                          &minDouble) ||
        !CFNumberGetValue(static_cast<CFNumberRef>(max), kCFNumberDoubleType,
                          &maxDouble) ||
        !CFNumberGetValue(static_cast<CFNumberRef>(def), kCFNumberDoubleType,
                          &defDouble)) {
      return nullptr;
    }

    double value = defDouble;
    for (uint32_t j = 0; j < aVariationCount; ++j) {
      if (aVariations[j].mTag == tagLong) {
        value = std::min(std::max<double>(aVariations[j].mValue,
                                          minDouble),
                         maxDouble);
        if (value != defDouble) {
          allDefaultValues = false;
        }
        break;
      }
    }
    AutoRelease<CFNumberRef> valueNumber(CFNumberCreate(kCFAllocatorDefault,
                                                        kCFNumberDoubleType,
                                                        &value));
    CFDictionaryAddValue(dict, axisName, valueNumber);
  }

  if (allDefaultValues) {
    // We didn't actually set any non-default values, so throw away the
    // variations dictionary and just use the default rendering.
    return nullptr;
  }

  return dict.forget();
}

CGFontRef
UnscaledFontMac::CreateCGFontWithVariations(CGFontRef aFont,
                                            uint32_t aVariationCount,
                                            const FontVariation* aVariations)
{
  MOZ_ASSERT(aVariationCount > 0);
  MOZ_ASSERT(aVariations);

  AutoRelease<CFDictionaryRef>
    varDict(CreateVariationDictionaryOrNull(aFont, aVariationCount, aVariations));
  if (!varDict) {
    return nullptr;
  }

  return CGFontCreateCopyWithVariations(aFont, varDict);
}

already_AddRefed<ScaledFont>
UnscaledFontMac::CreateScaledFont(Float aGlyphSize,
                                  const uint8_t* aInstanceData,
                                  uint32_t aInstanceDataLength,
                                  const FontVariation* aVariations,
                                  uint32_t aNumVariations)

{
  CGFontRef fontRef = mFont;
  if (aNumVariations > 0) {
    CGFontRef varFont =
      CreateCGFontWithVariations(mFont, aNumVariations, aVariations);
    if (varFont) {
      fontRef = varFont;
    }
  }

  RefPtr<ScaledFontMac> scaledFont =
    new ScaledFontMac(fontRef, this, aGlyphSize, fontRef != mFont);

  if (!scaledFont->PopulateCairoScaledFont()) {
    gfxWarning() << "Unable to create cairo scaled Mac font.";
    return nullptr;
  }

  return scaledFont.forget();
}

#ifdef USE_CAIRO_SCALED_FONT
cairo_font_face_t*
ScaledFontMac::GetCairoFontFace()
{
  MOZ_ASSERT(mFont);
  return cairo_quartz_font_face_create_for_cgfont(mFont);
}
#endif

} // namespace gfx
} // namespace mozilla
