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
 *      Eugeniy Meshcheryakov <eugen@debian.org>
 */

/*
 * Useful links:
 * http://www.adobe.com/devnet/font/pdfs/5176.CFF.pdf
 */

#define _BSD_SOURCE /* for snprintf(), strdup() */
#include "cairoint.h"

#if CAIRO_HAS_FONT_SUBSET

#include "cairo-scaled-font-subsets-private.h"
#include "cairo-truetype-subset-private.h"
#include <string.h>

/* CFF Dict Operators. If the high byte is 0 the command is encoded
 * with a single byte. */
#define BASEFONTNAME_OP  0x0c16
#define CIDCOUNT_OP      0x0c22
#define CHARSET_OP       0x000f
#define CHARSTRINGS_OP   0x0011
#define COPYRIGHT_OP     0x0c00
#define ENCODING_OP      0x0010
#define FAMILYNAME_OP    0x0003
#define FDARRAY_OP       0x0c24
#define FDSELECT_OP      0x0c25
#define FONTBBOX_OP      0x0005
#define FONTNAME_OP      0x0c26
#define FULLNAME_OP      0x0002
#define LOCAL_SUB_OP     0x0013
#define NOTICE_OP        0x0001
#define POSTSCRIPT_OP    0x0c15
#define PRIVATE_OP       0x0012
#define ROS_OP           0x0c1e
#define UNIQUEID_OP      0x000d
#define VERSION_OP       0x0000
#define WEIGHT_OP        0x0004
#define XUID_OP          0x000e

