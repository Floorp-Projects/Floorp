/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2006 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Kristian Høgsberg <krh@redhat.com>
 */

#include "cairoint.h"
#include "cairo-scaled-font-subsets-private.h"
#include "cairo-output-stream-private.h"

/* XXX: Eventually, we need to handle other font backends */
#include "cairo-ft-private.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_TYPE1_TABLES_H

typedef struct _cairo_type1_font_subset {

    cairo_scaled_font_subset_t *scaled_font_subset;

    struct {
	cairo_unscaled_font_t *unscaled_font;
	unsigned int font_id;
	char *base_font;
	int num_glyphs;
	long x_min, y_min, x_max, y_max;
	long ascent, descent;

	const char    *data;
	unsigned long  header_size;
	unsigned long  data_size;
	unsigned long  trailer_size;

    } base;

    FT_Face face;
    int num_glyphs;

    struct {
	int subset_index;
	int width;
	char *name;
    } *glyphs;

    cairo_output_stream_t *output;
    cairo_array_t contents;

    const char *rd, *nd;

    char *type1_data;
    unsigned int type1_length;
    char *type1_end;

    char *header_segment;
    int header_segment_size;
    char *eexec_segment;
    int eexec_segment_size;
    cairo_bool_t eexec_segment_is_ascii;

    char *cleartext;
    char *cleartext_end;

    int header_size;

    unsigned short eexec_key;
    cairo_bool_t hex_encode;
    int hex_column;

    cairo_status_t status;
} cairo_type1_font_subset_t;


