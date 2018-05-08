/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Xdr_h
#define vm_Xdr_h

#include "mozilla/EndianUtils.h"
#include "mozilla/TypeTraits.h"

#include "jsfriendapi.h"
#include "NamespaceImports.h"

#include "js/TypeDecls.h"
#include "vm/JSAtom.h"

namespace js {

class LifoAlloc;

enum XDRMode {
    XDR_ENCODE,
    XDR_DECODE
};

using XDRResult = mozilla::Result<mozilla::Ok, JS::TranscodeResult>;

class XDRBufferBase
{
  public:
    explicit XDRBufferBase(JSContext* cx, size_t cursor = 0)
      : context_(cx), cursor_(cursor)
#ifdef DEBUG
        // Note, when decoding the buffer can be set to a range, which does not
        // have any alignment requirement as opposed to allocations.
      , aligned_(false)
#endif
    { }

    JSContext* cx() const {
        return context_;
    }

    size_t cursor() const {
        return cursor_;
    }

#ifdef DEBUG
    // This function records if the cursor got changed by codeAlign or by any
    // other read/write of data. This is used for AutoXDRTree assertions, as a
    // way to ensure that the last thing done is properly setting the alignment
    // with codeAlign function.
    void setAligned(bool aligned) { aligned_ = aligned; }
    bool isAligned() const { return aligned_; }
#else
    void setAligned(bool) const {}
    bool isAligned() const { return true; }
#endif

  protected:
    JSContext* const context_;
    size_t cursor_;
#ifdef DEBUG
    bool aligned_;
#endif
};

template <XDRMode mode>
class XDRBuffer;

template <>
class XDRBuffer<XDR_ENCODE> : public XDRBufferBase
{
  public:
    XDRBuffer(JSContext* cx, JS::TranscodeBuffer& buffer, size_t cursor = 0)
      : XDRBufferBase(cx, cursor),
        buffer_(buffer) { }

    uint8_t* write(size_t n) {
        MOZ_ASSERT(n != 0);
        setAligned(false);
        if (!buffer_.growByUninitialized(n)) {
            ReportOutOfMemory(cx());
            return nullptr;
        }
        uint8_t* ptr = &buffer_[cursor_];
        cursor_ += n;
        return ptr;
    }

    const uint8_t* read(size_t n) {
        MOZ_CRASH("Should never read in encode mode");
        return nullptr;
    }

    uintptr_t uptr() const {
        // Note: Avoid bounds check assertion if the buffer is not yet allocated.
        return reinterpret_cast<uintptr_t>(buffer_.begin() + cursor_);
    }

  private:
    JS::TranscodeBuffer& buffer_;
};

template <>
class XDRBuffer<XDR_DECODE> : public XDRBufferBase
{
  public:
    XDRBuffer(JSContext* cx, const JS::TranscodeRange& range)
      : XDRBufferBase(cx),
        buffer_(range) { }

    XDRBuffer(JSContext* cx, JS::TranscodeBuffer& buffer, size_t cursor = 0)
      : XDRBufferBase(cx, cursor),
        buffer_(buffer.begin(), buffer.length()) { }

    const uint8_t* read(size_t n) {
        MOZ_ASSERT(cursor_ < buffer_.length());
        setAligned(false);
        uint8_t* ptr = &buffer_[cursor_];
        cursor_ += n;

        // Don't let buggy code read past our buffer
        if (cursor_ > buffer_.length())
            return nullptr;

        return ptr;
    }

    uint8_t* write(size_t n) {
        MOZ_CRASH("Should never write in decode mode");
        return nullptr;
    }

    uintptr_t uptr() const {
        // Note: Avoid bounds check assertion at the end of the buffer.
        return reinterpret_cast<uintptr_t>(buffer_.begin().get() + cursor_);
    }

