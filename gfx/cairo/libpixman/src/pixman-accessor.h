#ifdef PIXMAN_FB_ACCESSORS

#define ACCESS(sym) sym##_accessors

#define READ(img, ptr)							\
    (((bits_image_t *)(img))->read_func ((ptr), sizeof(*(ptr))))
#define WRITE(img, ptr,val)						\
    (((bits_image_t *)(img))->write_func ((ptr), (val), sizeof (*(ptr))))

#define MEMCPY_WRAPPED(img, dst, src, size)				\
    do {								\
	size_t _i;							\
	uint8_t *_dst = (uint8_t*)(dst), *_src = (uint8_t*)(src);	\
	for(_i = 0; _i < size; _i++) {					\
	    WRITE((img), _dst +_i, READ((img), _src + _i));		\
	}								\
    } while (0)

#define MEMSET_WRAPPED(img, dst, val, size)				\
    do {								\
	size_t _i;							\
	uint8_t *_dst = (uint8_t*)(dst);				\
	for(_i = 0; _i < (size_t) size; _i++) {				\
	    WRITE((img), _dst +_i, (val));				\
	}								\
    } while (0)

#else

#define ACCESS(sym) sym

#define READ(img, ptr)		(*(ptr))
#define WRITE(img, ptr, val)	(*(ptr) = (val))
#define MEMCPY_WRAPPED(img, dst, src, size)				\
    memcpy(dst, src, size)
#define MEMSET_WRAPPED(img, dst, val, size)				\
    memset(dst, val, size)

#endif

