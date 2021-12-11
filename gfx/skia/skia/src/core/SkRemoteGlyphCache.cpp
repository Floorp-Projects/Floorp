/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/core/SkRemoteGlyphCache.h"

#include <iterator>
#include <memory>
#include <new>
#include <string>
#include <tuple>

#include "src/core/SkDevice.h"
#include "src/core/SkDraw.h"
#include "src/core/SkGlyphRun.h"
#include "src/core/SkStrike.h"
#include "src/core/SkStrikeCache.h"
#include "src/core/SkTLazy.h"
#include "src/core/SkTraceEvent.h"
#include "src/core/SkTypeface_remote.h"

#if SK_SUPPORT_GPU
#include "src/gpu/GrDrawOpAtlas.h"
#include "src/gpu/text/GrTextContext.h"
#endif

static SkDescriptor* auto_descriptor_from_desc(const SkDescriptor* source_desc,
                                               SkFontID font_id,
                                               SkAutoDescriptor* ad) {
    ad->reset(source_desc->getLength());
    auto* desc = ad->getDesc();
    desc->init();

    // Rec.
    {
        uint32_t size;
        auto ptr = source_desc->findEntry(kRec_SkDescriptorTag, &size);
        SkScalerContextRec rec;
        memcpy(&rec, ptr, size);
        rec.fFontID = font_id;
        desc->addEntry(kRec_SkDescriptorTag, sizeof(rec), &rec);
    }

    // Effects.
    {
        uint32_t size;
        auto ptr = source_desc->findEntry(kEffects_SkDescriptorTag, &size);
        if (ptr) { desc->addEntry(kEffects_SkDescriptorTag, size, ptr); }
    }

    desc->computeChecksum();
    return desc;
}

static const SkDescriptor* create_descriptor(
        const SkPaint& paint, const SkFont& font, const SkMatrix& m,
        const SkSurfaceProps& props, SkScalerContextFlags flags,
        SkAutoDescriptor* ad, SkScalerContextEffects* effects) {
    SkScalerContextRec rec;
    SkScalerContext::MakeRecAndEffects(font, paint, props, flags, m, &rec, effects);
    return SkScalerContext::AutoDescriptorGivenRecAndEffects(rec, *effects, ad);
}

// -- Serializer -----------------------------------------------------------------------------------
size_t pad(size_t size, size_t alignment) { return (size + (alignment - 1)) & ~(alignment - 1); }

// Alignment between x86 and x64 differs for some types, in particular
// int64_t and doubles have 4 and 8-byte alignment, respectively.
// Be consistent even when writing and reading across different architectures.
template<typename T>
size_t serialization_alignment() {
  return sizeof(T) == 8 ? 8 : alignof(T);
}

class Serializer {
public:
    Serializer(std::vector<uint8_t>* buffer) : fBuffer{buffer} { }

    template <typename T, typename... Args>
    T* emplace(Args&&... args) {
        auto result = allocate(sizeof(T), serialization_alignment<T>());
        return new (result) T{std::forward<Args>(args)...};
    }

    template <typename T>
    void write(const T& data) {
        T* result = (T*)allocate(sizeof(T), serialization_alignment<T>());
        memcpy(result, &data, sizeof(T));
    }

    template <typename T>
    T* allocate() {
        T* result = (T*)allocate(sizeof(T), serialization_alignment<T>());
        return result;
    }

    void writeDescriptor(const SkDescriptor& desc) {
        write(desc.getLength());
        auto result = allocate(desc.getLength(), alignof(SkDescriptor));
        memcpy(result, &desc, desc.getLength());
    }

    void* allocate(size_t size, size_t alignment) {
        size_t aligned = pad(fBuffer->size(), alignment);
        fBuffer->resize(aligned + size);
        return &(*fBuffer)[aligned];
    }

private:
    std::vector<uint8_t>* fBuffer;
};

// -- Deserializer -------------------------------------------------------------------------------
// Note that the Deserializer is reading untrusted data, we need to guard against invalid data.
class Deserializer {
public:
    Deserializer(const volatile char* memory, size_t memorySize)
            : fMemory(memory), fMemorySize(memorySize) {}

    template <typename T>
    bool read(T* val) {
        auto* result = this->ensureAtLeast(sizeof(T), serialization_alignment<T>());
        if (!result) return false;

        memcpy(val, const_cast<const char*>(result), sizeof(T));
        return true;
    }