  private:
    const JS::TranscodeRange buffer_;
};

class XDRCoderBase;
class XDRIncrementalEncoder;
using XDRAlignment = char16_t;
static const uint8_t AlignPadding[sizeof(XDRAlignment)] = { 0, 0 };

// An AutoXDRTree is used to identify section encoded by an XDRIncrementalEncoder.
//
// Its primary goal is to identify functions, such that we can first encode them
// as LazyScript, and later replaced by them by their corresponding bytecode
// once delazified.
//
// As a convenience, this is also used to identify the top-level of the content
// encoded by an XDRIncrementalEncoder.
//
// Sections can be encoded any number of times in an XDRIncrementalEncoder, and
// the latest encoded version would replace all the previous one.
class MOZ_RAII AutoXDRTree
{
  public:
    // For a JSFunction, a tree key is defined as being:
    //     script()->begin << 32 | script()->end
    //
    // Based on the invariant that |begin <= end|, we can make special
    // keys, such as the top-level script.
    using Key = uint64_t;

    AutoXDRTree(XDRCoderBase* xdr, Key key);
    ~AutoXDRTree();

    // Indicate the lack of a key for the current tree.
    static constexpr Key noKey = 0;

    // Used to end the slices when there is no children.
    static constexpr Key noSubTree = Key(1) << 32;

    // Used as the root key of the tree in the hash map.
    static constexpr Key topLevel = Key(2) << 32;

  private:
    friend class XDRIncrementalEncoder;

    Key key_;
    AutoXDRTree* parent_;
    XDRCoderBase* xdr_;
};

class XDRCoderBase
{
  private:
#ifdef DEBUG
    JS::TranscodeResult resultCode_;
#endif

  protected:
    XDRCoderBase()
#ifdef DEBUG
      : resultCode_(JS::TranscodeResult_Ok)
#endif
    {}

  public:
    virtual AutoXDRTree::Key getTopLevelTreeKey() const { return AutoXDRTree::noKey; }
    virtual AutoXDRTree::Key getTreeKey(JSFunction* fun) const { return AutoXDRTree::noKey; }
    virtual void createOrReplaceSubTree(AutoXDRTree* child) {};
    virtual void endSubTree() {};
    virtual bool isAligned(size_t n) = 0;

#ifdef DEBUG
    // Record logical failures of XDR.
    JS::TranscodeResult resultCode() const {
        return resultCode_;
    }
    void setResultCode(JS::TranscodeResult code) {
        MOZ_ASSERT(resultCode() == JS::TranscodeResult_Ok);
        resultCode_ = code;
    }
    bool validateResultCode(JSContext* cx, JS::TranscodeResult code) const;
#endif
};

/*
 * XDR serialization state.  All data is encoded in little endian.
 */
template <XDRMode mode>
class XDRState : public XDRCoderBase
{
  protected:
    XDRBuffer<mode> buf;

  public:
    XDRState(JSContext* cx, JS::TranscodeBuffer& buffer, size_t cursor = 0)
      : buf(cx, buffer, cursor)
    {
    }

    template <typename RangeType>
    XDRState(JSContext* cx, const RangeType& range)
      : buf(cx, range)
    {
    }

    virtual ~XDRState() {};

    JSContext* cx() const {
        return buf.cx();
    }
    virtual LifoAlloc& lifoAlloc() const;

    virtual bool hasOptions() const { return false; }
    virtual const ReadOnlyCompileOptions& options() {
        MOZ_CRASH("does not have options");
    }
    virtual bool hasScriptSourceObjectOut() const { return false; }
    virtual ScriptSourceObject** scriptSourceObjectOut() {
        MOZ_CRASH("does not have scriptSourceObjectOut.");
    }

    XDRResult fail(JS::TranscodeResult code) {
#ifdef DEBUG
        MOZ_ASSERT(code != JS::TranscodeResult_Ok);
        MOZ_ASSERT(validateResultCode(cx(), code));
        setResultCode(code);
#endif
        return mozilla::Err(code);
    }

    XDRResult peekData(const uint8_t** pptr, size_t length) {
        const uint8_t* ptr = buf.read(length);
        if (!ptr)
            return fail(JS::TranscodeResult_Failure_BadDecode);
        *pptr = ptr;
        return Ok();
    }

