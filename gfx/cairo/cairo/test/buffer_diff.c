/* imagediff - Compare two images
 *
 * Copyright © 2004 Richard D. Worth
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Richard Worth
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * Richard Worth makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 * RICHARD WORTH DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL RICHARD WORTH BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Richard D. Worth <richard@theworths.org> */

#include "buffer_diff.h"

/* Image comparison code courttesy of Richard Worth.
 * Returns number of pixels changed.
 * Also fills out a "diff" image intended to visually show where the
 * images differ.
 */
int
buffer_diff (char *buf_a, char *buf_b, char *buf_diff,
	    int width, int height, int stride)
{
    int x, y;
    int total_pixels_changed = 0;
    unsigned char *row_a, *row_b, *row;

    for (y = 0; y < height; y++)
    {
	row_a = buf_a + y * stride;
	row_b = buf_b + y * stride;
	row = buf_diff + y * stride;
	for (x = 0; x < width; x++)
	{
	    int channel;
	    unsigned char value_a, value_b;
	    int pixel_changed = 0;
	    for (channel = 0; channel < 4; channel++)
	    {
		double diff;
		value_a = row_a[x * 4 + channel];
		value_b = row_b[x * 4 + channel];
		if (value_a != value_b)
		    pixel_changed = 1;
		diff = value_a - value_b;
		row[x * 4 + channel] = 128 + diff / 3.0;
	    }
	    if (pixel_changed) {
		total_pixels_changed++;
	    } else {
		row[x*4+0] = 0;
		row[x*4+1] = 0;
		row[x*4+2] = 0;
	    }
	    row[x * 4 + 3] = 0xff; /* Set ALPHA to 100% (opaque) */
	}
    }

    return total_pixels_changed;
}