    bool readDescriptor(SkAutoDescriptor* ad) {
        uint32_t descLength = 0u;
        if (!read<uint32_t>(&descLength)) return false;
        if (descLength < sizeof(SkDescriptor)) return false;
        if (descLength != SkAlign4(descLength)) return false;

        auto* result = this->ensureAtLeast(descLength, alignof(SkDescriptor));
        if (!result) return false;

        ad->reset(descLength);
        memcpy(ad->getDesc(), const_cast<const char*>(result), descLength);

        if (ad->getDesc()->getLength() > descLength) return false;
        return ad->getDesc()->isValid();
    }

    const volatile void* read(size_t size, size_t alignment) {
      return this->ensureAtLeast(size, alignment);
    }

    size_t bytesRead() const { return fBytesRead; }

private:
    const volatile char* ensureAtLeast(size_t size, size_t alignment) {
        size_t padded = pad(fBytesRead, alignment);

        // Not enough data.
        if (padded > fMemorySize) return nullptr;
        if (size > fMemorySize - padded) return nullptr;

        auto* result = fMemory + padded;
        fBytesRead = padded + size;
        return result;
    }

    // Note that we read each piece of memory only once to guard against TOCTOU violations.
    const volatile char* fMemory;
    size_t fMemorySize;
    size_t fBytesRead = 0u;
};

// Paths use a SkWriter32 which requires 4 byte alignment.
static const size_t kPathAlignment  = 4u;

// -- StrikeSpec -----------------------------------------------------------------------------------
struct StrikeSpec {
    StrikeSpec() = default;
    StrikeSpec(SkFontID typefaceID_, SkDiscardableHandleId discardableHandleId_)
            : typefaceID{typefaceID_}, discardableHandleId(discardableHandleId_) {}
    SkFontID typefaceID = 0u;
    SkDiscardableHandleId discardableHandleId = 0u;
    /* desc */
    /* n X (glyphs ids) */
};

// -- RemoteStrike ----------------------------------------------------------------------------
class SkStrikeServer::RemoteStrike : public SkStrikeForGPU {
public:
    // N.B. RemoteStrike is not valid until ensureScalerContext is called.
    RemoteStrike(const SkDescriptor& descriptor,
                 std::unique_ptr<SkScalerContext> context,
                 SkDiscardableHandleId discardableHandleId);
    ~RemoteStrike() override;

    void addGlyph(SkPackedGlyphID, bool asPath);
    void writePendingGlyphs(Serializer* serializer);
    SkDiscardableHandleId discardableHandleId() const { return fDiscardableHandleId; }

    const SkDescriptor& getDescriptor() const override {
        return *fDescriptor.getDesc();
    }

    void setTypefaceAndEffects(const SkTypeface* typeface, SkScalerContextEffects effects);

    const SkGlyphPositionRoundingSpec& roundingSpec() const override {
        return fRoundingSpec;
    }

    SkSpan<const SkGlyphPos>
    prepareForDrawingRemoveEmpty(
            const SkPackedGlyphID packedGlyphIDs[],
            const SkPoint positions[], size_t n,
            int maxDimension,
            SkGlyphPos results[]) override;

    void onAboutToExitScope() override {}

    bool hasPendingGlyphs() const {
        return !fPendingGlyphImages.empty() || !fPendingGlyphPaths.empty();
    }

    void resetScalerContext();

private:
    void writeGlyphPath(const SkPackedGlyphID& glyphID, Serializer* serializer) const;

    void ensureScalerContext();

    // The set of glyphs cached on the remote client.
    SkTHashSet<SkPackedGlyphID> fCachedGlyphImages;
    SkTHashSet<SkPackedGlyphID> fCachedGlyphPaths;

    // The set of glyphs which has not yet been serialized and sent to the
    // remote client.
    std::vector<SkPackedGlyphID> fPendingGlyphImages;
    std::vector<SkPackedGlyphID> fPendingGlyphPaths;

    const SkAutoDescriptor fDescriptor;

    const SkDiscardableHandleId fDiscardableHandleId;

    const SkGlyphPositionRoundingSpec fRoundingSpec;

    // The context built using fDescriptor
    std::unique_ptr<SkScalerContext> fContext;

    // These fields are set every time getOrCreateCache. This allows the code to maintain the
    // fContext as lazy as possible.
    const SkTypeface* fTypeface{nullptr};
    SkScalerContextEffects fEffects;

    bool fHaveSentFontMetrics{false};

    class GlyphMapHashTraits {
    public:
        static SkPackedGlyphID GetKey(const SkGlyph* glyph) {
            return glyph->getPackedID();
        }
        static uint32_t Hash(SkPackedGlyphID glyphId) {
            return glyphId.hash();
        }
    };

