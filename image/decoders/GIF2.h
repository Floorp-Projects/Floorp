/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _GIF_H_
#define _GIF_H_

#define MAX_LZW_BITS          12
#define MAX_BITS            4097 /* 2^MAX_LZW_BITS+1 */
#define MAX_COLORS           256
#define MIN_HOLD_SIZE        256

enum { GIF_TRAILER                     = 0x3B }; //';'
enum { GIF_IMAGE_SEPARATOR             = 0x2C }; //','
enum { GIF_EXTENSION_INTRODUCER        = 0x21 }; //'!'
enum { GIF_GRAPHIC_CONTROL_LABEL       = 0xF9 };
enum { GIF_COMMENT_LABEL               = 0xFE };
enum { GIF_PLAIN_TEXT_LABEL            = 0x01 };
enum { GIF_APPLICATION_EXTENSION_LABEL = 0xFF };

/* gif2.h  
   The interface for the GIF87/89a decoder. 
*/
// List of possible parsing states
typedef enum {
    gif_type,
    gif_global_header,
    gif_global_colormap,
    gif_image_start,
    gif_image_header,
    gif_image_header_continue,
    gif_image_colormap,
    gif_image_body,
    gif_lzw_start,
    gif_lzw,
    gif_sub_block,
    gif_extension,
    gif_control_extension,
    gif_consume_block,
    gif_skip_block,
    gif_done,
    gif_error,
    gif_comment_extension,
    gif_application_extension,
    gif_netscape_extension_block,
    gif_consume_netscape_extension,
    gif_consume_comment
} gstate;

/* A GIF decoder's state */
typedef struct gif_struct {
    /* Parsing state machine */
    gstate state;                   /* Curent decoder master state */
    uint32_t bytes_to_consume;      /* Number of bytes to accumulate */
    uint32_t bytes_in_hold;         /* bytes accumulated so far*/

    /* LZW decoder state machine */
    uint8_t *stackp;              /* Current stack pointer */
    int datasize;
    int codesize;
    int codemask;
    int avail;                  /* Index of next available slot in dictionary */
    int oldcode;
    uint8_t firstchar;
    int count;                  /* Remaining # bytes in sub-block */
    int bits;                   /* Number of unread bits in "datum" */
    int32_t datum;              /* 32-bit input buffer */

    /* Output state machine */
    int ipass;                  /* Interlace pass; Ranges 1-4 if interlaced. */
    unsigned rows_remaining;        /* Rows remaining to be output */
    unsigned irow;                  /* Current output row, starting at zero */
    uint8_t *rowp;                 /* Current output pointer */

    /* Parameters for image frame currently being decoded*/
    unsigned x_offset, y_offset;    /* With respect to "screen" origin */
    unsigned height, width;
    int tpixel;                 /* Index of transparent pixel */
    int32_t disposal_method;    /* Restore to background, leave in place, etc.*/
    uint32_t *local_colormap;   /* Per-image colormap */
    int local_colormap_size;    /* Size of local colormap array. */
    uint32_t delay_time;        /* Display time, in milliseconds,
                                   for this image in a multi-image GIF */

    /* Global (multi-image) state */
    int version;                /* Either 89 for GIF89 or 87 for GIF87 */
    unsigned screen_width;       /* Logical screen width & height */
    unsigned screen_height;
    uint32_t global_colormap_depth;  /* Depth of global colormap array. */
    int images_decoded;         /* Counts images for multi-part GIFs */
    int loop_count;             /* Netscape specific extension block to control
                                   the number of animation loops a GIF renders. */

    bool progressive_display;    /* If TRUE, do Haeberli interlace hack */
    bool interlaced;             /* TRUE, if scanlines arrive interlaced order */
    bool is_transparent;         /* TRUE, if tpixel is valid */

    uint16_t  prefix[MAX_BITS];          /* LZW decoding tables */
    uint8_t*  hold;                      /* Accumulation buffer */
    uint32_t  global_colormap[MAX_COLORS];   /* Default colormap if local not supplied */
    uint8_t   suffix[MAX_BITS];          /* LZW decoding tables */
    uint8_t   stack[MAX_BITS];           /* Base of LZW decoder stack */

} gif_struct;

#endif

