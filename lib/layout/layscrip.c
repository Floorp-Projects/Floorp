/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*
 * SCRIPT tag support.
 */

#include "libmocha.h"
#include "lo_ele.h"
#include "net.h"
#include "pa_parse.h"
#include "pa_tags.h"
#include "layout.h"
#include "libevent.h"
#include "css.h"
#include "laystyle.h"
#include "mcom_db.h"
#include "laylayer.h"
#include "prefapi.h"

static char lo_jsAllowFileSrcFromNonFile[]
    = "javascript.allow.file_src_from_non_file";

/*
 * XXX Should dynamic layer resize-reload really rerun scripts, potentially
 * XXX destroying and reinitializing JS state?  An onResize event handler
 * XXX would be consistent with how windows and static layers resize-reload.
 */
#define SCRIPT_EXEC_OK(top_state, state, type, script_type)                   \
        (top_state->resize_reload == FALSE ||                                 \
         type != script_type ||                                               \
         (lo_InsideLayer(state) && (lo_IsAnyCurrentAncestorDynamic(state) ||  \
                                    lo_IsAnyCurrentAncestorSourced(state))))

static void
lo_ParseScriptLanguage(MWContext * context, PA_Tag * tag, int8 * type,
		       JSVersion * version)
{
    PA_Block buff;
    char * str;

    buff = lo_FetchParamValue(context, tag, PARAM_LANGUAGE);
    if (buff != NULL) {
        PA_LOCK(str, char *, buff);
        if ((XP_STRCASECMP(str, js_language_name) == 0) ||
            (XP_STRCASECMP(str, "LiveScript") == 0) ||
            (XP_STRCASECMP(str, "Mocha") == 0)) {
            *type = SCRIPT_TYPE_MOCHA;
            *version = JSVERSION_DEFAULT;
        } 
        else if (XP_STRCASECMP(str, "JavaScript1.1") == 0) {
            *type = SCRIPT_TYPE_MOCHA;
            *version = JSVERSION_1_1;
        } 
        else if (XP_STRCASECMP(str, "JavaScript1.2") == 0) {
            *type = SCRIPT_TYPE_MOCHA;
            *version = JSVERSION_1_2;
        } 
        else if (XP_STRCASECMP(str, "JavaScript1.3") == 0) {
            *type = SCRIPT_TYPE_MOCHA;
            *version = JSVERSION_1_3;
        } 

        else {
            *type = SCRIPT_TYPE_UNKNOWN;
        }
        PA_FREE(buff);
    }
}

void
lo_BlockScriptTag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
    lo_TopState *top_state;
    pa_DocData *doc_data;

    if (!LM_CanDoJS(context))
	return;

    /*
     * Find the parser stream from its private data, doc_data, pointed at by
     * a top_state member for just this situation.
     */
    top_state = state->top_state;
    doc_data = top_state->doc_data;
    XP_ASSERT(doc_data != NULL && doc_data->url_struct != NULL);
    if (doc_data == NULL || doc_data->url_struct == NULL)   /* XXX paranoia */
        return;

    /*
     * The parser must not parse ahead after a script or style end-tag, or
     * it could get into a brute_tag state (XMP, PRE, etc.) and enter a mode
     * that would mess up document.writes coming from the script or style.
     * So we put it into overflow mode here, and decrement overflow when we
     * unblock.
     *
     * We test tag first to handle the call from lo_ProcessScriptTag, which
     * wants us to create an input state if needed, but not to count comment
     * bytes via lo_data.  Sad to say, this means lo_ProcessScriptTag must
     * also increment doc_data->overflow, but only if tag->is_end == 1 (a.k.a.
     * PR_TRUE), meaning the script tag was not blocked by an earlier blocking
     * tag that caused it to go through here and set is_end to 2.  XXX we can
     * do this only because all layout and libparse tests of is_end are of the
     * form "tag->is_end == FALSE".
     */
    if (tag) {
        /* Tag non-null means we're called from lo_BlockTag in layout.c. */
        if (tag->is_end == FALSE) {
			int8 script_type;
			JSVersion ver;

            /* Keep track of whether we're in a script or style container... */
			script_type = SCRIPT_TYPE_UNKNOWN;
			lo_ParseScriptLanguage(context, tag, &script_type, &ver);
			if (tag->type == P_STYLE || tag->type == P_LINK || 
				script_type != SCRIPT_TYPE_UNKNOWN)
				top_state->in_blocked_script = TRUE;
        }
        else if (top_state->in_blocked_script) {
            /* ...so we can avoid overflowing the parser on superfluous end
             * tags, such as those emitted by libmime.
             */
            top_state->in_blocked_script = FALSE;

            if (SCRIPT_EXEC_OK(top_state, state, tag->type, P_SCRIPT)) {
                tag->is_end = (PRPackedBool)2;
				PA_PushOverflow(doc_data);
				doc_data->overflow_depth ++;
            }
        }
    }

    /*
     * Count the bytes of HTML comments and heuristicly "lost" newlines in
     * tag so lo_ProcessScriptTag can recover it there later, in preference
     * to the (then far-advanced) value of doc_data->comment_bytes.  Ensure
     * that this hack is not confused for NULL by adding 1 here and taking
     * it away later.  Big XXX for future cleanup, needless to say.
     */
    if (tag)
        tag->lo_data = (void *)(doc_data->comment_bytes + 1);
    
    if (top_state->script_tag_count++ == 0)
        ET_SetDecoderStream(context, doc_data->parser_stream,
                            doc_data->url_struct, JS_FALSE);
}


typedef struct {
    MWContext *context;
    lo_DocState *state;
    PA_Tag *tag;
    char *url, *archiveSrc, *id, *codebase;
    char *buffer;
    uint32 bufferSize;
    JSVersion version;
    JSBool inlineSigned;
} ScriptData;