    // FallbackTextHelper cases require glyph metrics when analyzing a glyph run, in which case
    // we cache them here.
    SkTHashTable<SkGlyph*, SkPackedGlyphID, GlyphMapHashTraits> fGlyphMap;

    SkArenaAlloc fAlloc{256};
};

SkStrikeServer::RemoteStrike::RemoteStrike(
        const SkDescriptor& descriptor,
        std::unique_ptr<SkScalerContext> context,
        uint32_t discardableHandleId)
        : fDescriptor{descriptor}
        , fDiscardableHandleId(discardableHandleId)
        , fRoundingSpec{context->isSubpixel(), context->computeAxisAlignmentForHText()}
        // N.B. context must come last because it is used above.
        , fContext{std::move(context)} {
    SkASSERT(fDescriptor.getDesc() != nullptr);
    SkASSERT(fContext != nullptr);
}

SkStrikeServer::RemoteStrike::~RemoteStrike() = default;

void SkStrikeServer::RemoteStrike::addGlyph(SkPackedGlyphID glyph, bool asPath) {
    auto* cache = asPath ? &fCachedGlyphPaths : &fCachedGlyphImages;
    auto* pending = asPath ? &fPendingGlyphPaths : &fPendingGlyphImages;

    // Already cached.
    if (cache->contains(glyph)) {
        return;
    }

    // A glyph is going to be sent. Make sure we have a scaler context to send it.
    this->ensureScalerContext();

    // Serialize and cache. Also create the scalar context to use when serializing
    // this glyph.
    cache->add(glyph);
    pending->push_back(glyph);
}

size_t SkStrikeServer::MapOps::operator()(const SkDescriptor* key) const {
    return key->getChecksum();
}

bool SkStrikeServer::MapOps::operator()(const SkDescriptor* lhs, const SkDescriptor* rhs) const {
    return *lhs == *rhs;
}


// -- TrackLayerDevice -----------------------------------------------------------------------------
class SkTextBlobCacheDiffCanvas::TrackLayerDevice final : public SkNoPixelsDevice {
public:
    TrackLayerDevice(
            const SkIRect& bounds, const SkSurfaceProps& props, SkStrikeServer* server,
            sk_sp<SkColorSpace> colorSpace, bool DFTSupport)
            : SkNoPixelsDevice(bounds, props, std::move(colorSpace))
            , fStrikeServer(server)
            , fDFTSupport(DFTSupport)
            , fPainter{props, kUnknown_SkColorType, imageInfo().colorSpace(), fStrikeServer} {
        SkASSERT(fStrikeServer != nullptr);
    }

    SkBaseDevice* onCreateDevice(const CreateInfo& cinfo, const SkPaint*) override {
        const SkSurfaceProps surfaceProps(this->surfaceProps().flags(), cinfo.fPixelGeometry);
        return new TrackLayerDevice(this->getGlobalBounds(), surfaceProps, fStrikeServer,
                                    cinfo.fInfo.refColorSpace(), fDFTSupport);
    }

protected:
    void drawGlyphRunList(const SkGlyphRunList& glyphRunList) override {
#if SK_SUPPORT_GPU
        GrTextContext::Options options;
        GrTextContext::SanitizeOptions(&options);

        fPainter.processGlyphRunList(glyphRunList,
                                     this->ctm(),
                                     this->surfaceProps(),
                                     fDFTSupport,
                                     options,
                                     nullptr);
#endif  // SK_SUPPORT_GPU
    }

private:
    SkStrikeServer* const fStrikeServer;
    const bool fDFTSupport{false};
    SkGlyphRunListPainter fPainter;
};

// -- SkTextBlobCacheDiffCanvas -------------------------------------------------------------------
SkTextBlobCacheDiffCanvas::SkTextBlobCacheDiffCanvas(int width, int height,
                                                     const SkSurfaceProps& props,
                                                     SkStrikeServer* strikeServer,
                                                     bool DFTSupport)
    : SkTextBlobCacheDiffCanvas{width, height, props, strikeServer, nullptr, DFTSupport} { }

SkTextBlobCacheDiffCanvas::SkTextBlobCacheDiffCanvas(int width, int height,
                                                     const SkSurfaceProps& props,
                                                     SkStrikeServer* strikeServer,
                                                     sk_sp<SkColorSpace> colorSpace,
                                                     bool DFTSupport)
     : SkNoDrawCanvas{sk_make_sp<TrackLayerDevice>(SkIRect::MakeWH(width, height),
                                                   props,
                                                   strikeServer,
                                                   std::move(colorSpace),
                                                   DFTSupport)} { }

