/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "xp.h"
#include "pa_tags.h"
#include "edttypes.h"
#include "csid.h"

#define PA_ABORT    -1
#define PA_PARSED   0
#define PA_COMPLETE 1

#define COMMENT_NO        0
#define COMMENT_YES       1
#define COMMENT_MAYBE     2
#define COMMENT_UNCOMMENT 3         /* A conditional comment which turned
                                   out not to be a comment.  The
                                   commit delimiters need to be
                                   stripped off. */
#define COMMENT_PROCESS   4	/* processing instruction <? gunk ?> */


/*************************
 * The following is to speed up case conversion
 * to allow faster checking of caseless equal among strings.
 * Used in pa_TagEqual().
 *************************/
#ifdef NON_ASCII_STRINGS
# define TOLOWER(x) (tolower((unsigned int)(x)))
#else /* ASCII TABLE LOOKUP */
  extern unsigned char lower_lookup[256];
# define TOLOWER(x)      (lower_lookup[(unsigned int)(x)])
#endif /* NON_ASCII_STRINGS */


/*******************************
 * PRIVATE STRUCTURES
 *******************************/
typedef struct pa_Overflow_struct {
  XP_Block buf;
  int32 size;
  int32 len;
  struct pa_Overflow_struct *next;
} pa_Overflow;

typedef struct pa_DocData_struct {
    int32 doc_id;
    MWContext *window_id;
    URL_Struct *url_struct;
    PA_OutputFunction *output_tag;
    XP_Block hold_buf;	   /* for when we are partway though a tag */
    int32 hold_size;
    int32 hold_len;
	pa_Overflow *overflow_stack;
    int overflow_depth;	      /* send data to overflow_buf */
    int32 brute_tag;
    int32 comment_bytes;
    void *layout_state;
    char *url;
    FO_Present_Types format_out;
    ED_Buffer *edit_buffer;
    NET_StreamClass *parser_stream;
    uint no_newline_count;
    uint newline_count;
    int stream_count;
    int stream_status;
    PRPackedBool from_net;
    PRPackedBool is_inline_stream;    /* Does this doc_data correspond to an inline
				         stream */
    PRPackedBool hold;		      /* send data to hold_buf */
    PRPackedBool waiting_for_js_thread;
    PRPackedBool lose_newline;
} pa_DocData;

struct pa_TagTable { char *name; int id; };
 

/*******************************
 * PUBLIC FUNCTIONS
 *******************************/
extern void PA_FreeTag(PA_Tag *);
extern void PA_FetchRequestedNameValues(PA_Tag *tag, char *namesToFind[], int32 numNamesToFind, char *values[], uint16 win_csid);
extern PA_Block PA_FetchParamValue(PA_Tag *, char *, uint16);
extern int32 PA_FetchAllNameValues(PA_Tag *, char ***, char ***, uint16);
extern Bool PA_TagHasParams(PA_Tag *);

extern pa_DocData * PA_HoldDocData(pa_DocData * doc_data);
extern pa_DocData * PA_DropDocData(NET_StreamClass *stream);

extern void PA_PushOverflow(pa_DocData* doc_data);
extern pa_Overflow* PA_PopOverflow(pa_DocData* doc_data);
extern void PA_FreeOverflow(pa_Overflow *overflow);
extern int PA_GetOverflowDepth(pa_DocData *doc_data);
extern XP_Block PA_GetOverflowBuf(pa_DocData *doc_data);

/*******************************
 * PRIVATE FUNCTIONS
 *******************************/
extern intn pa_TagEqual(char *, char *);
extern char *pa_FindMDLTag(pa_DocData *, char *, int32, intn *);
extern char *pa_FindMDLEndTag(pa_DocData *, char *, int32);
extern char *pa_FindMDLEndComment(pa_DocData *, char *, int32);
extern char *pa_FindMDLEndProcessInstruction(pa_DocData *, char *, int32);
extern PA_Tag *pa_CreateTextTag(pa_DocData *, char *, int32);
extern PA_Tag *pa_CreateMDLTag(pa_DocData *, char *, int32);
extern char *pa_ExpandEscapes(char *, int32, int32 *, Bool, int16);
extern struct pa_TagTable * pa_LookupTag (char* str, unsigned int len);

extern const char *pa_PrintTagToken(int32);    /* for debugging only */
extern intn pa_tokenize_tag(char *str);
extern void pa_FlushOverflow(NET_StreamClass *stream);

/*
extern intn LO_Format(int32, PA_Tag *, intn);
*/