#define NUM_STD_STRINGS 391

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
    cairo_bool_t         is_cid;

    /* CID Font Data */
    int                 *fdselect;
    unsigned int         num_fontdicts;
    cairo_hash_table_t **fd_dict;
    cairo_hash_table_t **fd_private_dict;
    cairo_array_t       *fd_local_sub_index;

    /* Subsetted Font Data */
    char                *subset_font_name;
    cairo_array_t        charstrings_subset_index;
    cairo_array_t        strings_subset_index;
    int                 *fdselect_subset;
    unsigned int         num_subset_fontdicts;
    int                 *fd_subset_map;
    int                 *private_dict_offset;
    cairo_array_t        output;

    /* Subset Metrics */
    int                 *widths;
    int                  x_min, y_min, x_max, y_max;
    int                  ascent, descent;

} cairo_cff_font_t;

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
    } else if (i >= -32768 && i <= 32767) {
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
cff_index_append_copy (cairo_array_t *index,
                       const unsigned char *object,
                       unsigned int length)
{
    cff_index_element_t element;
    cairo_status_t status;

    element.length = length;
    element.is_copy = TRUE;
    element.data = malloc (element.length);
    if (element.data == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    memcpy (element.data, object, element.length);

    status = _cairo_array_append (index, &element);
    if (status) {
	free (element.data);
	return status;
    }

    return CAIRO_STATUS_SUCCESS;
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

static cairo_status_t
cff_dict_init (cairo_hash_table_t **dict)
{
    *dict = _cairo_hash_table_create (_cairo_cff_dict_equal);
    if (*dict == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_dict_init_key (cff_dict_operator_t *key, int operator)
{
    key->base.hash = (unsigned long) operator;
    key->operator = operator;
}

static cairo_status_t
cff_dict_create_operator (int            operator,
                          unsigned char *operand,
                          int            operand_length,
			  cff_dict_operator_t **out)
{
    cff_dict_operator_t *op;

    op = malloc (sizeof (cff_dict_operator_t));
    if (op == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_dict_init_key (op, operator);
    op->operand = malloc (operand_length);
    if (op->operand == NULL) {
        free (op);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    memcpy (op->operand, operand, operand_length);
    op->operand_length = operand_length;
    op->operand_offset = -1;

    *out = op;
    return CAIRO_STATUS_SUCCESS;
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
            status = cff_dict_create_operator (operator,
                                          _cairo_array_index (&operands, 0),
                                          _cairo_array_num_elements (&operands),
					  &op);
            if (status)
                goto fail;

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
        free (op);
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
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

        memcpy (op->operand, operand, size);
        op->operand_length = size;
    }
    else
    {
        status = cff_dict_create_operator (operator, operand, size, &op);
        if (status)
	    return status;

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
cairo_dict_write_operator (cff_dict_operator_t *op, dict_write_info_t *write_info)
{
    unsigned char data;

    op->operand_offset = _cairo_array_num_elements (write_info->output);
    write_info->status = _cairo_array_append_multiple (write_info->output, op->operand, op->operand_length);
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

static void
_cairo_dict_collect (void *entry, void *closure)
{
    dict_write_info_t   *write_info = closure;
    cff_dict_operator_t *op = entry;

    if (write_info->status)
        return;

    /* The ROS operator is handled separately in cff_dict_write() */
    if (op->operator != ROS_OP)
        cairo_dict_write_operator (op, write_info);
}

static cairo_status_t
cff_dict_write (cairo_hash_table_t *dict, cairo_array_t *output)
{
    dict_write_info_t write_info;
    cff_dict_operator_t key, *op;

    write_info.output = output;
    write_info.status = CAIRO_STATUS_SUCCESS;

    /* The CFF specification requires that the Top Dict of CID fonts
     * begin with the ROS operator. */
    _cairo_dict_init_key (&key, ROS_OP);
    if (_cairo_hash_table_lookup (dict, &key.base,
                                  (cairo_hash_entry_t **) &op))
        cairo_dict_write_operator (op, &write_info);

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
                                  cairo_hash_table_t *private_dict,
                                  cairo_array_t      *local_sub_index,
                                  unsigned char      *ptr,
                                  int                 size)
{
    cairo_int_status_t status;
    unsigned char buf[10];
    unsigned char *end_buf;
    int offset;
    int i;
    unsigned char *operand;
    unsigned char *p;

    status = cff_dict_read (private_dict, ptr, size);
    if (status)
	return status;

    operand = cff_dict_get_operands (private_dict, LOCAL_SUB_OP, &i);
    if (operand) {
        decode_integer (operand, &offset);
        p = ptr + offset;
        status = cff_index_read (local_sub_index, &p, font->data_end);
	if (status)
	    return status;

        /* Use maximum sized encoding to reserve space for later modification. */
        end_buf = encode_integer_max (buf, 0);
        status = cff_dict_set_operands (private_dict, LOCAL_SUB_OP, buf, end_buf - buf);
	if (status)
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_cff_font_read_fdselect (cairo_cff_font_t *font, unsigned char *p)
{
    int type, num_ranges, first, last, fd, i, j;

    font->fdselect = calloc (font->num_glyphs, sizeof (int));
    if (font->fdselect == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    type = *p++;
    if (type == 0)
    {
        for (i = 0; i < font->num_glyphs; i++)
            font->fdselect[i] = *p++;
    } else if (type == 3) {
        num_ranges = be16_to_cpu( *((uint16_t *)p) );
        p += 2;
        for  (i = 0; i < num_ranges; i++)
        {
            first = be16_to_cpu( *((uint16_t *)p) );
            p += 2;
            fd = *p++;
            last = be16_to_cpu( *((uint16_t *)p) );
            for (j = first; j < last; j++)
                font->fdselect[j] = fd;
        }
    } else {
        return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_cff_font_read_cid_fontdict (cairo_cff_font_t *font, unsigned char *ptr)
{
    cairo_array_t index;
    cff_index_element_t *element;
    unsigned int i;
    int size;
    unsigned char *operand;
    int offset;
    cairo_int_status_t status;
    unsigned char buf[100];
    unsigned char *end_buf;

    cff_index_init (&index);
    status = cff_index_read (&index, &ptr, font->data_end);
    if (status)
        goto fail;

    font->num_fontdicts = _cairo_array_num_elements (&index);

    font->fd_dict = calloc (sizeof (cairo_hash_table_t *), font->num_fontdicts);
    if (font->fd_dict == NULL) {
        status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
        goto fail;
    }

    font->fd_private_dict = calloc (sizeof (cairo_hash_table_t *), font->num_fontdicts);
    if (font->fd_private_dict == NULL) {
        status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
        goto fail;
    }

    font->fd_local_sub_index = calloc (sizeof (cairo_array_t), font->num_fontdicts);
    if (font->fd_local_sub_index == NULL) {
        status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
        goto fail;
    }

    for (i = 0; i < font->num_fontdicts; i++) {
        status = cff_dict_init (&font->fd_dict[i]);
        if (status)
            goto fail;

        element = _cairo_array_index (&index, i);
        status = cff_dict_read (font->fd_dict[i], element->data, element->length);
        if (status)
            goto fail;

        operand = cff_dict_get_operands (font->fd_dict[i], PRIVATE_OP, &size);
        if (operand == NULL) {
            status = CAIRO_INT_STATUS_UNSUPPORTED;
            goto fail;
        }
        operand = decode_integer (operand, &size);
        decode_integer (operand, &offset);
        status = cff_dict_init (&font->fd_private_dict[i]);
	if (status)
            goto fail;

        cff_index_init (&font->fd_local_sub_index[i]);
        status = cairo_cff_font_read_private_dict (font,
                                                   font->fd_private_dict[i],
                                                   &font->fd_local_sub_index[i],
                                                   font->data + offset,
                                                   size);
        if (status)
            goto fail;

        /* Set integer operand to max value to use max size encoding to reserve
         * space for any value later */
        end_buf = encode_integer_max (buf, 0);
        end_buf = encode_integer_max (end_buf, 0);
        status = cff_dict_set_operands (font->fd_dict[i], PRIVATE_OP, buf, end_buf - buf);
        if (status)
            goto fail;
    }

    return CAIRO_STATUS_SUCCESS;

fail:
    cff_index_fini (&index);

    return status;
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
    status = cff_dict_read (font->top_dict, element->data, element->length);
    if (status)
        goto fail;

    if (cff_dict_get_operands (font->top_dict, ROS_OP, &size) != NULL)
        font->is_cid = TRUE;
    else
        font->is_cid = FALSE;

    operand = cff_dict_get_operands (font->top_dict, CHARSTRINGS_OP, &size);
    decode_integer (operand, &offset);
    p = font->data + offset;
    status = cff_index_read (&font->charstrings_index, &p, font->data_end);
    if (status)
        goto fail;
    font->num_glyphs = _cairo_array_num_elements (&font->charstrings_index);

    if (font->is_cid) {
        operand = cff_dict_get_operands (font->top_dict, FDSELECT_OP, &size);
        decode_integer (operand, &offset);
        status = cairo_cff_font_read_fdselect (font, font->data + offset);
	if (status)
	    goto fail;

        operand = cff_dict_get_operands (font->top_dict, FDARRAY_OP, &size);
        decode_integer (operand, &offset);
        status = cairo_cff_font_read_cid_fontdict (font, font->data + offset);
	if (status)
	    goto fail;
    } else {
        operand = cff_dict_get_operands (font->top_dict, PRIVATE_OP, &size);
        operand = decode_integer (operand, &size);
        decode_integer (operand, &offset);
        status = cairo_cff_font_read_private_dict (font,
                                                   font->private_dict,
						   &font->local_sub_index,
						   font->data + offset,
						   size);
	if (status)
	    goto fail;
    }

    /* Use maximum sized encoding to reserve space for later modification. */
    end_buf = encode_integer_max (buf, 0);
    status = cff_dict_set_operands (font->top_dict,
	                            CHARSTRINGS_OP, buf, end_buf - buf);
    if (status)
	goto fail;

    status = cff_dict_set_operands (font->top_dict,
	                            FDSELECT_OP, buf, end_buf - buf);
    if (status)
	goto fail;

    status = cff_dict_set_operands (font->top_dict,
	                            FDARRAY_OP, buf, end_buf - buf);
    if (status)
	goto fail;

    status = cff_dict_set_operands (font->top_dict,
	                            CHARSET_OP, buf, end_buf - buf);
    if (status)
	goto fail;

    cff_dict_remove (font->top_dict, ENCODING_OP);
    cff_dict_remove (font->top_dict, PRIVATE_OP);

    /* Remove the unique identifier operators as the subsetted font is
     * not the same is the original font. */
    cff_dict_remove (font->top_dict, UNIQUEID_OP);
    cff_dict_remove (font->top_dict, XUID_OP);

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
cairo_cff_font_set_ros_strings (cairo_cff_font_t *font)
{
    cairo_status_t status;
    unsigned char buf[30];
    unsigned char *p;
    int sid1, sid2;
    const char *registry = "Adobe";
    const char *ordering = "Identity";

    sid1 = NUM_STD_STRINGS + _cairo_array_num_elements (&font->strings_subset_index);
    status = cff_index_append_copy (&font->strings_subset_index,
                                    (unsigned char *)registry,
                                    strlen(registry));
    if (status)
	return status;

    sid2 = NUM_STD_STRINGS + _cairo_array_num_elements (&font->strings_subset_index);
    status = cff_index_append_copy (&font->strings_subset_index,
                                    (unsigned char *)ordering,
				    strlen(ordering));
    if (status)
	return status;

    p = encode_integer (buf, sid1);
    p = encode_integer (p, sid2);
    p = encode_integer (p, 0);
    status = cff_dict_set_operands (font->top_dict, ROS_OP, buf, p - buf);
    if (status)
	return status;

    p = encode_integer (buf, font->scaled_font_subset->num_glyphs);
    status = cff_dict_set_operands (font->top_dict, CIDCOUNT_OP, buf, p - buf);
    if (status)
	return status;

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
    status = cff_dict_set_operands (dict, operator, buf, p - buf);
    if (status)
	return status;

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
cairo_cff_font_subset_charstrings (cairo_cff_font_t  *font)
{
    cff_index_element_t *element;
    unsigned int i;
    cairo_status_t status;

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
cairo_cff_font_subset_fontdict (cairo_cff_font_t  *font)
{
    unsigned int i;
    int fd;
    int *reverse_map;

    font->fdselect_subset = calloc (font->scaled_font_subset->num_glyphs,
                                     sizeof (int));
    if (font->fdselect_subset == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    font->fd_subset_map = calloc (font->num_fontdicts, sizeof (int));
    if (font->fd_subset_map == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    font->private_dict_offset = calloc (font->num_fontdicts, sizeof (int));
    if (font->private_dict_offset == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    reverse_map = calloc (font->num_fontdicts, sizeof (int));
    if (reverse_map == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    for (i = 0; i < font->num_fontdicts; i++)
        reverse_map[i] = -1;

    font->num_subset_fontdicts = 0;
    for (i = 0; i < font->scaled_font_subset->num_glyphs; i++) {
        fd = font->fdselect[font->scaled_font_subset->glyphs[i]];
        if (reverse_map[fd] < 0) {
            font->fd_subset_map[font->num_subset_fontdicts] = fd;
            reverse_map[fd] = font->num_subset_fontdicts++;
        }
        font->fdselect_subset[i] = reverse_map[fd];
    }

    free (reverse_map);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_cff_font_create_cid_fontdict (cairo_cff_font_t *font)
{
    unsigned char buf[100];
    unsigned char *end_buf;
    cairo_status_t status;

    font->num_fontdicts = 1;
    font->fd_dict = malloc (sizeof (cairo_hash_table_t *));
    if (font->fd_dict == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    if (cff_dict_init (&font->fd_dict[0])) {
	free (font->fd_dict);
	font->fd_dict = NULL;
	font->num_fontdicts = 0;
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    font->fd_subset_map = malloc (sizeof (int));
    if (font->fd_subset_map == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    font->private_dict_offset = malloc (sizeof (int));
    if (font->private_dict_offset == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    font->fd_subset_map[0] = 0;
    font->num_subset_fontdicts = 1;

    /* Set integer operand to max value to use max size encoding to reserve
     * space for any value later */
    end_buf = encode_integer_max (buf, 0);
    end_buf = encode_integer_max (end_buf, 0);
    status = cff_dict_set_operands (font->fd_dict[0], PRIVATE_OP, buf, end_buf - buf);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_cff_font_subset_strings (cairo_cff_font_t *font)
{
    cairo_status_t status;
    unsigned int i;

    status = cairo_cff_font_subset_dict_strings (font, font->top_dict);
    if (status)
        return status;

    if (font->is_cid) {
        for (i = 0; i < font->num_subset_fontdicts; i++) {
            status = cairo_cff_font_subset_dict_strings (font, font->fd_dict[font->fd_subset_map[i]]);
            if (status)
                return status;

            status = cairo_cff_font_subset_dict_strings (font, font->fd_private_dict[font->fd_subset_map[i]]);
            if (status)
                return status;
        }
    } else {
        status = cairo_cff_font_subset_dict_strings (font, font->private_dict);
    }

    return status;
}

static cairo_status_t
cairo_cff_font_subset_font (cairo_cff_font_t  *font)
{
    cairo_status_t status;

    status = cairo_cff_font_set_ros_strings (font);
    if (status)
	return status;

    status = cairo_cff_font_subset_charstrings (font);
    if (status)
        return status;

    if (font->is_cid)
        status = cairo_cff_font_subset_fontdict (font);
    else
        status = cairo_cff_font_create_cid_fontdict (font);
    if (status)
	return status;

    status = cairo_cff_font_subset_strings (font);
    if (status)
        return status;

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
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_array_t index;

    cff_index_init (&index);

    status = cff_index_append_copy (&index,
                                    (unsigned char *) font->subset_font_name,
                                    strlen(font->subset_font_name));
    if (status)
	goto FAIL;

    status = cff_index_write (&index, &font->output);
    if (status)
        goto FAIL;

FAIL:
    cff_index_fini (&index);

    return status;
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
cairo_cff_font_write_fdselect (cairo_cff_font_t  *font)
{
    unsigned char data;
    unsigned int i;
    cairo_int_status_t status;

    cairo_cff_font_set_topdict_operator_to_cur_pos (font, FDSELECT_OP);

    if (font->is_cid) {
        data = 0;
        status = _cairo_array_append (&font->output, &data);
        if (status)
            return status;

        for (i = 0; i < font->scaled_font_subset->num_glyphs; i++) {
            data = font->fdselect_subset[i];
            status = _cairo_array_append (&font->output, &data);
            if (status)
                return status;
        }
    } else {
        unsigned char byte;
        uint16_t word;

        status = _cairo_array_grow_by (&font->output, 9);
        if (status)
            return status;

        byte = 3;
        status = _cairo_array_append (&font->output, &byte);
        assert (status == CAIRO_STATUS_SUCCESS);

        word = cpu_to_be16 (1);
        status = _cairo_array_append_multiple (&font->output, &word, 2);
        assert (status == CAIRO_STATUS_SUCCESS);

        word = cpu_to_be16 (0);
        status = _cairo_array_append_multiple (&font->output, &word, 2);
        assert (status == CAIRO_STATUS_SUCCESS);

        byte = 0;
        status = _cairo_array_append (&font->output, &byte);
        assert (status == CAIRO_STATUS_SUCCESS);

        word = cpu_to_be16 (font->scaled_font_subset->num_glyphs);
        status = _cairo_array_append_multiple (&font->output, &word, 2);
        assert (status == CAIRO_STATUS_SUCCESS);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_cff_font_write_charset (cairo_cff_font_t  *font)
{
    unsigned char byte;
    uint16_t word;
    cairo_status_t status;

    cairo_cff_font_set_topdict_operator_to_cur_pos (font, CHARSET_OP);
    status = _cairo_array_grow_by (&font->output, 5);
    if (status)
        return status;

    byte = 2;
    status = _cairo_array_append (&font->output, &byte);
    assert (status == CAIRO_STATUS_SUCCESS);

    word = cpu_to_be16 (1);
    status = _cairo_array_append_multiple (&font->output, &word, 2);
    assert (status == CAIRO_STATUS_SUCCESS);

    word = cpu_to_be16 (font->scaled_font_subset->num_glyphs - 2);
    status = _cairo_array_append_multiple (&font->output, &word, 2);
    assert (status == CAIRO_STATUS_SUCCESS);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_cff_font_write_charstrings (cairo_cff_font_t  *font)
{
    cairo_cff_font_set_topdict_operator_to_cur_pos (font, CHARSTRINGS_OP);

    return cff_index_write (&font->charstrings_subset_index, &font->output);
}

static cairo_status_t
cairo_cff_font_write_cid_fontdict (cairo_cff_font_t *font)
{
    unsigned int i;
    cairo_int_status_t status;
    uint32_t *offset_array;
    int offset_base;
    uint16_t count;
    uint8_t offset_size = 4;

    cairo_cff_font_set_topdict_operator_to_cur_pos (font, FDARRAY_OP);
    count = cpu_to_be16 (font->num_subset_fontdicts);
    status = _cairo_array_append_multiple (&font->output, &count, sizeof (uint16_t));
    if (status)
        return status;
    status = _cairo_array_append (&font->output, &offset_size);
    if (status)
        return status;
    status = _cairo_array_allocate (&font->output,
                                    (font->num_subset_fontdicts + 1)*offset_size,
                                    (void **) &offset_array);
    if (status)
        return status;
    offset_base = _cairo_array_num_elements (&font->output) - 1;
    *offset_array++ = cpu_to_be32(1);
    for (i = 0; i < font->num_subset_fontdicts; i++) {
        status = cff_dict_write (font->fd_dict[font->fd_subset_map[i]],
                                 &font->output);
        if (status)
            return status;
        *offset_array++ = cpu_to_be32(_cairo_array_num_elements (&font->output) - offset_base);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_cff_font_write_private_dict (cairo_cff_font_t   *font,
                                   int                 dict_num,
                                   cairo_hash_table_t *parent_dict,
                                   cairo_hash_table_t *private_dict)
{
    int offset;
    int size;
    unsigned char buf[10];
    unsigned char *buf_end;
    unsigned char *p;
    cairo_status_t status;

    /* Write private dict and update offset and size in top dict */
    font->private_dict_offset[dict_num] = _cairo_array_num_elements (&font->output);
    status = cff_dict_write (private_dict, &font->output);
    if (status)
        return status;

    size = _cairo_array_num_elements (&font->output) - font->private_dict_offset[dict_num];
    /* private entry has two operands - size and offset */
    buf_end = encode_integer_max (buf, size);
    buf_end = encode_integer_max (buf_end, font->private_dict_offset[dict_num]);
    offset = cff_dict_get_location (parent_dict, PRIVATE_OP, &size);
    assert (offset > 0);
    p = _cairo_array_index (&font->output, offset);
    memcpy (p, buf, buf_end - buf);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_cff_font_write_local_sub (cairo_cff_font_t   *font,
                                int                 dict_num,
                                cairo_hash_table_t *private_dict,
                                cairo_array_t      *local_sub_index)
{
    int offset;
    int size;
    unsigned char buf[10];
    unsigned char *buf_end;
    unsigned char *p;
    cairo_status_t status;

    if (_cairo_array_num_elements (local_sub_index) > 0) {
        /* Write local subroutines and update offset in private
         * dict. Local subroutines offset is relative to start of
         * private dict */
        offset = _cairo_array_num_elements (&font->output) - font->private_dict_offset[dict_num];
        buf_end = encode_integer_max (buf, offset);
        offset = cff_dict_get_location (private_dict, LOCAL_SUB_OP, &size);
        assert (offset > 0);
        p = _cairo_array_index (&font->output, offset);
        memcpy (p, buf, buf_end - buf);
        status = cff_index_write (local_sub_index, &font->output);
        if (status)
            return status;
    }

    return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t
cairo_cff_font_write_cid_private_dict_and_local_sub (cairo_cff_font_t  *font)
{
    unsigned int i;
    cairo_int_status_t status;

    if (font->is_cid) {
        for (i = 0; i < font->num_subset_fontdicts; i++) {
            status = cairo_cff_font_write_private_dict (
                            font,
                            i,
                            font->fd_dict[font->fd_subset_map[i]],
                            font->fd_private_dict[font->fd_subset_map[i]]);
            if (status)
                return status;
        }

        for (i = 0; i < font->num_subset_fontdicts; i++) {
            status = cairo_cff_font_write_local_sub (
                            font,
                            i,
                            font->fd_private_dict[font->fd_subset_map[i]],
                           &font->fd_local_sub_index[font->fd_subset_map[i]]);
            if (status)
                return status;
        }
    } else {
        status = cairo_cff_font_write_private_dict (font,
                                                    0,
                                                    font->fd_dict[0],
                                                    font->private_dict);
	if (status)
	    return status;

        status = cairo_cff_font_write_local_sub (font,
                                                 0,
                                                 font->private_dict,
                                                 &font->local_sub_index);
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
    cairo_cff_font_write_charset,
    cairo_cff_font_write_fdselect,
    cairo_cff_font_write_charstrings,
    cairo_cff_font_write_cid_fontdict,
    cairo_cff_font_write_cid_private_dict_and_local_sub,
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

    for (i = 1; i < font->scaled_font_subset->num_glyphs; i++) {
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
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = backend->load_truetype_table (scaled_font_subset->scaled_font,
                                           TT_TAG_name, 0,
                                           (unsigned char *) name, &size);
    if (status)
        goto fail1;

    font = malloc (sizeof (cairo_cff_font_t));
    if (font == NULL) {
        status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
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
        status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto fail2;
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
            status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
            goto fail3;
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
        status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
        goto fail4;
    }

    status = cairo_cff_font_create_set_widths (font);
    if (status)
	goto fail5;

    font->data_length = data_length;
    font->data = malloc (data_length);
    if (font->data == NULL) {
        status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
        goto fail5;
    }
    status = font->backend->load_truetype_table ( font->scaled_font_subset->scaled_font,
                                                  TT_TAG_CFF, 0, font->data,
                                                  &font->data_length);
    if (status)
        goto fail6;

    font->data_end = font->data + font->data_length;

    status = cff_dict_init (&font->top_dict);
    if (status)
	goto fail6;

    status = cff_dict_init (&font->private_dict);
    if (status)
	goto fail7;

    cff_index_init (&font->strings_index);
    cff_index_init (&font->charstrings_index);
    cff_index_init (&font->global_sub_index);
    cff_index_init (&font->local_sub_index);
    cff_index_init (&font->charstrings_subset_index);
    cff_index_init (&font->strings_subset_index);
    font->fdselect = NULL;
    font->fd_dict = NULL;
    font->fd_private_dict = NULL;
    font->fd_local_sub_index = NULL;
    font->fdselect_subset = NULL;
    font->fd_subset_map = NULL;
    font->private_dict_offset = NULL;

    free (name);
    *font_return = font;

    return CAIRO_STATUS_SUCCESS;

fail7:
    _cairo_hash_table_destroy (font->top_dict);
fail6:
    free (font->data);
fail5:
    free (font->widths);
fail4:
    free (font->font_name);
fail3:
    free (font->subset_font_name);
fail2:
    _cairo_array_fini (&font->output);
    free (font);
fail1:
    free (name);
    return status;
}

static void
cairo_cff_font_destroy (cairo_cff_font_t *font)
{
    unsigned int i;

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

    /* If we bailed out early as a result of an error some of the
     * following cairo_cff_font_t members may still be NULL */
    if (font->fd_dict) {
        for (i = 0; i < font->num_fontdicts; i++) {
            if (font->fd_dict[i])
                cff_dict_fini (font->fd_dict[i]);
        }
        free (font->fd_dict);
    }
    if (font->fd_subset_map)
        free (font->fd_subset_map);
    if (font->private_dict_offset)
        free (font->private_dict_offset);

    if (font->is_cid) {
        if (font->fdselect)
            free (font->fdselect);
        if (font->fdselect_subset)
            free (font->fdselect_subset);
        if (font->fd_private_dict) {
            for (i = 0; i < font->num_fontdicts; i++) {
                if (font->fd_private_dict[i])
                    cff_dict_fini (font->fd_private_dict[i]);
            }
            free (font->fd_private_dict);
        }
        if (font->fd_local_sub_index) {
            for (i = 0; i < font->num_fontdicts; i++)
                cff_index_fini (&font->fd_local_sub_index[i]);
            free (font->fd_local_sub_index);
        }
    }

    if (font->data)
        free (font->data);

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
    if (cff_subset->base_font == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto fail1;
    }

    cff_subset->widths = calloc (sizeof (int), font->scaled_font_subset->num_glyphs);
    if (cff_subset->widths == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto fail2;
    }
    for (i = 0; i < font->scaled_font_subset->num_glyphs; i++)
        cff_subset->widths[i] = font->widths[i];

    cff_subset->x_min = font->x_min;
    cff_subset->y_min = font->y_min;
    cff_subset->x_max = font->x_max;
    cff_subset->y_max = font->y_max;
    cff_subset->ascent = font->ascent;
    cff_subset->descent = font->descent;

    cff_subset->data = malloc (length);
    if (cff_subset->data == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto fail3;
    }

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

static cairo_int_status_t
_cairo_cff_font_fallback_create (cairo_scaled_font_subset_t  *scaled_font_subset,
                                 cairo_cff_font_t           **font_return,
                                 const char                  *subset_name)
{
    cairo_status_t status;
    cairo_cff_font_t *font;

    font = malloc (sizeof (cairo_cff_font_t));
    if (font == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    font->backend = NULL;
    font->scaled_font_subset = scaled_font_subset;

    _cairo_array_init (&font->output, sizeof (char));
    status = _cairo_array_grow_by (&font->output, 4096);
    if (status)
	goto fail1;

    font->subset_font_name = strdup (subset_name);
    if (font->subset_font_name == NULL) {
        status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto fail1;
    }

    font->font_name = strdup (subset_name);
    if (font->subset_font_name == NULL) {
        status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto fail2;
    }

    font->x_min = 0;
    font->y_min = 0;
    font->x_max = 0;
    font->y_max = 0;
    font->ascent = 0;
    font->descent = 0;

    font->widths = calloc (font->scaled_font_subset->num_glyphs, sizeof (int));
    if (font->widths == NULL) {
        status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
        goto fail3;
    }

    font->data_length = 0;
    font->data = NULL;
    font->data_end = NULL;

    status = cff_dict_init (&font->top_dict);
    if (status)
	goto fail4;

    status = cff_dict_init (&font->private_dict);
    if (status)
	goto fail5;

    cff_index_init (&font->strings_index);
    cff_index_init (&font->charstrings_index);
    cff_index_init (&font->global_sub_index);
    cff_index_init (&font->local_sub_index);
    cff_index_init (&font->charstrings_subset_index);
    cff_index_init (&font->strings_subset_index);
    font->fdselect = NULL;
    font->fd_dict = NULL;
    font->fd_private_dict = NULL;
    font->fd_local_sub_index = NULL;
    font->fdselect_subset = NULL;
    font->fd_subset_map = NULL;
    font->private_dict_offset = NULL;

    *font_return = font;

    return CAIRO_STATUS_SUCCESS;

fail5:
    _cairo_hash_table_destroy (font->top_dict);
fail4:
    free (font->widths);
fail3:
    free (font->font_name);
fail2:
    free (font->subset_font_name);
fail1:
    _cairo_array_fini (&font->output);
    free (font);
    return status;
}

static cairo_int_status_t
cairo_cff_font_fallback_generate (cairo_cff_font_t           *font,
                                  cairo_type2_charstrings_t  *type2_subset,
                                  const char                **data,
                                  unsigned long              *length)
{
    cairo_int_status_t status;
    cff_header_t header;
    cairo_array_t *charstring;
    unsigned char buf[40];
    unsigned char *end_buf;
    unsigned int i;

    /* Create header */
    header.major = 1;
    header.minor = 0;
    header.header_size = 4;
    header.offset_size = 4;
    font->header = &header;

    /* Create Top Dict */
    font->is_cid = FALSE;
    end_buf = encode_integer (buf, type2_subset->x_min);
    end_buf = encode_integer (end_buf, type2_subset->y_min);
    end_buf = encode_integer (end_buf, type2_subset->x_max);
    end_buf = encode_integer (end_buf, type2_subset->y_max);
    status = cff_dict_set_operands (font->top_dict,
	                            FONTBBOX_OP, buf, end_buf - buf);
    if (status)
	return status;

    end_buf = encode_integer_max (buf, 0);
    status = cff_dict_set_operands (font->top_dict,
	                            CHARSTRINGS_OP, buf, end_buf - buf);
    if (status)
	return status;

    status = cff_dict_set_operands (font->top_dict,
	                            FDSELECT_OP, buf, end_buf - buf);
    if (status)
	return status;

    status = cff_dict_set_operands (font->top_dict,
	                            FDARRAY_OP, buf, end_buf - buf);
    if (status)
	return status;

    status = cff_dict_set_operands (font->top_dict,
	                            CHARSET_OP, buf, end_buf - buf);
    if (status)
	return status;

    status = cairo_cff_font_set_ros_strings (font);
    if (status)
	return status;

    /* Create CID FD dictionary */
    status = cairo_cff_font_create_cid_fontdict (font);
    if (status)
	return status;

    /* Create charstrings */
    for (i = 0; i < font->scaled_font_subset->num_glyphs; i++) {
        charstring = _cairo_array_index(&type2_subset->charstrings, i);

        status = cff_index_append (&font->charstrings_subset_index,
                                   _cairo_array_index (charstring, 0),
                                   _cairo_array_num_elements (charstring));

        if (status)
            return status;
    }

    status = cairo_cff_font_write_subset (font);
    if (status)
        return status;

    *data = _cairo_array_index (&font->output, 0);
    *length = _cairo_array_num_elements (&font->output);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_cff_fallback_init (cairo_cff_subset_t          *cff_subset,
                          const char		      *subset_name,
                          cairo_scaled_font_subset_t  *font_subset)
{
    cairo_cff_font_t *font = NULL; /* squelch bogus compiler warning */
    cairo_status_t status;
    const char *data = NULL; /* squelch bogus compiler warning */
    unsigned long length = 0; /* squelch bogus compiler warning */
    unsigned int i;
    cairo_type2_charstrings_t type2_subset;

    status = _cairo_cff_font_fallback_create (font_subset, &font, subset_name);
    if (status)
	return status;

    status = _cairo_type2_charstrings_init (&type2_subset, font_subset);
    if (status)
	goto fail1;

    status = cairo_cff_font_fallback_generate (font, &type2_subset, &data, &length);
    if (status)
	goto fail2;

    cff_subset->base_font = strdup (font->font_name);
    if (cff_subset->base_font == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto fail2;
    }

    cff_subset->widths = calloc (sizeof (int), font->scaled_font_subset->num_glyphs);
    if (cff_subset->widths == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto fail3;
    }
    for (i = 0; i < font->scaled_font_subset->num_glyphs; i++)
        cff_subset->widths[i] = type2_subset.widths[i];

    cff_subset->x_min = type2_subset.x_min;
    cff_subset->y_min = type2_subset.y_min;
    cff_subset->x_max = type2_subset.x_max;
    cff_subset->y_max = type2_subset.y_max;
    cff_subset->ascent = type2_subset.y_max;
    cff_subset->descent = type2_subset.y_min;

    cff_subset->data = malloc (length);
    if (cff_subset->data == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto fail4;
    }

    memcpy (cff_subset->data, data, length);
    cff_subset->data_length = length;
    cff_subset->data_length = length;

    _cairo_type2_charstrings_fini (&type2_subset);
    cairo_cff_font_destroy (font);

    return CAIRO_STATUS_SUCCESS;

 fail4:
    free (cff_subset->widths);
 fail3:
    free (cff_subset->base_font);
 fail2:
    _cairo_type2_charstrings_fini (&type2_subset);
 fail1:
    cairo_cff_font_destroy (font);

    return status;
}

void
_cairo_cff_fallback_fini (cairo_cff_subset_t *subset)
{
    free (subset->base_font);
    free (subset->widths);
    free (subset->data);
}

#endif /* CAIRO_HAS_FONT_SUBSET */