SkTextBlobCacheDiffCanvas::~SkTextBlobCacheDiffCanvas() = default;

SkCanvas::SaveLayerStrategy SkTextBlobCacheDiffCanvas::getSaveLayerStrategy(
        const SaveLayerRec& rec) {
    return kFullLayer_SaveLayerStrategy;
}

bool SkTextBlobCacheDiffCanvas::onDoSaveBehind(const SkRect*) {
    return false;
}

void SkTextBlobCacheDiffCanvas::onDrawTextBlob(const SkTextBlob* blob, SkScalar x, SkScalar y,
                                               const SkPaint& paint) {
    SkCanvas::onDrawTextBlob(blob, x, y, paint);
}

// -- WireTypeface ---------------------------------------------------------------------------------
struct WireTypeface {
    WireTypeface() = default;
    WireTypeface(SkFontID typeface_id, int glyph_count, SkFontStyle style, bool is_fixed)
            : typefaceID(typeface_id), glyphCount(glyph_count), style(style), isFixed(is_fixed) {}

    SkFontID        typefaceID{0};
    int             glyphCount{0};
    SkFontStyle     style;
    bool            isFixed{false};
};

// SkStrikeServer ----------------------------------------------------------------------------------
SkStrikeServer::SkStrikeServer(DiscardableHandleManager* discardableHandleManager)
        : fDiscardableHandleManager(discardableHandleManager) {
    SkASSERT(fDiscardableHandleManager);
}

SkStrikeServer::~SkStrikeServer() = default;

sk_sp<SkData> SkStrikeServer::serializeTypeface(SkTypeface* tf) {
    auto* data = fSerializedTypefaces.find(SkTypeface::UniqueID(tf));
    if (data) {
        return *data;
    }

    WireTypeface wire(SkTypeface::UniqueID(tf), tf->countGlyphs(), tf->fontStyle(),
                      tf->isFixedPitch());
    data = fSerializedTypefaces.set(SkTypeface::UniqueID(tf),
                                    SkData::MakeWithCopy(&wire, sizeof(wire)));
    return *data;
}

void SkStrikeServer::writeStrikeData(std::vector<uint8_t>* memory) {
    size_t strikesToSend = 0;
    fRemoteStrikesToSend.foreach ([&strikesToSend](RemoteStrike* strike) {
        if (strike->hasPendingGlyphs()) {
            strikesToSend++;
        } else {
            strike->resetScalerContext();
        }
    });

    if (strikesToSend == 0 && fTypefacesToSend.empty()) {
        fRemoteStrikesToSend.reset();
        return;
    }

    Serializer serializer(memory);
    serializer.emplace<uint64_t>(fTypefacesToSend.size());
    for (const auto& tf : fTypefacesToSend) {
        serializer.write<WireTypeface>(tf);
    }
    fTypefacesToSend.clear();

    serializer.emplace<uint64_t>(SkTo<uint64_t>(strikesToSend));
    fRemoteStrikesToSend.foreach (
#ifdef SK_DEBUG
            [&serializer, this](RemoteStrike* strike) {
                if (strike->hasPendingGlyphs()) {
                    strike->writePendingGlyphs(&serializer);
                    strike->resetScalerContext();
                }
                auto it = fDescToRemoteStrike.find(&strike->getDescriptor());
                SkASSERT(it != fDescToRemoteStrike.end());
                SkASSERT(it->second.get() == strike);
            }

#else
            [&serializer](RemoteStrike* strike) {
                if (strike->hasPendingGlyphs()) {
                    strike->writePendingGlyphs(&serializer);
                    strike->resetScalerContext();
                }
            }
#endif
    );
    fRemoteStrikesToSend.reset();
}

SkStrikeServer::RemoteStrike* SkStrikeServer::getOrCreateCache(
        const SkPaint& paint,
        const SkFont& font,
        const SkSurfaceProps& props,
        const SkMatrix& matrix,
        SkScalerContextFlags flags,
        SkScalerContextEffects* effects) {
    SkAutoDescriptor descStorage;
    auto desc = create_descriptor(paint, font, matrix, props, flags, &descStorage, effects);

    return this->getOrCreateCache(*desc, *font.getTypefaceOrDefault(), *effects);
}