void 
lo_DestroyScriptData(void *arg)
{
    ScriptData *data = arg;

    if (data == NULL)
	return;
    XP_FREEIF(data->url);
    XP_FREEIF(data->archiveSrc);
    XP_FREEIF(data->id);
    XP_FREEIF(data->codebase);
    XP_FREEIF(data->buffer);
    if (data->tag)
	PA_FreeTag(data->tag);
    XP_FREE(data);
}

static void
lo_script_src_exit_fn(URL_Struct *url_struct, int status, MWContext *context);

static void
lo_script_archive_exit_fn(URL_Struct *url_struct, int status, MWContext *context);

static Bool
lo_create_script_blockage(MWContext *context, lo_DocState *state, int type)
{
    lo_TopState *top_state;
    LO_Element *block_ele;

    top_state = state->top_state;
    if (!top_state) {
        XP_ASSERT(0);
        return FALSE;
    }
    
    block_ele = lo_NewElement(context, state, LO_SCRIPT, NULL, 0);
    if (block_ele == NULL) {
        top_state->out_of_memory = TRUE;
	return FALSE;
    }

    block_ele->type = type;
    block_ele->lo_any.ele_id = NEXT_ELEMENT;
    top_state->layout_blocking_element = block_ele;
    if (type == LO_SCRIPT)
	top_state->current_script = block_ele;

    return TRUE;
}

/*
 * used for ARCHIVE= and SRC= 
 * Create name of form "archive.jar/src.js" 
 */
static char *
lo_BuildJSArchiveURL( char *archive_name, char *filename )
{
    uint32 len = XP_STRLEN(archive_name) + XP_STRLEN(filename) + 2;
    char *path = XP_ALLOC(len);
    if (path)
    {
        XP_STRCPY(path, archive_name);
        XP_STRCAT(path, "/");
        XP_STRCAT(path, filename);
    }
    return path;
}

/*
 * Load a document from an external URL.  Assumes that layout is
 *   already blocked
 */
static void
lo_GetScriptFromURL(ScriptData *data, int script_type) 
{
    URL_Struct *url_struct;
    lo_TopState *top_state = data->state->top_state;
    PA_Tag *tag = data->tag;

    url_struct = NET_CreateURLStruct(data->url, top_state->force_reload);
    if (url_struct == NULL) {
        top_state->out_of_memory = TRUE;
	lo_DestroyScriptData(data);
	return;
    }

    url_struct->must_cache = TRUE;
    url_struct->preset_content_type = TRUE;
    if (script_type == SCRIPT_TYPE_CSS) {
        StrAllocCopy(url_struct->content_type, TEXT_CSS);
    } else {
	if (tag->type == P_STYLE || tag->type == P_LINK) {
	    StrAllocCopy(url_struct->content_type, TEXT_JSSS);
	} else {
	    StrAllocCopy(url_struct->content_type, js_content_type);
	}
    }

    XP_ASSERT(top_state->layout_blocking_element);

    if (!url_struct->content_type)
	goto out;

    if (!data->inlineSigned) {
        if (data->archiveSrc) {
            /* ARCHIVE= and SRC= */
            /*
             * Need to set nesting url. Create name of form
             * "archive.jar/src.js" 
             */
             
            char *path = lo_BuildJSArchiveURL(url_struct->address,
                                              data->archiveSrc);    
            if (!path)
                goto out;
            ET_SetNestingUrl(data->context, path);
            /* version taken care of in lo_script_archive_exit_fn */
            XP_FREE(path);
        } else {
            /* SRC= but no ARCHIVE= */
            ET_SetNestingUrl(data->context, url_struct->address);
            ET_SetVersion(data->context, data->version);
        }
    }

    url_struct->fe_data = data;

    if (data->archiveSrc != NULL || data->inlineSigned) {
        NET_GetURL(url_struct, 
                   FO_CACHE_ONLY,
                   data->context,
                   lo_script_archive_exit_fn);
    } else {
        NET_GetURL(url_struct, 
                   FO_CACHE_AND_PRESENT, 
                   data->context,
                   lo_script_src_exit_fn);
    }

    return;
  out:
    top_state->out_of_memory = TRUE;
    NET_FreeURLStruct(url_struct);
    lo_DestroyScriptData(data);
    return;
}

/*
 * A script has just completed.  If we are blocked on that script
 *   free the blockage
 */
static void
lo_unblock_script_tag(MWContext * context, Bool messWithParser)
{
    lo_TopState *top_state;
    lo_DocState *state;
    LO_Element * block_ele;
    LO_Element * current_script;
    PA_Tag * tag;
	NET_StreamClass stream;

    top_state = lo_FetchTopState(XP_DOCID(context));
    if (!top_state || !top_state->doc_state)
        return;
    state = top_state->doc_state;
    
    /* 
     * Remember the fake element we created but clear the current
     *   script flag of the top_state since the FlushBlockage call
     *   might just turn around and block us on another script
     */
    current_script = top_state->current_script;
    top_state->current_script = NULL;

    /* 
     * if we are finishing a nested script make sure the current_script
     *   points to our parent script
     */
    for (tag = top_state->tags; tag; tag = tag->next) {
        if (tag->type == P_NSCP_REBLOCK) {
	    top_state->current_script = tag->lo_data;
	    break;
        }
    }


    /* Flush tags blocked by this <SCRIPT SRC="URL"> tag. */
    block_ele = top_state->layout_blocking_element;

    /* no longer in a script */
    top_state->in_script = SCRIPT_TYPE_NOT;

    /* 
     * we must be blocked on something either the script that just ended or
     *   some other object
     */
    if (!block_ele)
        goto done;

    if (messWithParser && top_state->doc_data &&
        SCRIPT_EXEC_OK(top_state, state, block_ele->type, LO_SCRIPT)) {
	  top_state->doc_data->overflow_depth --;
	XP_ASSERT(top_state->doc_data->overflow_depth >= 0);
    }

	lo_FlushLineBuffer(context, top_state->doc_state);

    /* 
     * unblock on UNKNOWN's as well since that's the type for
     * style attribute scripts
     */
    if (block_ele->type == LO_SCRIPT || block_ele->type == LO_UNKNOWN) {
        lo_UnblockLayout(context, top_state);
    }
    else {
        /* 
         * we're still blocked on something else - make sure
         *   there are no reblock tags that are going to hose
         *   us.  Find the reblock tags that point to our script
         *   tag and render them hermless
         */
        PA_Tag * tag;
        for (tag = top_state->tags; tag; tag = tag->next) {
            if (tag->type == P_NSCP_REBLOCK) {
                LO_Element * lo_ele = tag->lo_data;
                if (lo_ele == current_script) {
                    tag->lo_data = NULL;
                }
            }
        }
    }

    /* free the fake element we created earlier */
    lo_FreeElement(context, current_script, FALSE);

done:
    
    /* the parser is now able to receive new data from netlib */
	stream.data_object=(pa_DocData *)top_state->doc_data;
    pa_FlushOverflow(&stream);

    /*
     * The initial call to lo_UnblockLayout() might have not torn
     *   everything down because we were waiting for the last
     *   chunk of data to come out of the doc_data->overflow_buf
     * We need to get to lo_FlushedBlockedTags() and lo_FinishLayout()
     *   at the bottom of lo_FlushBlockage().
     */
    if (!top_state->layout_blocking_element)
	lo_FlushBlockage(context, top_state->doc_state, top_state->doc_state);

}