    // Alignment is required when doing memcpy of data which contains element
    // largers than 1 byte.
    bool isAligned(size_t n) override {
        MOZ_ASSERT(mozilla::IsPowerOfTwo(n));
        size_t mask = n - 1;
        size_t offset = buf.uptr() & mask;
        // In debug build, we not only check if the cursor is aligned, but also
        // if the last cursor manipulation was made by the codeAlign function.
        return offset == 0 && buf.isAligned();
    }
    XDRResult codeAlign(size_t n) {
        MOZ_ASSERT(mozilla::IsPowerOfTwo(n));
        size_t mask = n - 1;
        MOZ_ASSERT_IF(mode == XDR_ENCODE, (buf.uptr() & mask) == (buf.cursor() & mask));
        size_t offset = buf.uptr() & mask;
        if (offset) {
            size_t padding = n - offset;
            MOZ_ASSERT(padding < sizeof(AlignPadding));
            if (mode == XDR_ENCODE) {
                uint8_t* ptr = buf.write(padding);
                if (!ptr)
                    return fail(JS::TranscodeResult_Throw);
                memcpy(ptr, AlignPadding, padding);
            } else {
                const uint8_t* ptr = buf.read(padding);
                if (!ptr)
                    return fail(JS::TranscodeResult_Failure_BadDecode);
                if (memcmp(ptr, AlignPadding, padding) != 0)
                    return fail(JS::TranscodeResult_Failure_BadDecode);
            }
        }
        buf.setAligned(true);
        MOZ_ASSERT(isAligned(n));
        return Ok();
    }

    XDRResult codeUint8(uint8_t* n) {
        if (mode == XDR_ENCODE) {
            uint8_t* ptr = buf.write(sizeof(*n));
            if (!ptr)
                return fail(JS::TranscodeResult_Throw);
            *ptr = *n;
        } else {
            const uint8_t* ptr = buf.read(sizeof(*n));
            if (!ptr)
                return fail(JS::TranscodeResult_Failure_BadDecode);
            *n = *ptr;
        }
        return Ok();
    }

    XDRResult codeUint16(uint16_t* n) {
        if (mode == XDR_ENCODE) {
            uint8_t* ptr = buf.write(sizeof(*n));
            if (!ptr)
                return fail(JS::TranscodeResult_Throw);
            mozilla::LittleEndian::writeUint16(ptr, *n);
        } else {
            const uint8_t* ptr = buf.read(sizeof(*n));
            if (!ptr)
                return fail(JS::TranscodeResult_Failure_BadDecode);
            *n = mozilla::LittleEndian::readUint16(ptr);
        }
        return Ok();
    }

    XDRResult codeUint32(uint32_t* n) {
        if (mode == XDR_ENCODE) {
            uint8_t* ptr = buf.write(sizeof(*n));
            if (!ptr)
                return fail(JS::TranscodeResult_Throw);
            mozilla::LittleEndian::writeUint32(ptr, *n);
        } else {
            const uint8_t* ptr = buf.read(sizeof(*n));
            if (!ptr)
                return fail(JS::TranscodeResult_Failure_BadDecode);
            *n = mozilla::LittleEndian::readUint32(ptr);
        }
        return Ok();
    }

    XDRResult codeUint64(uint64_t* n) {
        if (mode == XDR_ENCODE) {
            uint8_t* ptr = buf.write(sizeof(*n));
            if (!ptr)
                return fail(JS::TranscodeResult_Throw);
            mozilla::LittleEndian::writeUint64(ptr, *n);
        } else {
            const uint8_t* ptr = buf.read(sizeof(*n));
            if (!ptr)
                return fail(JS::TranscodeResult_Failure_BadDecode);
            *n = mozilla::LittleEndian::readUint64(ptr);
        }
        return Ok();
    }