SkScopedStrikeForGPU SkStrikeServer::findOrCreateScopedStrike(const SkDescriptor& desc,
                                                              const SkScalerContextEffects& effects,
                                                              const SkTypeface& typeface) {
    return SkScopedStrikeForGPU{this->getOrCreateCache(desc, typeface, effects)};
}

void SkStrikeServer::AddGlyphForTesting(
        RemoteStrike* cache, SkPackedGlyphID glyphID, bool asPath) {
    cache->addGlyph(glyphID, asPath);
}

void SkStrikeServer::checkForDeletedEntries() {
    auto it = fDescToRemoteStrike.begin();
    while (fDescToRemoteStrike.size() > fMaxEntriesInDescriptorMap &&
           it != fDescToRemoteStrike.end()) {
        RemoteStrike* strike = it->second.get();
        if (fDiscardableHandleManager->isHandleDeleted(strike->discardableHandleId())) {
            // If we are removing the strike, we better not be trying to send it at the same time.
            SkASSERT(!fRemoteStrikesToSend.contains(strike));
            it = fDescToRemoteStrike.erase(it);
        } else {
            ++it;
        }
    }
}

SkStrikeServer::RemoteStrike* SkStrikeServer::getOrCreateCache(
        const SkDescriptor& desc, const SkTypeface& typeface, SkScalerContextEffects effects) {

    // In cases where tracing is turned off, make sure not to get an unused function warning.
    // Lambdaize the function.
    TRACE_EVENT1("skia", "RecForDesc", "rec",
            TRACE_STR_COPY(
                    [&desc](){
                        auto ptr = desc.findEntry(kRec_SkDescriptorTag, nullptr);
                        SkScalerContextRec rec;
                        std::memcpy(&rec, ptr, sizeof(rec));
                        return rec.dump();
                    }().c_str()
            )
    );

    auto it = fDescToRemoteStrike.find(&desc);
    if (it != fDescToRemoteStrike.end()) {
        // We have processed the RemoteStrike before. Reuse it.
        RemoteStrike* strike = it->second.get();
        strike->setTypefaceAndEffects(&typeface, effects);
        if (fRemoteStrikesToSend.contains(strike)) {
            // Already tracking
            return strike;
        }

        // Strike is in unknown state on GPU. Start tracking strike on GPU by locking it.
        bool locked = fDiscardableHandleManager->lockHandle(it->second->discardableHandleId());
        if (locked) {
            fRemoteStrikesToSend.add(strike);
            return strike;
        }

        fDescToRemoteStrike.erase(it);
    }

    // Create a new RemoteStrike. Start by processing the typeface.
    const SkFontID typefaceId = typeface.uniqueID();
    if (!fCachedTypefaces.contains(typefaceId)) {
        fCachedTypefaces.add(typefaceId);
        fTypefacesToSend.emplace_back(typefaceId, typeface.countGlyphs(),
                                      typeface.fontStyle(),
                                      typeface.isFixedPitch());
    }

    auto context = typeface.createScalerContext(effects, &desc);
    auto newHandle = fDiscardableHandleManager->createHandle();  // Locked on creation
    auto remoteStrike = skstd::make_unique<RemoteStrike>(desc, std::move(context), newHandle);
    remoteStrike->setTypefaceAndEffects(&typeface, effects);
    auto remoteStrikePtr = remoteStrike.get();
    fRemoteStrikesToSend.add(remoteStrikePtr);
    auto d = &remoteStrike->getDescriptor();
    fDescToRemoteStrike[d] = std::move(remoteStrike);

    checkForDeletedEntries();

    // Be sure we can build glyphs with this RemoteStrike.
    remoteStrikePtr->setTypefaceAndEffects(&typeface, effects);
    return remoteStrikePtr;
}

// No need to write fForceBW because it is a flag private to SkScalerContext_DW, which will never
// be called on the GPU side.
static void writeGlyph(SkGlyph* glyph, Serializer* serializer) {
    serializer->write<SkPackedGlyphID>(glyph->getPackedID());
    serializer->write<float>(glyph->advanceX());
    serializer->write<float>(glyph->advanceY());
    serializer->write<uint16_t>(glyph->width());
    serializer->write<uint16_t>(glyph->height());
    serializer->write<int16_t>(glyph->top());
    serializer->write<int16_t>(glyph->left());
    serializer->write<uint8_t>(glyph->maskFormat());
}

