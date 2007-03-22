/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2006 Adrian Johnson
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
 * The Initial Developer of the Original Code is Adrian Johnson.
 *
 * Contributor(s):
 *	Adrian Johnson <ajohnson@redneon.com>
 */

#include "cairoint.h"
#include "cairo-scaled-font-subsets-private.h"
#include "cairo-truetype-subset-private.h"

/* CFF Dict Operators. If the high byte is 0 the command is encoded
 * with a single byte. */
#define BASEFONTNAME_OP  0x0c16
#define CHARSET_OP       0x000f
#define CHARSTRINGS_OP   0x0011
#define COPYRIGHT_OP     0x0c00
#define ENCODING_OP      0x0010
#define FAMILYNAME_OP    0x0003
#define FONTNAME_OP      0x0c26
#define FULLNAME_OP      0x0002
#define LOCAL_SUB_OP     0x0013
#define NOTICE_OP        0x0001
#define POSTSCRIPT_OP    0x0c15
#define PRIVATE_OP       0x0012
#define ROS_OP           0x0c1e
#define VERSION_OP       0x0000
#define WEIGHT_OP        0x0004

#define NUM_STD_STRINGS 391

#define ARRAY_LENGTH(a) ( (sizeof (a)) / (sizeof ((a)[0])) )

typedef struct _cff_header {
    uint8_t major;
    uint8_t minor;
    uint8_t header_size;
    uint8_t offset_size;
} cff_header_t;

typedef struct _cff_index_element {
    cairo_bool_t   is_copy;
    unsigned char *data;
    int            length;
} cff_index_element_t;

typedef struct _cff_dict_operator {
    cairo_hash_entry_t base;

    unsigned short operator;
    unsigned char *operand;
    int            operand_length;
    int            operand_offset;
} cff_dict_operator_t;

typedef struct _cairo_cff_font {

    cairo_scaled_font_subset_t *scaled_font_subset;
    const cairo_scaled_font_backend_t *backend;

    /* Font Data */
    unsigned char       *data;
    unsigned long        data_length;
    unsigned char       *current_ptr;
    unsigned char       *data_end;
    cff_header_t        *header;
    char                *font_name;
    cairo_hash_table_t  *top_dict;
    cairo_hash_table_t  *private_dict;
    cairo_array_t        strings_index;
    cairo_array_t        charstrings_index;
    cairo_array_t        global_sub_index;
    cairo_array_t        local_sub_index;
    int                  num_glyphs;

    /* Subsetted Font Data */
    char                *subset_font_name;
    cairo_array_t        charstrings_subset_index;
    cairo_array_t        strings_subset_index;
    cairo_array_t        output;

    /* Subset Metrics */
    int                 *widths;
    int                  x_min, y_min, x_max, y_max;
    int                  ascent, descent;

} cairo_cff_font_t;

#ifdef WORDS_BIGENDIAN

#define cpu_to_be16(v) (v)
#define be16_to_cpu(v) (v)
#define cpu_to_be32(v) (v)

#else

static inline uint16_t
cpu_to_be16(uint16_t v)
{
    return (v << 8) | (v >> 8);
}

static inline uint16_t
be16_to_cpu(uint16_t v)
{
    return cpu_to_be16 (v);
}

static inline uint32_t
cpu_to_be32(uint32_t v)
{
    return (cpu_to_be16 (v) << 16) | cpu_to_be16 (v >> 16);
}

#endif

/* Encoded integer using maximum sized encoding. This is required for
 * operands that are later modified after encoding. */
static unsigned char *
encode_integer_max (unsigned char *p, int i)
{
    *p++ = 29;
    *p++ = i >> 24;
    *p++ = (i >> 16) & 0xff;
    *p++ = (i >> 8)  & 0xff;
    *p++ = i & 0xff;
    return p;
}

static unsigned char *
encode_integer (unsigned char *p, int i)
{
    if (i >= -107 && i <= 107) {
        *p++ = i + 139;
    } else if (i >= 108 && i <= 1131) {
        i -= 108;
        *p++ = (i >> 8)+ 247;
        *p++ = i & 0xff;
    } else if (i >= -1131 && i <= -108) {
        i = -i - 108;
        *p++ = (i >> 8)+ 251;
        *p++ = i & 0xff;
    } else if (i >= -1131 && i <= -108) {
        *p++ = 28;
        *p++ = (i >> 8)  & 0xff;
        *p++ = i & 0xff;
    } else {
        p = encode_integer_max (p, i);
    }
    return p;
}

static unsigned char *
decode_integer (unsigned char *p, int *integer)
{
    if (*p == 28) {
        *integer = (int)(p[1]<<8 | p[2]);
        p += 3;
    } else if (*p == 29) {
        *integer = (int)((p[1] << 24) | (p[2] << 16) | (p[3] << 8) | p[4]);
        p += 5;
    } else if (*p >= 32 && *p <= 246) {
        *integer = *p++ - 139;
    } else if (*p <= 250) {
        *integer = (p[0] - 247) * 256 + p[1] + 108;
        p += 2;
    } else if (*p <= 254) {
        *integer = -(p[0] - 251) * 256 - p[1] - 108;
        p += 2;
    } else {
        *integer = 0;
        p += 1;
    }
    return p;
}