static void
lo_script_src_exit_fn(URL_Struct *url_struct, int status, MWContext *context)
{
    lo_DestroyScriptData(url_struct->fe_data);
    NET_FreeURLStruct(url_struct);

    /*
     * If our status is negative that means there was an error and the
     *   script won't be executed.  Therefore the MochaDecoder's nesting
     *   url must be cleared, and layout must be unblocked.
     */
    if (status < 0) {
        ET_SetNestingUrl(context, NULL);
        lo_unblock_script_tag(context, TRUE);
    }
}

static void
lo_script_archive_exit_fn(URL_Struct *url_struct, int status, MWContext *context)
{
    ScriptData *data = NULL;
#ifdef JAVA
	/* Vars only used in JAVA context */
    char *name;
    JSPrincipals *principals;
#endif
    ETEvalStuff * stuff;

    data = (ScriptData *) url_struct->fe_data;
    stuff = (ETEvalStuff *) XP_NEW_ZAP(ETEvalStuff);
    if (!stuff)
	return;

    stuff->line_no = 1;
    stuff->scope_to = NULL;
    stuff->want_result = JS_FALSE;
    stuff->version = data->version;
    stuff->data = context;

#ifndef JAVA
    /* No Java; execute without principals. */
    if (data->buffer) {
	stuff->principals = NULL;
        ET_EvaluateScript(context, data->buffer, stuff, lo_ScriptEvalExitFn);
    }
    else {
	XP_FREE(stuff);
    }
#else
    name = data->archiveSrc ? data->archiveSrc : data->id;
    principals = LM_NewJSPrincipals(url_struct, name, data->codebase);
    if (principals != NULL) {
        char *src;
        uint srcSize;

        if (data->archiveSrc) {
            /* Extract from archive using "SRC=" value */
            src = LM_ExtractFromPrincipalsArchive(principals, data->archiveSrc, 
                                                  &srcSize);
                                                  
#ifdef JSDEBUGGER
        if( src != NULL && LM_GetJSDebugActive() )
        {
            char *path = lo_BuildJSArchiveURL(url_struct->address,
            data->archiveSrc);
            if (path)
            {
                LM_JamSourceIntoJSDebug( path, src, srcSize, context );
                XP_FREE(path);
            }
        }
#endif /* JSDEBUGGER */

            if (src == NULL) {
                /* Unsuccessful extracting from archive. Now try normal SRC= lookup. */
                (*principals->destroy)(NULL, principals);
                data->url = NET_MakeAbsoluteURL(data->state->top_state->base_url, 
                                                data->archiveSrc);
                XP_FREEIF(data->archiveSrc);
                data->archiveSrc = NULL;
                lo_GetScriptFromURL(data, data->state->top_state->in_script);
                goto out;
	    }
        } else {
            /* Should be an inline script */
            src = data->buffer;
            srcSize = data->bufferSize;
        }

	stuff->len = srcSize;
	stuff->principals = principals;
        ET_EvaluateScript(context, src, stuff, lo_ScriptEvalExitFn);

        if (data->archiveSrc){
            ET_SetNestingUrl(context, NULL);
            XP_FREE(src);
        }            
    }
    else {
	XP_FREE(stuff);
    }
	 
#endif /* ifdef JAVA */

    lo_DestroyScriptData(data);

#ifdef JAVA	/* Label only used in this context */
out:
#endif

    /* Always free (or drop a ref on) the url_struct before returning. */
    NET_FreeURLStruct(url_struct);
}    

/*
 * This function gets called when the LM_EvaluateBuffer() operation
 *   completes (in the mocha thread, but we run in the mozilla thread
 *   here)
 */
void
lo_ScriptEvalExitFn(void * data, char * str, size_t len, char * wysiwyg_url, 
                    char * base_href, Bool valid)
{

    /* we don't care about the strings here just kill them */
    XP_FREEIF(str);
    XP_FREEIF(wysiwyg_url);
    XP_FREEIF(base_href);

    lo_unblock_script_tag(data, TRUE);

}

/*
 * This function gets called when the LM_EvaluateBuffer() operation
 *   completes (in the mocha thread, but we run in the mozilla thread
 *   here)
 */
PRIVATE
void
lo_StyleEvalExitFn(void * data, char * str, size_t len, char * wysiwyg_url, 
                    char * base_href, Bool valid)
{

    /* we don't care about the strings here just kill them */
    XP_FREEIF(str);
    XP_FREEIF(wysiwyg_url);
    XP_FREEIF(base_href);

    lo_unblock_script_tag(data, FALSE);

}