void SkStrikeServer::RemoteStrike::writePendingGlyphs(Serializer* serializer) {
    SkASSERT(this->hasPendingGlyphs());

    // Write the desc.
    serializer->emplace<StrikeSpec>(fContext->getTypeface()->uniqueID(), fDiscardableHandleId);
    serializer->writeDescriptor(*fDescriptor.getDesc());

    serializer->emplace<bool>(fHaveSentFontMetrics);
    if (!fHaveSentFontMetrics) {
        // Write FontMetrics if not sent before.
        SkFontMetrics fontMetrics;
        fContext->getFontMetrics(&fontMetrics);
        serializer->write<SkFontMetrics>(fontMetrics);
        fHaveSentFontMetrics = true;
    }

    // Write glyphs images.
    serializer->emplace<uint64_t>(fPendingGlyphImages.size());
    for (const auto& glyphID : fPendingGlyphImages) {
        SkGlyph glyph{glyphID};
        fContext->getMetrics(&glyph);
        SkASSERT(SkMask::IsValidFormat(glyph.fMaskFormat));

        writeGlyph(&glyph, serializer);
        auto imageSize = glyph.imageSize();
        if (imageSize == 0u) continue;

        glyph.fImage = serializer->allocate(imageSize, glyph.formatAlignment());
        fContext->getImage(glyph);
        // TODO: Generating the image can change the mask format, do we need to update it in the
        // serialized glyph?
    }
    fPendingGlyphImages.clear();

    // Write glyphs paths.
    serializer->emplace<uint64_t>(fPendingGlyphPaths.size());
    for (const auto& glyphID : fPendingGlyphPaths) {
        SkGlyph glyph{glyphID};
        fContext->getMetrics(&glyph);
        SkASSERT(SkMask::IsValidFormat(glyph.fMaskFormat));

        writeGlyph(&glyph, serializer);
        writeGlyphPath(glyphID, serializer);
    }
    fPendingGlyphPaths.clear();
}

void SkStrikeServer::RemoteStrike::ensureScalerContext() {
    if (fContext == nullptr) {
        fContext = fTypeface->createScalerContext(fEffects, fDescriptor.getDesc());
    }
}

void SkStrikeServer::RemoteStrike::resetScalerContext() {
    fContext.reset();
    fTypeface = nullptr;
}

void SkStrikeServer::RemoteStrike::setTypefaceAndEffects(
        const SkTypeface* typeface, SkScalerContextEffects effects) {
    fTypeface = typeface;
    fEffects = effects;
}

void SkStrikeServer::RemoteStrike::writeGlyphPath(const SkPackedGlyphID& glyphID,
                                                  Serializer* serializer) const {
    SkPath path;
    if (!fContext->getPath(glyphID, &path)) {
        serializer->write<uint64_t>(0u);
        return;
    }

    size_t pathSize = path.writeToMemory(nullptr);
    serializer->write<uint64_t>(pathSize);
    path.writeToMemory(serializer->allocate(pathSize, kPathAlignment));
}


// Be sure to read and understand the comment for prepareForDrawingRemoveEmpty in
// SkStrikeForGPU.h before working on this code.
SkSpan<const SkGlyphPos>
SkStrikeServer::RemoteStrike::prepareForDrawingRemoveEmpty(
        const SkPackedGlyphID packedGlyphIDs[],
        const SkPoint positions[], size_t n,
        int maxDimension,
        SkGlyphPos results[]) {
    size_t drawableGlyphCount = 0;
    for (size_t i = 0; i < n; i++) {
        SkPoint glyphPos = positions[i];

        // Check the cache for the glyph.
        SkGlyph* glyphPtr = fGlyphMap.findOrNull(packedGlyphIDs[i]);

        // Has this glyph ever been seen before?
        if (glyphPtr == nullptr) {

            // Never seen before. Make a new glyph.
            glyphPtr = fAlloc.make<SkGlyph>(packedGlyphIDs[i]);
            fGlyphMap.set(glyphPtr);
            this->ensureScalerContext();
            fContext->getMetrics(glyphPtr);

            if (glyphPtr->maxDimension() <= maxDimension) {
                // do nothing
            } else if (!glyphPtr->isColor()) {
                // The glyph is too big for the atlas, but it is not color, so it is handled
                // with a path.
                if (glyphPtr->setPath(&fAlloc, fContext.get())) {
                    // Always send the path data, even if its not available, to make sure empty
                    // paths are not incorrectly assumed to be cache misses.
                    fCachedGlyphPaths.add(glyphPtr->getPackedID());
                    fPendingGlyphPaths.push_back(glyphPtr->getPackedID());
                }
            } else {
                // This will be handled by the fallback strike.
                SkASSERT(glyphPtr->maxDimension() > maxDimension && glyphPtr->isColor());
            }

            // Make sure to send the glyph to the GPU because we always send the image for a glyph.
            fCachedGlyphImages.add(packedGlyphIDs[i]);
            fPendingGlyphImages.push_back(packedGlyphIDs[i]);
        }

        // Each non-empty glyph needs to be added as per the contract for
        // prepareForDrawingRemoveEmpty.
        // TODO(herb): Change the code to only send the glyphs for fallback?
        if (!glyphPtr->isEmpty()) {
            results[drawableGlyphCount++] = {i, glyphPtr, glyphPos};
        }
    }
    return SkMakeSpan(results, drawableGlyphCount);
}