    /*
     * Use SFINAE to refuse any specialization which is not an enum.  Uses of
     * this function do not have to specialize the type of the enumerated field
     * as C++ will extract the parameterized from the argument list.
     */
    template <typename T>
    XDRResult codeEnum32(T* val, typename mozilla::EnableIf<mozilla::IsEnum<T>::value, T>::Type * = NULL)
    {
        // Mix the enumeration value with a random magic number, such that a
        // corruption with a low-ranged value (like 0) is less likely to cause a
        // miss-interpretation of the XDR content and instead cause a failure.
        const uint32_t MAGIC = 0x21AB218C;
        uint32_t tmp;
        if (mode == XDR_ENCODE)
            tmp = uint32_t(*val) ^ MAGIC;
        MOZ_TRY(codeUint32(&tmp));
        if (mode == XDR_DECODE)
            *val = T(tmp ^ MAGIC);
        return Ok();
    }

    XDRResult codeDouble(double* dp) {
        union DoublePun {
            double d;
            uint64_t u;
        } pun;
        if (mode == XDR_ENCODE)
            pun.d = *dp;
        MOZ_TRY(codeUint64(&pun.u));
        if (mode == XDR_DECODE)
            *dp = pun.d;
        return Ok();
    }

    XDRResult codeMarker(uint32_t magic) {
        uint32_t actual = magic;
        MOZ_TRY(codeUint32(&actual));
        if (actual != magic) {
            // Fail in debug, but only soft-fail in release
            MOZ_ASSERT(false, "Bad XDR marker");
            return fail(JS::TranscodeResult_Failure_BadDecode);
        }
        return Ok();
    }

    XDRResult codeBytes(void* bytes, size_t len) {
        if (len == 0)
            return Ok();
        if (mode == XDR_ENCODE) {
            uint8_t* ptr = buf.write(len);
            if (!ptr)
                return fail(JS::TranscodeResult_Throw);
            memcpy(ptr, bytes, len);
        } else {
            const uint8_t* ptr = buf.read(len);
            if (!ptr)
                return fail(JS::TranscodeResult_Failure_BadDecode);
            memcpy(bytes, ptr, len);
        }
        return Ok();
    }

    /*
     * During encoding the string is written into the buffer together with its
     * terminating '\0'. During decoding the method returns a pointer into the
     * decoding buffer and the caller must copy the string if it will outlive
     * the decoding buffer.
     */
    XDRResult codeCString(const char** sp) {
        uint64_t len64;
        if (mode == XDR_ENCODE)
            len64 = (uint64_t)(strlen(*sp) + 1);
        MOZ_TRY(codeUint64(&len64));
        size_t len = (size_t) len64;

        if (mode == XDR_ENCODE) {
            uint8_t* ptr = buf.write(len);
            if (!ptr)
                return fail(JS::TranscodeResult_Throw);
            memcpy(ptr, *sp, len);
        } else {
            const uint8_t* ptr = buf.read(len);
            if (!ptr || ptr[len] != '\0')
                return fail(JS::TranscodeResult_Failure_BadDecode);
            *sp = reinterpret_cast<const char*>(ptr);
        }
        return Ok();
    }

    XDRResult codeChars(const JS::Latin1Char* chars, size_t nchars);
    XDRResult codeChars(char16_t* chars, size_t nchars);

    XDRResult codeFunction(JS::MutableHandleFunction objp,
                           HandleScriptSourceObject sourceObject = nullptr);
    XDRResult codeScript(MutableHandleScript scriptp);
};

using XDREncoder = XDRState<XDR_ENCODE>;
using XDRDecoder = XDRState<XDR_DECODE>;

class XDROffThreadDecoder : public XDRDecoder
{
    const ReadOnlyCompileOptions* options_;
    ScriptSourceObject** sourceObjectOut_;
    LifoAlloc& alloc_;