void
lo_UnblockLayout(MWContext *context, lo_TopState *top_state)
{
    XP_ASSERT(top_state && top_state->doc_state);
    if (!top_state || !top_state->doc_state)
        return;

    top_state->layout_blocking_element = NULL;
    lo_FlushBlockage(context, top_state->doc_state, top_state->doc_state);
}


Bool
lo_CloneTagAndBlockLayout(MWContext *context, lo_DocState *state, PA_Tag *tag)
{

    PA_Tag *new_tag;
    lo_TopState * top_state = state->top_state;

    /*
     * Since we're adding tags to the blocked tags list without going through
     * lo_BlockTag, we need to see if this is a flushed to block transition
     * and, if so, add to the doc_data's ref count. We don't want to do this
     * if we're in the process of flushing blockage - in that case, the count
     *  has already been incremented.
     */
    if (top_state->tags == NULL && top_state->flushing_blockage == FALSE)
	PA_HoldDocData(top_state->doc_data);

    /* 
     * stick the current tag onto the front of the tag list
     */
    new_tag = PA_CloneMDLTag(tag);
    if (!new_tag) {
        top_state->out_of_memory = TRUE;
        return FALSE;
    }

    new_tag->next = top_state->tags;
    top_state->tags = new_tag;
    if (top_state->tags_end == &top_state->tags)
        top_state->tags_end = &new_tag->next;

    return lo_create_script_blockage(context, state, LO_UNKNOWN);
}

/* evaluate the buffer encompassed by the style attribute on any tag.
 * this will be JSS or CSS 
 *
 * Returns TRUE if it needs to block layout
 * Returns FALSE if not.
 */
XP_Bool
lo_ProcessStyleAttribute(MWContext *context, lo_DocState *state, PA_Tag *tag, char *script_buff)
{
    lo_TopState *top_state;
    char *scope_to = NULL;
    int32 script_buff_len;
    char *new_id;
    XP_Bool in_double_quote=FALSE, in_single_quote=FALSE;
    char *ptr, *end;
    ETEvalStuff * stuff;

    if (!state)
  	{
       XP_FREEIF(script_buff);
	   return FALSE;
	}


    if (!script_buff)
        return FALSE;

    /* @@@ A bit of a hack.
     * Get rid of the style attribute in the tag->data so that we don't
     * go circular
     *
     * get rid of STYLE by rewriting it as SYTLE so that the parser 
     * ignores it :)
     */
    for (ptr=(char*)tag->data, end = ptr+tag->data_len; ptr < end; ptr++) {
        if (*ptr == '"') {
            in_double_quote = !in_double_quote;
        }
        else if (*ptr == '\'') {
            in_single_quote = !in_single_quote;
        }
        else if (!in_single_quote &&
                 !in_double_quote &&
                 !strncasecomp(ptr, PARAM_STYLE, sizeof(PARAM_STYLE)-1)) {
            *ptr = 'T';    /* ttyle NOT style */
            break;
        }
    }

    script_buff_len = XP_STRLEN(script_buff);

    top_state = state->top_state;

    /* get the tag ID if it has one */
    new_id = (char *)lo_FetchParamValue(context, tag, PARAM_ID);

    if (!new_id)
        new_id = PR_smprintf(NSIMPLICITID"%ld", top_state->tag_count);

    if (!new_id)
        return FALSE;

    if (top_state->default_style_script_type == SCRIPT_TYPE_JSSS ||
	top_state->default_style_script_type == SCRIPT_TYPE_MOCHA) {

    	StrAllocCopy(scope_to, "document.ids.");
    	StrAllocCat(scope_to, new_id);

    	if (!scope_to) {
    	    XP_FREE(new_id);
	    return(FALSE);
	}
    }
    else if (top_state->default_style_script_type == SCRIPT_TYPE_CSS) {
        char *new_jss_buff;
        int32 new_jss_buff_len;
        char *new_css_buff;
        int32 new_css_buff_len;

        /* scope the css to the ns implicit ID */
        new_css_buff = PR_smprintf("#%s { %s }", new_id, script_buff);

        XP_FREE(script_buff);

        if (!new_css_buff) {
	    XP_FREE(new_id);
	    return FALSE;
	}

        new_css_buff_len = XP_STRLEN(new_css_buff);
        
        /* convert to JSS */
        CSS_ConvertToJS(new_css_buff,
                        new_css_buff_len,
                        &new_jss_buff,
                        &new_jss_buff_len);

        XP_FREE(new_css_buff);

        if (!new_jss_buff) {
	    XP_FREE(new_id);
	    return FALSE;
	}

        script_buff = new_jss_buff;
        script_buff_len = new_jss_buff_len;
    }
    else {
        /* unknown script type, punt */
        XP_FREE(script_buff);
	XP_FREE(new_id);
        return FALSE;
    }

    /* not needed anymore */
    XP_FREE(new_id);

    stuff = (ETEvalStuff *) XP_NEW_ZAP(ETEvalStuff);
    if (!stuff) {
	/* what do we free here? */
	XP_FREE(new_id);
        return FALSE;
    }

    if (lo_CloneTagAndBlockLayout(context, state, tag)) {
        tag->type = P_UNKNOWN;
	stuff->len = XP_STRLEN(script_buff);
	stuff->line_no = top_state->script_lineno;
	stuff->scope_to = scope_to;
	stuff->want_result = JS_FALSE;
	stuff->data = context;
	stuff->version = top_state->version;
	stuff->principals = NULL;

        ET_EvaluateScript(context, script_buff, stuff, lo_StyleEvalExitFn);

	/* don't free scope_to, ET_EvaluateScript has taken possession */
        XP_FREE(script_buff);

        return TRUE;
    }

    XP_FREEIF(scope_to);
    XP_FREE(script_buff);
    
    return FALSE;
}

