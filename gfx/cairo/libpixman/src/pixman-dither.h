#define R16_BITS     5
#define G16_BITS     6
#define B16_BITS     5

#define R16_SHIFT    (B16_BITS + G16_BITS)
#define G16_SHIFT    (B16_BITS)
#define B16_SHIFT    0

#define MASK 0xff
#define ONE_HALF 0x80

#define A_SHIFT 8 * 3
#define R_SHIFT 8 * 2
#define G_SHIFT 8
#define A_MASK 0xff000000
#define R_MASK 0xff0000
#define G_MASK 0xff00

#define RB_MASK 0xff00ff
#define AG_MASK 0xff00ff00
#define RB_ONE_HALF 0x800080
#define RB_MASK_PLUS_ONE 0x10000100

#define ALPHA_8(x) ((x) >> A_SHIFT)
#define RED_8(x) (((x) >> R_SHIFT) & MASK)
#define GREEN_8(x) (((x) >> G_SHIFT) & MASK)
#define BLUE_8(x) ((x) & MASK)

// This uses the same dithering technique that Skia does.
// It is essentially preturbing the lower bit based on the
// high bit
static inline uint16_t dither_32_to_16(uint32_t c)
{
    uint8_t b = BLUE_8(c);
    uint8_t g = GREEN_8(c);
    uint8_t r = RED_8(c);
    r = ((r << 1) - ((r >> (8 - R16_BITS) << (8 - R16_BITS)) | (r >> R16_BITS))) >> (8 - R16_BITS);
    g = ((g << 1) - ((g >> (8 - G16_BITS) << (8 - G16_BITS)) | (g >> G16_BITS))) >> (8 - G16_BITS);
    b = ((b << 1) - ((b >> (8 - B16_BITS) << (8 - B16_BITS)) | (b >> B16_BITS))) >> (8 - B16_BITS);
    return ((r << R16_SHIFT) | (g << G16_SHIFT) | (b << B16_SHIFT));
}

static inline uint16_t dither_8888_to_0565(uint32_t color, pixman_bool_t toggle)
{
    // alternate between a preturbed truncation and a regular truncation
    if (toggle) {
	return dither_32_to_16(color);
    } else {
	return convert_8888_to_0565(color);
    }
}