  public:
    // Note, when providing an JSContext, where isJSContext is false,
    // then the initialization of the ScriptSourceObject would remain
    // incomplete. Thus, the sourceObjectOut must be used to finish the
    // initialization with ScriptSourceObject::initFromOptions after the
    // decoding.
    //
    // When providing a sourceObjectOut pointer, you have to ensure that it is
    // marked by the GC to avoid dangling pointers.
    XDROffThreadDecoder(JSContext* cx, LifoAlloc& alloc,
                        const ReadOnlyCompileOptions* options,
                        ScriptSourceObject** sourceObjectOut,
                        const JS::TranscodeRange& range)
      : XDRDecoder(cx, range),
        options_(options),
        sourceObjectOut_(sourceObjectOut),
        alloc_(alloc)
    {
        MOZ_ASSERT(options);
        MOZ_ASSERT(sourceObjectOut);
        MOZ_ASSERT(*sourceObjectOut == nullptr);
    }

    LifoAlloc& lifoAlloc() const override {
        return alloc_;
    }

    bool hasOptions() const override { return true; }
    const ReadOnlyCompileOptions& options() override {
        return *options_;
    }
    bool hasScriptSourceObjectOut() const override { return true; }
    ScriptSourceObject** scriptSourceObjectOut() override {
        return sourceObjectOut_;
    }
};

class XDRIncrementalEncoder : public XDREncoder
{
    // The incremental encoder encodes the content of scripts and functions in
    // the XDRBuffer. It can be used to encode multiple times the same AutoXDRTree,
    // and uses its key to identify which part to replace.
    //
    // Internally, this encoder keeps a tree representation of the scopes. Each
    // node is composed of a vector of slices which are interleaved by child
    // nodes.
    //
    // A slice corresponds to an index and a length within the content of the
    // slices_ buffer. The index is updated when a slice is created, and the
    // length is updated when the slice is ended, either by creating a new scope
    // child, or by closing the scope and going back to the parent.
    //
    //                  +---+---+---+
    //        begin     |   |   |   |
    //        length    |   |   |   |
    //        child     | . | . | . |
    //                  +-|-+-|-+---+
    //                    |   |
    //          +---------+   +---------+
    //          |                       |
    //          v                       v
    //      +---+---+                 +---+
    //      |   |   |                 |   |
    //      |   |   |                 |   |
    //      | . | . |                 | . |
    //      +-|-+---+                 +---+
    //        |
    //        |
    //        |
    //        v
    //      +---+
    //      |   |
    //      |   |
    //      | . |
    //      +---+
    //
    //
    // The tree key is used to identify the child nodes, and to make them
    // easily replaceable.
    //
    // The tree is rooted at the |topLevel| key.
    //

    struct Slice {
        size_t sliceBegin;
        size_t sliceLength;
        AutoXDRTree::Key child;
    };

    using SlicesNode = Vector<Slice, 1, SystemAllocPolicy>;
    using SlicesTree = HashMap<AutoXDRTree::Key, SlicesNode, DefaultHasher<AutoXDRTree::Key>,
                               SystemAllocPolicy>;

    // Last opened XDR-tree on the stack.
    AutoXDRTree* scope_;
    // Node corresponding to the opened scope.
    SlicesNode* node_;
    // Tree of slices.
    SlicesTree tree_;
    JS::TranscodeBuffer slices_;
    bool oom_;

  public:
    explicit XDRIncrementalEncoder(JSContext* cx)
      : XDREncoder(cx, slices_, 0),
        scope_(nullptr),
        node_(nullptr),
        oom_(false)
    {
    }

    virtual ~XDRIncrementalEncoder() {}

    AutoXDRTree::Key getTopLevelTreeKey() const override;
    AutoXDRTree::Key getTreeKey(JSFunction* fun) const override;

    MOZ_MUST_USE bool init();

    void createOrReplaceSubTree(AutoXDRTree* child) override;
    void endSubTree() override;

    // Append the content collected during the incremental encoding into the
    // buffer given as argument.
    XDRResult linearize(JS::TranscodeBuffer& buffer);
};

template<XDRMode mode>
XDRResult
XDRAtom(XDRState<mode>* xdr, js::MutableHandleAtom atomp);

} /* namespace js */

#endif /* vm_Xdr_h */