#ifdef DEBUG_ScriptPlugin
#include "np.h"

PA_Tag *
LO_CreateHiddenEmbedTag(PA_Tag *old_tag, char * newTagStr)
{
	PA_Tag *new_tag = PA_CloneMDLTag(old_tag);
	if(new_tag)
	{
		char * buf = NULL;
		new_tag->type = P_EMBED;
		XP_FREE(new_tag->data);
		new_tag->data = NULL;
		StrAllocCopy(buf,newTagStr);
		XP_ASSERT(buf != NULL);
		new_tag->data = (PA_Block)buf;
		new_tag->data_len = XP_STRLEN((char*)new_tag->data);
		XP_ASSERT(new_tag->data_len != 0);
	}

	return(new_tag);
}

/* Generate mimetype string corresponding to this script language.  
   Specifically, <SCRIPT language = blah> should yield a mimetype
   of "script/blah"
 */
char * npl_Script2mimeType(MWContext *context, PA_Tag *tag)
{
	PA_Block buff;
	char * language;
	char * preface = "script/";
    uint32 prefLen = XP_STRLEN(preface);
	char * mimebuf = NULL;
	uint32 len;

	buff = lo_FetchParamValue(context, tag, PARAM_LANGUAGE);
	if (buff != NULL) {
		PA_LOCK(language, char *, buff);
		len = prefLen + XP_STRLEN(language);
		mimebuf = XP_ALLOC(len+1);
		if (mimebuf)
		{
			XP_STRCPY(mimebuf, preface);
			XP_STRCAT(mimebuf, language);
		}
		PA_FREE(buff);
	}
	return mimebuf;
}
	
/* A plugin wants to handle this (otherwise unhandled) script language.
   Now we redirect it to the plugin using the following steps:

	1.  Create a cachefile w/ the appropriate extension and store the contents of the script delims
	2.  Build a hidden, semi-internal embed tag pointing to the cachefile. 
	3.  Cause Layout to initiate the hidden embed tag, which further starts netlib and other plugin
	    initialization.

 */
#define ALLOC_CHECK(a,id,abnormal_exit)   {id++; if (!a) goto abnormal_exit;}
void npl_ScriptPlugin(MWContext *context, lo_DocState *state, PA_Tag *tag, size_t line_buf_len, char * mimebuf)
{
	char * suffix				= NULL;
    char * fname				= NULL;
	char * embedtagstr			= NULL;
	char * NET_suffix			= NULL;
	PA_Tag * newTag				= NULL;
	NET_StreamClass* viewstream = NULL;
    URL_Struct* urls			= NULL;
	int streamStatus			= 0;
	int id						= 0;

	if (!context || !state || !tag || (line_buf_len <= 0) || !mimebuf){
		XP_Trace("npl_ScriptPlugin:  Invalid input(s).");
		return; /* no need to goto clean_exit, since havn't alloc'd anything */
	}

	urls = NET_CreateURLStruct("internal_url.", NET_DONT_RELOAD);
			ALLOC_CHECK(urls,id,abnormal_exit);

	/* NETLib gives me a pointer here instead of a copy of the string, so I have to make
	   my own, make checks, etc. 
	 */
	NET_suffix = NET_cinfo_find_ext(mimebuf);
	if (!NET_suffix)
		goto abnormal_exit;

	suffix = PR_smprintf("internal_url.%s", NET_suffix);
			ALLOC_CHECK(suffix,id,abnormal_exit);

			XP_FREEIF(urls->address);
	StrAllocCopy(urls->address     , suffix);
			ALLOC_CHECK(urls->address,id,abnormal_exit);

			XP_FREEIF(urls->content_name);
	StrAllocCopy(urls->content_name, suffix);
			ALLOC_CHECK(urls->content_name,id,abnormal_exit);

			XP_FREEIF(urls->content_type);
	StrAllocCopy(urls->content_type, mimebuf);
			ALLOC_CHECK(urls->content_type,id,abnormal_exit);

	urls->must_cache = 1;  	/* ugly flag for mailto and saveas */

	viewstream = NET_StreamBuilder(FO_CACHE_ONLY, urls, context);
			ALLOC_CHECK(viewstream,id,abnormal_exit);

	streamStatus =
		(*viewstream->put_block)(viewstream,(CONST char*)state->line_buf, line_buf_len);
	if (streamStatus <= 0)
		goto abnormal_exit;
	(*viewstream->complete)(viewstream);

	/* Now build internal embed tag pointing to this source:  */
    fname = WH_FileName(urls->cache_file, xpCache);
			ALLOC_CHECK(fname,id,abnormal_exit);
	embedtagstr = PR_smprintf("<EMBED SRC=file:///%s HIDDEN=TRUE>", fname);
			ALLOC_CHECK(embedtagstr,id,abnormal_exit);

	newTag = LO_CreateHiddenEmbedTag(tag,embedtagstr);
			ALLOC_CHECK(newTag,id,abnormal_exit);

	lo_FormatEmbed(context,state,newTag);	 
	goto normal_exit;

abnormal_exit:
	XP_Trace("npl_ScriptPlugin:  Abnormal termination, id = %d",id);
	if (urls) {
		XP_FREEIF(urls->content_type);
		XP_FREEIF(urls->content_name);
		XP_FREEIF(urls->address);
		XP_FREE(  urls);
	}

normal_exit:
	XP_FREEIF(newTag);
	XP_FREEIF(embedtagstr);
	XP_FREEIF(fname);
	XP_FREEIF(viewstream);
	XP_FREEIF(suffix);
	return;

}
#endif /* DEBUG_ScriptPlugin */

