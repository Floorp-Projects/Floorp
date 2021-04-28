/*
   Copyright (C) Intel Corp.  2006.  All Rights Reserved.
   Intel funded Tungsten Graphics (http://www.tungstengraphics.com) to
   develop this 3D driver.

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice (including the
   next paragraph) shall be included in all copies or substantial
   portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
   LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
   OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 **********************************************************************/
/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 */

#include "cairoint.h"
#include "cairo-drm-intel-brw-eu.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>


/* How does predicate control work when execution_size != 8?  Do I
 * need to test/set for 0xffff when execution_size is 16?
 */
void brw_set_predicate_control_flag_value( struct brw_compile *p, uint32_t value )
{
    p->current->header.predicate_control = BRW_PREDICATE_NONE;

    if (value != 0xff) {
	if (value != p->flag_value) {
	    brw_push_insn_state(p);
	    brw_MOV(p, brw_flag_reg(), brw_imm_uw(value));
	    p->flag_value = value;
	    brw_pop_insn_state(p);
	}

	p->current->header.predicate_control = BRW_PREDICATE_NORMAL;
    }
}

void brw_set_predicate_control( struct brw_compile *p, uint32_t pc )
{
    p->current->header.predicate_control = pc;
}

void brw_set_conditionalmod( struct brw_compile *p, uint32_t conditional )
{
    p->current->header.destreg__conditonalmod = conditional;
}

void brw_set_access_mode( struct brw_compile *p, uint32_t access_mode )
{
    p->current->header.access_mode = access_mode;
}

void brw_set_compression_control( struct brw_compile *p, int compression_control )
{
    p->current->header.compression_control = compression_control;
}

void brw_set_mask_control( struct brw_compile *p, uint32_t value )
{
    p->current->header.mask_control = value;
}

void brw_set_saturate( struct brw_compile *p, uint32_t value )
{
    p->current->header.saturate = value;
}

void brw_push_insn_state( struct brw_compile *p )
{
    assert(p->current != &p->stack[BRW_EU_MAX_INSN_STACK-1]);
    memcpy(p->current+1, p->current, sizeof(struct brw_instruction));
    p->current++;
}

void brw_pop_insn_state( struct brw_compile *p )
{
    assert(p->current != p->stack);
    p->current--;
}

/************************************************************************/
void
brw_compile_init (struct brw_compile *p,
		  cairo_bool_t is_g4x)
{
    p->nr_insn = 0;
    p->current = p->stack;
    memset (p->current, 0, sizeof (p->current[0]));

    p->is_g4x = is_g4x;

    /* Some defaults?  */
    brw_set_mask_control (p, BRW_MASK_ENABLE); /* what does this do? */
    brw_set_saturate (p, 0);
    brw_set_compression_control (p, BRW_COMPRESSION_NONE);
    brw_set_predicate_control_flag_value (p, 0xff);
}

const uint32_t *
brw_get_program (struct brw_compile *p,
		 uint32_t *sz)
{
    *sz = p->nr_insn * sizeof (struct brw_instruction);
    return (const uint32_t *)p->store;
}



/*
 * Subroutine calls require special attention.
 * Mesa instructions may be expanded into multiple hardware instructions
 * so the prog_instruction::BranchTarget field can't be used as an index
 * into the hardware instructions.
 *
 * The BranchTarget field isn't needed, however.  Mesa's GLSL compiler
 * emits CAL and BGNSUB instructions with labels that can be used to map
 * subroutine calls to actual subroutine code blocks.
 *
 * The structures and function here implement patching of CAL instructions
 * so they jump to the right subroutine code...
 */


/*
 * For each OPCODE_BGNSUB we create one of these.
 */
struct brw_glsl_label
{
    const char *name; /*< the label string */
    uint32_t position;  /*< the position of the brw instruction for this label */
    struct brw_glsl_label *next;  /*< next in linked list */
};


/*
 * For each OPCODE_CAL we create one of these.
 */
struct brw_glsl_call
{
    uint32_t call_inst_pos;  /*< location of the CAL instruction */
    const char *sub_name;  /*< name of subroutine to call */
    struct brw_glsl_call *next;  /*< next in linked list */
};


/*
 * Called for each OPCODE_BGNSUB.
 */
    void
brw_save_label(struct brw_compile *c, const char *name, uint32_t position)
{
    struct brw_glsl_label *label = calloc(1, sizeof *label);
    label->name = name;
    label->position = position;
    label->next = c->first_label;
    c->first_label = label;
}


/*
 * Called for each OPCODE_CAL.
 */
    void
brw_save_call(struct brw_compile *c, const char *name, uint32_t call_pos)
{
    struct brw_glsl_call *call = calloc(1, sizeof *call);
    call->call_inst_pos = call_pos;
    call->sub_name = name;
    call->next = c->first_call;
    c->first_call = call;
}


/*
 * Lookup a label, return label's position/offset.
 */
    static uint32_t
brw_lookup_label(struct brw_compile *c, const char *name)
{
    const struct brw_glsl_label *label;
    for (label = c->first_label; label; label = label->next) {
	if (strcmp(name, label->name) == 0) {
	    return label->position;
	}
    }
    abort();  /* should never happen */
    return ~0;
}


/*
 * When we're done generating code, this function is called to resolve
 * subroutine calls.
 */
    void
brw_resolve_cals(struct brw_compile *c)
{
    const struct brw_glsl_call *call;

    for (call = c->first_call; call; call = call->next) {
	const uint32_t sub_loc = brw_lookup_label(c, call->sub_name);
	struct brw_instruction *brw_call_inst = &c->store[call->call_inst_pos];
	struct brw_instruction *brw_sub_inst = &c->store[sub_loc];
	int32_t offset = brw_sub_inst - brw_call_inst;

	/* patch brw_inst1 to point to brw_inst2 */
	brw_set_src1(brw_call_inst, brw_imm_d(offset * 16));
    }

    /* free linked list of calls */
    {
	struct brw_glsl_call *call, *next;
	for (call = c->first_call; call; call = next) {
	    next = call->next;
	    free(call);
	}
	c->first_call = NULL;
    }

    /* free linked list of labels */
    {
	struct brw_glsl_label *label, *next;
	for (label = c->first_label; label; label = next) {
	    next = label->next;
	    free(label);
	}
	c->first_label = NULL;
    }
}
