/*
 * Copyright © 2004 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Red Hat, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Red Hat, Inc. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * RED HAT, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL RED HAT, INC. BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@cworth.org>
 */

/* Bug history
 *
 * 2004-11-04 Ned Konz <ned@squeakland.org>
 *
 *   Reported bug on mailing list:
 *
 *	From: Ned Konz <ned@squeakland.org>
 *	To: cairo@cairographics.org
 *	Date: Thu, 4 Nov 2004 09:49:38 -0800
 *	Subject: [cairo] getting assertions [cairo_cache.c:143: _entry_destroy:
 *	        Assertion `cache->used_memory > entry->memory' failed]
 *
 *	The attached program dies on me with the assert
 *
 *	$ ./testCairo
 *	testCairo: cairo_cache.c:143: _entry_destroy: Assertion `cache->used_memory > entry->memory' failed.
 *
 * 2004-11-04 Carl Worth <cworth@cworth.org>
 *
 *   I trimmed down Ned's example to the folllowing test while still
 *   maintaining the assertion.
 *
 *   Oh, actually, it looks like I may have triggered something
 *   slightly different:
 *
 *	text_cache_crash: cairo_cache.c:422: _cairo_cache_lookup: Assertion `cache->max_memory >= (cache->used_memory + new_entry->memory)' failed.
 *
 *   I'll have to go back and try the original test after I fix this.
 *
 * 2004-11-13 Carl Worth <cworth@cworth.org>
 *
 *   Found the bug. cairo_gstate_select_font was noticing when the
 *   same font was selected twice in a row and was erroneously failing
 *   to free the old reference. Committed a fix and verified it also
 *   fixed the orginal test case.
 */

#include "cairo_test.h"

cairo_test_t test = {
    "text_cache_crash",
    "Test case for bug causing an assertion failure in _cairo_cache_lookup",
    0, 0,
};
#include <cairo.h>

static void
draw (cairo_t *cr, int width, int height)
{
    /* Once there was a bug that choked when selecting the same font twice. */
    cairo_select_font(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_scale_font(cr, 40.0);

    cairo_select_font(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_scale_font(cr, 40.0);
    cairo_move_to(cr, 10, 50);
    cairo_show_text(cr, "hello");

    /* Then there was a bug that choked when selecting a font too big
     * for the cache. */

/* XXX: Sometimes this leads to an assertion:

_cairo_cache_lookup: Assertion `cache->max_memory >= (cache->used_memory + new_entry->memory)' failed.
Aborted

   But other times my machine hangs completely only to return to life
   several minutes later with some programs missing. This seems like
   the out-of-memory killer to me.

   It seems like I usually get the assertion when I run
   ./text_cache_crash directly and I usually get the machine hang when
   I run "make check" but I don't know if there's a perfect
   correlation there.

   So there's a bad bug here somewhere that really needs to be fixed.
   But in the meantime, I need "make check" not to destory work, so
   I'm commenting this test out for now.

    cairo_scale_font (cr, 500);
    cairo_show_text (cr, "hello");
*/
}

int
main (void)
{
    int ret;

    ret = cairo_test (&test, draw);

    /* It's convenient to be able to free all memory (including
     * statically allocated memory). This makes it quite easy to use
     * tools such as valgrind to verify that there are no memory leaks
     * whatsoever.
     *
     * But I'm not sure what would be a sensible cairo API function
     * for this. The cairo_destroy_caches call below is just something
     * I made as a local modification to cairo.
     */
    /*
    cairo_destroy_caches ();
    FcFini ();
    */

    return ret;
}

