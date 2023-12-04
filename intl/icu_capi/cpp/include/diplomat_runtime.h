#ifndef DIPLOMAT_RUNTIME_C_H
#define DIPLOMAT_RUNTIME_C_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

// uchar.h doesn't always exist, but char32_t is always available
// in C++ anyway
#ifndef __cplusplus
#ifdef __APPLE__
#include <stdint.h>
typedef uint16_t char16_t;
typedef uint32_t char32_t;
#else
#include <uchar.h>
#endif
#endif


#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

typedef struct DiplomatWriteable {
    void* context;
    char* buf;
    size_t len;
    size_t cap;
    void (*flush)(struct DiplomatWriteable*);
    bool (*grow)(struct DiplomatWriteable*, size_t);
} DiplomatWriteable;

DiplomatWriteable diplomat_simple_writeable(char* buf, size_t buf_size);

typedef struct DiplomatStringView {
    const char* data;
    size_t len;
} DiplomatStringView;

#define MAKE_SLICE_VIEW(name, c_ty) \
    typedef struct Diplomat##name##View { \
        const c_ty* data; \
        size_t len; \
    } Diplomat##name##View;

MAKE_SLICE_VIEW(I8, int8_t)
MAKE_SLICE_VIEW(U8, uint8_t)
MAKE_SLICE_VIEW(I16, int16_t)
MAKE_SLICE_VIEW(U16, uint16_t)
MAKE_SLICE_VIEW(I32, int32_t)
MAKE_SLICE_VIEW(U32, uint32_t)
MAKE_SLICE_VIEW(I64, int64_t)
MAKE_SLICE_VIEW(U64, uint64_t)
MAKE_SLICE_VIEW(Isize, intptr_t)
MAKE_SLICE_VIEW(Usize, size_t)
MAKE_SLICE_VIEW(F32, float)
MAKE_SLICE_VIEW(F64, double)
MAKE_SLICE_VIEW(Bool, bool)
MAKE_SLICE_VIEW(Char, char32_t)


#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif

#endif