static cairo_status_t
_cairo_type1_font_subset_create (cairo_unscaled_font_t      *unscaled_font,
				 cairo_type1_font_subset_t **subset_return)
{
    cairo_ft_unscaled_font_t *ft_unscaled_font;
    FT_Face face;
    PS_FontInfoRec font_info;
    cairo_type1_font_subset_t *font;
    int i, j;

    ft_unscaled_font = (cairo_ft_unscaled_font_t *) unscaled_font;

    face = _cairo_ft_unscaled_font_lock_face (ft_unscaled_font);

    if (FT_Get_PS_Font_Info(face, &font_info) != 0)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    font = calloc (sizeof (cairo_type1_font_subset_t), 1);
    if (font == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    font->base.unscaled_font = _cairo_unscaled_font_reference (unscaled_font);
    font->base.num_glyphs = face->num_glyphs;
    font->base.x_min = face->bbox.xMin;
    font->base.y_min = face->bbox.yMin;
    font->base.x_max = face->bbox.xMax;
    font->base.y_max = face->bbox.yMax;
    font->base.ascent = face->ascender;
    font->base.descent = face->descender;
    font->base.base_font = strdup (face->family_name);
    if (font->base.base_font == NULL)
	goto fail1;

    for (i = 0, j = 0; font->base.base_font[j]; j++) {
	if (font->base.base_font[j] == ' ')
	    continue;
	font->base.base_font[i++] = font->base.base_font[j];
    }
    font->base.base_font[i] = '\0';

    font->glyphs = calloc (face->num_glyphs, sizeof font->glyphs[0]);
    if (font->glyphs == NULL)
	goto fail2;

    font->num_glyphs = 0;
    for (i = 0; i < face->num_glyphs; i++)
	font->glyphs[i].subset_index = -1;

    _cairo_array_init (&font->contents, sizeof (char));

    _cairo_ft_unscaled_font_unlock_face (ft_unscaled_font);

    *subset_return = font;

    return CAIRO_STATUS_SUCCESS;

 fail2:
    free (font->base.base_font);
 fail1:
    free (font);

    return CAIRO_STATUS_NO_MEMORY;
}

static int
cairo_type1_font_subset_use_glyph (cairo_type1_font_subset_t *font, int glyph)
{
    if (font->glyphs[glyph].subset_index >= 0)
	return font->glyphs[glyph].subset_index;

    font->glyphs[glyph].subset_index = font->num_glyphs;
    font->num_glyphs++;

    return font->glyphs[glyph].subset_index;
}

/* Magic constants for the type1 eexec encryption */
static const unsigned short c1 = 52845, c2 = 22719;
static const unsigned short private_dict_key = 55665;
static const unsigned short charstring_key = 4330;

static cairo_bool_t
is_ps_delimiter(int c)
{
    const static char delimiters[] = "()[]{}<>/% \t\r\n";

    return strchr (delimiters, c) != NULL;
}

static const char *
find_token (const char *buffer, const char *end, const char *token)
{
    int i, length;
    /* FIXME: find substring really must be find_token */

    length = strlen (token);
    for (i = 0; buffer + i < end - length + 1; i++)
	if (memcmp (buffer + i, token, length) == 0)
	    if ((i == 0 || token[0] == '/' || is_ps_delimiter(buffer[i - 1])) &&
		(buffer + i == end - length || is_ps_delimiter(buffer[i + length])))
		return buffer + i;

    return NULL;
}

static cairo_status_t
cairo_type1_font_subset_find_segments (cairo_type1_font_subset_t *font)
{
    unsigned char *p;
    const char *eexec_token;
    int size;

    p = (unsigned char *) font->type1_data;
    font->type1_end = font->type1_data + font->type1_length;
    if (p[0] == 0x80 && p[1] == 0x01) {
	font->header_segment_size =
	    p[2] | (p[3] << 8) | (p[4] << 16) | (p[5] << 24);
	font->header_segment = (char *) p + 6;

	p += 6 + font->header_segment_size;
	font->eexec_segment_size =
	    p[2] | (p[3] << 8) | (p[4] << 16) | (p[5] << 24);
	font->eexec_segment = (char *) p + 6;
	font->eexec_segment_is_ascii = (p[1] == 1);

        p += 6 + font->eexec_segment_size;
        while (p < (unsigned char *) (font->type1_end) && p[1] != 0x03) {
            size = p[2] | (p[3] << 8) | (p[4] << 16) | (p[5] << 24);
            p += 6 + size;
        }
        font->type1_end = (char *) p;
    } else {
	eexec_token = find_token ((char *) p, font->type1_end, "eexec");
	if (eexec_token == NULL)
	    return font->status = CAIRO_INT_STATUS_UNSUPPORTED;

	font->header_segment_size = eexec_token - (char *) p + strlen ("eexec\n");
	font->header_segment = (char *) p;
	font->eexec_segment_size = font->type1_length - font->header_segment_size;
	font->eexec_segment = (char *) p + font->header_segment_size;
	font->eexec_segment_is_ascii = TRUE;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_type1_font_subset_write_header (cairo_type1_font_subset_t *font,
					 const char *name)
{
    const char *start, *end, *segment_end;
    int i;

    segment_end = font->header_segment + font->header_segment_size;

    start = find_token (font->header_segment, segment_end, "/FontName");
    if (start == NULL)
	return font->status = CAIRO_INT_STATUS_UNSUPPORTED;

    _cairo_output_stream_write (font->output, font->header_segment,
				start - font->header_segment);

    _cairo_output_stream_printf (font->output, "/FontName /%s def", name);

    end = find_token (start, segment_end, "def");
    if (end == NULL)
	return font->status = CAIRO_INT_STATUS_UNSUPPORTED;
    end += 3;

    start = find_token (end, segment_end, "/Encoding");
    if (start == NULL)
	return font->status = CAIRO_INT_STATUS_UNSUPPORTED;
    _cairo_output_stream_write (font->output, end, start - end);

    _cairo_output_stream_printf (font->output,
				 "/Encoding 256 array\n"
				 "0 1 255 {1 index exch /.notdef put} for\n");
    for (i = 0; i < font->base.num_glyphs; i++) {
	if (font->glyphs[i].subset_index < 0)
	    continue;
	_cairo_output_stream_printf (font->output,
				     "dup %d /%s put\n",
				     font->glyphs[i].subset_index,
				     font->glyphs[i].name);
    }
    _cairo_output_stream_printf (font->output, "readonly def");

    end = find_token (start, segment_end, "def");
    if (end == NULL)
	return font->status = CAIRO_INT_STATUS_UNSUPPORTED;
    end += 3;

    _cairo_output_stream_write (font->output, end, segment_end - end);

    return font->status;
}

static int
hex_to_int (int ch)
{
    if (ch <= '9')
	return ch - '0';
    else if (ch <= 'F')
	return ch - 'A' + 10;
    else
	return ch - 'a' + 10;
}

static void
cairo_type1_font_subset_write_encrypted (cairo_type1_font_subset_t *font,
					 const char *data, unsigned int length)
{
    const unsigned char *in, *end;
    int c, p;
    static const char hex_digits[16] = "0123456789abcdef";
    char digits[3];

    in = (const unsigned char *) data;
    end = (const unsigned char *) data + length;
    while (in < end) {
	p = *in++;
	c = p ^ (font->eexec_key >> 8);
	font->eexec_key = (c + font->eexec_key) * c1 + c2;

	if (font->hex_encode) {
	    digits[0] = hex_digits[c >> 4];
	    digits[1] = hex_digits[c & 0x0f];
	    digits[2] = '\n';
	    font->hex_column += 2;

	    if (font->hex_column == 78) {
		_cairo_output_stream_write (font->output, digits, 3);
		font->hex_column = 0;
	    } else {
		_cairo_output_stream_write (font->output, digits, 2);
	    }
	} else {
	    digits[0] = c;
	    _cairo_output_stream_write (font->output, digits, 1);
	}
    }
}

static cairo_status_t
cairo_type1_font_subset_decrypt_eexec_segment (cairo_type1_font_subset_t *font)
{
    unsigned short r = private_dict_key;
    unsigned char *in, *end;
    char *out;
    int c, p;

    in = (unsigned char *) font->eexec_segment;
    end = (unsigned char *) in + font->eexec_segment_size;

    font->cleartext = malloc (font->eexec_segment_size);
    if (font->cleartext == NULL)
	return font->status = CAIRO_STATUS_NO_MEMORY;
    out = font->cleartext;

    while (in < end) {
	if (font->eexec_segment_is_ascii) {
	    c = *in++;
	    if (isspace (c))
		continue;
	    c = (hex_to_int (c) << 4) | hex_to_int (*in++);
	} else {
	    c = *in++;
	}
	p = c ^ (r >> 8);
	r = (c + r) * c1 + c2;

	*out++ = p;
    }

    font->cleartext_end = out;

    return font->status;
}

static const char *
skip_token (const char *p, const char *end)
{
    while (p < end && isspace(*p))
	p++;

    while (p < end && !isspace(*p))
	p++;

    if (p == end)
	return NULL;

    return p;
}

static int
cairo_type1_font_subset_lookup_glyph (cairo_type1_font_subset_t *font,
				      const char *glyph_name, int length)
{
    int i;

    for (i = 0; i < font->base.num_glyphs; i++) {
	if (font->glyphs[i].name &&
	    strncmp (font->glyphs[i].name, glyph_name, length) == 0 &&
	    font->glyphs[i].name[length] == '\0')
	    return i;
    }

    return -1;
}

static cairo_status_t
cairo_type1_font_subset_get_glyph_names_and_widths (cairo_type1_font_subset_t *font)
{
    int i;
    char buffer[256];
    FT_Error error;

    /* Get glyph names and width using the freetype API */
    for (i = 0; i < font->base.num_glyphs; i++) {
	if (font->glyphs[i].name != NULL)
	    continue;

	error = FT_Load_Glyph (font->face, i,
			       FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING |
			       FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM);
	if (error != 0) {
	    printf ("could not load glyph %d\n", i);
	    return font->status = CAIRO_STATUS_NO_MEMORY;
	}

	font->glyphs[i].width = font->face->glyph->metrics.horiAdvance;

	error = FT_Get_Glyph_Name(font->face, i, buffer, sizeof buffer);
	if (error != 0) {
	    printf ("could not get glyph name for glyph %d\n", i);
	    return font->status = CAIRO_STATUS_NO_MEMORY;
	}

	font->glyphs[i].name = strdup (buffer);
	if (font->glyphs[i].name == NULL)
	    return font->status = CAIRO_STATUS_NO_MEMORY;
    }

    return CAIRO_STATUS_SUCCESS;
}

static void
cairo_type1_font_subset_decrypt_charstring (const unsigned char *in, int size, unsigned char *out)
{
    unsigned short r = charstring_key;
    int c, p, i;

    for (i = 0; i < size; i++) {
        c = *in++;
	p = c ^ (r >> 8);
	r = (c + r) * c1 + c2;
	*out++ = p;
    }
}

static const unsigned char *
cairo_type1_font_subset_decode_integer (const unsigned char *p, int *integer)
{
    if (*p <= 246) {
        *integer = *p++ - 139;
    } else if (*p <= 250) {
        *integer = (p[0] - 247) * 256 + p[1] + 108;
        p += 2;
    } else if (*p <= 254) {
        *integer = -(p[0] - 251) * 256 - p[1] - 108;
        p += 2;
    } else {
        *integer = (p[1] << 24) | (p[2] << 16) | (p[3] << 8) | p[4];
        p += 5;
    }

    return p;
}

static const char *ps_standard_encoding[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/*   0 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/*  16 */
    "space", "exclam", "quotedbl", "numbersign",	/*  32 */
    "dollar", "percent", "ampersand", "quoteright",
    "parenleft", "parenright", "asterisk", "plus",
    "comma", "hyphen", "period", "slash",
    "zero", "one", "two", "three",			/*  48 */
    "four", "five", "six", "seven", "eight",
    "nine", "colon", "semicolon", "less",
    "equal", "greater", "question", "at",
    "A", "B", "C", "D", "E", "F", "G", "H",		/*  64 */
    "I", "J", "K", "L", "M", "N", "O", "P",
    "Q", "R", "S", "T", "U", "V", "W", "X",		/*  80 */
    "Y", "Z", "bracketleft", "backslash",
    "bracketright", "asciicircum", "underscore", "quoteleft",
    "a", "b", "c", "d", "e", "f", "g", "h",		/*  96 */
    "i", "j", "k", "l", "m", "n", "o", "p",
    "q", "r", "s", "t", "u", "v", "w", "x",		/* 112 */
    "y", "z", "braceleft", "bar",
    "braceright", "asciitilde", 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 128 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 144 */
    "exclamdown", "cent", "sterling", "fraction",
    "yen", "florin", "section", "currency",
    "quotesingle", "quotedblleft", "guillemotleft", "guilsinglleft",
    "guilsinglright", "fi", "fl", NULL,
    "endash", "dagger", "daggerdbl", "periodcentered",	/* 160 */
    NULL, "paragraph", "bullet", "quotesinglbase",
    "quotedblbase", "quotedblright", "guillemotright", "ellipsis",
    "perthousand", NULL, "questiondown", NULL,
    "grave", "acute", "circumflex", "tilde",		/* 176 */
    "macron", "breve", "dotaccent", "dieresis",
    NULL, "ring", "cedilla", NULL,
    "hungarumlaut", "ogonek", "caron", "emdash",
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 192 */
    "AE", 0, "ordfeminine", 0, 0, 0, 0, "Lslash",	/* 208 */
    "Oslash", "OE", "ordmasculine", 0, 0, 0, 0, 0,
    "ae", 0, 0, 0, "dotlessi", 0, 0, "lslash",		/* 224 */
    "oslash", "oe", "germandbls", 0, 0, 0, 0
};

static void
use_standard_encoding_glyph (cairo_type1_font_subset_t *font, int index)
{
    const char *glyph_name;

    if (index < 0 || index > 255)
	return;

    glyph_name = ps_standard_encoding[index];
    if (glyph_name == NULL)
	return;

    index = cairo_type1_font_subset_lookup_glyph (font,
						  glyph_name,
						  strlen(glyph_name));
    if (index < 0)
	return;

    cairo_type1_font_subset_use_glyph (font, index);
}

#define TYPE1_CHARSTRING_COMMAND_ESCAPE		(12)
#define TYPE1_CHARSTRING_COMMAND_SEAC		(32 + 6)

static void
cairo_type1_font_subset_look_for_seac(cairo_type1_font_subset_t *font,
				      const char *name, int name_length,
				      const char *encrypted_charstring, int encrypted_charstring_length)
{
    unsigned char *charstring;
    const unsigned char *end;
    const unsigned char *p;
    int stack[5], sp, value;
    int command;

    charstring = malloc (encrypted_charstring_length);
    if (charstring == NULL)
	return;

    cairo_type1_font_subset_decrypt_charstring ((const unsigned char *)
						encrypted_charstring,
						encrypted_charstring_length,
						charstring);
    end = charstring + encrypted_charstring_length;

    p = charstring + 4;
    sp = 0;

    while (p < end) {
        if (*p < 32) {
	    command = *p++;

	    if (command == TYPE1_CHARSTRING_COMMAND_ESCAPE)
		command = 32 + *p++;

	    switch (command) {
	    case TYPE1_CHARSTRING_COMMAND_SEAC:
		/* The seac command takes five integer arguments.  The
		 * last two are glyph indices into the PS standard
		 * encoding give the names of the glyphs that this
		 * glyph is composed from.  All we need to do is to
		 * make sure those glyphs are present in the subset
		 * under their standard names. */
		use_standard_encoding_glyph (font, stack[3]);
		use_standard_encoding_glyph (font, stack[4]);
		sp = 0;
		break;

	    default:
		sp = 0;
		break;
	    }
        } else {
            /* integer argument */
	    p = cairo_type1_font_subset_decode_integer (p, &value);
	    if (sp < 5)
		stack[sp++] = value;
        }
    }

    free (charstring);
}

static void
write_used_glyphs (cairo_type1_font_subset_t *font,
		   const char *name, int name_length,
		   const char *charstring, int charstring_length)
{
    char buffer[256];
    int length;

    length = snprintf (buffer, sizeof buffer,
		       "/%.*s %d %s ",
		       name_length, name, charstring_length, font->rd);
    cairo_type1_font_subset_write_encrypted (font, buffer, length);
    cairo_type1_font_subset_write_encrypted (font,
					     charstring, charstring_length);
    length = snprintf (buffer, sizeof buffer, "%s\n", font->nd);
    cairo_type1_font_subset_write_encrypted (font, buffer, length);
}

typedef void (*glyph_func_t) (cairo_type1_font_subset_t *font,
			      const char *name, int name_length,
			      const char *charstring, int charstring_length);

static const char *
cairo_type1_font_subset_for_each_glyph (cairo_type1_font_subset_t *font,
					const char *dict_start,
					const char *dict_end,
					glyph_func_t func)
{
    int charstring_length, name_length, glyph_index;
    const char *p, *charstring, *name;
    char *end;

    /* We're looking at '/' in the name of the first glyph.  The glyph
     * definitions are on the form:
     *
     *   /name 23 RD <23 binary bytes> ND
     *
     * or alternatively using -| and |- instead of RD and ND.
     *
     * We parse the glyph name and see if it is in the subset.  If it
     * is, we call the specified callback with the glyph name and
     * glyph data, otherwise we just skip it.  We need to parse
     * through a glyph definition; we can't just find the next '/',
     * since the binary data could contain a '/'.
     */

    p = dict_start;

    while (*p == '/') {
	name = p + 1;
	p = skip_token (p, dict_end);
	name_length = p - name;

	charstring_length = strtol (p, &end, 10);
	if (p == end) {
	    font->status = CAIRO_INT_STATUS_UNSUPPORTED;
	    return NULL;
	}

	/* Skip past -| or RD to binary data.  There is exactly one space
	 * between the -| or RD token and the encrypted data, thus '+ 1'. */
	charstring = skip_token (end, dict_end) + 1;

	/* Skip binary data and |- or ND token. */
	p = skip_token (charstring + charstring_length, dict_end);
	while (p < dict_end && isspace(*p))
	    p++;

	/* In case any of the skip_token() calls above reached EOF, p will
	 * be equal to dict_end. */
	if (p == dict_end) {
	    font->status = CAIRO_INT_STATUS_UNSUPPORTED;
	    return NULL;
	}

	glyph_index = cairo_type1_font_subset_lookup_glyph (font,
							    name, name_length);
	if (font->glyphs[glyph_index].subset_index >= 0)
	    func (font, name, name_length, charstring, charstring_length);
    }

    return p;
}


static const char *
cairo_type1_font_subset_write_private_dict (cairo_type1_font_subset_t *font,
					    const char                *name)
{
    const char *p, *charstrings, *dict_start;
    const char *closefile_token;
    char buffer[32], *glyph_count_end;
    int num_charstrings, length;

    /* The private dict holds hint information, common subroutines and
     * the actual glyph definitions (charstrings).
     *
     * FIXME: update this comment.
     *
     * What we do here is scan directly the /CharString token, which
     * marks the beginning of the glyph definitions.  Then we parse
     * through the glyph definitions and weed out the glyphs not in
     * our subset.  Everything else before and after the glyph
     * definitions is copied verbatim to the output.  It might be
     * worthwile to figure out which of the common subroutines are
     * used by the glyphs in the subset and get rid of the rest. */

    /* FIXME: The /Subrs array contains binary data and could
     * conceivably have "/CharStrings" in it, so we might need to skip
     * this more cleverly. */
    charstrings = find_token (font->cleartext, font->cleartext_end, "/CharStrings");
    if (charstrings == NULL) {
	font->status = CAIRO_INT_STATUS_UNSUPPORTED;
	return NULL;
    }

    /* Scan past /CharStrings and the integer following it. */
    p = charstrings + strlen ("/CharStrings");
    num_charstrings = strtol (p, &glyph_count_end, 10);
    if (p == glyph_count_end) {
	font->status = CAIRO_INT_STATUS_UNSUPPORTED;
	return NULL;
    }

    /* Look for a '/' which marks the beginning of the first glyph
     * definition. */
    for (p = glyph_count_end; p < font->cleartext_end; p++)
	if (*p == '/')
	    break;
    if (p == font->cleartext_end) {
	font->status = CAIRO_INT_STATUS_UNSUPPORTED;
	return NULL;
    }
    dict_start = p;

    if (cairo_type1_font_subset_get_glyph_names_and_widths (font))
	return NULL;

    /* Now that we have the private dictionary broken down in
     * sections, do the first pass through the glyph definitions to
     * figure out which subrs and othersubrs are use and which extra
     * glyphs may be required by the seac operator. */
    p = cairo_type1_font_subset_for_each_glyph (font,
						dict_start,
						font->cleartext_end,
						cairo_type1_font_subset_look_for_seac);

    closefile_token = find_token (p, font->cleartext_end, "closefile");
    if (closefile_token == NULL) {
	font->status = CAIRO_INT_STATUS_UNSUPPORTED;
	return NULL;
    }

    if (cairo_type1_font_subset_get_glyph_names_and_widths (font))
	return NULL;

    /* We're ready to start outputting. First write the header,
     * i.e. the public part of the font dict.*/
    if (cairo_type1_font_subset_write_header (font, name))
	return NULL;

    font->base.header_size = _cairo_output_stream_get_position (font->output);


    /* Start outputting the private dict.  First output everything up
     * to the /CharStrings token. */
    cairo_type1_font_subset_write_encrypted (font, font->cleartext,
					     charstrings - font->cleartext);

    /* Write out new charstring count */
    length = snprintf (buffer, sizeof buffer,
		       "/CharStrings %d", font->num_glyphs);
    cairo_type1_font_subset_write_encrypted (font, buffer, length);

    /* Write out text between the charstring count and the first
     * charstring definition */
    cairo_type1_font_subset_write_encrypted (font, glyph_count_end,
					     dict_start - glyph_count_end);

    /* Write out the charstring definitions for each of the glyphs in
     * the subset. */
    p = cairo_type1_font_subset_for_each_glyph (font,
						dict_start,
						font->cleartext_end,
						write_used_glyphs);

    /* Output what's left between the end of the glyph definitions and
     * the end of the private dict to the output. */
    cairo_type1_font_subset_write_encrypted (font, p,
					     closefile_token - p + strlen ("closefile") + 1);
    _cairo_output_stream_write (font->output, "\n", 1);

    return p;
}

static cairo_status_t
cairo_type1_font_subset_write_trailer(cairo_type1_font_subset_t *font)
{
    const char *cleartomark_token;
    int i;
    static const char zeros[65] =
	"0000000000000000000000000000000000000000000000000000000000000000\n";

    /* Some fonts have conditional save/restore around the entire font
     * dict, so we need to retain whatever postscript code that may
     * come after 'cleartomark'. */

    for (i = 0; i < 8; i++)
	_cairo_output_stream_write (font->output, zeros, sizeof zeros);

    cleartomark_token = find_token (font->type1_data, font->type1_end, "cleartomark");
    if (cleartomark_token == NULL)
	return font->status = CAIRO_INT_STATUS_UNSUPPORTED;

    _cairo_output_stream_write (font->output, cleartomark_token,
				font->type1_end - cleartomark_token);

    return font->status;
}

static cairo_status_t
type1_font_write (void *closure, const unsigned char *data, unsigned int length)
{
    cairo_type1_font_subset_t *font = closure;

    font->status =
	_cairo_array_append_multiple (&font->contents, data, length);

    return font->status;
}

static cairo_status_t
cairo_type1_font_subset_write (cairo_type1_font_subset_t *font,
			       const char *name)
{
    if (cairo_type1_font_subset_find_segments (font))
	return font->status;

    if (cairo_type1_font_subset_decrypt_eexec_segment (font))
	return font->status;

    /* Determine which glyph definition delimiters to use. */
    if (find_token (font->cleartext, font->cleartext_end, "/-|") != NULL) {
	font->rd = "-|";
	font->nd = "|-";
    } else if (find_token (font->cleartext, font->cleartext_end, "/RD") != NULL) {
	font->rd = "RD";
	font->nd = "ND";
    } else {
	/* Don't know *what* kind of font this is... */
	return font->status = CAIRO_INT_STATUS_UNSUPPORTED;
    }

    font->eexec_key = private_dict_key;
    font->hex_encode = TRUE;
    font->hex_column = 0;

    cairo_type1_font_subset_write_private_dict (font, name);

    font->base.data_size = _cairo_output_stream_get_position (font->output) -
	font->base.header_size;

    cairo_type1_font_subset_write_trailer (font);

    font->base.trailer_size =
	_cairo_output_stream_get_position (font->output) -
	font->base.header_size - font->base.data_size;

    return font->status;
}

static cairo_status_t
cairo_type1_font_subset_generate (void       *abstract_font,
				  const char *name)

{
    cairo_type1_font_subset_t *font = abstract_font;
    cairo_ft_unscaled_font_t *ft_unscaled_font;
    unsigned long ret;

    ft_unscaled_font = (cairo_ft_unscaled_font_t *) font->base.unscaled_font;
    font->face = _cairo_ft_unscaled_font_lock_face (ft_unscaled_font);

    /* If anything fails below, it's out of memory. */
    font->status = CAIRO_STATUS_NO_MEMORY;

    font->type1_length = font->face->stream->size;
    font->type1_data = malloc (font->type1_length);
    if (font->type1_data == NULL)
	goto fail;

    if (font->face->stream->read) {
	ret = font->face->stream->read (font->face->stream, 0,
					(unsigned char *) font->type1_data,
					font->type1_length);
	if (ret != font->type1_length)
	    goto fail;
    } else {
	memcpy (font->type1_data,
		font->face->stream->base, font->type1_length);
    }

    if (_cairo_array_grow_by (&font->contents, 4096) != CAIRO_STATUS_SUCCESS)
	goto fail;

    font->output = _cairo_output_stream_create (type1_font_write, NULL, font);
    if (font->output == NULL)
	goto fail;

    font->status = CAIRO_STATUS_SUCCESS;
    cairo_type1_font_subset_write (font, name);

    font->base.data = _cairo_array_index (&font->contents, 0);

 fail:
    _cairo_ft_unscaled_font_unlock_face (ft_unscaled_font);

    return font->status;
}

static void
cairo_type1_font_subset_destroy (void *abstract_font)
{
    cairo_type1_font_subset_t *font = abstract_font;
    int i;

    /* If the subset generation failed, some of the pointers below may
     * be NULL depending on at which point the error occurred. */

    _cairo_array_fini (&font->contents);

    free (font->type1_data);
    if (font->glyphs != NULL)
	for (i = 0; i < font->base.num_glyphs; i++) {
	    free (font->glyphs[i].name);
	}

    _cairo_unscaled_font_destroy (font->base.unscaled_font);

    free (font->base.base_font);
    free (font->glyphs);
    free (font);
}


cairo_status_t
_cairo_type1_subset_init (cairo_type1_subset_t		*type1_subset,
			  const char			*name,
			  cairo_scaled_font_subset_t	*scaled_font_subset)
{
    cairo_type1_font_subset_t *font;
    cairo_status_t status;
    unsigned long parent_glyph, length;
    int i;
    cairo_unscaled_font_t *unscaled_font;

    /* XXX: Need to fix this to work with a general cairo_unscaled_font_t. */
    if (!_cairo_scaled_font_is_ft (scaled_font_subset->scaled_font))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    unscaled_font = _cairo_ft_scaled_font_get_unscaled_font (scaled_font_subset->scaled_font);

    status = _cairo_type1_font_subset_create (unscaled_font, &font);
    if (status)
	return status;

    for (i = 0; i < scaled_font_subset->num_glyphs; i++) {
	parent_glyph = scaled_font_subset->glyphs[i];
	cairo_type1_font_subset_use_glyph (font, parent_glyph);
    }

    /* Pull in the .notdef glyph */
    cairo_type1_font_subset_use_glyph (font, 0);

    status = cairo_type1_font_subset_generate (font, name);
    if (status)
	goto fail1;

    type1_subset->base_font = strdup (font->base.base_font);
    if (type1_subset->base_font == NULL)
	goto fail1;

    type1_subset->widths = calloc (sizeof (int), font->num_glyphs);
    if (type1_subset->widths == NULL)
	goto fail2;
    for (i = 0; i < font->base.num_glyphs; i++) {
	if (font->glyphs[i].subset_index < 0)
	    continue;
	type1_subset->widths[font->glyphs[i].subset_index] =
	    font->glyphs[i].width;
    }

    type1_subset->x_min = font->base.x_min;
    type1_subset->y_min = font->base.y_min;
    type1_subset->x_max = font->base.x_max;
    type1_subset->y_max = font->base.y_max;
    type1_subset->ascent = font->base.ascent;
    type1_subset->descent = font->base.descent;

    length = font->base.header_size + font->base.data_size +
	font->base.trailer_size;
    type1_subset->data = malloc (length);
    if (type1_subset->data == NULL)
	goto fail3;

    memcpy (type1_subset->data,
	    _cairo_array_index (&font->contents, 0), length);

    type1_subset->header_length = font->base.header_size;
    type1_subset->data_length = font->base.data_size;
    type1_subset->trailer_length = font->base.trailer_size;

    cairo_type1_font_subset_destroy (font);

    return CAIRO_STATUS_SUCCESS;

 fail3:
    free (type1_subset->widths);
 fail2:
    free (type1_subset->base_font);
 fail1:
    cairo_type1_font_subset_destroy (font);

    return status;
}

void
_cairo_type1_subset_fini (cairo_type1_subset_t *subset)
{
    free (subset->base_font);
    free (subset->widths);
    free (subset->data);
}