static unsigned char *
decode_operator (unsigned char *p, unsigned short *operator)
{
    unsigned short op = 0;

    op = *p++;
    if (op == 12) {
        op <<= 8;
        op |= *p++;
    }
    *operator = op;
    return p;
}

/* return 0 if not an operand */
static int
operand_length (unsigned char *p)
{
    unsigned char *begin = p;

    if (*p == 28)
        return 3;

    if (*p == 29)
        return 5;

    if (*p >= 32 && *p <= 246)
        return 1;

    if (*p >= 247 && *p <= 254)
        return 2;

    if (*p == 30) {
        while ((*p & 0x0f) != 0x0f)
            p++;
        return p - begin + 1;
    }

    return 0;
}

static unsigned char *
encode_index_offset (unsigned char *p, int offset_size, unsigned long offset)
{
    while (--offset_size >= 0) {
        p[offset_size] = (unsigned char) (offset & 0xff);
        offset >>= 8;
    }
    return p + offset_size;
}

static unsigned long
decode_index_offset(unsigned char *p, int off_size)
{
    unsigned long offset = 0;

    while (off_size-- > 0)
        offset = offset*256 + *p++;
    return offset;
}

static void
cff_index_init (cairo_array_t *index)
{
    _cairo_array_init (index, sizeof (cff_index_element_t));
}