// SkStrikeClient ----------------------------------------------------------------------------------
class SkStrikeClient::DiscardableStrikePinner : public SkStrikePinner {
public:
    DiscardableStrikePinner(SkDiscardableHandleId discardableHandleId,
                            sk_sp<DiscardableHandleManager> manager)
            : fDiscardableHandleId(discardableHandleId), fManager(std::move(manager)) {}

    ~DiscardableStrikePinner() override = default;
    bool canDelete() override { return fManager->deleteHandle(fDiscardableHandleId); }

private:
    const SkDiscardableHandleId fDiscardableHandleId;
    sk_sp<DiscardableHandleManager> fManager;
};

SkStrikeClient::SkStrikeClient(sk_sp<DiscardableHandleManager> discardableManager,
                               bool isLogging,
                               SkStrikeCache* strikeCache)
        : fDiscardableHandleManager(std::move(discardableManager))
        , fStrikeCache{strikeCache ? strikeCache : SkStrikeCache::GlobalStrikeCache()}
        , fIsLogging{isLogging} {}

SkStrikeClient::~SkStrikeClient() = default;

#define READ_FAILURE                                                     \
    {                                                                    \
        SkDebugf("Bad font data serialization line: %d", __LINE__);      \
        DiscardableHandleManager::ReadFailureData data = {               \
                memorySize,  deserializer.bytesRead(), typefaceSize,     \
                strikeCount, glyphImagesCount,         glyphPathsCount}; \
        fDiscardableHandleManager->notifyReadFailure(data);              \
        return false;                                                    \
    }

// No need to read fForceBW because it is a flag private to SkScalerContext_DW, which will never
// be called on the GPU side.
bool SkStrikeClient::ReadGlyph(SkTLazy<SkGlyph>& glyph, Deserializer* deserializer) {
    SkPackedGlyphID glyphID;
    if (!deserializer->read<SkPackedGlyphID>(&glyphID)) return false;
    glyph.init(glyphID);
    if (!deserializer->read<float>(&glyph->fAdvanceX)) return false;
    if (!deserializer->read<float>(&glyph->fAdvanceY)) return false;
    if (!deserializer->read<uint16_t>(&glyph->fWidth)) return false;
    if (!deserializer->read<uint16_t>(&glyph->fHeight)) return false;
    if (!deserializer->read<int16_t>(&glyph->fTop)) return false;
    if (!deserializer->read<int16_t>(&glyph->fLeft)) return false;
    if (!deserializer->read<uint8_t>(&glyph->fMaskFormat)) return false;
    if (!SkMask::IsValidFormat(glyph->fMaskFormat)) return false;

    return true;
}

