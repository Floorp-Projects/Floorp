/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2016 Adrian Johnson
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
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


/* PDF Document Interchange features:
 *  - metadata
 *  - document outline
 *  - tagged pdf
 *  - hyperlinks
 *  - page labels
 */

#define _DEFAULT_SOURCE /* for localtime_r(), gmtime_r(), snprintf(), strdup() */
#include "cairoint.h"

#include "cairo-pdf.h"
#include "cairo-pdf-surface-private.h"

#include "cairo-array-private.h"
#include "cairo-error-private.h"
#include "cairo-output-stream-private.h"
#include "cairo-recording-surface-private.h"
#include "cairo-surface-snapshot-inline.h"

#include <time.h>

#ifndef HAVE_LOCALTIME_R
#define localtime_r(T, BUF) (*(BUF) = *localtime (T))
#endif
#ifndef HAVE_GMTIME_R
#define gmtime_r(T, BUF) (*(BUF) = *gmtime (T))
#endif

/* #define DEBUG_PDF_INTERCHANGE 1 */

#if DEBUG_PDF_INTERCHANGE
static void
print_tree (cairo_pdf_surface_t *surface, cairo_pdf_struct_tree_node_t *node);

static void
print_command (cairo_pdf_command_t *command, int indent);

static void
print_command_list(cairo_pdf_command_list_t *command_list);
#endif

static void
_cairo_pdf_command_init_key (cairo_pdf_command_entry_t *key)
{
    key->base.hash = _cairo_hash_uintptr (_CAIRO_HASH_INIT_VALUE, (uintptr_t)key->recording_id);
    key->base.hash = _cairo_hash_uintptr (key->base.hash, (uintptr_t)key->command_id);
}

static cairo_bool_t
_cairo_pdf_command_equal (const void *key_a, const void *key_b)
{
    const cairo_pdf_command_entry_t *a = key_a;
    const cairo_pdf_command_entry_t *b = key_b;

    return a->recording_id == b->recording_id && a->command_id == b->command_id;
}

static void
_cairo_pdf_command_pluck (void *entry, void *closure)
{
    cairo_pdf_command_entry_t *dest = entry;
    cairo_hash_table_t *table = closure;

    _cairo_hash_table_remove (table, &dest->base);
    free (dest);
}

static cairo_pdf_struct_tree_node_t *
lookup_node_for_command (cairo_pdf_surface_t    *surface,
			 unsigned int            recording_id,
			 unsigned int            command_id)
{
    cairo_pdf_command_entry_t entry_key;
    cairo_pdf_command_entry_t *entry;
    cairo_pdf_interchange_t *ic = &surface->interchange;

    entry_key.recording_id = recording_id;
    entry_key.command_id = command_id;
    _cairo_pdf_command_init_key (&entry_key);
    entry = _cairo_hash_table_lookup (ic->command_to_node_map, &entry_key.base);
    assert (entry != NULL);
    return entry->node;
}

static cairo_int_status_t
command_list_add (cairo_pdf_surface_t    *surface,
		  unsigned int            command_id,
		  cairo_pdf_operation_t   flags)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_pdf_command_t command;
    cairo_int_status_t status;

    unsigned num_elements = _cairo_array_num_elements (&ic->current_commands->commands);
    if (command_id > num_elements) {
	void *elements;
	unsigned additional_elements = command_id - num_elements;
	status = _cairo_array_allocate (&ic->current_commands->commands, additional_elements, &elements);
	if (unlikely (status))
	    return status;
	memset (elements, 0, additional_elements * sizeof(cairo_pdf_command_t));
    }

    command.group = NULL;
    command.node = NULL;
    command.command_id = command_id;
    command.mcid_index = 0;
    command.flags = flags;
    return _cairo_array_append (&ic->current_commands->commands, &command);
}

static cairo_int_status_t
command_list_push_group (cairo_pdf_surface_t    *surface,
			 unsigned int            command_id,
			 cairo_surface_t        *recording_surface,
			 unsigned int            region_id)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_pdf_command_t *command;
    cairo_pdf_command_list_t *group;
    cairo_pdf_recording_surface_commands_t recording_commands;
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;

    group = _cairo_malloc (sizeof(cairo_pdf_command_list_t));
    _cairo_array_init (&group->commands, sizeof(cairo_pdf_command_t));
    group->parent = ic->current_commands;

    command_list_add (surface, command_id, PDF_GROUP);
    command = _cairo_array_index (&ic->current_commands->commands, command_id);
    command->group = group;
    ic->current_commands = group;

    recording_commands.recording_surface = recording_surface;
    recording_commands.command_list = group;
    recording_commands.region_id = region_id;
    status = _cairo_array_append (&ic->recording_surface_commands, &recording_commands);

    return status;
}

static void
command_list_pop_group (cairo_pdf_surface_t    *surface)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;

    ic->current_commands = ic->current_commands->parent;
}

static cairo_bool_t
command_list_is_group (cairo_pdf_surface_t    *surface,
		       unsigned int            command_id)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_pdf_command_t *command;
    unsigned num_elements = _cairo_array_num_elements (&ic->current_commands->commands);

    if (command_id >= num_elements)
	return FALSE;

    command = _cairo_array_index (&ic->current_commands->commands, command_id);
    return command->flags == PDF_GROUP;
}


/* Is there any content between current command and next
 * begin/end/group? */
static cairo_bool_t
command_list_has_content (cairo_pdf_surface_t    *surface,
			  unsigned int            command_id,
			  unsigned int           *content_command_id)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_pdf_command_t *command;
    unsigned i;
    unsigned num_elements = _cairo_array_num_elements (&ic->current_commands->commands);

    for (i = command_id + 1; i < num_elements; i++) {
	command = _cairo_array_index (&ic->current_commands->commands, i);
	switch (command->flags) {
	    case PDF_CONTENT:
		if (content_command_id)
		    *content_command_id = i;
		return TRUE;
		break;
	    case PDF_BEGIN:
	    case PDF_END:
	    case PDF_GROUP:
		return FALSE;
	    case PDF_NONE:
		break;
	}
    }
    return FALSE;
}

static void
command_list_set_mcid (cairo_pdf_surface_t          *surface,
		       unsigned int                  command_id,
		       cairo_pdf_struct_tree_node_t *node,
		       int                           mcid_index)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_pdf_command_t *command;

    command = _cairo_array_index (&ic->current_commands->commands, command_id);
    command->node = node;
    command->mcid_index = mcid_index;
}

static void
command_list_set_current_recording_commands (cairo_pdf_surface_t    *surface,
					     cairo_surface_t        *recording_surface,
					     unsigned int            region_id)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    unsigned i;
    cairo_pdf_recording_surface_commands_t *commands;
    unsigned num_elements = _cairo_array_num_elements (&ic->recording_surface_commands);

    for (i = 0; i < num_elements; i++) {
	commands = _cairo_array_index (&ic->recording_surface_commands, i);
	if (commands->region_id == region_id) {
	    ic->current_commands = commands->command_list;
	    return;
	}
    }
    ASSERT_NOT_REACHED; /* recording_surface not found */
}

static void
update_mcid_order (cairo_pdf_surface_t       *surface,
		   cairo_pdf_command_list_t  *command_list)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_pdf_command_t *command;
    cairo_pdf_page_mcid_t *mcid_elem;
    unsigned i;
    unsigned num_elements = _cairo_array_num_elements (&command_list->commands);

    for (i = 0; i < num_elements; i++) {
	command = _cairo_array_index (&command_list->commands, i);
	if (command->node) {
	    mcid_elem = _cairo_array_index (&command->node->mcid, command->mcid_index);
	    mcid_elem->order = ic->mcid_order++;
	}

	if (command->group)
	    update_mcid_order (surface, command->group);
    }
}

static void
_cairo_pdf_content_tag_init_key (cairo_pdf_content_tag_t *key)
{
    key->base.hash = _cairo_hash_string (key->node->attributes.content.id);
}

static cairo_bool_t
_cairo_pdf_content_tag_equal (const void *key_a, const void *key_b)
{
    const cairo_pdf_content_tag_t *a = key_a;
    const cairo_pdf_content_tag_t *b = key_b;

    return strcmp (a->node->attributes.content.id, b->node->attributes.content.id) == 0;
}

static void
_cairo_pdf_content_tag_pluck (void *entry, void *closure)
{
    cairo_pdf_content_tag_t *content_tag = entry;
    cairo_hash_table_t *table = closure;

    _cairo_hash_table_remove (table, &content_tag->base);
    free (content_tag);
}

static cairo_status_t
lookup_content_node_for_ref_node (cairo_pdf_surface_t           *surface,
				  cairo_pdf_struct_tree_node_t  *ref_node,
				  cairo_pdf_struct_tree_node_t **node)
{
    cairo_pdf_content_tag_t entry_key;
    cairo_pdf_content_tag_t *entry;
    cairo_pdf_interchange_t *ic = &surface->interchange;

    entry_key.node = ref_node;
    _cairo_pdf_content_tag_init_key (&entry_key);
    entry = _cairo_hash_table_lookup (ic->content_tag_map, &entry_key.base);
    if (!entry) {
	return _cairo_tag_error ("CONTENT_REF ref='%s' not found",
				 ref_node->attributes.content_ref.ref);
    }

    *node = entry->node;
    return CAIRO_STATUS_SUCCESS;
}

static void
write_rect_to_pdf_quad_points (cairo_output_stream_t   *stream,
			       const cairo_rectangle_t *rect,
			       double                   surface_height)
{
    _cairo_output_stream_printf (stream,
				 "%f %f %f %f %f %f %f %f",
				 rect->x,
				 surface_height - rect->y,
				 rect->x + rect->width,
				 surface_height - rect->y,
				 rect->x + rect->width,
				 surface_height - (rect->y + rect->height),
				 rect->x,
				 surface_height - (rect->y + rect->height));
}

static void
write_rect_int_to_pdf_bbox (cairo_output_stream_t       *stream,
			    const cairo_rectangle_int_t *rect,
			    double                       surface_height)
{
    _cairo_output_stream_printf (stream,
				 "%d %f %d %f",
				 rect->x,
				 surface_height - (rect->y + rect->height),
				 rect->x + rect->width,
				 surface_height - rect->y);
}