static cairo_int_status_t
cff_index_read (cairo_array_t *index, unsigned char **ptr, unsigned char *end_ptr)
{
    cff_index_element_t element;
    unsigned char *data, *p;
    cairo_status_t status;
    int offset_size, count, start, i;
    int end = 0;

    p = *ptr;
    if (p + 2 > end_ptr)
        return CAIRO_INT_STATUS_UNSUPPORTED;
    count = be16_to_cpu( *((uint16_t *)p) );
    p += 2;
    if (count > 0) {
        offset_size = *p++;
        if (p + (count + 1)*offset_size > end_ptr)
            return CAIRO_INT_STATUS_UNSUPPORTED;
        data = p + offset_size*(count + 1) - 1;
        start = decode_index_offset (p, offset_size);
        p += offset_size;
        for (i = 0; i < count; i++) {
            end = decode_index_offset (p, offset_size);
            p += offset_size;
            if (p > end_ptr)
                return CAIRO_INT_STATUS_UNSUPPORTED;
            element.length = end - start;
            element.is_copy = FALSE;
            element.data = data + start;
            status = _cairo_array_append (index, &element);
            if (status)
                return status;
            start = end;
        }
        p = data + end;
    }
    *ptr = p;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cff_index_write (cairo_array_t *index, cairo_array_t *output)
{
    int offset_size;
    int offset;
    int num_elem;
    int i;
    cff_index_element_t *element;
    uint16_t count;
    unsigned char buf[5];
    cairo_status_t status;

    num_elem = _cairo_array_num_elements (index);
    count = cpu_to_be16 ((uint16_t) num_elem);
    status = _cairo_array_append_multiple (output, &count, 2);
    if (status)
        return status;

    if (num_elem == 0)
        return CAIRO_STATUS_SUCCESS;

    /* Find maximum offset to determine offset size */
    offset = 1;
    for (i = 0; i < num_elem; i++) {
        element = _cairo_array_index (index, i);
        offset += element->length;
    }
    if (offset < 0x100)
        offset_size = 1;
    else if (offset < 0x10000)
        offset_size = 2;
    else if (offset < 0x1000000)
        offset_size = 3;
    else
        offset_size = 4;

    buf[0] = (unsigned char) offset_size;
    status = _cairo_array_append (output, buf);
    if (status)
        return status;

    offset = 1;
    encode_index_offset (buf, offset_size, offset);
    status = _cairo_array_append_multiple (output, buf, offset_size);
    if (status)
        return status;

    for (i = 0; i < num_elem; i++) {
        element = _cairo_array_index (index, i);
        offset += element->length;
        encode_index_offset (buf, offset_size, offset);
        status = _cairo_array_append_multiple (output, buf, offset_size);
        if (status)
            return status;

    }

    for (i = 0; i < num_elem; i++) {
        element = _cairo_array_index (index, i);
        status = _cairo_array_append_multiple (output,
                                               element->data,
                                               element->length);
        if (status)
            return status;
    }
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cff_index_append (cairo_array_t *index, unsigned char *object , int length)
{
    cff_index_element_t element;

    element.length = length;
    element.is_copy = FALSE;
    element.data = object;

    return _cairo_array_append (index, &element);
}

static cairo_status_t
cff_index_append_copy (cairo_array_t *index, unsigned char *object , int length)
{
    cff_index_element_t element;

    element.length = length;
    element.is_copy = TRUE;
    element.data = malloc (element.length);
    if (element.data == NULL)
        return CAIRO_STATUS_NO_MEMORY;
    memcpy (element.data, object, element.length);

    return _cairo_array_append (index, &element);
}

static void
cff_index_fini (cairo_array_t *index)
{
    cff_index_element_t *element;
    int i;

    for (i = 0; i < _cairo_array_num_elements (index); i++) {
        element = _cairo_array_index (index, i);
        if (element->is_copy)
            free (element->data);
    }
    _cairo_array_fini (index);
}

static cairo_bool_t
_cairo_cff_dict_equal (const void *key_a, const void *key_b)
{
    const cff_dict_operator_t *op_a = key_a;
    const cff_dict_operator_t *op_b = key_b;

    return op_a->operator == op_b->operator;
}

static void
cff_dict_init (cairo_hash_table_t **dict)
{
    *dict = _cairo_hash_table_create (_cairo_cff_dict_equal);
}

static void
_cairo_dict_init_key (cff_dict_operator_t *key, int operator)
{
    key->base.hash = (unsigned long) operator;
    key->operator = operator;
}

static cff_dict_operator_t *
cff_dict_create_operator (int            operator,
                          unsigned char *operand,
                          int            operand_length)
{
    cff_dict_operator_t *op;

    op = malloc (sizeof (cff_dict_operator_t));
    if (op == NULL)
        return NULL;
    _cairo_dict_init_key (op, operator);
    op->operand = malloc (operand_length);
    if (op->operand == NULL) {
        free (op);
        return NULL;
    }
    memcpy (op->operand, operand, operand_length);
    op->operand_length = operand_length;
    op->operand_offset = -1;

    return op;
}

static cairo_status_t
cff_dict_read (cairo_hash_table_t *dict, unsigned char *p, int dict_size)
{
    unsigned char *end;
    cairo_array_t operands;
    cff_dict_operator_t *op;
    unsigned short operator;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    int size;

    end = p + dict_size;
    _cairo_array_init (&operands, 1);
    while (p < end) {
        size = operand_length (p);
        if (size != 0) {
            status = _cairo_array_append_multiple (&operands, p, size);
            if (status)
                goto fail;
            p += size;
        } else {
            p = decode_operator (p, &operator);
            op = cff_dict_create_operator (operator,
                                           _cairo_array_index (&operands, 0),
                                           _cairo_array_num_elements (&operands));
            if (op == NULL) {
                status = CAIRO_STATUS_NO_MEMORY;
                goto fail;
            }
            status = _cairo_hash_table_insert (dict, &op->base);
            if (status)
                goto fail;
            _cairo_array_truncate (&operands, 0);
        }
    }

fail:
    _cairo_array_fini (&operands);

    return status;
}

static void
cff_dict_remove (cairo_hash_table_t *dict, unsigned short operator)
{
    cff_dict_operator_t key, *op;

    _cairo_dict_init_key (&key, operator);
    if (_cairo_hash_table_lookup (dict, &key.base,
                                  (cairo_hash_entry_t **) &op))
    {
        free (op->operand);
        _cairo_hash_table_remove (dict, (cairo_hash_entry_t *) op);
    }
}

static unsigned char *
cff_dict_get_operands (cairo_hash_table_t *dict,
                       unsigned short      operator,
                       int                *size)
{
    cff_dict_operator_t key, *op;

    _cairo_dict_init_key (&key, operator);
    if (_cairo_hash_table_lookup (dict, &key.base,
                                  (cairo_hash_entry_t **) &op))
    {
        *size = op->operand_length;
        return op->operand;
    }

    return NULL;
}

static cairo_status_t
cff_dict_set_operands (cairo_hash_table_t *dict,
                       unsigned short      operator,
                       unsigned char      *operand,
                       int                 size)
{
    cff_dict_operator_t key, *op;
    cairo_status_t status;

    _cairo_dict_init_key (&key, operator);
    if (_cairo_hash_table_lookup (dict, &key.base,
                                  (cairo_hash_entry_t **) &op))
    {
        free (op->operand);
        op->operand = malloc (size);
        if (op->operand == NULL)
            return CAIRO_STATUS_NO_MEMORY;
        memcpy (op->operand, operand, size);
        op->operand_length = size;
    }
    else
    {
        op = cff_dict_create_operator (operator, operand, size);
        if (op == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
	status = _cairo_hash_table_insert (dict, &op->base);
	if (status)
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static int
cff_dict_get_location (cairo_hash_table_t *dict,
                       unsigned short      operator,
                       int                *size)
{
    cff_dict_operator_t key, *op;

    _cairo_dict_init_key (&key, operator);
    if (_cairo_hash_table_lookup (dict, &key.base,
                                  (cairo_hash_entry_t **) &op))
    {
        *size = op->operand_length;
        return op->operand_offset;
    }

    return -1;
}

typedef struct _dict_write_info {
    cairo_array_t *output;
    cairo_status_t status;
} dict_write_info_t;

static void
_cairo_dict_collect (void *entry, void *closure)
{
    dict_write_info_t *write_info = closure;
    cff_dict_operator_t *op = entry;
    unsigned char data;

    if (write_info->status)
        return;

    op->operand_offset = _cairo_array_num_elements (write_info->output);
    write_info->status = _cairo_array_append_multiple (write_info->output,
                                                       op->operand,
                                                       op->operand_length);
    if (write_info->status)
        return;

    if (op->operator & 0xff00) {
        data = op->operator >> 8;
        write_info->status = _cairo_array_append (write_info->output, &data);
        if (write_info->status)
            return;
    }
    data = op->operator & 0xff;
    write_info->status = _cairo_array_append (write_info->output, &data);
}

static cairo_status_t
cff_dict_write (cairo_hash_table_t *dict, cairo_array_t *output)
{
    dict_write_info_t write_info;

    write_info.output = output;
    write_info.status = CAIRO_STATUS_SUCCESS;
    _cairo_hash_table_foreach (dict, _cairo_dict_collect, &write_info);

    return write_info.status;
}

static void
cff_dict_fini (cairo_hash_table_t *dict)
{
    cff_dict_operator_t *entry;

    while (1) {
	entry = _cairo_hash_table_random_entry (dict, NULL);
	if (entry == NULL)
	    break;
        free (entry->operand);
        _cairo_hash_table_remove (dict, (cairo_hash_entry_t *) entry);
        free (entry);
    }
    _cairo_hash_table_destroy (dict);
}

static cairo_int_status_t
cairo_cff_font_read_header (cairo_cff_font_t *font)
{
    if (font->data_length < sizeof (cff_header_t))
        return CAIRO_INT_STATUS_UNSUPPORTED;

    font->header = (cff_header_t *) font->data;
    font->current_ptr = font->data + font->header->header_size;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_cff_font_read_name (cairo_cff_font_t *font)
{
    cairo_array_t index;
    cairo_int_status_t status;

    /* The original font name is not used in the subset. Read the name
     * index to skip over it. */
    cff_index_init (&index);
    status = cff_index_read (&index, &font->current_ptr, font->data_end);
    cff_index_fini (&index);

    return status;
}

static cairo_int_status_t
cairo_cff_font_read_private_dict (cairo_cff_font_t   *font,
                                  unsigned char      *ptr,
                                  int                 size)
{
    unsigned char buf[10];
    unsigned char *end_buf;
    int offset;
    int i;
    unsigned char *operand;
    unsigned char *p;

    cff_dict_read (font->private_dict, ptr, size);
    operand = cff_dict_get_operands (font->private_dict, LOCAL_SUB_OP, &i);
    if (operand) {
        decode_integer (operand, &offset);
        p = ptr + offset;
        cff_index_read (&font->local_sub_index, &p, font->data_end);

        /* Use maximum sized encoding to reserve space for later modification. */
        end_buf = encode_integer_max (buf, 0);
        cff_dict_set_operands (font->private_dict, LOCAL_SUB_OP, buf, end_buf - buf);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_cff_font_read_top_dict (cairo_cff_font_t *font)
{
    cairo_array_t index;
    cff_index_element_t *element;
    unsigned char buf[20];
    unsigned char *end_buf;
    unsigned char *operand;
    cairo_int_status_t status;
    unsigned char *p;
    int size;
    int offset;

    cff_index_init (&index);
    status = cff_index_read (&index, &font->current_ptr, font->data_end);
    if (status)
        goto fail;

    element = _cairo_array_index (&index, 0);
    cff_dict_read (font->top_dict, element->data, element->length);

    /* CID fonts are NYI */
    if (cff_dict_get_operands (font->top_dict, ROS_OP, &size) != NULL) {
        status = CAIRO_INT_STATUS_UNSUPPORTED;
        goto fail;
    }

    operand = cff_dict_get_operands (font->top_dict, CHARSTRINGS_OP, &size);
    decode_integer (operand, &offset);
    p = font->data + offset;
    status = cff_index_read (&font->charstrings_index, &p, font->data_end);
    if (status)
        goto fail;
    font->num_glyphs = _cairo_array_num_elements (&font->charstrings_index);

    operand = cff_dict_get_operands (font->top_dict, PRIVATE_OP, &size);
    operand = decode_integer (operand, &size);
    decode_integer (operand, &offset);
    cairo_cff_font_read_private_dict (font, font->data + offset, size);

    cff_dict_remove (font->top_dict, CHARSET_OP);

    /* Use maximum sized encoding to reserve space for later modification. */
    end_buf = encode_integer_max (buf, 0);
    cff_dict_set_operands (font->top_dict, CHARSTRINGS_OP, buf, end_buf - buf);
    cff_dict_set_operands (font->top_dict, ENCODING_OP, buf, end_buf - buf);
    /* Private has two operands - size and offset */
    end_buf = encode_integer_max (end_buf, 0);
    cff_dict_set_operands (font->top_dict, PRIVATE_OP, buf, end_buf - buf);

fail:
    cff_index_fini (&index);

    return status;
}

static cairo_int_status_t
cairo_cff_font_read_strings (cairo_cff_font_t *font)
{
    return cff_index_read (&font->strings_index, &font->current_ptr, font->data_end);
}

static cairo_int_status_t
cairo_cff_font_read_global_subroutines (cairo_cff_font_t *font)
{
    return cff_index_read (&font->global_sub_index, &font->current_ptr, font->data_end);
}

typedef cairo_int_status_t
(*font_read_t) (cairo_cff_font_t *font);

static const font_read_t font_read_funcs[] = {
    cairo_cff_font_read_header,
    cairo_cff_font_read_name,
    cairo_cff_font_read_top_dict,
    cairo_cff_font_read_strings,
    cairo_cff_font_read_global_subroutines,
};

static cairo_int_status_t
cairo_cff_font_read_font (cairo_cff_font_t *font)
{
    cairo_int_status_t status;
    unsigned int i;

    for (i = 0; i < ARRAY_LENGTH (font_read_funcs); i++) {
        status = font_read_funcs[i] (font);
        if (status)
            return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_cff_font_subset_dict_string(cairo_cff_font_t   *font,
                                  cairo_hash_table_t *dict,
                                  int                 operator)
{
    int size;
    unsigned char *p;
    int sid;
    unsigned char buf[100];
    cff_index_element_t *element;
    cairo_status_t status;

    p = cff_dict_get_operands (dict, operator, &size);
    if (!p)
        return CAIRO_STATUS_SUCCESS;

    decode_integer (p, &sid);
    if (sid < NUM_STD_STRINGS)
        return CAIRO_STATUS_SUCCESS;

    element = _cairo_array_index (&font->strings_index, sid - NUM_STD_STRINGS);
    sid = NUM_STD_STRINGS + _cairo_array_num_elements (&font->strings_subset_index);
    status = cff_index_append (&font->strings_subset_index, element->data, element->length);
    if (status)
        return status;

    p = encode_integer (buf, sid);
    cff_dict_set_operands (dict, operator, buf, p - buf);

    return CAIRO_STATUS_SUCCESS;
}

static const int dict_strings[] = {
    VERSION_OP,
    NOTICE_OP,
    COPYRIGHT_OP,
    FULLNAME_OP,
    FAMILYNAME_OP,
    WEIGHT_OP,
    POSTSCRIPT_OP,
    BASEFONTNAME_OP,
    FONTNAME_OP,
};

static cairo_status_t
cairo_cff_font_subset_dict_strings (cairo_cff_font_t   *font,
                                    cairo_hash_table_t *dict)
{
    cairo_status_t status;
    unsigned int i;

    for (i = 0; i < ARRAY_LENGTH (dict_strings); i++) {
        status = cairo_cff_font_subset_dict_string (font, dict, dict_strings[i]);
        if (status)
            return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_cff_font_subset_strings (cairo_cff_font_t *font)
{
    cairo_status_t status;

    status = cairo_cff_font_subset_dict_strings (font, font->top_dict);
    if (status)
        return status;

    status = cairo_cff_font_subset_dict_strings (font, font->private_dict);

    return status;
}

static cairo_status_t
cairo_cff_font_subset_charstrings (cairo_cff_font_t  *font)
{
    cff_index_element_t *element;
    unsigned int i;
    cairo_status_t status;

    /* add .notdef */
    element = _cairo_array_index (&font->charstrings_index, 0);
    status = cff_index_append (&font->charstrings_subset_index,
                               element->data,
                               element->length);
    if (status)
        return status;

    for (i = 0; i < font->scaled_font_subset->num_glyphs; i++) {
        element = _cairo_array_index (&font->charstrings_index,
                                      font->scaled_font_subset->glyphs[i]);
        status = cff_index_append (&font->charstrings_subset_index,
                                   element->data,
                                   element->length);
        if (status)
            return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_cff_font_subset_font (cairo_cff_font_t  *font)
{
    cairo_status_t status;

    /* TODO: subset subroutines */

    status = cairo_cff_font_subset_strings (font);
    if (status)
        return status;

    status = cairo_cff_font_subset_charstrings (font);

    return status;
}

/* Set the operand of the specified operator in the (already written)
 * top dict to point to the current position in the output
 * array. Operands updated with this function must have previously
 * been encoded with the 5-byte (max) integer encoding. */
static void
cairo_cff_font_set_topdict_operator_to_cur_pos (cairo_cff_font_t  *font,
                                                int                operator)
{
    int cur_pos;
    int offset;
    int size;
    unsigned char buf[10];
    unsigned char *buf_end;
    unsigned char *op_ptr;

    cur_pos = _cairo_array_num_elements (&font->output);
    buf_end = encode_integer_max (buf, cur_pos);
    offset = cff_dict_get_location (font->top_dict, operator, &size);
    assert (offset > 0);
    op_ptr = _cairo_array_index (&font->output, offset);
    memcpy (op_ptr, buf, buf_end - buf);
}

static cairo_status_t
cairo_cff_font_write_header (cairo_cff_font_t *font)
{
    return _cairo_array_append_multiple (&font->output,
                                         font->header,
                                         font->header->header_size);
}

static cairo_status_t
cairo_cff_font_write_name (cairo_cff_font_t *font)
{
    cairo_array_t index;
    cairo_status_t status;

    cff_index_init (&index);
    status = cff_index_append_copy (&index,
                                    (unsigned char *) font->subset_font_name,
                                    strlen(font->subset_font_name));
    if (status)
        return status;
    status = cff_index_write (&index, &font->output);
    if (status)
        return status;
    cff_index_fini (&index);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_cff_font_write_top_dict (cairo_cff_font_t *font)
{
    uint16_t count;
    unsigned char buf[10];
    unsigned char *p;
    int offset_index;
    int dict_start, dict_size;
    int offset_size = 4;
    cairo_status_t status;

    /* Write an index containing the top dict */

    count = cpu_to_be16 (1);
    status = _cairo_array_append_multiple (&font->output, &count, 2);
    if (status)
        return status;
    buf[0] = offset_size;
    status = _cairo_array_append (&font->output, buf);
    if (status)
        return status;
    encode_index_offset (buf, offset_size, 1);
    status = _cairo_array_append_multiple (&font->output, buf, offset_size);
    if (status)
        return status;

    /* Reserve space for last element of offset array and update after
     * dict is written */
    offset_index = _cairo_array_num_elements (&font->output);
    status = _cairo_array_append_multiple (&font->output, buf, offset_size);
    if (status)
        return status;

    dict_start = _cairo_array_num_elements (&font->output);
    status = cff_dict_write (font->top_dict, &font->output);
    if (status)
        return status;
    dict_size = _cairo_array_num_elements (&font->output) - dict_start;

    encode_index_offset (buf, offset_size, dict_size + 1);
    p = _cairo_array_index (&font->output, offset_index);
    memcpy (p, buf, offset_size);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_cff_font_write_strings (cairo_cff_font_t  *font)
{
    return cff_index_write (&font->strings_subset_index, &font->output);
}

static cairo_status_t
cairo_cff_font_write_global_subrs (cairo_cff_font_t  *font)
{
    return cff_index_write (&font->global_sub_index, &font->output);
}

static cairo_status_t
cairo_cff_font_write_encoding (cairo_cff_font_t  *font)
{
    unsigned char buf[10];

    cairo_cff_font_set_topdict_operator_to_cur_pos (font, ENCODING_OP);
    buf[0] = 1; /* Format 1 */
    buf[1] = 1; /* Number of ranges */
    buf[2] = 0; /* First code in range */
    /* Codes left in range excluding first */
    buf[3] = font->scaled_font_subset->num_glyphs - 1;

    return _cairo_array_append_multiple (&font->output, buf, 4);
}

static cairo_status_t
cairo_cff_font_write_charstrings (cairo_cff_font_t  *font)
{
    cairo_cff_font_set_topdict_operator_to_cur_pos (font, CHARSTRINGS_OP);

    return cff_index_write (&font->charstrings_subset_index, &font->output);
}

static cairo_status_t
cairo_cff_font_write_private_dict_and_local_sub (cairo_cff_font_t *font)
{
    int offset, private_dict_offset;
    int size;
    unsigned char buf[10];
    unsigned char *buf_end;
    unsigned char *p;
    cairo_status_t status;

    /* Write private dict and update offset and size in top dict */
    private_dict_offset = _cairo_array_num_elements (&font->output);
    status = cff_dict_write (font->private_dict, &font->output);
    if (status)
        return status;
    size = _cairo_array_num_elements (&font->output) - private_dict_offset;
    /* private entry has two operands - size and offset */
    buf_end = encode_integer_max (buf, size);
    buf_end = encode_integer_max (buf_end, private_dict_offset);
    offset = cff_dict_get_location (font->top_dict, PRIVATE_OP, &size);
    assert (offset > 0);
    p = _cairo_array_index (&font->output, offset);
    memcpy (p, buf, buf_end - buf);

    if (_cairo_array_num_elements (&font->local_sub_index) > 0) {
        /* Write local subroutines and update offset in private
         * dict. Local subroutines offset is relative to start of
         * private dict */
        offset = _cairo_array_num_elements (&font->output) - private_dict_offset;
        buf_end = encode_integer_max (buf, offset);
        offset = cff_dict_get_location (font->private_dict, LOCAL_SUB_OP, &size);
        assert (offset > 0);
        p = _cairo_array_index (&font->output, offset);
        memcpy (p, buf, buf_end - buf);
        status = cff_index_write (&font->local_sub_index, &font->output);
        if (status)
            return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

typedef cairo_status_t
(*font_write_t) (cairo_cff_font_t *font);

static const font_write_t font_write_funcs[] = {
    cairo_cff_font_write_header,
    cairo_cff_font_write_name,
    cairo_cff_font_write_top_dict,
    cairo_cff_font_write_strings,
    cairo_cff_font_write_global_subrs,
    cairo_cff_font_write_encoding,
    cairo_cff_font_write_charstrings,
    cairo_cff_font_write_private_dict_and_local_sub,
};

static cairo_status_t
cairo_cff_font_write_subset (cairo_cff_font_t *font)
{
    cairo_int_status_t status;
    unsigned int i;

    for (i = 0; i < ARRAY_LENGTH (font_write_funcs); i++) {
        status = font_write_funcs[i] (font);
        if (status)
            return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_cff_font_generate (cairo_cff_font_t  *font,
                         const char       **data,
                         unsigned long     *length)
{
    cairo_int_status_t status;

    status = cairo_cff_font_read_font (font);
    if (status)
        return status;

    status = cairo_cff_font_subset_font (font);
    if (status)
        return status;

    status = cairo_cff_font_write_subset (font);
    if (status)
        return status;

    *data = _cairo_array_index (&font->output, 0);
    *length = _cairo_array_num_elements (&font->output);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_cff_font_create_set_widths (cairo_cff_font_t *font)
{
    unsigned long size;
    unsigned long long_entry_size;
    unsigned long short_entry_size;
    unsigned int i;
    tt_hhea_t hhea;
    int num_hmetrics;
    unsigned char buf[10];
    int glyph_index;
    cairo_int_status_t status;

    size = sizeof (tt_hhea_t);
    status = font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
                                                 TT_TAG_hhea, 0,
                                                 (unsigned char*) &hhea, &size);
    if (status)
        return status;
    num_hmetrics = be16_to_cpu (hhea.num_hmetrics);

    for (i = 0; i < font->scaled_font_subset->num_glyphs; i++) {
        glyph_index = font->scaled_font_subset->glyphs[i];
        long_entry_size = 2 * sizeof (int16_t);
        short_entry_size = sizeof (int16_t);
        if (glyph_index < num_hmetrics) {
            status = font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
                                                         TT_TAG_hmtx,
                                                         glyph_index * long_entry_size,
                                                         buf, &short_entry_size);
            if (status)
                return status;
        }
        else
        {
            status = font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
                                                         TT_TAG_hmtx,
                                                         (num_hmetrics - 1) * long_entry_size,
                                                         buf, &short_entry_size);
            if (status)
                return status;
        }
        font->widths[i] = be16_to_cpu (*((int16_t*)buf));
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_cff_font_create (cairo_scaled_font_subset_t  *scaled_font_subset,
                        cairo_cff_font_t           **font_return,
                        const char                  *subset_name)
{
    const cairo_scaled_font_backend_t *backend;
    cairo_status_t status;
    cairo_cff_font_t *font;
    tt_head_t head;
    tt_hhea_t hhea;
    tt_name_t *name;
    tt_name_record_t *record;
    unsigned long size, data_length;
    int i, j;

    backend = scaled_font_subset->scaled_font->backend;
    if (!backend->load_truetype_table)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    data_length = 0;
    status = backend->load_truetype_table( scaled_font_subset->scaled_font,
                                           TT_TAG_CFF, 0, NULL, &data_length);
    if (status)
        return status;

    size = sizeof (tt_head_t);
    status = backend->load_truetype_table (scaled_font_subset->scaled_font,
                                           TT_TAG_head, 0,
                                           (unsigned char *) &head, &size);
    if (status)
        return status;

    size = sizeof (tt_hhea_t);
    status = backend->load_truetype_table (scaled_font_subset->scaled_font,
                                           TT_TAG_hhea, 0,
                                           (unsigned char *) &hhea, &size);
    if (status)
        return status;

    size = 0;
    status = backend->load_truetype_table (scaled_font_subset->scaled_font,
                                           TT_TAG_hmtx, 0, NULL, &size);
    if (status)
        return status;

    size = 0;
    status = backend->load_truetype_table (scaled_font_subset->scaled_font,
                                           TT_TAG_name, 0, NULL, &size);
    if (status)
        return status;

    name = malloc (size);
    if (name == NULL)
        return CAIRO_STATUS_NO_MEMORY;
    status = backend->load_truetype_table (scaled_font_subset->scaled_font,
                                           TT_TAG_name, 0,
                                           (unsigned char *) name, &size);
    if (status)
        goto fail1;

    font = malloc (sizeof (cairo_cff_font_t));
    if (font == NULL) {
        status = CAIRO_STATUS_NO_MEMORY;
	goto fail1;
    }

    font->backend = backend;
    font->scaled_font_subset = scaled_font_subset;

    _cairo_array_init (&font->output, sizeof (char));
    status = _cairo_array_grow_by (&font->output, 4096);
    if (status)
	goto fail2;

    font->subset_font_name = strdup (subset_name);
    if (font->subset_font_name == NULL) {
        status = CAIRO_STATUS_NO_MEMORY;
	goto fail3;
    }
    font->x_min = (int16_t) be16_to_cpu (head.x_min);
    font->y_min = (int16_t) be16_to_cpu (head.y_min);
    font->x_max = (int16_t) be16_to_cpu (head.x_max);
    font->y_max = (int16_t) be16_to_cpu (head.y_max);
    font->ascent = (int16_t) be16_to_cpu (hhea.ascender);
    font->descent = (int16_t) be16_to_cpu (hhea.descender);

    /* Extract the font name from the name table. At present this
     * just looks for the Mac platform/Roman encoded font name. It
     * should be extended to use any suitable font name in the
     * name table. If the mac/roman font name is not found a
     * CairoFont-x-y name is created.
     */
    font->font_name = NULL;
    for (i = 0; i < be16_to_cpu(name->num_records); i++) {
        record = &(name->records[i]);
        if ((be16_to_cpu (record->platform) == 1) &&
            (be16_to_cpu (record->encoding) == 0) &&
            (be16_to_cpu (record->name) == 4)) {
            font->font_name = malloc (be16_to_cpu(record->length) + 1);
            if (font->font_name) {
                strncpy(font->font_name,
                        ((char*)name) + be16_to_cpu (name->strings_offset) + be16_to_cpu (record->offset),
                        be16_to_cpu (record->length));
                font->font_name[be16_to_cpu (record->length)] = 0;
            }
            break;
        }
    }

    if (font->font_name == NULL) {
        font->font_name = malloc (30);
        if (font->font_name == NULL) {
            status = CAIRO_STATUS_NO_MEMORY;
            goto fail4;
        }
        snprintf(font->font_name, 30, "CairoFont-%u-%u",
                 scaled_font_subset->font_id,
                 scaled_font_subset->subset_id);
    }

    for (i = 0, j = 0; font->font_name[j]; j++) {
	if (font->font_name[j] == ' ')
	    continue;
	font->font_name[i++] = font->font_name[j];
    }
    font->font_name[i] = '\0';

    font->widths = calloc (font->scaled_font_subset->num_glyphs, sizeof (int));
    if (font->widths == NULL) {
        status = CAIRO_STATUS_NO_MEMORY;
        goto fail5;
    }
    cairo_cff_font_create_set_widths (font);

    font->data_length = data_length;
    font->data = malloc (data_length);
    if (font->data == NULL) {
        status = CAIRO_STATUS_NO_MEMORY;
        goto fail6;
    }
    status = font->backend->load_truetype_table ( font->scaled_font_subset->scaled_font,
                                                  TT_TAG_CFF, 0, font->data,
                                                  &font->data_length);
    if (status)
        goto fail7;
    font->data_end = font->data + font->data_length;

    cff_dict_init (&font->top_dict);
    cff_dict_init (&font->private_dict);
    cff_index_init (&font->strings_index);
    cff_index_init (&font->charstrings_index);
    cff_index_init (&font->global_sub_index);
    cff_index_init (&font->local_sub_index);
    cff_index_init (&font->charstrings_subset_index);
    cff_index_init (&font->strings_subset_index);

    free (name);
    *font_return = font;

    return CAIRO_STATUS_SUCCESS;

fail7:
    free (font->data);
fail6:
    free (font->widths);
fail5:
    free (font->font_name);
fail4:
    free (font->subset_font_name);
fail3:
    _cairo_array_fini (&font->output);
fail2:
    free (font);
fail1:
    free (name);
    return status;
}

static void
cairo_cff_font_destroy (cairo_cff_font_t *font)
{
    free (font->widths);
    free (font->font_name);
    free (font->subset_font_name);
    _cairo_array_fini (&font->output);
    cff_dict_fini (font->top_dict);
    cff_dict_fini (font->private_dict);
    cff_index_fini (&font->strings_index);
    cff_index_fini (&font->charstrings_index);
    cff_index_fini (&font->global_sub_index);
    cff_index_fini (&font->local_sub_index);
    cff_index_fini (&font->charstrings_subset_index);
    cff_index_fini (&font->strings_subset_index);
    free (font);
}

cairo_status_t
_cairo_cff_subset_init (cairo_cff_subset_t          *cff_subset,
                        const char		    *subset_name,
                        cairo_scaled_font_subset_t  *font_subset)
{
    cairo_cff_font_t *font = NULL; /* squelch bogus compiler warning */
    cairo_status_t status;
    const char *data = NULL; /* squelch bogus compiler warning */
    unsigned long length = 0; /* squelch bogus compiler warning */
    unsigned int i;

    status = _cairo_cff_font_create (font_subset, &font, subset_name);
    if (status)
	return status;

    status = cairo_cff_font_generate (font, &data, &length);
    if (status)
	goto fail1;

    cff_subset->base_font = strdup (font->font_name);
    if (cff_subset->base_font == NULL)
	goto fail1;

    cff_subset->widths = calloc (sizeof (int), font->scaled_font_subset->num_glyphs);
    if (cff_subset->widths == NULL)
	goto fail2;
    for (i = 0; i < font->scaled_font_subset->num_glyphs; i++)
        cff_subset->widths[i] = font->widths[i];

    cff_subset->x_min = font->x_min;
    cff_subset->y_min = font->y_min;
    cff_subset->x_max = font->x_max;
    cff_subset->y_max = font->y_max;
    cff_subset->ascent = font->ascent;
    cff_subset->descent = font->descent;

    cff_subset->data = malloc (length);
    if (cff_subset->data == NULL)
	goto fail3;

    memcpy (cff_subset->data, data, length);
    cff_subset->data_length = length;

    cairo_cff_font_destroy (font);

    return CAIRO_STATUS_SUCCESS;

 fail3:
    free (cff_subset->widths);
 fail2:
    free (cff_subset->base_font);
 fail1:
    cairo_cff_font_destroy (font);

    return status;
}

void
_cairo_cff_subset_fini (cairo_cff_subset_t *subset)
{
    free (subset->base_font);
    free (subset->widths);
    free (subset->data);
}