bool SkStrikeClient::readStrikeData(const volatile void* memory, size_t memorySize) {
    SkASSERT(memorySize != 0u);
    Deserializer deserializer(static_cast<const volatile char*>(memory), memorySize);

    uint64_t typefaceSize = 0u;
    uint64_t strikeCount = 0u;
    uint64_t glyphImagesCount = 0u;
    uint64_t glyphPathsCount = 0u;

    if (!deserializer.read<uint64_t>(&typefaceSize)) READ_FAILURE

    for (size_t i = 0; i < typefaceSize; ++i) {
        WireTypeface wire;
        if (!deserializer.read<WireTypeface>(&wire)) READ_FAILURE

        // TODO(khushalsagar): The typeface no longer needs a reference to the
        // SkStrikeClient, since all needed glyphs must have been pushed before
        // raster.
        addTypeface(wire);
    }

    if (!deserializer.read<uint64_t>(&strikeCount)) READ_FAILURE

    for (size_t i = 0; i < strikeCount; ++i) {
        StrikeSpec spec;
        if (!deserializer.read<StrikeSpec>(&spec)) READ_FAILURE

        SkAutoDescriptor sourceAd;
        if (!deserializer.readDescriptor(&sourceAd)) READ_FAILURE

        bool fontMetricsInitialized;
        if (!deserializer.read(&fontMetricsInitialized)) READ_FAILURE

        SkFontMetrics fontMetrics{};
        if (!fontMetricsInitialized) {
            if (!deserializer.read<SkFontMetrics>(&fontMetrics)) READ_FAILURE
        }

        // Get the local typeface from remote fontID.
        auto* tfPtr = fRemoteFontIdToTypeface.find(spec.typefaceID);
        // Received strikes for a typeface which doesn't exist.
        if (!tfPtr) READ_FAILURE
        auto* tf = tfPtr->get();

        // Replace the ContextRec in the desc from the server to create the client
        // side descriptor.
        // TODO: Can we do this in-place and re-compute checksum? Instead of a complete copy.
        SkAutoDescriptor ad;
        auto* client_desc = auto_descriptor_from_desc(sourceAd.getDesc(), tf->uniqueID(), &ad);

        auto strike = fStrikeCache->findStrikeExclusive(*client_desc);
        // Metrics are only sent the first time. If the metrics are not initialized, there must
        // be an existing strike.
        if (fontMetricsInitialized && strike == nullptr) READ_FAILURE
        if (strike == nullptr) {
            // Note that we don't need to deserialize the effects since we won't be generating any
            // glyphs here anyway, and the desc is still correct since it includes the serialized
            // effects.
            SkScalerContextEffects effects;
            auto scaler = SkStrikeCache::CreateScalerContext(*client_desc, effects, *tf);
            strike = fStrikeCache->createStrikeExclusive(
                    *client_desc, std::move(scaler), &fontMetrics,
                    skstd::make_unique<DiscardableStrikePinner>(spec.discardableHandleId,
                                                                fDiscardableHandleManager));
            auto proxyContext = static_cast<SkScalerContextProxy*>(strike->getScalerContext());
            proxyContext->initCache(strike.get(), fStrikeCache);
        }

        if (!deserializer.read<uint64_t>(&glyphImagesCount)) READ_FAILURE
        for (size_t j = 0; j < glyphImagesCount; j++) {
            SkTLazy<SkGlyph> glyph;
            if (!ReadGlyph(glyph, &deserializer)) READ_FAILURE

            if (!glyph->isEmpty()) {
                const volatile void* image =
                        deserializer.read(glyph->imageSize(), glyph->formatAlignment());
                if (!image) READ_FAILURE
                glyph->fImage = (void*)image;
            }

            strike->mergeGlyphAndImage(glyph->getPackedID(), *glyph);
        }

        if (!deserializer.read<uint64_t>(&glyphPathsCount)) READ_FAILURE
        for (size_t j = 0; j < glyphPathsCount; j++) {
            SkTLazy<SkGlyph> glyph;
            if (!ReadGlyph(glyph, &deserializer)) READ_FAILURE

            SkGlyph* allocatedGlyph = strike->mergeGlyphAndImage(glyph->getPackedID(), *glyph);

            SkPath* pathPtr = nullptr;
            SkPath path;
            uint64_t pathSize = 0u;
            if (!deserializer.read<uint64_t>(&pathSize)) READ_FAILURE

            if (pathSize > 0) {
                auto* pathData = deserializer.read(pathSize, kPathAlignment);
                if (!pathData) READ_FAILURE
                if (!path.readFromMemory(const_cast<const void*>(pathData), pathSize)) READ_FAILURE
                pathPtr = &path;
            }

            strike->preparePath(allocatedGlyph, pathPtr);
        }
    }

    return true;
}

sk_sp<SkTypeface> SkStrikeClient::deserializeTypeface(const void* buf, size_t len) {
    WireTypeface wire;
    if (len != sizeof(wire)) return nullptr;
    memcpy(&wire, buf, sizeof(wire));
    return this->addTypeface(wire);
}

sk_sp<SkTypeface> SkStrikeClient::addTypeface(const WireTypeface& wire) {
    auto* typeface = fRemoteFontIdToTypeface.find(wire.typefaceID);
    if (typeface) return *typeface;

    auto newTypeface = sk_make_sp<SkTypefaceProxy>(
            wire.typefaceID, wire.glyphCount, wire.style, wire.isFixed,
            fDiscardableHandleManager, fIsLogging);
    fRemoteFontIdToTypeface.set(wire.typefaceID, newTypeface);
    return newTypeface;
}
