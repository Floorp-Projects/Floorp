/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_GIF2_H
#define mozilla_image_decoders_GIF2_H

#define MAX_LZW_BITS          12
#define MAX_BITS            4097 // 2^MAX_LZW_BITS+1
#define MAX_COLORS           256
#define MIN_HOLD_SIZE        256

enum { GIF_TRAILER                     = 0x3B }; // ';'
enum { GIF_IMAGE_SEPARATOR             = 0x2C }; // ','
enum { GIF_EXTENSION_INTRODUCER        = 0x21 }; // '!'
enum { GIF_GRAPHIC_CONTROL_LABEL       = 0xF9 };
enum { GIF_APPLICATION_EXTENSION_LABEL = 0xFF };

// A GIF decoder's state
typedef struct gif_struct {
    // LZW decoder state machine
    uint8_t* stackp;            // Current stack pointer
    int datasize;
    int codesize;
    int codemask;
    int avail;                  // Index of next available slot in dictionary
    int oldcode;
    uint8_t firstchar;
    int bits;                   // Number of unread bits in "datum"
    int32_t datum;              // 32-bit input buffer

    // Output state machine
    int64_t pixels_remaining;  // Pixels remaining to be output.

    // Parameters for image frame currently being decoded
    int tpixel;                 // Index of transparent pixel
    int32_t disposal_method;    // Restore to background, leave in place, etc.
    uint32_t* local_colormap;   // Per-image colormap
    int local_colormap_size;    // Size of local colormap array.
    uint32_t delay_time;        // Display time, in milliseconds,
                                // for this image in a multi-image GIF

    // Global (multi-image) state
    int version;                // Either 89 for GIF89 or 87 for GIF87
    int32_t screen_width;       // Logical screen width & height
    int32_t screen_height;
    uint8_t global_colormap_depth; // Depth of global colormap array
    uint16_t global_colormap_count; // Number of colors in global colormap
    int images_decoded;         // Counts images for multi-part GIFs
    int loop_count;             // Netscape specific extension block to control
                                // the number of animation loops a GIF
                                // renders.

    bool is_transparent;        // TRUE, if tpixel is valid

    uint16_t  prefix[MAX_BITS];            // LZW decoding tables
    uint32_t  global_colormap[MAX_COLORS]; // Default colormap if local not
                                           //   supplied
    uint8_t   suffix[MAX_BITS];            // LZW decoding tables
    uint8_t   stack[MAX_BITS];             // Base of LZW decoder stack

} gif_struct;

#endif // mozilla_image_decoders_GIF2_H