void
lo_ProcessScriptTag(MWContext *context, lo_DocState *state, PA_Tag *tag, JSObject *obj)
{
    lo_TopState *top_state;
    pa_DocData *doc_data;
    XP_Bool type_explicitly_set=FALSE;
    XP_Bool saw_archive=FALSE;
#ifdef DEBUG_ScriptPlugin
	char * mimebuf = NULL;
#endif

    top_state = state->top_state;
    doc_data = (pa_DocData *)top_state->doc_data;
    XP_ASSERT(doc_data != NULL || state->in_relayout || tag->lo_data);

    if (tag->is_end == FALSE) {
        PA_Block buff;
        char *str, *url, *archiveSrc, *id, *codebase;

        /* Controversial default language value. */
        top_state->version = JSVERSION_DEFAULT;
        if (tag->type == P_STYLE || tag->type == P_LINK) {
            top_state->in_script = top_state->default_style_script_type;
        }
        else {
            /* in order to get old script behaviour, pretend
             * that the content-type is explicitly set for all scripts
             */
            type_explicitly_set = TRUE;
            top_state->in_script = SCRIPT_TYPE_MOCHA;
        }

        /* XXX account for HTML comment bytes and  "lost" newlines */
        if (lo_IsAnyCurrentAncestorSourced(state))
            top_state->script_bytes = top_state->layout_bytes;
        else
            top_state->script_bytes = top_state->layout_bytes - tag->true_len;
        if (tag->lo_data != NULL) {
            top_state->script_bytes += (int32)tag->lo_data - 1;
            tag->lo_data = NULL;
        } 
        else if (doc_data != NULL) {
            top_state->script_bytes += doc_data->comment_bytes;
        } 
        else {
            XP_ASSERT(state->in_relayout);
        }

	lo_ParseScriptLanguage(context, tag, &top_state->in_script,
			       &top_state->version);
#ifdef DEBUG_ScriptPlugin
		if (top_state->in_script == SCRIPT_TYPE_UNKNOWN)
		{
			char* pluginName;
			mimebuf = npl_Script2mimeType(context,tag);
			if (mimebuf){
				if ((pluginName = NPL_FindPluginEnabledForType(mimebuf)) != NULL){
					top_state->in_script = SCRIPT_TYPE_PLUGIN;
					XP_ASSERT(top_state->mimetype == NULL);
					StrAllocCopy((char *)top_state->mimetype,mimebuf);
					XP_FREE(mimebuf);
					mimebuf = NULL;
				}
				else{
					XP_FREE(mimebuf);
					mimebuf = NULL;
				}
			}
		}
#endif /* DEBUG_ScriptPlugin */

        buff = lo_FetchParamValue(context, tag, PARAM_TYPE);
        if (buff != NULL) {
            PA_LOCK(str, char *, buff);
            if ((XP_STRCASECMP(str, js_content_type) == 0) ||
                (!XP_STRCASECMP(str, "text/javascript"))) {
		if(tag->type == P_STYLE || tag->type == P_LINK)
		{
		    top_state->in_script = SCRIPT_TYPE_JSSS;
		    top_state->default_style_script_type = SCRIPT_TYPE_JSSS;
		}
		else
		{
		    top_state->in_script = SCRIPT_TYPE_MOCHA;                    
		}
                type_explicitly_set = TRUE;
            } 
            else if ((XP_STRCASECMP(str, TEXT_CSS) == 0)) {
                top_state->in_script = SCRIPT_TYPE_CSS;
                top_state->default_style_script_type = SCRIPT_TYPE_CSS;
                type_explicitly_set = TRUE;
            } 
            else {
                top_state->in_script = SCRIPT_TYPE_UNKNOWN;
                top_state->default_style_script_type = SCRIPT_TYPE_UNKNOWN;
            }
            PA_UNLOCK(buff);
            PA_FREE(buff);
        } 

	/* check for media=screen
	 * don't load the style sheet if there 
	 * is a media not equal to screen
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_MEDIA);
	if (buff) {
	    if (strcasecomp((char*)buff, "screen")) {
		/* set the script type to UNKNOWN
		 * so that it will get thrown away
		 */
		top_state->in_script = SCRIPT_TYPE_UNKNOWN;
	    }
	    PA_FREE(buff);
	}

        /*
         * Flush the line buffer so we can start storing Mocha script
         * source lines in there.
         */
        lo_FlushLineBuffer(context, state);

        url = archiveSrc = id = codebase = NULL;
        if (top_state->in_script != SCRIPT_TYPE_NOT) {
            /*
             * Check for the archive parameter for known languages.
             */
            buff = lo_FetchParamValue(context, tag, PARAM_ARCHIVE);
            if (buff != NULL) {
                saw_archive = TRUE;
                PA_LOCK(str, char *, buff);
                url = NET_MakeAbsoluteURL(top_state->base_url, str);
                PA_UNLOCK(buff);
                PA_FREE(buff);
		if (url == NULL) {
                    top_state->out_of_memory = TRUE;
                    return;
		}
            }

            /* 
             * Look for ID attribute. If it's there we have may have
             * an inline signed script.
             */
            buff = lo_FetchParamValue(context, tag, PARAM_ID);
            if (buff != NULL) {
                PA_LOCK(str, char *, buff);
                StrAllocCopy(id, str);
                PA_UNLOCK(buff);
                PA_FREE(buff);
		if (id == NULL) {
                    top_state->out_of_memory = TRUE;
		    XP_FREEIF(url);
                    return;
		}
            }

            /*
             * Now look for a SRC="url" attribute for known languages.
             * If found, synchronously load the url.
             */
	    buff = lo_FetchParamValue(context, tag, PARAM_SRC);  /* XXX overloaded rv */
            if (buff != NULL) {
		XP_Bool allowFileSrc = FALSE;
                char *absUrl;

                PA_LOCK(str, char *, buff);

		PREF_GetBoolPref(lo_jsAllowFileSrcFromNonFile, &allowFileSrc);
                absUrl = NET_MakeAbsoluteURL(top_state->base_url, str);
		if (absUrl == NULL) {
		    top_state->out_of_memory = TRUE;
		    XP_FREEIF(id);
                } else if (allowFileSrc == FALSE &&
		           NET_URL_Type(absUrl) == FILE_TYPE_URL &&
		           NET_URL_Type(top_state->url) != FILE_TYPE_URL) {
		    /*
		     * Deny access from http: to file: via SCRIPT SRC=...
		     * XXX silently
		     */
                    top_state->in_script = SCRIPT_TYPE_UNKNOWN;
                    XP_FREE(absUrl);
                    XP_FREEIF(url);
		    XP_FREEIF(id);
		} else if (url != NULL) {
                    XP_FREE(absUrl);
                    StrAllocCopy(archiveSrc, str);
		    if (archiveSrc == NULL) {
			top_state->out_of_memory = TRUE;
			XP_FREE(url);
			XP_FREEIF(id);
		    }
                } else {
                    url = absUrl;
                }
                PA_UNLOCK(buff);
                PA_FREE(buff);
		if (top_state->out_of_memory)
		    return;

                /*
                 * If we are doing a <script src=""> mocha script but JS
                 *   is turned off just ignore the tag
                 */
		if (!LM_CanDoJS(context)) {
                    top_state->in_script = SCRIPT_TYPE_UNKNOWN;
                    XP_FREE(url);
		    XP_FREEIF(id);
		    XP_FREEIF(archiveSrc);
                    return;
                }
            }
        }

        /*
         * Set text_divert so we know to accumulate text in line_buf
         * without interpretation.
         */
        state->text_divert = tag->type;

        /*
         * XXX need to stack these to handle blocked SCRIPT tags
         */
        top_state->script_lineno = tag->newline_count + 1;

        /* if we got here as a result of a LINK tag
         * check to make sure rel=stylesheet and then
         * check for an HREF and if one does not exist
         * fail
         */
        if (tag->type == P_LINK) {
            char *cbuff = (char*)lo_FetchParamValue(context, tag, PARAM_REL);
                        
            if (cbuff && !strcasecomp(cbuff, "stylesheet")) {
                XP_FREE(cbuff);

                cbuff = (char*)lo_FetchParamValue(context, tag, PARAM_HREF);

                if (cbuff) {
                    if (saw_archive && url) {
                        archiveSrc = XP_STRDUP(cbuff);
                    } else {
		        XP_FREEIF(url);
                        url = NET_MakeAbsoluteURL(top_state->base_url, cbuff);
                    }
		}
            }

            XP_FREEIF(cbuff);
        }

        if (url != NULL || id != NULL || codebase != NULL) {
            if ((doc_data != NULL) &&
                (state->in_relayout == FALSE) &&
                SCRIPT_EXEC_OK(top_state, state, tag->type, P_SCRIPT)) {
                ScriptData *data;

                data = XP_ALLOC(sizeof(ScriptData));
                if (data == NULL) {
                    top_state->out_of_memory = TRUE;
                    return;
                }
                data->context = context;
                data->state = state;
                data->tag = PA_CloneMDLTag(tag);
                if (data->tag == NULL) {
                    top_state->out_of_memory = TRUE;
		    XP_FREE(data);
                    return;
                }
                data->url = url;
                data->archiveSrc = archiveSrc;
                data->id = id;
                if (codebase == NULL) {
                    StrAllocCopy(codebase, top_state->base_url);
                }
                data->codebase = codebase;
                data->buffer = NULL;
                data->bufferSize = 0;
                data->version = top_state->version;

		/*
		 * Only SCRIPT ARCHIVE= ID= without SRC= is an inline signed
		 * script -- if there is a SRC= attribute, archiveSrc will be
		 * non-null.
		 */
                data->inlineSigned = (JSBool)
		    (url != NULL && archiveSrc == NULL && id != NULL);

                /* Reset version accumulator */
                top_state->version = JSVERSION_UNKNOWN;

                XP_ASSERT (tag->type == P_SCRIPT || tag->type == P_STYLE || 
			   tag->type == P_LINK);

	        /* 
		 * Out-of-line included (by src=) or inline signed script.
		 * Save continuatation data on top_state.  If it's signed,
		 * we'll verify the signature once we see </script> and
		 * have the inline script to verify.
		 */
		top_state->scriptData = data;

            } 
	    else {
                XP_FREE(url);
		XP_FREEIF(id);
		XP_FREEIF(archiveSrc);
            }
        }
    } 
    else {

	/*
	 * We are in the </script> tag now...
	 */

        size_t line_buf_len;
        intn script_type;
        char *scope_to=NULL;
        char *untransformed = NULL;

        script_type = top_state->in_script;
        top_state->in_script = SCRIPT_TYPE_NOT;

	/* guard against superfluous end tags */
	if (script_type == SCRIPT_TYPE_NOT)
	    goto end_tag_out;

	/* convert from CSS to JavaScript here */
        if (tag->type != P_LINK && script_type == SCRIPT_TYPE_CSS) {
            char *new_buffer;
            int32 new_buffer_length;

            CSS_ConvertToJS((char *)state->line_buf, 
                            state->line_buf_len,
                            &new_buffer,
                            &new_buffer_length);

            if (!new_buffer) {
                /* css translator error, unblock layout and return */
                state->text_divert = P_UNKNOWN;
                state->line_buf_len = 0; /* clear script text */
                goto end_tag_out;
            }

            untransformed = (char *) state->line_buf;
            state->line_buf = (PA_Block) new_buffer;
            state->line_buf_len = new_buffer_length;
            state->line_buf_size = new_buffer_length;

            if (state->line_buf_len)
                state->line_buf_len--; /* hack: subtract one to remove final \n */

            script_type = SCRIPT_TYPE_JSSS;
        }

        if (tag->type == P_STYLE) {
            /* mocha scoped to document == jsss */
            scope_to = "document";
        }

        /*
         * Reset these before potentially recursing indirectly through
         * the document.write() built-in function, which writes to the
         * very same doc_data->parser_stream that this <SCRIPT> tag
         * came in on.
         */
        state->text_divert = P_UNKNOWN;
        line_buf_len = state->line_buf_len;
        state->line_buf_len = 0;

        if (script_type != SCRIPT_TYPE_UNKNOWN && 
	    script_type != SCRIPT_TYPE_NOT) {
            /*
             * If mocha is disabled or can't be done in this context we
             *   are going to just ignore the buffer contents
             */
            if (!LM_CanDoJS(context)) {
				top_state->in_script = SCRIPT_TYPE_UNKNOWN;
                goto end_tag_out;
            }

            if ((doc_data != NULL) &&
                (state->in_relayout == FALSE) &&
                SCRIPT_EXEC_OK(top_state, state, tag->type, P_SCRIPT)) {

                /*
                 * First off, make sure layout is blocking on us
                 */
                if (lo_create_script_blockage(context, state, 
					tag->type == P_SCRIPT ? LO_SCRIPT : LO_UNKNOWN)) 
				{
                    ScriptData *data;

                    /*
                     * Extreme hackery.  Hideous and shameful.  See the comment
						* in lo_BlockScriptTag before similar is_end/overflow code
						* and commence vomiting.
                     */
                    lo_BlockScriptTag(context, state, NULL);

		    if (tag->is_end == (PRPackedBool)1) {
			  PA_PushOverflow(doc_data);
			  doc_data->overflow_depth ++;
		    }

		    /*
		     * Set the document.write tag insertion point.
		     */
             top_state->input_write_point[top_state->input_write_level] = &top_state->tags;

		    data = top_state->scriptData;
                    top_state->scriptData = NULL;
                    if (data && data->url) {
			/*
			 * Three cases:
			 * 1.  SCRIPT SRC=: url non-null
			 * 2.  SCRIPT ARCHIVE= SRC=: url, archiveSrc non-null
			 * 3.  SCRIPT ARCHIVE= ID=: url, id non-null
			 * In the last case, we copy the inline script into
			 * data's buffer and let lo_script_archive_exit_fn do
			 * the eval.  We use an inlineSigned flag to avoid a
			 * bunch of (url != NULL && archiveSrc == NULL && id
			 * != NULL) tests.
			 */
			if (data->inlineSigned) {
                            StrAllocCopy(data->buffer, (char *) state->line_buf);
                            data->bufferSize = line_buf_len;
			}
			lo_GetScriptFromURL(data, script_type);
                    }
		    else {
                        JSPrincipals *principals = NULL;
			ETEvalStuff * stuff;
			
                        if (data) {
			    principals = LM_NewJSPrincipals(NULL, data->id, 
							    data->codebase);
                            if (untransformed &&
				!LM_SetUntransformedSource(principals, 
                                                           untransformed,
                                                           (char *) state->line_buf))
                            {
                                top_state->out_of_memory = TRUE;
                            }
                            lo_DestroyScriptData(data);
			}

                        /* 
                         * send the buffer off to be evaluated 
			 */
#ifdef DEBUG_ScriptPlugin
			 			if (script_type == SCRIPT_TYPE_PLUGIN)
						{
							XP_ASSERT(mimebuf == NULL);
							npl_ScriptPlugin(context, state, tag, line_buf_len,top_state->mimetype);
						    lo_unblock_script_tag(context, TRUE);
						}
						else
#endif /* DEBUG_ScriptPlugin */
 

			stuff = (ETEvalStuff *) XP_NEW_ZAP(ETEvalStuff);
			if (!stuff)
			    goto end_tag_out;

			stuff->len = line_buf_len;
			stuff->line_no = top_state->script_lineno;
			if (scope_to)
			    stuff->scope_to = XP_STRDUP(scope_to);
			else
			    stuff->scope_to = NULL;
			stuff->want_result = JS_FALSE;
			stuff->data = context;
			stuff->version = top_state->version;
			stuff->principals = principals;

                        ET_EvaluateScript(context, 
                                          (char *) state->line_buf,
					  stuff,
                                          lo_ScriptEvalExitFn);
                    }

                    /* Reset version accumulator */
                    top_state->version = JSVERSION_UNKNOWN;
                }
            }
        }
      end_tag_out:
	/*
	 * If we got a </SCRIPT> and still have scriptData set here, it must
	 *   be left over from an error case above, so we free it.
	 */
	if (top_state->scriptData) {
	    XP_ASSERT(!top_state->layout_blocking_element);
            lo_DestroyScriptData(top_state->scriptData);
	    top_state->scriptData = NULL;
	}
        XP_FREEIF(untransformed);
    }
}

static char script_reblock_tag[]   = "<" PT_NSCP_REBLOCK ">";

/*
 * Create a tag that will reblock us
 */
void
LO_CreateReblockTag(MWContext * context, LO_Element * current_script)
{
    lo_TopState *top_state;
    pa_DocData *doc_data;
    PA_Tag * tag/*, ** tag_ptr*/;

    top_state = lo_FetchTopState(XP_DOCID(context));
    doc_data = (pa_DocData *)top_state->doc_data;

    tag = pa_CreateMDLTag(doc_data,
                          script_reblock_tag,
                          sizeof script_reblock_tag - 1);

    if (tag == NULL)
        return;

    tag->lo_data = current_script;
    
    /*
     * Kludge in the write level for sanity check in lo_LayoutTag.
     */
    tag->newline_count = (uint16)top_state->input_write_level;

    /*
     * Add the reblock tag to the tags list but don't advance the write_point
     *   in case we just wrote a nested script that also writes: the inner
     *   script must insert before the outer reblock tag, but after all the
     *   earlier tags.
     */
/*    tag_ptr = top_state->input_write_point; */
    lo_BlockTag(context, NULL, tag);
/*    top_state->input_write_point = tag_ptr; */
}