static cairo_int_status_t
add_tree_node (cairo_pdf_surface_t           *surface,
	       cairo_pdf_struct_tree_node_t  *parent,
	       const char                    *name,
	       const char                    *attributes,
	       cairo_pdf_struct_tree_node_t **new_node)
{
    cairo_pdf_struct_tree_node_t *node;
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;

    node = _cairo_malloc (sizeof(cairo_pdf_struct_tree_node_t));
    if (unlikely (node == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    node->name = strdup (name);
    node->res = _cairo_pdf_surface_new_object (surface);
    if (node->res.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    node->parent = parent;
    cairo_list_init (&node->children);
    _cairo_array_init (&node->mcid, sizeof (cairo_pdf_page_mcid_t));
    node->annot = NULL;
    node->extents.valid = FALSE;

    cairo_list_add_tail (&node->link, &parent->children);

    if (strcmp (node->name, CAIRO_TAG_CONTENT) == 0) {
	node->type = PDF_NODE_CONTENT;
	status = _cairo_tag_parse_content_attributes (attributes, &node->attributes.content);
    } else if (strcmp (node->name, CAIRO_TAG_CONTENT_REF) == 0) {
	node->type = PDF_NODE_CONTENT_REF;
	status = _cairo_tag_parse_content_ref_attributes (attributes, &node->attributes.content_ref);
    } else if (strcmp (node->name, "Artifact") == 0) {
	node->type = PDF_NODE_ARTIFACT;
    } else {
	node->type = PDF_NODE_STRUCT;
    }

    *new_node = node;
    return status;
}

static void
free_node (cairo_pdf_struct_tree_node_t *node)
{
    cairo_pdf_struct_tree_node_t *child, *next;

    if (!node)
	return;

    cairo_list_foreach_entry_safe (child, next, cairo_pdf_struct_tree_node_t,
				   &node->children,
				   link)
    {
	cairo_list_del (&child->link);
	free_node (child);
    }
    free (node->name);
    _cairo_array_fini (&node->mcid);
    if (node->type == PDF_NODE_CONTENT)
	_cairo_tag_free_content_attributes (&node->attributes.content);

    if (node->type == PDF_NODE_CONTENT_REF)
	_cairo_tag_free_content_ref_attributes (&node->attributes.content_ref);

    free (node);
}

static cairo_status_t
add_mcid_to_node (cairo_pdf_surface_t          *surface,
		  cairo_pdf_struct_tree_node_t *node,
		  unsigned int                  command_id,
		  int                          *mcid)
{
    cairo_pdf_page_mcid_t mcid_elem;
    cairo_int_status_t status;
    cairo_pdf_interchange_t *ic = &surface->interchange;

    status = _cairo_array_append (&ic->mcid_to_tree, &node);
    if (unlikely (status))
	return status;

    mcid_elem.order = -1;
    mcid_elem.page = _cairo_array_num_elements (&surface->pages);
    mcid_elem.xobject_res = ic->current_recording_surface_res;
    mcid_elem.mcid = _cairo_array_num_elements (&ic->mcid_to_tree) - 1;
    mcid_elem.child_node = NULL;
    command_list_set_mcid (surface, command_id, node, _cairo_array_num_elements (&node->mcid));
    *mcid = mcid_elem.mcid;
    return _cairo_array_append (&node->mcid, &mcid_elem);
}

static cairo_status_t
add_child_to_mcid_array (cairo_pdf_surface_t          *surface,
			 cairo_pdf_struct_tree_node_t *node,
			 unsigned int                  command_id,
			 cairo_pdf_struct_tree_node_t *child)
{
    cairo_pdf_page_mcid_t mcid_elem;

    mcid_elem.order = -1;
    mcid_elem.page = 0;
    mcid_elem.xobject_res.id = 0;
    mcid_elem.mcid = 0;
    mcid_elem.child_node = child;
    command_list_set_mcid (surface, command_id, node, _cairo_array_num_elements (&node->mcid));
    return _cairo_array_append (&node->mcid, &mcid_elem);
}

static cairo_int_status_t
add_annotation (cairo_pdf_surface_t           *surface,
		cairo_pdf_struct_tree_node_t  *node,
		const char                    *name,
		const char                    *attributes)
{
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_pdf_annotation_t *annot;

    annot = _cairo_malloc (sizeof (cairo_pdf_annotation_t));
    if (unlikely (annot == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = _cairo_tag_parse_link_attributes (attributes, &annot->link_attrs);
    if (unlikely (status)) {
	free (annot);
	return status;
    }

    if (annot->link_attrs.link_page == 0)
	annot->link_attrs.link_page = _cairo_array_num_elements (&surface->pages);

    annot->node = node;

    annot->res = _cairo_pdf_surface_new_object (surface);
    if (annot->res.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    node->annot = annot;
    status = _cairo_array_append (&ic->annots, &annot);

    return status;
}

static void
free_annotation (cairo_pdf_annotation_t *annot)
{
    _cairo_tag_free_link_attributes (&annot->link_attrs);
    free (annot);
}

static void
cairo_pdf_interchange_clear_annotations (cairo_pdf_surface_t *surface)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    int num_elems, i;

    num_elems = _cairo_array_num_elements (&ic->annots);
    for (i = 0; i < num_elems; i++) {
	cairo_pdf_annotation_t * annot;

	_cairo_array_copy_element (&ic->annots, i, &annot);
	free_annotation (annot);
    }
    _cairo_array_truncate (&ic->annots, 0);
}

static void
cairo_pdf_interchange_write_node_mcid (cairo_pdf_surface_t            *surface,
				       cairo_pdf_page_mcid_t          *mcid_elem,
				       int                             page)
{
    cairo_pdf_page_info_t *page_info;

    page_info = _cairo_array_index (&surface->pages, mcid_elem->page - 1);
    if (mcid_elem->page == page && mcid_elem->xobject_res.id == 0) {
	_cairo_output_stream_printf (surface->object_stream.stream, "%d ", mcid_elem->mcid);
    } else {
	_cairo_output_stream_printf (surface->object_stream.stream,
				     "\n       << /Type /MCR ");
	if (mcid_elem->page != page) {
	    _cairo_output_stream_printf (surface->object_stream.stream,
					 "/Pg %d 0 R ",
					 page_info->page_res.id);
	}
	if (mcid_elem->xobject_res.id != 0) {
	    _cairo_output_stream_printf (surface->object_stream.stream,
					 "/Stm %d 0 R ",
					 mcid_elem->xobject_res.id);
	}
	_cairo_output_stream_printf (surface->object_stream.stream,
				     "/MCID %d >> ",
				     mcid_elem->mcid);
    }
}

static int
_mcid_order_compare (const void *a,
		     const void *b)
{
    const cairo_pdf_page_mcid_t *mcid_a = a;
    const cairo_pdf_page_mcid_t *mcid_b = b;

    if (mcid_a->order < mcid_b->order)
	return -1;
    else if (mcid_a->order > mcid_b->order)
	return 1;
    else
	return 0;
}

static cairo_int_status_t
cairo_pdf_interchange_write_node_object (cairo_pdf_surface_t            *surface,
					 cairo_pdf_struct_tree_node_t   *node,
					 int                             depth)
{
    cairo_pdf_page_mcid_t *mcid_elem, *child_mcid_elem;
    unsigned i, j, num_mcid;
    int first_page = 0;
    cairo_pdf_page_info_t *page_info;
    cairo_int_status_t status;
    cairo_bool_t has_children = FALSE;

    /* The Root node is written in cairo_pdf_interchange_write_struct_tree(). */
    if (!node->parent)
	return CAIRO_STATUS_SUCCESS;

    if (node->type == PDF_NODE_CONTENT ||
	node->type == PDF_NODE_CONTENT_REF ||
	node->type == PDF_NODE_ARTIFACT)
    {
	return CAIRO_STATUS_SUCCESS;
    }

    status = _cairo_pdf_surface_object_begin (surface, node->res);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (surface->object_stream.stream,
				 "<< /Type /StructElem\n"
				 "   /S /%s\n"
				 "   /P %d 0 R\n",
				 node->name,
				 node->parent->res.id);

    /* Write /K entry (children of this StructElem) */
    num_mcid = _cairo_array_num_elements (&node->mcid);
    if (num_mcid > 0 ) {
	_cairo_array_sort (&node->mcid, _mcid_order_compare);
	/* Find the first MCID element and use the page number to set /Pg */
	for (i = 0; i < num_mcid; i++) {
	    mcid_elem = _cairo_array_index (&node->mcid, i);
	    assert (mcid_elem->order != -1);
	    if (mcid_elem->child_node) {
		if (mcid_elem->child_node->type == PDF_NODE_CONTENT_REF) {
		    cairo_pdf_struct_tree_node_t *content_node;
		    status = lookup_content_node_for_ref_node (surface, mcid_elem->child_node, &content_node);
		    if (status)
			return status;

		    /* CONTENT_REF will not have child nodes */
		    if (_cairo_array_num_elements (&content_node->mcid) > 0) {
			child_mcid_elem = _cairo_array_index (&content_node->mcid, 0);
			first_page = child_mcid_elem->page;
			page_info = _cairo_array_index (&surface->pages, first_page - 1);
			_cairo_output_stream_printf (surface->object_stream.stream,
						     "   /Pg %d 0 R\n",
						     page_info->page_res.id);
			has_children = TRUE;
			break;
		    }
		} else {
		    has_children = TRUE;
		}
	    } else {
		first_page = mcid_elem->page;
		page_info = _cairo_array_index (&surface->pages, first_page - 1);
		_cairo_output_stream_printf (surface->object_stream.stream,
					     "   /Pg %d 0 R\n",
					     page_info->page_res.id);
		has_children = TRUE;
		break;
	    }
	}

	if (has_children || node->annot) {
	    _cairo_output_stream_printf (surface->object_stream.stream, "   /K ");

	    if (num_mcid > 1 || node->annot)
		_cairo_output_stream_printf (surface->object_stream.stream, "[ ");

	    for (i = 0; i < num_mcid; i++) {
		if (node->annot) {
		    if (node->annot->link_attrs.link_page != first_page) {
			page_info = _cairo_array_index (&surface->pages, node->annot->link_attrs.link_page - 1);
			_cairo_output_stream_printf (surface->object_stream.stream,
						     "<< /Type /OBJR /Pg %d 0 R /Obj %d 0 R >> ",
						     page_info->page_res.id,
						     node->annot->res.id);
		    } else {
			_cairo_output_stream_printf (surface->object_stream.stream,
						     "<< /Type /OBJR /Obj %d 0 R >> ",
						     node->annot->res.id);
		    }
		}
		mcid_elem = _cairo_array_index (&node->mcid, i);
		if (mcid_elem->child_node) {
		    if (mcid_elem->child_node->type == PDF_NODE_CONTENT_REF) {
			cairo_pdf_struct_tree_node_t *content_node;
			status = lookup_content_node_for_ref_node (surface, mcid_elem->child_node, &content_node);
			if (status)
			    return status;

			assert (content_node->type == PDF_NODE_CONTENT);

			/* CONTENT_REF will not have child nodes */
			for (j = 0; j < _cairo_array_num_elements (&content_node->mcid); j++) {
			    child_mcid_elem = _cairo_array_index (&content_node->mcid, j);
			    cairo_pdf_interchange_write_node_mcid (surface, child_mcid_elem, first_page);
			}
		    } else if (mcid_elem->child_node->type != PDF_NODE_CONTENT) {
			_cairo_output_stream_printf (surface->object_stream.stream,
						     " %d 0 R ",
						     mcid_elem->child_node->res.id);
		    }
		} else {
		    cairo_pdf_interchange_write_node_mcid (surface, mcid_elem, first_page);
		}
	    }

	    if (num_mcid > 1 || node->annot)
		_cairo_output_stream_printf (surface->object_stream.stream, "]");
	}

	_cairo_output_stream_printf (surface->object_stream.stream, "\n");
    }

    _cairo_output_stream_printf (surface->object_stream.stream,
				 ">>\n");

    _cairo_pdf_surface_object_end (surface);

    return _cairo_output_stream_get_status (surface->object_stream.stream);
}

static void
init_named_dest_key (cairo_pdf_named_dest_t *dest)
{
    dest->base.hash = _cairo_hash_bytes (_CAIRO_HASH_INIT_VALUE,
					 dest->attrs.name,
					 strlen (dest->attrs.name));
}

static cairo_bool_t
_named_dest_equal (const void *key_a, const void *key_b)
{
    const cairo_pdf_named_dest_t *a = key_a;
    const cairo_pdf_named_dest_t *b = key_b;

    return strcmp (a->attrs.name, b->attrs.name) == 0;
}

static void
_named_dest_pluck (void *entry, void *closure)
{
    cairo_pdf_named_dest_t *dest = entry;
    cairo_hash_table_t *table = closure;

    _cairo_hash_table_remove (table, &dest->base);
    _cairo_tag_free_dest_attributes (&dest->attrs);
    free (dest);
}

static cairo_int_status_t
cairo_pdf_interchange_write_explicit_dest (cairo_pdf_surface_t *surface,
                                          int                  page,
                                          cairo_bool_t         has_pos,
                                          double               x,
                                          double               y)
{
    cairo_pdf_page_info_t *page_info;

    page_info = _cairo_array_index (&surface->pages, page - 1);

    if (has_pos) {
       _cairo_output_stream_printf (surface->object_stream.stream,
                                    "[%d 0 R /XYZ %f %f 0]\n",
                                    page_info->page_res.id,
                                    x,
                                    page_info->height - y);
    } else {
       _cairo_output_stream_printf (surface->object_stream.stream,
                                    "[%d 0 R /XYZ null null 0]\n",
                                    page_info->page_res.id);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_pdf_interchange_write_dest (cairo_pdf_surface_t *surface,
				  cairo_link_attrs_t  *link_attrs)
{
    cairo_int_status_t status;
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_pdf_forward_link_t *link;
    cairo_pdf_resource_t link_res;

    /* If the dest is known, emit an explicit dest */
    if (link_attrs->dest) {
	cairo_pdf_named_dest_t key;
	cairo_pdf_named_dest_t *named_dest;

	/* check if we already have this dest */
	key.attrs.name = link_attrs->dest;
	init_named_dest_key (&key);
	named_dest = _cairo_hash_table_lookup (ic->named_dests, &key.base);
	if (named_dest) {
	    double x = 0;
	    double y = 0;

	    if (named_dest->extents.valid) {
		x = named_dest->extents.extents.x;
		y = named_dest->extents.extents.y;
	    }

	    if (named_dest->attrs.x_valid)
		x = named_dest->attrs.x;

	    if (named_dest->attrs.y_valid)
		y = named_dest->attrs.y;

	    _cairo_output_stream_printf (surface->object_stream.stream, "   /Dest ");
	    status = cairo_pdf_interchange_write_explicit_dest (surface,
                                                                named_dest->page,
                                                                TRUE,
                                                                x, y);
	    return status;
	}
    }

    /* If the page is known, emit an explicit dest */
    if (!link_attrs->dest) {
	if (link_attrs->page < 1)
	    return _cairo_tag_error ("Link attribute: \"page=%d\" page must be >= 1", link_attrs->page);

	if (link_attrs->page <= (int)_cairo_array_num_elements (&surface->pages)) {
	    _cairo_output_stream_printf (surface->object_stream.stream, "   /Dest ");
	    return cairo_pdf_interchange_write_explicit_dest (surface,
							      link_attrs->page,
							      link_attrs->has_pos,
							      link_attrs->pos.x,
							      link_attrs->pos.y);
	}
    }

    /* Link refers to a future or unknown page. Use an indirect object
     * and write the link at the end of the document */

    link = _cairo_malloc (sizeof (cairo_pdf_forward_link_t));
    if (unlikely (link == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    link_res = _cairo_pdf_surface_new_object (surface);
    if (link_res.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_output_stream_printf (surface->object_stream.stream,
				 "   /Dest %d 0 R\n",
				 link_res.id);

    link->res = link_res;
    link->dest = link_attrs->dest ? strdup (link_attrs->dest) : NULL;
    link->page = link_attrs->page;
    link->has_pos = link_attrs->has_pos;
    link->pos = link_attrs->pos;
    status = _cairo_array_append (&surface->forward_links, link);

    return status;
}

static cairo_int_status_t
_cairo_utf8_to_pdf_utf8_hexstring (const char *utf8, char **str_out)
{
    int i;
    int len;
    unsigned char *p;
    cairo_bool_t ascii;
    char *str;
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;

    ascii = TRUE;
    p = (unsigned char *)utf8;
    len = 0;
    while (*p) {
	if (*p < 32 || *p > 126) {
	    ascii = FALSE;
	}
	if (*p == '(' || *p == ')' || *p == '\\')
	    len += 2;
	else
	    len++;
	p++;
    }

    if (ascii) {
	str = _cairo_malloc (len + 3);
	if (str == NULL)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	str[0] = '(';
	p = (unsigned char *)utf8;
	i = 1;
	while (*p) {
	    if (*p == '(' || *p == ')' || *p == '\\')
		str[i++] = '\\';
	    str[i++] = *p;
	    p++;
	}
	str[i++] = ')';
	str[i++] = 0;
    } else {
	str = _cairo_malloc (len*2 + 3);
	if (str == NULL)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	str[0] = '<';
	p = (unsigned char *)utf8;
	i = 1;
	while (*p) {
	    if (*p == '\\') {
		snprintf(str + i, 3, "%02x", '\\');
		i += 2;
	    }
	    snprintf(str + i, 3, "%02x", *p);
	    i += 2;
	    p++;
	}
	str[i++] = '>';
	str[i++] = 0;
    }
    *str_out = str;

    return status;
}

static cairo_int_status_t
cairo_pdf_interchange_write_link_action (cairo_pdf_surface_t   *surface,
					 cairo_link_attrs_t    *link_attrs)
{
    cairo_int_status_t status;
    char *dest = NULL;

    if (link_attrs->link_type == TAG_LINK_DEST) {
	status = cairo_pdf_interchange_write_dest (surface, link_attrs);
	if (unlikely (status))
	    return status;

    } else if (link_attrs->link_type == TAG_LINK_URI) {
	status = _cairo_utf8_to_pdf_string (link_attrs->uri, &dest);
	if (unlikely (status))
	    return status;

	if (dest[0] != '(') {
	    free (dest);
	    return _cairo_tag_error ("Link attribute: \"url=%s\" URI may only contain ASCII characters",
				     link_attrs->uri);
	}

	_cairo_output_stream_printf (surface->object_stream.stream,
				     "   /A <<\n"
				     "      /Type /Action\n"
				     "      /S /URI\n"
				     "      /URI %s\n"
				     "   >>\n",
				     dest);
	free (dest);
    } else if (link_attrs->link_type == TAG_LINK_FILE) {
	/* According to "Developing with PDF", Leonard Rosenthol, 2013,
	 * The F key is encoded in the "standard encoding for the
	 * platform on which the document is being viewed. For most
	 * modern operating systems, that's UTF-8"
	 *
	 * As we don't know the target platform, we assume UTF-8. The
	 * F key may contain multi-byte encodings using the hex
	 * encoding.
	 *
	 * For PDF 1.7 we also include the UF key which uses the
	 * standard PDF UTF-16BE strings.
	 */
	status = _cairo_utf8_to_pdf_utf8_hexstring (link_attrs->file, &dest);
	if (unlikely (status))
	    return status;

	_cairo_output_stream_printf (surface->object_stream.stream,
				     "   /A <<\n"
				     "      /Type /Action\n"
				     "      /S /GoToR\n"
				     "      /F %s\n",
				     dest);
	free (dest);

	if (surface->pdf_version >= CAIRO_PDF_VERSION_1_7)
	{
	    status = _cairo_utf8_to_pdf_string (link_attrs->file, &dest);
	    if (unlikely (status))
		return status;

	    _cairo_output_stream_printf (surface->object_stream.stream,
				     "      /UF %s\n",
				     dest);
	    free (dest);
	}

	if (link_attrs->dest) {
	    status = _cairo_utf8_to_pdf_string (link_attrs->dest, &dest);
	    if (unlikely (status))
		return status;

	    _cairo_output_stream_printf (surface->object_stream.stream,
					 "      /D %s\n",
					 dest);
	    free (dest);
	} else {
	    if (link_attrs->has_pos) {
		_cairo_output_stream_printf (surface->object_stream.stream,
					     "      /D [%d /XYZ %f %f 0]\n",
					     link_attrs->page,
					     link_attrs->pos.x,
					     link_attrs->pos.y);
	    } else {
		_cairo_output_stream_printf (surface->object_stream.stream,
					     "      /D [%d /XYZ null null 0]\n",
					     link_attrs->page);
	    }
	}
	_cairo_output_stream_printf (surface->object_stream.stream,
				     "   >>\n");
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_pdf_interchange_write_annot (cairo_pdf_surface_t    *surface,
				   cairo_pdf_annotation_t *annot,
				   cairo_bool_t            struct_parents)
{
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_pdf_struct_tree_node_t *node = annot->node;
    int sp;
    int i, num_rects;
    double height;

    num_rects = _cairo_array_num_elements (&annot->link_attrs.rects);
    if (strcmp (node->name, CAIRO_TAG_LINK) == 0 &&
	annot->link_attrs.link_type != TAG_LINK_EMPTY &&
	(node->extents.valid || num_rects > 0))
    {
	status = _cairo_array_append (&ic->parent_tree, &node->res);
	if (unlikely (status))
	    return status;

	sp = _cairo_array_num_elements (&ic->parent_tree) - 1;

	status = _cairo_pdf_surface_object_begin (surface, annot->res);
	if (unlikely (status))
	    return status;

	_cairo_output_stream_printf (surface->object_stream.stream,
				     "<< /Type /Annot\n"
				     "   /Subtype /Link\n");

	if (struct_parents) {
	    _cairo_output_stream_printf (surface->object_stream.stream,
					 "   /StructParent %d\n",
					 sp);
	}

	height = surface->height;
	if (num_rects > 0) {
	    cairo_rectangle_int_t bbox_rect;

	    _cairo_output_stream_printf (surface->object_stream.stream,
					 "   /QuadPoints [ ");
	    for (i = 0; i < num_rects; i++) {
		cairo_rectangle_t rectf;
		cairo_rectangle_int_t recti;

		_cairo_array_copy_element (&annot->link_attrs.rects, i, &rectf);
		_cairo_rectangle_int_from_double (&recti, &rectf);
		if (i == 0)
		    bbox_rect = recti;
		else
		    _cairo_rectangle_union (&bbox_rect, &recti);

		write_rect_to_pdf_quad_points (surface->object_stream.stream, &rectf, height);
		_cairo_output_stream_printf (surface->object_stream.stream, " ");
	    }
	    _cairo_output_stream_printf (surface->object_stream.stream,
					 "]\n"
					 "   /Rect [ ");
	    write_rect_int_to_pdf_bbox (surface->object_stream.stream, &bbox_rect, height);
	    _cairo_output_stream_printf (surface->object_stream.stream, " ]\n");
	} else {
	    _cairo_output_stream_printf (surface->object_stream.stream,
					 "   /Rect [ ");
	    write_rect_int_to_pdf_bbox (surface->object_stream.stream, &node->extents.extents, height);
	    _cairo_output_stream_printf (surface->object_stream.stream, " ]\n");
	}

	status = cairo_pdf_interchange_write_link_action (surface, &annot->link_attrs);
	if (unlikely (status))
	    return status;

	_cairo_output_stream_printf (surface->object_stream.stream,
				     "   /BS << /W 0 >>\n"
				     ">>\n");

	_cairo_pdf_surface_object_end (surface);
	status = _cairo_output_stream_get_status (surface->object_stream.stream);
    }

    return status;
}

static cairo_int_status_t
cairo_pdf_interchange_walk_struct_tree (cairo_pdf_surface_t          *surface,
					cairo_pdf_struct_tree_node_t *node,
					int                           depth,
					cairo_int_status_t (*func) (cairo_pdf_surface_t          *surface,
								    cairo_pdf_struct_tree_node_t *node,
					                            int                           depth))
{
    cairo_int_status_t status;
    cairo_pdf_struct_tree_node_t *child;

    status = func (surface, node, depth);
    if (unlikely (status))
	return status;

    depth++;
    cairo_list_foreach_entry (child, cairo_pdf_struct_tree_node_t,
			      &node->children, link)
    {
	status = cairo_pdf_interchange_walk_struct_tree (surface, child, depth, func);
	if (unlikely (status))
	    return status;
    }
    depth--;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_pdf_interchange_write_struct_tree (cairo_pdf_surface_t *surface)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_pdf_struct_tree_node_t *child;
    cairo_int_status_t status;

    if (cairo_list_is_empty (&ic->struct_root->children))
	return CAIRO_STATUS_SUCCESS;

    status = cairo_pdf_interchange_walk_struct_tree (surface,
						     ic->struct_root,
						     0,
						     cairo_pdf_interchange_write_node_object);
    if (unlikely (status))
	return status;

    status = _cairo_pdf_surface_object_begin (surface, surface->struct_tree_root);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (surface->object_stream.stream,
				 "<< /Type /StructTreeRoot\n"
				 "   /ParentTree %d 0 R\n",
				 ic->parent_tree_res.id);

    if (cairo_list_is_singular (&ic->struct_root->children)) {
	child = cairo_list_first_entry (&ic->struct_root->children, cairo_pdf_struct_tree_node_t, link);
	_cairo_output_stream_printf (surface->object_stream.stream, "   /K [ %d 0 R ]\n", child->res.id);
    } else {
	_cairo_output_stream_printf (surface->object_stream.stream, "   /K [ ");

	cairo_list_foreach_entry (child, cairo_pdf_struct_tree_node_t,
				  &ic->struct_root->children, link)
	{
	    if (child->type == PDF_NODE_CONTENT || child->type == PDF_NODE_ARTIFACT)
		continue;

	    _cairo_output_stream_printf (surface->object_stream.stream, "%d 0 R ", child->res.id);
	}
	_cairo_output_stream_printf (surface->object_stream.stream, "]\n");
    }

    _cairo_output_stream_printf (surface->object_stream.stream,
				 ">>\n");
    _cairo_pdf_surface_object_end (surface);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_pdf_interchange_write_annots (cairo_pdf_surface_t *surface,
				    cairo_bool_t         struct_parents)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    int num_elems, i, page_num;
    cairo_pdf_page_info_t *page_info;
    cairo_pdf_annotation_t *annot;
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;

    num_elems = _cairo_array_num_elements (&ic->annots);
    for (i = 0; i < num_elems; i++) {
	_cairo_array_copy_element (&ic->annots, i, &annot);
	page_num = annot->link_attrs.link_page;
	if (page_num > (int)_cairo_array_num_elements (&surface->pages)) {
	    return _cairo_tag_error ("Link attribute: \"link_page=%d\" page exceeds page count (%d)",
				     page_num,
				     _cairo_array_num_elements (&surface->pages));
	}

	page_info = _cairo_array_index (&surface->pages, page_num - 1);
	status = _cairo_array_append (&page_info->annots, &annot->res);
	if (status)
	    return status;

	status = cairo_pdf_interchange_write_annot (surface, annot, struct_parents);
	if (unlikely (status))
	    return status;
    }

    return status;
}

static cairo_int_status_t
cairo_pdf_interchange_write_content_parent_elems (cairo_pdf_surface_t *surface)
{
    int num_elems, i;
    cairo_pdf_struct_tree_node_t *node;
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;

    num_elems = _cairo_array_num_elements (&ic->mcid_to_tree);
    status = _cairo_pdf_surface_object_begin (surface, ic->content_parent_res);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (surface->object_stream.stream,
				     "[\n");
    for (i = 0; i < num_elems; i++) {
	_cairo_array_copy_element (&ic->mcid_to_tree, i, &node);
	_cairo_output_stream_printf (surface->object_stream.stream, "  %d 0 R\n", node->res.id);
    }
    _cairo_output_stream_printf (surface->object_stream.stream,
				 "]\n");
    _cairo_pdf_surface_object_end (surface);

    return status;
}

static cairo_int_status_t
cairo_pdf_interchange_apply_extents_from_content_ref (cairo_pdf_surface_t            *surface,
						      cairo_pdf_struct_tree_node_t   *node,
						      int                             depth)
{
    cairo_int_status_t status;

    if (node->type != PDF_NODE_CONTENT_REF)
	return CAIRO_STATUS_SUCCESS;

    cairo_pdf_struct_tree_node_t *content_node;
    status = lookup_content_node_for_ref_node (surface, node, &content_node);
    if (status)
	return status;

    /* Merge extents with all parent nodes */
    node = node->parent;
    while (node) {
	if (node->extents.valid) {
	    _cairo_rectangle_union (&node->extents.extents, &content_node->extents.extents);
	} else {
	    node->extents = content_node->extents;
	}
	node = node->parent;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_pdf_interchange_update_extents (cairo_pdf_surface_t *surface)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;

    return cairo_pdf_interchange_walk_struct_tree (surface,
						   ic->struct_root,
						   0,
						   cairo_pdf_interchange_apply_extents_from_content_ref);
}

static cairo_int_status_t
cairo_pdf_interchange_write_parent_tree (cairo_pdf_surface_t *surface)
{
    int num_elems, i;
    cairo_pdf_resource_t *res;
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_int_status_t status;

    num_elems = _cairo_array_num_elements (&ic->parent_tree);
    if (num_elems > 0) {
	ic->parent_tree_res = _cairo_pdf_surface_new_object (surface);
	if (ic->parent_tree_res.id == 0)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	status = _cairo_pdf_surface_object_begin (surface, ic->parent_tree_res);
	if (unlikely (status))
	    return status;

	_cairo_output_stream_printf (surface->object_stream.stream,
				     "<< /Nums [\n");
	for (i = 0; i < num_elems; i++) {
	    res = _cairo_array_index (&ic->parent_tree, i);
	    if (res->id) {
		_cairo_output_stream_printf (surface->object_stream.stream,
					     "   %d %d 0 R\n",
					     i,
					     res->id);
	    }
	}
	_cairo_output_stream_printf (surface->object_stream.stream,
				     "  ]\n"
				     ">>\n");
	_cairo_pdf_surface_object_end (surface);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_pdf_interchange_write_outline (cairo_pdf_surface_t *surface)
{
    int num_elems, i;
    cairo_pdf_outline_entry_t *outline;
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_int_status_t status;
    char *name = NULL;

    num_elems = _cairo_array_num_elements (&ic->outline);
    if (num_elems < 2)
	return CAIRO_INT_STATUS_SUCCESS;

    _cairo_array_copy_element (&ic->outline, 0, &outline);
    outline->res = _cairo_pdf_surface_new_object (surface);
    if (outline->res.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    surface->outlines_dict_res = outline->res;
    status = _cairo_pdf_surface_object_begin (surface, outline->res);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (surface->object_stream.stream,
				 "<< /Type /Outlines\n"
				 "   /First %d 0 R\n"
				 "   /Last %d 0 R\n"
				 "   /Count %d\n"
				 ">>\n",
				 outline->first_child->res.id,
				 outline->last_child->res.id,
				 outline->count);
    _cairo_pdf_surface_object_end (surface);

    for (i = 1; i < num_elems; i++) {
	_cairo_array_copy_element (&ic->outline, i, &outline);
	_cairo_pdf_surface_update_object (surface, outline->res);

	status = _cairo_utf8_to_pdf_string (outline->name, &name);
	if (unlikely (status))
	    return status;

	status = _cairo_pdf_surface_object_begin (surface, outline->res);
	if (unlikely (status))
	    return status;

	_cairo_output_stream_printf (surface->object_stream.stream,
				     "<< /Title %s\n"
				     "   /Parent %d 0 R\n",
				     name,
				     outline->parent->res.id);
	free (name);

	if (outline->prev) {
	    _cairo_output_stream_printf (surface->object_stream.stream,
					 "   /Prev %d 0 R\n",
					 outline->prev->res.id);
	}

	if (outline->next) {
	    _cairo_output_stream_printf (surface->object_stream.stream,
					 "   /Next %d 0 R\n",
					 outline->next->res.id);
	}

	if (outline->first_child) {
	    _cairo_output_stream_printf (surface->object_stream.stream,
					 "   /First %d 0 R\n"
					 "   /Last %d 0 R\n"
					 "   /Count %d\n",
					 outline->first_child->res.id,
					 outline->last_child->res.id,
					 outline->count);
	}

	if (outline->flags) {
	    int flags = 0;
	    if (outline->flags & CAIRO_PDF_OUTLINE_FLAG_ITALIC)
		flags |= 1;
	    if (outline->flags & CAIRO_PDF_OUTLINE_FLAG_BOLD)
		flags |= 2;
	    _cairo_output_stream_printf (surface->object_stream.stream,
					 "   /F %d\n",
					 flags);
	}

	status = cairo_pdf_interchange_write_link_action (surface, &outline->link_attrs);
	if (unlikely (status))
	    return status;

	_cairo_output_stream_printf (surface->object_stream.stream,
				     ">>\n");
	_cairo_pdf_surface_object_end (surface);
    }

    return status;
}

/*
 * Split a page label into a text prefix and numeric suffix. Leading '0's are
 * included in the prefix. eg
 *  "3"     => NULL,    3
 *  "cover" => "cover", 0
 *  "A-2"   => "A-",    2
 *  "A-002" => "A-00",  2
 */
static char *
split_label (const char* label, int *num)
{
    int len, i;

    *num = 0;
    len = strlen (label);
    if (len == 0)
	return NULL;

    i = len;
    while (i > 0 && _cairo_isdigit (label[i-1]))
	   i--;

    while (i < len && label[i] == '0')
	i++;

    if (i < len)
	sscanf (label + i, "%d", num);

    if (i > 0) {
	char *s;
	s = _cairo_malloc (i + 1);
	if (!s)
	    return NULL;

	memcpy (s, label, i);
	s[i] = 0;
	return s;
    }

    return NULL;
}

/* strcmp that handles NULL arguments */
static cairo_bool_t
strcmp_null (const char *s1, const char *s2)
{
    if (s1 && s2)
	return strcmp (s1, s2) == 0;

    if (!s1 && !s2)
	return TRUE;

    return FALSE;
}

static cairo_int_status_t
cairo_pdf_interchange_write_forward_links (cairo_pdf_surface_t *surface)
{
    int num_elems, i;
    cairo_pdf_forward_link_t *link;
    cairo_int_status_t status;
    cairo_pdf_named_dest_t key;
    cairo_pdf_named_dest_t *named_dest;
    cairo_pdf_interchange_t *ic = &surface->interchange;

    num_elems = _cairo_array_num_elements (&surface->forward_links);
    for (i = 0; i < num_elems; i++) {
	link = _cairo_array_index (&surface->forward_links, i);
	if (link->page > (int)_cairo_array_num_elements (&surface->pages))
	    return _cairo_tag_error ("Link attribute: \"page=%d\" page exceeds page count (%d)",
				     link->page, _cairo_array_num_elements (&surface->pages));


	status = _cairo_pdf_surface_object_begin (surface, link->res);
	if (unlikely (status))
	    return status;

	if (link->dest) {
	    key.attrs.name = link->dest;
	    init_named_dest_key (&key);
	    named_dest = _cairo_hash_table_lookup (ic->named_dests, &key.base);
	    if (named_dest) {
		double x = 0;
		double y = 0;

		if (named_dest->extents.valid) {
		    x = named_dest->extents.extents.x;
		    y = named_dest->extents.extents.y;
		}

		if (named_dest->attrs.x_valid)
		    x = named_dest->attrs.x;

		if (named_dest->attrs.y_valid)
		    y = named_dest->attrs.y;

		status = cairo_pdf_interchange_write_explicit_dest (surface,
								    named_dest->page,
								    TRUE,
								    x, y);
	    } else {
		return _cairo_tag_error ("Link to dest=\"%s\" not found", link->dest);
	    }
	} else {
	    cairo_pdf_interchange_write_explicit_dest (surface,
						       link->page,
						       link->has_pos,
						       link->pos.x,
						       link->pos.y);
	}
	_cairo_pdf_surface_object_end (surface);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_pdf_interchange_write_page_labels (cairo_pdf_surface_t *surface)
{
    int num_elems, i;
    char *label;
    char *prefix;
    char *prev_prefix;
    int num, prev_num;
    cairo_int_status_t status;
    cairo_bool_t has_labels;

    /* Check if any labels defined */
    num_elems = _cairo_array_num_elements (&surface->page_labels);
    has_labels = FALSE;
    for (i = 0; i < num_elems; i++) {
	_cairo_array_copy_element (&surface->page_labels, i, &label);
	if (label) {
	    has_labels = TRUE;
	    break;
	}
    }

    if (!has_labels)
	return CAIRO_STATUS_SUCCESS;

    surface->page_labels_res = _cairo_pdf_surface_new_object (surface);
    if (surface->page_labels_res.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = _cairo_pdf_surface_object_begin (surface, surface->page_labels_res);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (surface->object_stream.stream,
				 "<< /Nums [\n");
    prefix = NULL;
    prev_prefix = NULL;
    num = 0;
    prev_num = 0;
    for (i = 0; i < num_elems; i++) {
	_cairo_array_copy_element (&surface->page_labels, i, &label);
	if (label) {
	    prefix = split_label (label, &num);
	} else {
	    prefix = NULL;
	    num = i + 1;
	}

	if (!strcmp_null (prefix, prev_prefix) || num != prev_num + 1) {
	    _cairo_output_stream_printf (surface->object_stream.stream,  "   %d << ", i);

	    if (num)
		_cairo_output_stream_printf (surface->object_stream.stream,  "/S /D /St %d ", num);

	    if (prefix) {
		char *s;
		status = _cairo_utf8_to_pdf_string (prefix, &s);
		if (unlikely (status))
		    return status;

		_cairo_output_stream_printf (surface->object_stream.stream,  "/P %s ", s);
		free (s);
	    }

	    _cairo_output_stream_printf (surface->object_stream.stream,  ">>\n");
	}
	free (prev_prefix);
	prev_prefix = prefix;
	prefix = NULL;
	prev_num = num;
    }
    free (prefix);
    free (prev_prefix);
    _cairo_output_stream_printf (surface->object_stream.stream,
				 "  ]\n"
				 ">>\n");
    _cairo_pdf_surface_object_end (surface);

    return CAIRO_STATUS_SUCCESS;
}

static void
_collect_external_dest (void *entry, void *closure)
{
    cairo_pdf_named_dest_t *dest = entry;
    cairo_pdf_surface_t *surface = closure;
    cairo_pdf_interchange_t *ic = &surface->interchange;

    if (!dest->attrs.internal)
	ic->sorted_dests[ic->num_dests++] = dest;
}

static int
_dest_compare (const void *a, const void *b)
{
    const cairo_pdf_named_dest_t * const *dest_a = a;
    const cairo_pdf_named_dest_t * const *dest_b = b;

    return strcmp ((*dest_a)->attrs.name, (*dest_b)->attrs.name);
}

static cairo_int_status_t
_cairo_pdf_interchange_write_document_dests (cairo_pdf_surface_t *surface)
{
    int i;
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_int_status_t status;
    cairo_pdf_page_info_t *page_info;

    if (ic->num_dests == 0) {
	ic->dests_res.id = 0;
        return CAIRO_STATUS_SUCCESS;
    }

    ic->sorted_dests = calloc (ic->num_dests, sizeof (cairo_pdf_named_dest_t *));
    if (unlikely (ic->sorted_dests == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    ic->num_dests = 0;
    _cairo_hash_table_foreach (ic->named_dests, _collect_external_dest, surface);

    qsort (ic->sorted_dests, ic->num_dests, sizeof (cairo_pdf_named_dest_t *), _dest_compare);

    ic->dests_res = _cairo_pdf_surface_new_object (surface);
    if (ic->dests_res.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = _cairo_pdf_surface_object_begin (surface, ic->dests_res);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (surface->object_stream.stream,
				 "<< /Names [\n");
    for (i = 0; i < ic->num_dests; i++) {
	cairo_pdf_named_dest_t *dest = ic->sorted_dests[i];
	double x = 0;
	double y = 0;

	if (dest->attrs.internal)
	    continue;

	if (dest->extents.valid) {
	    x = dest->extents.extents.x;
	    y = dest->extents.extents.y;
	}

	if (dest->attrs.x_valid)
	    x = dest->attrs.x;

	if (dest->attrs.y_valid)
	    y = dest->attrs.y;

	page_info = _cairo_array_index (&surface->pages, dest->page - 1);
	_cairo_output_stream_printf (surface->object_stream.stream,
				     "   (%s) [%d 0 R /XYZ %f %f 0]\n",
				     dest->attrs.name,
				     page_info->page_res.id,
				     x,
				     page_info->height - y);
    }
    _cairo_output_stream_printf (surface->object_stream.stream,
				 "  ]\n"
				 ">>\n");
    _cairo_pdf_surface_object_end (surface);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_pdf_interchange_write_names_dict (cairo_pdf_surface_t *surface)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_int_status_t status;

    status = _cairo_pdf_interchange_write_document_dests (surface);
    if (unlikely (status))
	return status;

    surface->names_dict_res.id = 0;
    if (ic->dests_res.id != 0) {
	surface->names_dict_res = _cairo_pdf_surface_new_object (surface);
	if (surface->names_dict_res.id == 0)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	status = _cairo_pdf_surface_object_begin (surface, surface->names_dict_res);
	if (unlikely (status))
	    return status;

	_cairo_output_stream_printf (surface->object_stream.stream,
				     "<< /Dests %d 0 R >>\n",
				     ic->dests_res.id);
	_cairo_pdf_surface_object_end (surface);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_pdf_interchange_write_docinfo (cairo_pdf_surface_t *surface)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_int_status_t status;
    unsigned int i, num_elems;
    struct metadata *data;
    unsigned char *p;

    surface->docinfo_res = _cairo_pdf_surface_new_object (surface);
    if (surface->docinfo_res.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = _cairo_pdf_surface_object_begin (surface, surface->docinfo_res);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (surface->object_stream.stream,
				 "<< /Producer (cairo %s (https://cairographics.org))\n",
				 cairo_version_string ());

    if (ic->docinfo.title)
	_cairo_output_stream_printf (surface->object_stream.stream, "   /Title %s\n", ic->docinfo.title);

    if (ic->docinfo.author)
	_cairo_output_stream_printf (surface->object_stream.stream, "   /Author %s\n", ic->docinfo.author);

    if (ic->docinfo.subject)
	_cairo_output_stream_printf (surface->object_stream.stream, "   /Subject %s\n", ic->docinfo.subject);

    if (ic->docinfo.keywords)
	_cairo_output_stream_printf (surface->object_stream.stream, "   /Keywords %s\n", ic->docinfo.keywords);

    if (ic->docinfo.creator)
	_cairo_output_stream_printf (surface->object_stream.stream, "   /Creator %s\n", ic->docinfo.creator);

    if (ic->docinfo.create_date)
	_cairo_output_stream_printf (surface->object_stream.stream, "   /CreationDate %s\n", ic->docinfo.create_date);

    if (ic->docinfo.mod_date)
	_cairo_output_stream_printf (surface->object_stream.stream, "   /ModDate %s\n", ic->docinfo.mod_date);

    num_elems = _cairo_array_num_elements (&ic->custom_metadata);
    for (i = 0; i < num_elems; i++) {
	data = _cairo_array_index (&ic->custom_metadata, i);
	if (data->value) {
	    _cairo_output_stream_printf (surface->object_stream.stream, "   /");
	    /* The name can be any utf8 string. Use hex codes as
	     * specified in section 7.3.5 of PDF reference
	     */
	    p = (unsigned char *)data->name;
	    while (*p) {
		if (*p < 0x21 || *p > 0x7e || *p == '#' || *p == '/')
		    _cairo_output_stream_printf (surface->object_stream.stream, "#%02x", *p);
		else
		    _cairo_output_stream_printf (surface->object_stream.stream, "%c", *p);
		p++;
	    }
	    _cairo_output_stream_printf (surface->object_stream.stream, " %s\n", data->value);
	}
    }

    _cairo_output_stream_printf (surface->object_stream.stream,
				 ">>\n");
    _cairo_pdf_surface_object_end (surface);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_pdf_interchange_begin_structure_tag (cairo_pdf_surface_t    *surface,
					    cairo_tag_type_t        tag_type,
					    const char             *name,
					    const char             *attributes)
{
    int mcid;
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_pdf_command_entry_t *command_entry;
    cairo_pdf_struct_tree_node_t *parent_node;
    unsigned int content_command_id;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) {
	ic->content_emitted = FALSE;
	status = add_tree_node (surface, ic->current_analyze_node, name, attributes, &ic->current_analyze_node);
	if (unlikely (status))
	    return status;

	status = command_list_add (surface, ic->command_id, PDF_BEGIN);
	if (unlikely (status))
	    return status;

	/* Add to command_id to node map. */
	command_entry = _cairo_malloc (sizeof(cairo_pdf_command_entry_t));
	command_entry->recording_id = ic->recording_id;
	command_entry->command_id = ic->command_id;
	command_entry->node = ic->current_analyze_node;
	_cairo_pdf_command_init_key (command_entry);
	status = _cairo_hash_table_insert (ic->command_to_node_map, &command_entry->base);
	if (unlikely(status))
	    return status;

	if (tag_type & TAG_TYPE_LINK) {
	    status = add_annotation (surface, ic->current_analyze_node, name, attributes);
	    if (unlikely (status))
		return status;
	}

	if (ic->current_analyze_node->type == PDF_NODE_CONTENT) {
	    cairo_pdf_content_tag_t *content = _cairo_malloc (sizeof(cairo_pdf_content_tag_t));
	    content->node = ic->current_analyze_node;
	    _cairo_pdf_content_tag_init_key (content);
	    status = _cairo_hash_table_insert (ic->content_tag_map, &content->base);
	    if (unlikely (status))
		return status;
	}

	ic->content_emitted = FALSE;

    } else if (surface->paginated_mode == CAIRO_PAGINATED_MODE_RENDER) {
	if (ic->marked_content_open) {
	    status = _cairo_pdf_operators_tag_end (&surface->pdf_operators);
	    ic->marked_content_open = FALSE;
	    if (unlikely (status))
		return status;
	}

	ic->current_render_node = lookup_node_for_command (surface, ic->recording_id, ic->command_id);
	if (ic->current_render_node->type == PDF_NODE_ARTIFACT) {
	    if (command_list_has_content (surface, ic->command_id, NULL)) {
		status = _cairo_pdf_operators_tag_begin (&surface->pdf_operators, name, -1);
		ic->marked_content_open = TRUE;
	    }
	} else if (ic->current_render_node->type == PDF_NODE_CONTENT_REF) {
	    parent_node = ic->current_render_node->parent;
	    add_child_to_mcid_array (surface, parent_node, ic->command_id, ic->current_render_node);
	} else {
	    parent_node = ic->current_render_node->parent;
	    add_child_to_mcid_array (surface, parent_node, ic->command_id, ic->current_render_node);
	    if (command_list_has_content (surface, ic->command_id, &content_command_id)) {
		add_mcid_to_node (surface, ic->current_render_node, content_command_id, &mcid);
		const char *tag_name = name;
		if (ic->current_render_node->type == PDF_NODE_CONTENT)
		    tag_name = ic->current_render_node->attributes.content.tag_name;

		status = _cairo_pdf_operators_tag_begin (&surface->pdf_operators, tag_name, mcid);
		ic->marked_content_open = TRUE;
	    }
	}
    }

    return status;
}

static cairo_int_status_t
_cairo_pdf_interchange_begin_dest_tag (cairo_pdf_surface_t    *surface,
				       cairo_tag_type_t        tag_type,
				       const char             *name,
				       const char             *attributes)
{
    cairo_pdf_named_dest_t *dest;
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) {
	dest = calloc (1, sizeof (cairo_pdf_named_dest_t));
	if (unlikely (dest == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	status = _cairo_tag_parse_dest_attributes (attributes, &dest->attrs);
	if (unlikely (status))
	{
	    free (dest);
	    return status;
	}

	dest->page = _cairo_array_num_elements (&surface->pages);
	init_named_dest_key (dest);
	status = _cairo_hash_table_insert (ic->named_dests, &dest->base);
	if (unlikely (status)) {
	    free (dest->attrs.name);
	    free (dest);
	    return status;
	}

	_cairo_tag_stack_set_top_data (&ic->analysis_tag_stack, dest);
	ic->num_dests++;
    }

    return status;
}

cairo_int_status_t
_cairo_pdf_interchange_tag_begin (cairo_pdf_surface_t    *surface,
				  const char             *name,
				  const char             *attributes)
{
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_tag_type_t tag_type;
    cairo_pdf_interchange_t *ic = &surface->interchange;

    if (ic->ignore_current_surface)
        return CAIRO_STATUS_SUCCESS;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) {
	status = _cairo_tag_stack_push (&ic->analysis_tag_stack, name, attributes);

    } else if (surface->paginated_mode == CAIRO_PAGINATED_MODE_RENDER) {
	status = _cairo_tag_stack_push (&ic->render_tag_stack, name, attributes);
    }
    if (unlikely (status))
	return status;

    tag_type = _cairo_tag_get_type (name);
    if (tag_type & (TAG_TYPE_STRUCTURE|TAG_TYPE_CONTENT|TAG_TYPE_CONTENT_REF|TAG_TYPE_ARTIFACT)) {
	status = _cairo_pdf_interchange_begin_structure_tag (surface, tag_type, name, attributes);
	if (unlikely (status))
	    return status;
    }

    if (tag_type & TAG_TYPE_DEST) {
	status = _cairo_pdf_interchange_begin_dest_tag (surface, tag_type, name, attributes);
	if (unlikely (status))
	    return status;
    }

    return status;
}

static cairo_int_status_t
_cairo_pdf_interchange_end_structure_tag (cairo_pdf_surface_t    *surface,
					  cairo_tag_type_t        tag_type,
					  cairo_tag_stack_elem_t *elem)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;
    int mcid;
    unsigned int content_command_id;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) {
	assert (ic->current_analyze_node->parent != NULL);
	status = command_list_add (surface, ic->command_id, PDF_END);
	if (unlikely (status))
	    return status;

	ic->content_emitted = FALSE;
	ic->current_analyze_node = ic->current_analyze_node->parent;

    } else if (surface->paginated_mode == CAIRO_PAGINATED_MODE_RENDER) {
	if (ic->marked_content_open) {
	    status = _cairo_pdf_operators_tag_end (&surface->pdf_operators);
	    ic->marked_content_open = FALSE;
	    if (unlikely (status))
		return status;
	}
	ic->current_render_node = ic->current_render_node->parent;
	if (ic->current_render_node->parent &&
	    command_list_has_content (surface, ic->command_id, &content_command_id))
	{
	    add_mcid_to_node (surface, ic->current_render_node, content_command_id, &mcid);
	    status = _cairo_pdf_operators_tag_begin (&surface->pdf_operators,
						     ic->current_render_node->name, mcid);
	    ic->marked_content_open = TRUE;
	}
    }

    return status;
}

cairo_int_status_t
_cairo_pdf_interchange_tag_end (cairo_pdf_surface_t *surface,
				const char          *name)
{
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_tag_type_t tag_type;
    cairo_tag_stack_elem_t *elem;

    if (ic->ignore_current_surface)
        return CAIRO_STATUS_SUCCESS;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) {
	status = _cairo_tag_stack_pop (&ic->analysis_tag_stack, name, &elem);

    } else if (surface->paginated_mode == CAIRO_PAGINATED_MODE_RENDER) {
	status = _cairo_tag_stack_pop (&ic->render_tag_stack, name, &elem);
    }
    if (unlikely (status))
	return status;

    tag_type = _cairo_tag_get_type (name);
    if (tag_type & (TAG_TYPE_STRUCTURE|TAG_TYPE_CONTENT|TAG_TYPE_CONTENT_REF|TAG_TYPE_ARTIFACT)) {
	status = _cairo_pdf_interchange_end_structure_tag (surface, tag_type, elem);
	if (unlikely (status))
	    goto cleanup;
    }

  cleanup:
    _cairo_tag_stack_free_elem (elem);

    return status;
}

cairo_int_status_t
_cairo_pdf_interchange_command_id (cairo_pdf_surface_t  *surface,
				   unsigned int          recording_id,
				   unsigned int          command_id)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    int mcid;
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;

    ic->recording_id = recording_id;
    ic->command_id = command_id;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_RENDER && ic->current_render_node) {
	/* TODO If the group does not have tags we don't need to close the current tag. */
	if (command_list_is_group (surface, command_id)) {
	    if (ic->marked_content_open) {
		status = _cairo_pdf_operators_tag_end (&surface->pdf_operators);
		ic->marked_content_open = FALSE;
	    }
	    if (command_list_has_content (surface, command_id, NULL)) {
		ic->render_next_command_has_content = TRUE;
	    }
	} else if (ic->render_next_command_has_content) {
	    add_mcid_to_node (surface, ic->current_render_node, ic->command_id, &mcid);
	    status = _cairo_pdf_operators_tag_begin (&surface->pdf_operators,
						     ic->current_render_node->name, mcid);
	    ic->marked_content_open = TRUE;
	    ic->render_next_command_has_content = FALSE;
	}
    }

    return status;
}

/* Check if this use of recording surface is or will need to be part of the the struct tree */
cairo_bool_t
_cairo_pdf_interchange_struct_tree_requires_recording_surface (
    cairo_pdf_surface_t           *surface,
    const cairo_surface_pattern_t *recording_surface_pattern,
    cairo_analysis_source_t        source_type)
{
    cairo_surface_t *recording_surface = recording_surface_pattern->surface;
    cairo_surface_t *free_me = NULL;
    cairo_bool_t requires_recording = FALSE;

    if (recording_surface_pattern->base.extend != CAIRO_EXTEND_NONE)
	return FALSE;

    if (_cairo_surface_is_snapshot (recording_surface))
	free_me = recording_surface = _cairo_surface_snapshot_get_target (recording_surface);

    if (_cairo_recording_surface_has_tags (recording_surface)) {
	/* Check if tags are to be ignored in this source */
	switch (source_type) {
	    case CAIRO_ANALYSIS_SOURCE_PAINT:
	    case CAIRO_ANALYSIS_SOURCE_FILL:
		requires_recording = TRUE;
		break;
	    case CAIRO_ANALYSIS_SOURCE_MASK: /* TODO: allow SOURCE_MASK with solid MASK_MASK */
	    case CAIRO_ANALYSIS_MASK_MASK:
	    case CAIRO_ANALYSIS_SOURCE_STROKE:
	    case CAIRO_ANALYSIS_SOURCE_SHOW_GLYPHS:
	    case CAIRO_ANALYSIS_SOURCE_NONE:
		break;
	}
    }

    cairo_surface_destroy (free_me);
    return requires_recording;
}

/* Called at the start of a recording group during analyze. This will
 * be called during the analysis of the drawing operation. */
cairo_int_status_t
_cairo_pdf_interchange_recording_source_surface_begin (
    cairo_pdf_surface_t           *surface,
    const cairo_surface_pattern_t *recording_surface_pattern,
    unsigned int                   region_id,
    cairo_analysis_source_t        source_type)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_recording_surface_stack_entry_t element;
    cairo_int_status_t status;

    /* A new recording surface is being replayed */
    ic->ignore_current_surface = TRUE;
    if (_cairo_pdf_interchange_struct_tree_requires_recording_surface (surface,
								       recording_surface_pattern,
								       source_type))
    {
	ic->ignore_current_surface = FALSE;
    }

    element.ignore_surface = ic->ignore_current_surface;
    element.current_node = ic->current_analyze_node;
    ic->content_emitted = FALSE;

    /* Push to stack so that the current source identifiers can be
     * restored after this recording surface has ended. */
    status = _cairo_array_append (&ic->recording_surface_stack, &element);
    if (status)
	return status;

    if (ic->ignore_current_surface)
	return CAIRO_STATUS_SUCCESS;

    status = command_list_push_group (surface, ic->command_id, recording_surface_pattern->surface, region_id);
    if (unlikely (status))
	return status;

    return CAIRO_INT_STATUS_ANALYZE_RECORDING_SURFACE_PATTERN;
}

/* Called at the end of a recording group during analyze. */
cairo_int_status_t
_cairo_pdf_interchange_recording_source_surface_end (
    cairo_pdf_surface_t           *surface,
    const cairo_surface_pattern_t *recording_surface_pattern,
    unsigned int                   region_id,
    cairo_analysis_source_t        source_type)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_recording_surface_stack_entry_t element;
    cairo_recording_surface_stack_entry_t *element_ptr;

    if (!ic->ignore_current_surface)
	command_list_pop_group (surface);

    if (_cairo_array_pop_element (&ic->recording_surface_stack, &element)) {
	element_ptr = _cairo_array_last_element (&ic->recording_surface_stack);
	if (element_ptr) {
	    ic->ignore_current_surface = element_ptr->ignore_surface;
	    assert (ic->current_analyze_node == element_ptr->current_node);
	} else {
	    /* Back at the page content. */
	    ic->ignore_current_surface = FALSE;
	}
	ic->content_emitted = FALSE;
	return CAIRO_STATUS_SUCCESS;
    }
    ASSERT_NOT_REACHED; /* _recording_source_surface_begin/end mismatch */

    return CAIRO_STATUS_SUCCESS;
}

/* Called at the start of a recording group during render. This will
 * be called after the end of page content. */
cairo_int_status_t
_cairo_pdf_interchange_emit_recording_surface_begin (cairo_pdf_surface_t     *surface,
						     cairo_surface_t         *recording_surface,
						     int                      region_id,
						     cairo_pdf_resource_t     surface_resource,
						     int                     *struct_parents)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_int_status_t status;

    /* When
     * _cairo_pdf_interchange_struct_tree_requires_recording_surface()
     * is false, the region_id of the recording surface is set to 0.
     */
    if (region_id == 0) {
	ic->ignore_current_surface = TRUE;
	return CAIRO_STATUS_SUCCESS;
    }

    command_list_set_current_recording_commands (surface, recording_surface, region_id);

    ic->ignore_current_surface = FALSE;
    _cairo_array_truncate (&ic->mcid_to_tree, 0);
    ic->current_recording_surface_res = surface_resource;

    ic->content_parent_res = _cairo_pdf_surface_new_object (surface);
    if (ic->content_parent_res.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = _cairo_array_append (&ic->parent_tree, &ic->content_parent_res);
    if (unlikely (status))
	return status;

    *struct_parents = _cairo_array_num_elements (&ic->parent_tree) - 1;

    ic->render_next_command_has_content = FALSE;

    return CAIRO_STATUS_SUCCESS;
}

/* Called at the end of a recording group during render. */
cairo_int_status_t
_cairo_pdf_interchange_emit_recording_surface_end (cairo_pdf_surface_t     *surface,
						   cairo_surface_t         *recording_surface)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;

    if (ic->ignore_current_surface)
	return CAIRO_STATUS_SUCCESS;

    ic->current_recording_surface_res.id = 0;
    return cairo_pdf_interchange_write_content_parent_elems (surface);
}

static void _add_operation_extents_to_dest_tag (cairo_tag_stack_elem_t *elem,
						void                   *closure)
{
    const cairo_rectangle_int_t *extents = (const cairo_rectangle_int_t *) closure;
    cairo_pdf_named_dest_t *dest;

    if (_cairo_tag_get_type (elem->name)  & TAG_TYPE_DEST) {
	if (elem->data) {
	    dest = (cairo_pdf_named_dest_t *) elem->data;
	    if (dest->extents.valid) {
		_cairo_rectangle_union (&dest->extents.extents, extents);
	    } else {
		dest->extents.extents = *extents;
		dest->extents.valid = TRUE;
	    }
	}
    }
}

cairo_int_status_t
_cairo_pdf_interchange_add_operation_extents (cairo_pdf_surface_t         *surface,
					      const cairo_rectangle_int_t *extents)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;

    /* Add extents to current node and all DEST tags on the stack */
    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) {
	if (ic->current_analyze_node) {
	    if (ic->current_analyze_node->extents.valid) {
		_cairo_rectangle_union (&ic->current_analyze_node->extents.extents, extents);
	    } else {
		ic->current_analyze_node->extents.extents = *extents;
		ic->current_analyze_node->extents.valid = TRUE;
	    }
	}

	_cairo_tag_stack_foreach (&ic->analysis_tag_stack,
				  _add_operation_extents_to_dest_tag,
				  (void*)extents);
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_int_status_t
_cairo_pdf_interchange_add_content (cairo_pdf_surface_t *surface)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;

    if (ic->ignore_current_surface)
        return CAIRO_STATUS_SUCCESS;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) {
	status = command_list_add (surface, ic->command_id, PDF_CONTENT);
	if (unlikely (status))
	    return status;
    }

    return status;
}

/* Called at the start of 1emiting the page content during analyze or render */
cairo_int_status_t
_cairo_pdf_interchange_begin_page_content (cairo_pdf_surface_t *surface)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;
    int mcid;
    unsigned int content_command_id;
    cairo_pdf_command_list_t *page_commands;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE) {
	status = _cairo_array_allocate (&ic->page_commands, 1, (void**)&page_commands);
	if (unlikely (status))
	    return status;

	_cairo_array_init (&page_commands->commands, sizeof(cairo_pdf_command_t));
	page_commands->parent = NULL;
	ic->current_commands = page_commands;
	ic->ignore_current_surface = FALSE;
    } else if (surface->paginated_mode == CAIRO_PAGINATED_MODE_RENDER) {
	ic->current_commands = _cairo_array_last_element (&ic->page_commands);
	/* Each page has its own parent tree to map MCID to nodes. */
	_cairo_array_truncate (&ic->mcid_to_tree, 0);
	ic->ignore_current_surface = FALSE;
	ic->content_parent_res = _cairo_pdf_surface_new_object (surface);
	if (ic->content_parent_res.id == 0)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	status = _cairo_array_append (&ic->parent_tree, &ic->content_parent_res);
	if (unlikely (status))
	    return status;

	surface->page_parent_tree = _cairo_array_num_elements (&ic->parent_tree) - 1;

	if (ic->next_page_render_node && ic->next_page_render_node->parent &&
	    command_list_has_content (surface, -1, &content_command_id))
	{
	    add_mcid_to_node (surface, ic->next_page_render_node, content_command_id, &mcid);
	    const char *tag_name = ic->next_page_render_node->name;
	    if (ic->next_page_render_node->type == PDF_NODE_CONTENT)
		tag_name = ic->next_page_render_node->attributes.content.tag_name;

	    status = _cairo_pdf_operators_tag_begin (&surface->pdf_operators, tag_name, mcid);
	    ic->marked_content_open = TRUE;
	}
	ic->render_next_command_has_content = FALSE;
    }

    return status;
}

/* Called at the end of emiting the page content during analyze or render */
cairo_int_status_t
_cairo_pdf_interchange_end_page_content (cairo_pdf_surface_t *surface)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_RENDER) {
	/* If a content tag is open across pages, the old page needs an EMC emitted. */
	if (ic->marked_content_open) {
	    status = _cairo_pdf_operators_tag_end (&surface->pdf_operators);
	    ic->marked_content_open = FALSE;
	}
	ic->next_page_render_node = ic->current_render_node;
    }

    return status;
}

cairo_int_status_t
_cairo_pdf_interchange_write_page_objects (cairo_pdf_surface_t *surface)
{
    return cairo_pdf_interchange_write_content_parent_elems (surface);
}

cairo_int_status_t
_cairo_pdf_interchange_write_document_objects (cairo_pdf_surface_t *surface)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_tag_stack_structure_type_t tag_type;
    cairo_bool_t write_struct_tree = FALSE;

    status = cairo_pdf_interchange_update_extents (surface);
    if (unlikely (status))
	return status;

    tag_type = _cairo_tag_stack_get_structure_type (&ic->analysis_tag_stack);
    if (tag_type == TAG_TREE_TYPE_TAGGED || tag_type == TAG_TREE_TYPE_STRUCTURE ||
	tag_type == TAG_TREE_TYPE_LINK_ONLY)
    {
	write_struct_tree = TRUE;
    }

    status = cairo_pdf_interchange_write_annots (surface, write_struct_tree);
    if (unlikely (status))
	return status;

    if (write_struct_tree) {
	surface->struct_tree_root = _cairo_pdf_surface_new_object (surface);
	if (surface->struct_tree_root.id == 0)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	ic->struct_root->res = surface->struct_tree_root;

	status = cairo_pdf_interchange_write_parent_tree (surface);
	if (unlikely (status))
	    return status;

	unsigned num_pages = _cairo_array_num_elements (&ic->page_commands);
	for (unsigned i = 0; i < num_pages; i++) {
	    cairo_pdf_command_list_t *command_list;
	    command_list = _cairo_array_index (&ic->page_commands, i);
	    update_mcid_order (surface, command_list);
	}

	status = cairo_pdf_interchange_write_struct_tree (surface);
	if (unlikely (status))
	    return status;

	if (tag_type == TAG_TREE_TYPE_TAGGED)
	    surface->tagged = TRUE;
    }

    status = cairo_pdf_interchange_write_outline (surface);
    if (unlikely (status))
	return status;

    status = cairo_pdf_interchange_write_page_labels (surface);
    if (unlikely (status))
	return status;

    status = cairo_pdf_interchange_write_forward_links (surface);
    if (unlikely (status))
	return status;

    status = cairo_pdf_interchange_write_names_dict (surface);
    if (unlikely (status))
	return status;

    status = cairo_pdf_interchange_write_docinfo (surface);

    return status;
}

static void
_cairo_pdf_interchange_set_create_date (cairo_pdf_surface_t *surface)
{
    time_t utc, local, offset;
    struct tm tm_local, tm_utc;
    char buf[50];
    int buf_size;
    char *p;
    cairo_pdf_interchange_t *ic = &surface->interchange;

    utc = time (NULL);
    localtime_r (&utc, &tm_local);
    strftime (buf, sizeof(buf), "(D:%Y%m%d%H%M%S", &tm_local);

    /* strftime "%z" is non standard and does not work on windows (it prints zone name, not offset).
     * Calculate time zone offset by comparing local and utc time_t values for the same time.
     */
    gmtime_r (&utc, &tm_utc);
    tm_utc.tm_isdst = tm_local.tm_isdst;
    local = mktime (&tm_utc);
    offset = difftime (utc, local);

    if (offset == 0) {
	strcat (buf, "Z");
    } else {
	if (offset > 0) {
	    strcat (buf, "+");
	} else {
	    strcat (buf, "-");
	    offset = -offset;
	}
	p = buf + strlen (buf);
	buf_size = sizeof (buf) - strlen (buf);
	snprintf (p, buf_size, "%02d'%02d", (int)(offset/3600), (int)(offset%3600)/60);
    }
    strcat (buf, ")");
    ic->docinfo.create_date = strdup (buf);
}

cairo_int_status_t
_cairo_pdf_interchange_init (cairo_pdf_surface_t *surface)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_pdf_outline_entry_t *outline_root;
    cairo_int_status_t status;

    _cairo_tag_stack_init (&ic->analysis_tag_stack);
    _cairo_tag_stack_init (&ic->render_tag_stack);
    ic->struct_root = calloc (1, sizeof(cairo_pdf_struct_tree_node_t));
    if (unlikely (ic->struct_root == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    ic->struct_root->res.id = 0;
    cairo_list_init (&ic->struct_root->children);
    _cairo_array_init (&ic->struct_root->mcid, sizeof(cairo_pdf_page_mcid_t));

    ic->current_analyze_node = ic->struct_root;
    ic->current_render_node = NULL;
    ic->next_page_render_node = ic->struct_root;
    _cairo_array_init (&ic->recording_surface_stack, sizeof(cairo_recording_surface_stack_entry_t));
    ic->current_recording_surface_res.id = 0;
    ic->command_to_node_map = _cairo_hash_table_create (_cairo_pdf_command_equal);
    if (unlikely (ic->command_to_node_map == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    ic->content_tag_map = _cairo_hash_table_create (_cairo_pdf_content_tag_equal);
    if (unlikely (ic->content_tag_map == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_array_init (&ic->parent_tree, sizeof(cairo_pdf_resource_t));
    _cairo_array_init (&ic->mcid_to_tree, sizeof(cairo_pdf_struct_tree_node_t *));
    _cairo_array_init (&ic->annots, sizeof(cairo_pdf_annotation_t *));
    ic->parent_tree_res.id = 0;
    cairo_list_init (&ic->extents_list);
    ic->named_dests = _cairo_hash_table_create (_named_dest_equal);
    if (unlikely (ic->named_dests == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_array_init (&ic->page_commands, sizeof(cairo_pdf_command_list_t));
    ic->current_commands = NULL;
    _cairo_array_init (&ic->recording_surface_commands, sizeof(cairo_pdf_recording_surface_commands_t));

    ic->num_dests = 0;
    ic->sorted_dests = NULL;
    ic->dests_res.id = 0;
    ic->ignore_current_surface = FALSE;
    ic->content_emitted = FALSE;
    ic->marked_content_open = FALSE;
    ic->render_next_command_has_content = FALSE;
    ic->mcid_order = 0;

    _cairo_array_init (&ic->outline, sizeof(cairo_pdf_outline_entry_t *));
    outline_root = calloc (1, sizeof(cairo_pdf_outline_entry_t));
    if (unlikely (outline_root == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    memset (&ic->docinfo, 0, sizeof (ic->docinfo));
    _cairo_array_init (&ic->custom_metadata, sizeof(struct metadata));
    _cairo_pdf_interchange_set_create_date (surface);
    status = _cairo_array_append (&ic->outline, &outline_root);

    return status;
}

static void
_cairo_pdf_interchange_free_outlines (cairo_pdf_surface_t *surface)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    int num_elems, i;

    num_elems = _cairo_array_num_elements (&ic->outline);
    for (i = 0; i < num_elems; i++) {
	cairo_pdf_outline_entry_t *outline;

	_cairo_array_copy_element (&ic->outline, i, &outline);
	free (outline->name);
	_cairo_tag_free_link_attributes (&outline->link_attrs);
	free (outline);
    }
    _cairo_array_fini (&ic->outline);
}

void
_cairo_pdf_interchange_fini (cairo_pdf_surface_t *surface)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    unsigned int i, num_elems;
    struct metadata *data;

    _cairo_tag_stack_fini (&ic->analysis_tag_stack);
    _cairo_tag_stack_fini (&ic->render_tag_stack);
    _cairo_array_fini (&ic->mcid_to_tree);
    cairo_pdf_interchange_clear_annotations (surface);
    _cairo_array_fini (&ic->annots);

    _cairo_array_fini (&ic->recording_surface_stack);
    _cairo_array_fini (&ic->parent_tree);

    _cairo_hash_table_foreach (ic->command_to_node_map,
			       _cairo_pdf_command_pluck,
			       ic->command_to_node_map);
    _cairo_hash_table_destroy (ic->command_to_node_map);

    _cairo_hash_table_foreach (ic->named_dests, _named_dest_pluck, ic->named_dests);
    _cairo_hash_table_destroy (ic->named_dests);

    _cairo_hash_table_foreach (ic->content_tag_map, _cairo_pdf_content_tag_pluck, ic->content_tag_map);
    _cairo_hash_table_destroy(ic->content_tag_map);

    free_node (ic->struct_root);

    num_elems = _cairo_array_num_elements (&ic->recording_surface_commands);
    for (i = 0; i < num_elems; i++) {
	cairo_pdf_recording_surface_commands_t *recording_command;
	cairo_pdf_command_list_t *command_list;

	recording_command = _cairo_array_index (&ic->recording_surface_commands, i);
	command_list = recording_command->command_list;
	_cairo_array_fini (&command_list->commands);
	free (command_list);
    }
    _cairo_array_fini (&ic->recording_surface_commands);

    num_elems = _cairo_array_num_elements (&ic->page_commands);
    for (i = 0; i < num_elems; i++) {
	cairo_pdf_command_list_t *command_list;
	command_list = _cairo_array_index (&ic->page_commands, i);
	_cairo_array_fini (&command_list->commands);
    }
    _cairo_array_fini (&ic->page_commands);

    free (ic->sorted_dests);
    _cairo_pdf_interchange_free_outlines (surface);
    free (ic->docinfo.title);
    free (ic->docinfo.author);
    free (ic->docinfo.subject);
    free (ic->docinfo.keywords);
    free (ic->docinfo.creator);
    free (ic->docinfo.create_date);
    free (ic->docinfo.mod_date);

    num_elems = _cairo_array_num_elements (&ic->custom_metadata);
    for (i = 0; i < num_elems; i++) {
	data = _cairo_array_index (&ic->custom_metadata, i);
	free (data->name);
	free (data->value);
    }
    _cairo_array_fini (&ic->custom_metadata);
}

cairo_int_status_t
_cairo_pdf_interchange_add_outline (cairo_pdf_surface_t        *surface,
				    int                         parent_id,
				    const char                 *name,
				    const char                 *link_attribs,
				    cairo_pdf_outline_flags_t   flags,
				    int                        *id)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_pdf_outline_entry_t *outline;
    cairo_pdf_outline_entry_t *parent;
    cairo_int_status_t status;

    if (parent_id < 0 || parent_id >= (int)_cairo_array_num_elements (&ic->outline))
	return CAIRO_STATUS_SUCCESS;

    outline = _cairo_malloc (sizeof(cairo_pdf_outline_entry_t));
    if (unlikely (outline == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = _cairo_tag_parse_link_attributes (link_attribs, &outline->link_attrs);
    if (unlikely (status)) {
	free (outline);
	return status;
    }

    outline->res = _cairo_pdf_surface_new_object (surface);
    if (outline->res.id == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    outline->name = strdup (name);
    outline->flags = flags;
    outline->count = 0;

    _cairo_array_copy_element (&ic->outline, parent_id, &parent);

    outline->parent = parent;
    outline->first_child = NULL;
    outline->last_child = NULL;
    outline->next = NULL;
    if (parent->last_child) {
	parent->last_child->next = outline;
	outline->prev = parent->last_child;
	parent->last_child = outline;
    } else {
	parent->first_child = outline;
	parent->last_child = outline;
	outline->prev = NULL;
    }

    *id = _cairo_array_num_elements (&ic->outline);
    status = _cairo_array_append (&ic->outline, &outline);
    if (unlikely (status))
	return status;

    /* Update Count */
    outline = outline->parent;
    while (outline) {
	if (outline->flags & CAIRO_PDF_OUTLINE_FLAG_OPEN) {
	    outline->count++;
	} else {
	    outline->count--;
	    break;
	}
	outline = outline->parent;
    }

    return CAIRO_STATUS_SUCCESS;
}

/*
 * Date must be in the following format:
 *
 *     YYYY-MM-DDThh:mm:ss[Z+-]hh:mm
 *
 * Only the year is required. If a field is included all preceding
 * fields must be included.
 */
static char *
iso8601_to_pdf_date_string (const char *iso)
{
    char buf[40];
    const char *p;
    int i;

    /* Check that utf8 contains only the characters "0123456789-T:Z+" */
    p = iso;
    while (*p) {
       if (!_cairo_isdigit (*p) && *p != '-' && *p != 'T' &&
           *p != ':' && *p != 'Z' && *p != '+')
           return NULL;
       p++;
    }

    p = iso;
    strcpy (buf, "(");

   /* YYYY (required) */
    if (strlen (p) < 4)
       return NULL;

    strncat (buf, p, 4);
    p += 4;

    /* -MM, -DD, Thh, :mm, :ss */
    for (i = 0; i < 5; i++) {
	if (strlen (p) < 3)
	    goto finish;

	strncat (buf, p + 1, 2);
	p += 3;
    }

    /* Z, +, - */
    if (strlen (p) < 1)
       goto finish;
    strncat (buf, p, 1);
    p += 1;

    /* hh */
    if (strlen (p) < 2)
	goto finish;

    strncat (buf, p, 2);
    strcat (buf, "'");
    p += 2;

    /* :mm */
    if (strlen (p) < 3)
	goto finish;

    strncat (buf, p + 1, 2);
    strcat (buf, "'");

  finish:
    strcat (buf, ")");
    return strdup (buf);
}

cairo_int_status_t
_cairo_pdf_interchange_set_metadata (cairo_pdf_surface_t  *surface,
				     cairo_pdf_metadata_t  metadata,
				     const char           *utf8)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    cairo_status_t status;
    char *s = NULL;

    if (utf8) {
	if (metadata == CAIRO_PDF_METADATA_CREATE_DATE ||
	    metadata == CAIRO_PDF_METADATA_MOD_DATE) {
	    s = iso8601_to_pdf_date_string (utf8);
	} else {
	    status = _cairo_utf8_to_pdf_string (utf8, &s);
	    if (unlikely (status))
		return status;
	}
    }

    switch (metadata) {
	case CAIRO_PDF_METADATA_TITLE:
	    free (ic->docinfo.title);
	    ic->docinfo.title = s;
	    break;
	case CAIRO_PDF_METADATA_AUTHOR:
	    free (ic->docinfo.author);
	    ic->docinfo.author = s;
	    break;
	case CAIRO_PDF_METADATA_SUBJECT:
	    free (ic->docinfo.subject);
	    ic->docinfo.subject = s;
	    break;
	case CAIRO_PDF_METADATA_KEYWORDS:
	    free (ic->docinfo.keywords);
	    ic->docinfo.keywords = s;
	    break;
	case CAIRO_PDF_METADATA_CREATOR:
	    free (ic->docinfo.creator);
	    ic->docinfo.creator = s;
	    break;
	case CAIRO_PDF_METADATA_CREATE_DATE:
	    free (ic->docinfo.create_date);
	    ic->docinfo.create_date = s;
	    break;
	case CAIRO_PDF_METADATA_MOD_DATE:
	    free (ic->docinfo.mod_date);
	    ic->docinfo.mod_date = s;
	    break;
    }

    return CAIRO_STATUS_SUCCESS;
}

static const char *reserved_metadata_names[] = {
    "",
    "Title",
    "Author",
    "Subject",
    "Keywords",
    "Creator",
    "Producer",
    "CreationDate",
    "ModDate",
    "Trapped",
};

cairo_int_status_t
_cairo_pdf_interchange_set_custom_metadata (cairo_pdf_surface_t  *surface,
					    const char           *name,
					    const char           *value)
{
    cairo_pdf_interchange_t *ic = &surface->interchange;
    struct metadata *data;
    struct metadata new_data;
    int i, num_elems;
    cairo_int_status_t status;
    char *s = NULL;

    if (name == NULL)
	return CAIRO_STATUS_NULL_POINTER;

    for (i = 0; i < ARRAY_LENGTH (reserved_metadata_names); i++) {
	if (strcmp(name, reserved_metadata_names[i]) == 0)
	    return CAIRO_STATUS_INVALID_STRING;
    }

    /* First check if we already have an entry for this name. If so,
     * update the value. A NULL value means the entry has been removed
     * and will not be emitted. */
    num_elems = _cairo_array_num_elements (&ic->custom_metadata);
    for (i = 0; i < num_elems; i++) {
	data = _cairo_array_index (&ic->custom_metadata, i);
	if (strcmp(name, data->name) == 0) {
	    free (data->value);
	    data->value = NULL;
	    if (value && strlen(value)) {
		status = _cairo_utf8_to_pdf_string (value, &s);
		if (unlikely (status))
		    return status;
		data->value = s;
	    }
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    /* Add new entry */
    status = CAIRO_STATUS_SUCCESS;
    if (value && strlen(value)) {
	new_data.name = strdup (name);
	status = _cairo_utf8_to_pdf_string (value, &s);
	if (unlikely (status))
	    return status;
	new_data.value = s;
	status = _cairo_array_append (&ic->custom_metadata, &new_data);
    }

    return status;
}

#if DEBUG_PDF_INTERCHANGE
static cairo_int_status_t
print_node (cairo_pdf_surface_t          *surface,
	    cairo_pdf_struct_tree_node_t *node,
	    int                           depth)
{
    if (node == NULL) {
	printf("%*sNode: ptr: NULL\n", depth*2, "");
    } else if (node == surface->interchange.struct_root) {
       printf("%*sNode: ptr: %p root\n", depth*2, "", node);
    } else {
       printf("%*sNode: ptr: %p name: '%s'\n", depth*2, "", node, node->name);
    }
    depth++;
    printf("%*sType: ", depth*2, "");
    switch (node->type) {
	case PDF_NODE_STRUCT:
	    printf("STRUCT\n");
	    break;
	case PDF_NODE_CONTENT:
	    printf("CONTENT\n");
	    printf("%*sContent.id: %s\n", depth*2, "", node->attributes.content.id);
	    printf("%*sContent.tag_name: %s\n", depth*2, "", node->attributes.content.tag_name);
	    break;
	case PDF_NODE_CONTENT_REF:
	    printf("CONTENT_REF\n");
	    printf("%*sContent_Ref.ref: %s\n", depth*2, "", node->attributes.content_ref.ref);
	    break;
	case PDF_NODE_ARTIFACT:
	    printf("ARTIFACT\n");
	    break;
    }
    printf("%*sres: %d\n", depth*2, "", node->res.id);
    printf("%*sparent: %p\n", depth*2, "", node->parent);
    printf("%*sannot:", depth*2, "");
    if (node->annot)
	printf(" node: %p res: %d", node->annot->node,  node->annot->res.id);
    printf("\n");
    printf("%*sextents: ", depth*2, "");
    if (node->extents.valid) {
	printf("x: %d  y: %d  w: %d  h: %d\n",
	       node->extents.extents.x,
	       node->extents.extents.y,
	       node->extents.extents.width,
	       node->extents.extents.height);
    } else {
	printf("not valid\n");
    }

    printf("%*smcid: ", depth*2, "");
    int num_mcid = _cairo_array_num_elements (&node->mcid);
    for (int i = 0; i < num_mcid; i++) {
	cairo_pdf_page_mcid_t *mcid_elem = _cairo_array_index (&node->mcid, i);
	if (mcid_elem->child_node) {
	    printf("(order: %d, %p) ", mcid_elem->order, mcid_elem->child_node);
	} else {
	    printf("(order: %d, pg: %d, xobject_res: %d, mcid: %d) ",
		   mcid_elem->order, mcid_elem->page, mcid_elem->xobject_res.id, mcid_elem->mcid);
	}
    }
    printf("\n");
    return CAIRO_STATUS_SUCCESS;
}

static void
print_tree (cairo_pdf_surface_t          *surface,
	    cairo_pdf_struct_tree_node_t *node)
{
    printf("Structure Tree:\n");
    cairo_pdf_interchange_walk_struct_tree (surface, node, 0, print_node);
}

static void
print_command (cairo_pdf_command_t *command, int indent)
{
    printf("%*s%d  ", indent*2, "", command->command_id);
    switch (command->flags) {
	case PDF_CONTENT:
	    printf("CONTENT");
	    break;
	case PDF_BEGIN:
	    printf("BEGIN");
	    break;
	case PDF_END:
	    printf("END");
		break;
	case PDF_GROUP:
	    printf("GROUP: %p", command->group);
	    break;
	case PDF_NONE:
	    printf("NONE");
	    break;
    }
    printf("  node: %p index: %d\n", command->node, command->mcid_index);
}

static void
print_commands (cairo_pdf_command_list_t *command_list, int indent)
{
    cairo_pdf_command_t *command;
    unsigned i;
    unsigned num_elements = _cairo_array_num_elements (&command_list->commands);

    for (i = 0; i < num_elements; i++) {
	command = _cairo_array_index (&command_list->commands, i);
	print_command (command, indent);
	if (command->flags == PDF_GROUP)
	    print_commands (command->group, indent + 1);
    }
}

static void
print_command_list(cairo_pdf_command_list_t *command_list)
{
    printf("Command List: %p\n", command_list);
    print_commands (command_list, 0);
    printf("end\n");
}
#endif
