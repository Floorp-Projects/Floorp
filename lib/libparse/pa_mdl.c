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

#include "pa_parse.h"
#include <stdio.h>
#include "pa_tags.h"
#include "libmocha.h" /* For JavaScript conditional HTML comments */

#ifdef PROFILE
#pragma profile on
#endif


#define NAME_LIST_INC	5	/* step to increase tag name/value arrays by */


static char *pa_token_to_name[] = {
#include "pa_hash.rmap"
};

/*************
 * Used by layout/laytags.c if MOCHA_CACHES_WRITES is defined.
 **************/
const char *
pa_PrintTagToken(int32 token)
{
	if ((uint32)token >= P_MAX)
	{
		return "UNKNOWN";
	}

	return pa_token_to_name[token];
}

/*************************************
 * Function: pa_CloneMDLTag
 *
 * Description: This function allocates and fills in an entire
 * 		PA_Tag structure for any MDL tag element type.
 *
 * Params: The tag to clone
 *
 * Returns: A pointer to a completely filled in PA_Tag structure
 *      that is a clone of the tag passed in.  On failure it 
 *      returns NULL.
 *************************************/
PA_Tag *
PA_CloneMDLTag(PA_Tag * src)
{
	PA_Tag *tag;
	PA_Block buff;
	char *locked_buff, *src_buff;

	/*
	 * Allocate a new tag structure, return NULL
	 * if you can't.
	 */
	tag = XP_NEW(PA_Tag);
	if (tag == NULL)
	{
		return(NULL);
	}
	tag->type = src->type;
	tag->is_end = src->is_end;
	tag->newline_count = src->newline_count;
	tag->data_len = src->data_len;
	tag->true_len = src->true_len;
	tag->lo_data = NULL;
	tag->next = NULL;

	/* sure wish we could just do a strdup() here */
	buff = PA_ALLOC((tag->data_len + 1) * sizeof(char));
	if (buff != NULL)
	{
		PA_LOCK(locked_buff, char *, buff);
		PA_LOCK(src_buff, char *, src->data);
		XP_BCOPY(src_buff, locked_buff, tag->data_len);
		locked_buff[tag->data_len] = '\0';
		PA_UNLOCK(locked_buff);
		PA_UNLOCK(src_buff);
	}
	else
	{
		XP_DELETE(tag);
		return(NULL);
	}
	
	tag->data = buff;

	return(tag);
}

/*************************************
 * Function: pa_CreateMDLTag
 *
 * Description: This function allocates and fills in an entire
 * 		PA_Tag structure for any MDL tag element type.
 *
 * Params: Takes a buffer containing a complete tag element string,
 *	   and a length for that buffer.  The buffer is NOT a \0
 *	   terminated string.
 *
 * Returns: A pointer to a completely filled in PA_Tag structure
 *	    created based on the data contained in the passed
 *	    tag element string. On failure it returns NULL.
 *************************************/
PA_Tag *
pa_CreateMDLTag(pa_DocData *doc_data, char *buf, int32 len)
{
	PA_Tag *tag;
	char *start;
	char *tptr;
	char tchar;
	int32 cnt;
	PA_Block buff;
	char *locked_buff;
	int32 blen;

	/*
	 * Allocate a new tag structure, return NULL
	 * if you can't.
	 */
	tag = XP_NEW(PA_Tag);
	if (tag == NULL)
	{
		return(NULL);
	}
	tag->lo_data = NULL;
	tag->newline_count = doc_data->newline_count;

	/*
	 * Find the start of the tag element text
	 */
	tptr = buf;
	tptr++;		/* skip the '<' */
	cnt = 1;
	/*
	 * Check if this is an end tag
	 */
	if (*tptr == '/')
	{
		tag->is_end = TRUE;
		tptr++;
		cnt++;
	}
	else
	{
		tag->is_end = FALSE;
	}
	start = tptr;

	/*
	 * Find the end of the tag element text
	 */
	while ((*tptr != '>')&&(!XP_IS_SPACE(*tptr)))
	{
		tptr++;
		cnt++;
		/*
		 * Reach the end of the tag but find no end
		 */
		if (cnt == len)
		{
			XP_DELETE(tag);
			return(NULL);
		}
	}

	/*
	 * Temporarily null ternimate the string to do
	 * the string compares.
	 */
	tchar = *tptr;
	*tptr = '\0';
	tag->type = pa_tokenize_tag(start);
	*tptr = tchar;

	/*
	 * For UNKNOWN tags, stick the tag name in the
	 * paramater list as a boolean.
	 * This is in case a later module wants to try and interpret it.
	 */
	if (tag->type == P_UNKNOWN)
	{
		tptr = start;
	}

	/*
	 * Allocate a buffer for the parameter list and
	 * put it in the tag structure.
	 */
	blen = (len - (int)(tptr - buf));
	buff = PA_ALLOC((blen+1) * sizeof (char)); /* LAM no need to use handles here */
	/* BAD_OOM_HANDLING should return NULL tag*/
	if (buff != NULL)
	{
		PA_LOCK(locked_buff, char *, buff);
		XP_BCOPY(tptr, locked_buff, blen);
		locked_buff[blen] = '\0';
		PA_UNLOCK(buff);
	}
	else
	{
		blen = 0;
	}
	tag->data = buff;
	tag->data_len = blen;
	tag->true_len = len;

	tag->next = NULL;
	tag->edit_element = NULL;

	return(tag);
}


/*************************************
 * Function: pa_CreateTextTag
 *
 * Description: This function allocates and fills in an entire
 * 		PA_Tag structure for a P_TEXT tag element.
 *		P_TEXT is a special tag, where the text passed
 *		is simply shoved into the first value field
 *		of the values array.
 *
 * Params: Takes a buffer containing a text string,
 *	   and a length for that buffer.  The buffer is NOT a \0
 *	   terminated string.
 *
 * Returns: A pointer to a completely filled in PA_Tag structure
 *	    of type P_TEXT containing the text passed in.
 *	    On failure it returns NULL.
 *************************************/
PA_Tag *
pa_CreateTextTag(pa_DocData *doc_data, char *buf, int32 len)
{
	PA_Tag *tag;
	PA_Block buff;
	char *locked_buff;

	/*
	 * Allocate a new tag structure, return NULL
	 * if you can't.
	 */
	tag = XP_NEW(PA_Tag);
	if (tag == NULL)
	{
		return(NULL);
	}
	tag->lo_data = NULL;

	tag->type = P_TEXT;
	tag->is_end = FALSE;
	tag->newline_count = doc_data->newline_count;

	buff = PA_ALLOC((len + 1) * sizeof(char));
	if (buff != NULL)
	{
		PA_LOCK(locked_buff, char *, buff);
		XP_BCOPY(buf, locked_buff, len);
		locked_buff[len] = '\0';
		PA_UNLOCK(buff);
	}
	else
	{
		XP_DELETE(tag);
		return(NULL);
	}

	tag->data = buff;
	tag->data_len = len;
	tag->true_len = len;

	tag->next = NULL;
	tag->edit_element = NULL;

	return(tag);
}

static PRBool
pa_eval_javascript_expression(pa_DocData *doc_data,
                              char *expression,
                              PRBool *return_value)
{
    char *eval_str;
    
    /* If JS is not enabled, assume expression evaluates to true */
    if (LM_GetMochaEnabled() == FALSE) {
        *return_value = PR_TRUE;
        return PR_TRUE;
    }

    if (!LM_AttemptLockJS(NULL, NULL)) {
        doc_data->waiting_for_js_thread = PR_TRUE;
        return PR_FALSE;
    }
    doc_data->waiting_for_js_thread = PR_FALSE;

    eval_str = LM_EvaluateAttribute(doc_data->window_id, expression,
                                    doc_data->newline_count + 1);
    *return_value = eval_str && !XP_STRCMP(eval_str, "true");
    if (eval_str)
        XP_FREE(eval_str);
	LM_UnlockJS();
    return PR_TRUE;
}

static PRBool
pa_could_be_javascript_entity(char *entity, int len)
{
    if (len == 0)
        return PR_TRUE;
    if (len == 1)
        return (entity[0] == '&');
    return (entity[1] == '{');
}

static PRBool
pa_is_javascript_entity(char *entity, int len)
{
    return ((len >=2) && pa_could_be_javascript_entity(entity, len));
}

static char *
pa_isolate_javascript_expression(char *entity, int len, char **terminatorp)
{
    char *tptr;
    
    /* Skip over &{ */
    tptr = entity + 2;
    len -= 2;
    
    while (len >= 2) 
    {
        if ((*tptr == '}') && (*(tptr + 1) == ';')) {
            *tptr = '\0';
            *terminatorp = tptr;
            return entity + 2;
        }
        tptr++;
        len--;
    }
    return NULL;
}

/* This function is used only to evaluate JavaScript entities used to form
   conditional comments.  Other JS entities are handled within layout.
   Returns PR_TRUE if the JS entity could be evaluated, or PR_FALSE
   if it could not be because the JS thread was busy.  In the latter case,
   we'll have to try again later. */
static PRBool
pa_try_eval_javascript_entity(pa_DocData *doc_data, char *entity, int len, PRBool *retval)
{
    char *expr, *terminator_char;
    PRBool success;
    
    if (!pa_is_javascript_entity(entity, len))
        return PR_FALSE;

    expr = pa_isolate_javascript_expression(entity, len, &terminator_char);
    if (!expr)
        return PR_FALSE;

    success = pa_eval_javascript_expression(doc_data, expr, retval);

    /* Eliminate null-termination performed by pa_eval_javascript_expression()*/
    if (!success)
        *terminator_char = '}';
    return success;
}

/*************************************
 * Function: pa_FindMDLTag
 *
 * Description: This function finds the start of any MDL tags
 * 		in the passed buffer.  It also finds MDL comments
 *		and flags partial MDL tags and partial comments.
 *
 * Params: Takes a buffer and a length for that buffer.  The buffer is NOT
 *	   a \0 terminated string.  It also takes a pointer to an
 *	   int in which to store special return codes.
 *
 * Returns: A character pointer.  This is NULL if the entire string has
 *	    no MDL tags.  It is a pointer to the beginning of any such
 *	    tags or possible partial tags if any are found.
 *************************************/
char *
pa_FindMDLTag(pa_DocData *doc_data, char *buf, int32 len, intn *is_comment)
{
	char *tptr;

	*is_comment = COMMENT_NO;

	/*
	 * A NULL buffer obviously has no MDL tags.
	 */
	if (buf == NULL)
	{
		return(NULL);
	}

	for (tptr = buf; --len >= 0; tptr++)
	{
		if (*tptr == '\n' || (*tptr == '\r' && len && *(tptr+1) != '\n') )
		{
			if (doc_data->no_newline_count == 0)
				doc_data->newline_count++;
			continue;
		}
		/*
		 * A '<' might be the start of a MDL tag or comment.
		 */
		if (*tptr == '<')
		{
			/*
			 * If this is the last character in the string
			 * we don't know if it is an anchor yet or not.
			 * we want to save it until the next buffer to make
			 * sure.
			 */
			if (len == 0)
			{
				*is_comment = COMMENT_MAYBE;
				return(tptr);
			}
			/*
			 * else if the next character is a letter or
			 * a '/' , this should be some kind of tag.
			 */
			else if ((XP_IS_ALPHA(*(tptr + 1)))||
				(*(tptr + 1) == '/'))
			{
				return(tptr);
			}
			/*
			 * else is the next character is an exclamation point,
			 * this might be a comment.
			 */
			else if (*(tptr + 1) == '!')
			{
				char *ptr;

				ptr = (char *)(tptr + 1);

				/*
				 * If there are 2 more chars in this buffer
				 * we can know for sure if this is a comment,
				 * otherwise we have to wait for the next
				 * buffer.
				 */
				if (len > 2)
				{
					if ((*(ptr + 1) == '-')&&
					    (*(ptr + 2) == '-'))
					{
                        /* Check for a javascript entity immediately after the opening
                           of the comment.  This indicates the presence of a conditional
                           comment. */
                        if (pa_could_be_javascript_entity(ptr + 3, len - 3) &&
                            (doc_data->brute_tag == P_UNKNOWN))
                        {
                            PRBool js_result;
                            if (pa_try_eval_javascript_entity(doc_data, ptr + 3,
                                                              len - 3, &js_result))
                            {
                                if (js_result)
                                    *is_comment = COMMENT_UNCOMMENT;
                                else
                                    *is_comment = COMMENT_YES;
                            } else {
                                /* Unable to evaluate.  Try again later */
                                *is_comment = COMMENT_MAYBE;
                            }
                        }
                        else
                        {
                            *is_comment = COMMENT_YES;
                        }
						return(tptr);
					}
					/*
					 * Else we may have an HTML++ tag
					 * which we will become an UNKNOWN
					 * tag type.
					 */
/*
 * So many people do broken stuff, anything
 * That starts <! and is not a comment I will assume
 * is a tag
					else if ((XP_IS_ALPHA(*(ptr + 1)))||
					    (*(ptr + 1) == '/'))
 */
					{
						return(tptr);
					}
				}
				else
				{
					*is_comment = COMMENT_MAYBE;
					return(tptr);
				}
			}
			/*
			 * else if the next character is a question mark
			 * this might be a 'processing instruction'.
			 * We treat processing instructions just like
			 * comments and ignore their content.
			 */
			else if (*(tptr + 1) == '?')
			{
				*is_comment = COMMENT_PROCESS;
				return(tptr);
			}
		}
	}

	return(NULL);
}


/*************************************
 * Function: pa_FindMDLEndTag
 *
 * Description: This function finds the end of a MDL tag in the
 * 		passed buffer.
 *
 * Params: Takes a buffer and a length for that buffer.  The buffer is NOT
 *	   a \0 terminated string.
 *
 * Returns: A character pointer.  This is NULL if the string has
 *	    no end to the MDL tag.  It is a pointer to the end of
 *	    the tag if any is found.
 *************************************/
char *
pa_FindMDLEndTag(pa_DocData *doc_data, char *buf, int32 len)
{
	char *tptr;
	uint8 in_quote; /* 	0 = none, 
						1 = single quote, 
						2 = double quote,
						3 = back quote  */
	uint newlines;

	/*
	 * There can be no end to a MDL tag in a NULL buffer.
	 */
	if (buf == NULL)
	{
		return(NULL);
	}

#ifdef LENIENT_END_TAG
	/*
	 * MDL tags are always ended with a '>', so it is REALLY
	 * easy to find their end.
	 */
	for (tptr = buf; --len >= 0; tptr++)
	{
		if (*tptr == '\n' || (*tptr == '\r' && len && *(tptr+1) != '\n') )
		{
			if (doc_data->no_newline_count == 0)
				doc_data->newline_count++;
			continue;
		}
		if (*tptr == '>')
		{
			return(tptr);
		}
	}
#else
	/*
	 * MDL tags are always ended with a '>', but '>' can also
	 * appear in quoted attribute values, so be careful.
	 */
	in_quote = 0;
	newlines = 0;
	for (tptr = buf; --len >= 0; tptr++)
	{
		if (*tptr == '\n' || (*tptr == '\r' && len && *(tptr+1) != '\n') )
		{
			newlines++;
			continue;
		}
		if (*tptr == '\"')
		{
			/*
			 * If we are not in a double quote already and this
			 * double quote is immediately after some whitespace
			 * or an equal sign, it is the start of a quoted
			 * attribute value, and greater than signs inside
			 * do not close this tag.
			 */
		    if (in_quote == 0)
		    {
			if ((tptr > buf)&&((*(tptr - 1) == '=')||
				(XP_IS_SPACE(*(tptr - 1)))))
			{
				in_quote = 2;
			}
		    }
			/*
			 * else if we are already in a double quote,
			 * this double quote closes it.
			 */
		    else if (in_quote == 2)
		    {
			in_quote = 0;
		    }
		}
		/*
		 * Else the same as above except for single quoted values
		 */
		else if (*tptr == '\'')
		{
			/*
			 * If we are not in a single quote already and this
			 * single quote is immediately after some whitespace
			 * or an equal sign, it is the start of a quoted
			 * attribute value, and greater than signs inside
			 * do not close this tag.
			 */
		    if (in_quote == 0)
		    {
			if ((tptr > buf)&&((*(tptr - 1) == '=')||
				(XP_IS_SPACE(*(tptr - 1)))))
			{
				in_quote = 1;
			}
		    }
			/*
			 * else if we are already in a single quote,
			 * this single quote closes it.
			 */
		    else if (in_quote == 1)
		    {
			in_quote = 0;
		    }
		}
		if (*tptr == '`')
		{
			/*
			 * If we are not in a double quote already and this
			 * double quote is immediately after some whitespace
			 * or an equal sign, it is the start of a quoted
			 * attribute value, and greater than signs inside
			 * do not close this tag.
			 */
		    if (in_quote == 0)
		    {
			if ((tptr > buf)&&((*(tptr - 1) == '=')||
				(XP_IS_SPACE(*(tptr - 1)))))
			{
				in_quote = 3;
			}
		    }
			/*
			 * else if we are already in a double quote,
			 * this double quote closes it.
			 */
		    else if (in_quote == 3)
		    {
			in_quote = 0;
		    }
		}
		else if ((*tptr == '>')&&(in_quote == 0))
		{
			if (doc_data->no_newline_count == 0)
				doc_data->newline_count += newlines;
			return(tptr);
		}
	}
#endif /* LENIENT_END_TAG */
	return(NULL);
}


/*************************************
 * Function: pa_FindMDLEndComment
 *
 * Description: This function finds the end of a MDL comment in the
 * 		passed buffer.
 *
 * Params: Takes a buffer and a length for that buffer.  The buffer is NOT
 *	   a \0 terminated string.
 *
 * Returns: A character pointer.  This is NULL if the string has
 *	    no end to the MDL comment.  It is a pointer to the end of
 *	    the comment if any is found.
 *************************************/
char *
pa_FindMDLEndComment(pa_DocData *doc_data, char *buf, int32 len)
{
	char *tptr;
	int newline_count = 0;

	/*
	 * There can be no end to a MDL comment in a NULL buffer.
	 */
	if (buf == NULL)
	{
		return(NULL);
	}

	/*
	 * Finding the end to a comment is kind of complex since
	 * the comment end sequence is actually 3 characters "-->",
	 * and there can be other '>' chararacters inside the comment.
	 */
	for (tptr = buf; --len >= 0; tptr++)
	{
		if (*tptr == '\n' || (*tptr == '\r' && len && *(tptr+1) != '\n') )
		{
			newline_count++;
			continue;
		}
		if ((*tptr == '>')&&(tptr - buf) >= 2)
		{
			if ((*(tptr - 1) == '-')&&(*(tptr - 2) == '-'))
			{
				if (doc_data->no_newline_count == 0)
					doc_data->newline_count += newline_count;
				return(tptr);
			}
		}
	}
	return(NULL);
}


char *
pa_FindMDLEndProcessInstruction(pa_DocData *doc_data, char *buf, int32 len)
{
	char *tptr;
	int newline_count = 0;

	/*
	 * There can be no end to a processing instruction in a NULL buffer.
	 */
	if (buf == NULL)
	{
		return(NULL);
	}

	/*
	 * Finding the end to a processing instruction is kind of complex
	 * since the end sequence is actually 2 characters "?>",
	 * and there can be other '>' chararacters inside the
	 * processing instruction.
	 */
	for (tptr = buf; --len >= 0; tptr++)
	{
		if (*tptr == '\n' || (*tptr == '\r' && len && *(tptr+1) != '\n') )
		{
			newline_count++;
			continue;
		}
		if ((*tptr == '>')&&(tptr - buf) >= 1)
		{
			if (*(tptr - 1) == '?')
			{
				if (doc_data->no_newline_count == 0)
					doc_data->newline_count += newline_count;
				return(tptr);
			}
		}
	}
	return(NULL);
}


/*
 * A simple test to see if this tag might have attributes.
 * Look and see if there are any non-whitespace characters between
 * the end of the tag and the closing '>' character.
 */
Bool
PA_TagHasParams(PA_Tag *tag)
{
	int32 cnt;
	int32 len;
	char *buf;
	char *tptr;

	/*
	 * Punt on obvious error
	 */
	if ((tag == NULL)||(tag->data == NULL))
	{
		return(FALSE);
	}

	PA_LOCK(buf, char *, tag->data);
	len = tag->data_len;

	/*
	 * To simplify enormously, we require that buf[len - 1] is
	 * the tag element terminating character '>'
	 *
	 * If not, punt here.
	 */
	if (buf[len - 1] != '>')
	{
		PA_UNLOCK(tag->data);
		return(FALSE);
	}

	/*
	 * Skip all whitespace starting at the front of the string.
	 */
	cnt = 0;
	tptr = buf;
	while ((XP_IS_SPACE(*tptr))&&(cnt < len))
	{
		cnt++;
		tptr++;
	}
	PA_UNLOCK(tag->data);

	/*
	 * If there is something we didn't skip it might be an attribute.
	 */
	if (cnt < (len - 1))
	{
		return(TRUE);
	}

	return(FALSE);
}

void
PA_FetchRequestedNameValues(PA_Tag *tag, char *namesToFind[], int32 numNamesToFind, char *values[], uint16 win_csid)
{
	int32 cnt;
	char *buf;
	char *param_ptr;
	char tchar;
	int32 len;


	int32 nameNum;
	Bool nameFound;

#ifndef LENIENT_END_TAG
	char *buf_end;
#endif /* LENIENT_END_TAG */

	nameFound = FALSE;
	/*
	 * Punt on obvious error
	 */
	if ((tag == NULL)||(tag->data == NULL))
		return;

	PA_LOCK(buf, char *, tag->data);
	len = tag->data_len;

	/*
	 * To simplify enormously, we require that buf[len - 1] is
	 * the tag element terminating character '>'
	 *
	 * If not, punt here.
	 */
	if (buf[len - 1] != '>')
	{
		PA_UNLOCK(tag->data);
		return;
	}

#ifndef LENIENT_END_TAG
	buf_end = (char *)(buf + len - 1);
#endif /* LENIENT_END_TAG */
	while (*buf != '>')
	{
		char *tptr;
		int32 plen;
		int32 vlen;

		/*
		 * Skip whitespace before the next possible
		 * parameter name.  If no more parameters, break
		 * out of the while loop.
		 */
		
		/* get char so we don't do so many loads from memory */
		tchar = *buf; 
		while ((XP_IS_SPACE(tchar))&&(tchar != '>'))
		{
			buf++;
			tchar = *buf; 
		}
		if (*buf == '>')
		{
			break;
		}

		/*
		 * Find the end of the parameter name.
		 */
		tptr = buf;
		cnt = 0;
		/* get char so we don't do so many loads from memory */
		tchar = *tptr; 
		while ( (!XP_IS_SPACE(tchar)) && (tchar != '=') && (tchar != '>') )
		{
			tptr++;
			tchar = *tptr;
		}
		cnt = tptr - buf;
		/*
		 * param_ptr points to the start of the name.
		 * plen is its length.
		 */
		param_ptr = buf;
		plen = cnt;

		/*
		 * Move forward to the '=' which delimits the value
		 */
		buf = tptr;
		while (XP_IS_SPACE(*buf))
		{
			buf++;
		}

		/*
		 * clip out the paramater name
		 */
		tptr = (char *)(param_ptr + plen);
		tchar = *tptr;
		*tptr = '\0';

		/*
		 * Look for the name in the list of requested names
		 * 
		 */
		nameNum = 0;
		nameFound = FALSE;
		while( nameNum < numNamesToFind )
		{
			if (pa_TagEqual(namesToFind[nameNum], param_ptr))
			{
				nameFound = TRUE;
				break;
			}
			nameNum++;
		}

		/*
		 * Restore character after parameter name
		 */
		*tptr = tchar;

		/*
		 * We have a value associated with this name.
		 */
		if (*buf == '=')
		{
			uint8 quoted; /* 0=none, 1=single, 2=double */
			char *nptr;
			char *vend_ptr;
			char *tmp_value;
			int32 nlen;

			/*
			 * Skip past the '='.
			 */
			buf++;

			/*
			 * skip any white space between '=' and
			 * the value.
			 */
			while (XP_IS_SPACE(*buf))
			{
				buf++;
			}

			/*
			 * Check if this is going to be a quoted
			 * value string.
			 */
			quoted = 0;
			tptr = buf;
			cnt = 0;
			if (*buf == '\'')
			{
				/*
				 * Skip the quote.
				 */
				buf++;
				tptr = buf;
				quoted = 1;
			}
			if (*buf == '\"')
			{
				/*
				 * Skip the quote.
				 */
				buf++;
				tptr = buf;
				quoted = 2;
			}
			if (*buf == '`')
			{
				/*
				 * DON'T skip the quote.
				 */
				tptr = buf+1;
				quoted = 3;
			}

			/*
			 * extract the end and length of the value string.
			 */
			if (quoted == 0)
			{
				/* get char so we don't do so many loads from memory */
				tchar = *tptr; 
				while ((!XP_IS_SPACE(tchar))&&(tchar != '>'))
				{
					tptr++;
					tchar = *tptr; 
					cnt++;
				}
			}
			else if (quoted == 1)
			{
#ifdef LENIENT_END_TAG
				/* get char so we don't do so many loads from memory */
				tchar = *tptr; 
				while ((tchar != '\'')&&(tchar != '>'))
				{
					tptr++;
					tchar = *tptr; 
					cnt++;
				}
#else
				/*
				 * Allow '>' inside the quoted value.
				 */
				while ((*tptr != '\'')&&(tptr != buf_end))
				{
					tptr++;
					cnt++;
				}
#endif /* LENIENT_END_TAG */
			}
			else if (quoted == 2)
			{
#ifdef LENIENT_END_TAG
				/* get char so we don't do so many loads from memory */
				tchar = *tptr; 
				while ((tchar != '\"')&&(tchar != '>'))
				{
					tptr++;
					tchar = *tptr; 
					cnt++;
				}
#else
				/*
				 * Allow '>' inside the quoted value.
				 */
				while ((*tptr != '\"')&&(tptr != buf_end))
				{
					tptr++;
					cnt++;
				}
#endif /* LENIENT_END_TAG */
			}
			else if (quoted == 3)
			{
				/*
				 * Allow '>' inside the quoted value.
				 */
				while ((*tptr != '`')&&(tptr != buf_end))
				{
					tptr++;
					cnt++;
				}
				/* include the end of the back quote when retrieving values. */
				if( *tptr == '`' )
				{
					tptr++;
					cnt++;
				}
			}

			if( nameFound == TRUE )
			{

				/*
				 * buf is the beginning of the parameter value.
				 * vlen is its length.  tptr is the character
				 * right after the end of the original end which
				 * may have been changed by the amperstand
				 * escapes.
				 */
				vlen = cnt;

				/*
				 * Find the end of the value and terminate
				 * the string.
				 */
				vend_ptr = (char *)(buf + vlen);
				tchar = *vend_ptr;
				*vend_ptr = '\0';

				tmp_value = XP_STRDUP(buf);
				/*
				 * Amperstand escapes are allowed in
				 * parameter values, expand them now.
				 * The last parameter should force no
				 * partial amperstand escapes here.
				 */
				nptr = pa_ExpandEscapes(tmp_value, vlen, &nlen, TRUE, win_csid);
				tmp_value[nlen] = '\0';

				/*
				 * Stuff the value into the list, and restore
				 * the character afer the value string.
				 */
				values[nameNum] = tmp_value;
				
				*vend_ptr = tchar;
			}

			buf = tptr;
			if ((quoted == 1)&&(*buf == '\''))
			{
				buf++;
			}
			else if ((quoted == 2)&&(*buf == '\"'))
			{
				buf++;
			}

			/*
			 * remove white space trailing the value.
			 */
			while (XP_IS_SPACE(*buf))
			{
				buf++;
			}
		}
		/*
		 * Else a name with no value, just a boolean tag.
		 * No need to do anything
		 */
		else
		{
		}
	}
	PA_UNLOCK(tag->data);
}


/*************************************
 * Function: PA_FetchParamValue
 *
 * Description: This messy function should take the parameter string
 *		that was part of the tag element, and turn
 *		look trhough it for a name that matches the
 *		parameter name passed in, and return its value.
 *		It is complicated by
 *		the fact that a value can be a single unquoted word,
 *		or a quoted string.  Also by the fact that some names
 *		may have no matching values, and by the fact that there
 *		can be arbitrary whitespace around the equal sign.
 *		Unmatching quotes in quoted values are assumed to
 *		close at the end of the tag element.
 *
 * Params: Takes a tag element structure containing the parameter string
 *	   and the parameter name to look for.
 *
 * Returns: A pointer to an allocated buffer containing a copy
 *	    of the value string.  Boolean parameters return a value
 *	    of '\0'.
 *************************************/
PA_Block
PA_FetchParamValue(PA_Tag *tag, char *param_name, uint16 win_csid)
{
	int32 cnt;
	char *buf;
#ifndef LENIENT_END_TAG
	char *buf_end;
#endif /* LENIENT_END_TAG */
	char *param_ptr;
	char tchar;
	int32 len;
	PA_Block buff;
	char *locked_buff;

	/*
	 * Punt on obvious error
	 */
	if ((tag == NULL)||(tag->data == NULL))
	{
		return(NULL);
	}

	PA_LOCK(buf, char *, tag->data);
	len = tag->data_len;

	/*
	 * To simplify enormously, we require that buf[len - 1] is
	 * the tag element terminating character '>'
	 *
	 * If not, punt here.
	 */
	if (buf[len - 1] != '>')
	{
		PA_UNLOCK(tag->data);
		return(NULL);
	}

#ifndef LENIENT_END_TAG
	buf_end = (char *)(buf + len - 1);
#endif /* LENIENT_END_TAG */

	while (*buf != '>')
	{
		char *tptr;
		int32 plen;
		int32 vlen;

		/*
		 * Skip whitespace before the next possible
		 * parameter name.  If no more parameters, break
		 * out of the while loop.
		 */
		while ((XP_IS_SPACE(*buf))&&(*buf != '>'))
		{
			buf++;
		}
		if (*buf == '>')
		{
			break;
		}

		/*
		 * Find the end of the parameter name.
		 */
		tptr = buf;
		cnt = 0;
		while ((!XP_IS_SPACE(*tptr))&&(*tptr != '=')&&(*tptr != '>'))
		{
			tptr++;
			cnt++;
		}

		/*
		 * param_ptr points to the start of the name.
		 * plen is its length.
		 */
		param_ptr = buf;
		plen = cnt;

		/*
		 * Move forward to the '=' which delimits the value
		 */
		buf = tptr;
		while (XP_IS_SPACE(*buf))
		{
			buf++;
		}

		/*
		 * We have a value associated with this name.
		 */
		if (*buf == '=')
		{
			uint8 quoted; /* 0=none, 1=single, 2=double */
			char *pptr;

			/*
			 * Skip past the '='.
			 */
			buf++;

			/*
			 * skip any white space between '=' and
			 * the value.
			 */
			while (XP_IS_SPACE(*buf))
			{
				buf++;
			}

			/*
			 * Check if this is going to be a quoted
			 * value string.
			 */
			quoted = 0;
			tptr = buf;
			cnt = 0;
			if (*buf == '\"')
			{
				/*
				 * Skip the quote.
				 */
				buf++;
				tptr = buf;
				quoted = 2;
			}
			else if (*buf == '\'')
			{
				/*
				 * Skip the quote.
				 */
				buf++;
				tptr = buf;
				quoted = 1;
			}
			else if (*buf == '`')
			{
				/* we want to include the ` as part of the string we return */
				tptr = buf+1;
				cnt = 1;
				quoted = 3;
			}

			/*
			 * extract the end and length of the value string.
			 */
			if (quoted == 0)
			{
				while ((!XP_IS_SPACE(*tptr))&&(*tptr != '>'))
				{
					tptr++;
					cnt++;
				}
			}
			else if (quoted == 1)
			{
#ifdef LENIENT_END_TAG
				while ((*tptr != '\'')&&(*tptr != '>'))
				{
					tptr++;
					cnt++;
				}
#else
				/*
				 * Allow '>' inside the quoted value.
				 */
				while ((*tptr != '\'')&&(tptr != buf_end))
				{
					tptr++;
					cnt++;
				}
#endif /* LENIENT_END_TAG */
			}
			else if (quoted == 2)
			{
#ifdef LENIENT_END_TAG
				while ((*tptr != '\"')&&(*tptr != '>'))
				{
					tptr++;
					cnt++;
				}
#else
				/*
				 * Allow '>' inside the quoted value.
				 */
				while ((*tptr != '\"')&&(tptr != buf_end))
				{
					tptr++;
					cnt++;
				}
#endif /* LENIENT_END_TAG */
			}
			else if (quoted == 3)
			{
				/*
				 * Allow '>' inside the quoted value.
				 */
				while ((*tptr != '`')&&(tptr != buf_end))
				{
					tptr++;
					cnt++;
				}

				if( *tptr == '`' )
				{
					tptr++;
					cnt++;
				}
			}

			/*
			 * buf is the beginning of the parameter value.
			 * vlen is its length.  tptr is the character
			 * right after the end of the original end which
			 * may have been changed by the amperstand
			 * escapes.
			 */

			vlen = cnt;

			/*
			 * clip out the paramater name
			 */
			pptr = (char *)(param_ptr + plen);
			tchar = *pptr;
			*pptr = '\0';

			/*
			 * If this parameter name matches, this is
			 * the right value, make a copy to return.
			 */
			if (pa_TagEqual(param_name, param_ptr))
			{
				char *vend_ptr;

				/*
				 * Restore character after parameter name
				 */
				*pptr = tchar;

				/*
				 * Find the end of the value and terminate
				 * the string.
				 */
				vend_ptr = (char *)(buf + vlen);
				tchar = *vend_ptr;
				*vend_ptr = '\0';

				buff = PA_ALLOC((vlen + 1) * sizeof(char));
				if (buff != NULL)
				{
					char *nptr;
					int32 nlen;

					PA_LOCK(locked_buff, char *, buff);
					XP_STRCPY(locked_buff, buf);

					/*
					 * Amperstand escapes are allowed in
					 * parameter values, expand them now.
					 * The last parameter should force no
					 * partial amperstand escapes here.
					 */
					nptr = pa_ExpandEscapes(locked_buff,
						vlen, &nlen, TRUE, win_csid);
					locked_buff[nlen] = '\0';

					PA_UNLOCK(buff);
				}
				*vend_ptr = tchar;

				PA_UNLOCK(tag->data);
				return(buff);
			}
			*pptr = tchar;

			buf = tptr;
			if ((quoted == 1)&&(*buf == '\''))
			{
				buf++;
			}
			else if ((quoted == 2)&&(*buf == '\"'))
			{
				buf++;
			}
			else if ((quoted == 3)&&(*buf == '`'))
			{
				buf++;
			}

			/*
			 * remove white space trailing the value.
			 */
			while (XP_IS_SPACE(*buf))
			{
				buf++;
			}
		}
		/*
		 * Else a name with no value, just a boolean tag.
		 * For boolean tag set value to '\0' ("").
		 */
		else
		{
			/*
			 * clip out the paramater name
			 */
			tptr = (char *)(param_ptr + plen);
			tchar = *tptr;
			*tptr = '\0';

			/*
			 * If this parameter name matches, this is
			 * a boolean, and return '\0' as its value.
			 */
			if (pa_TagEqual(param_name, param_ptr))
			{
				*tptr = tchar;

				buff = PA_ALLOC(1 * sizeof(char));
				if (buff == NULL)
				{
					PA_UNLOCK(tag->data);
					return(NULL);
				}
				PA_LOCK(locked_buff, char *, buff);
				locked_buff[0] = '\0';
				PA_UNLOCK(buff);
				PA_UNLOCK(tag->data);
				return(buff);
			}
			*tptr = tchar;
		}
	}
	PA_UNLOCK(tag->data);
	return(NULL);
}

int32
PA_FetchAllNameValues(PA_Tag *tag, char ***names, char ***values, uint16 win_csid)
{
	int32 cnt;
	char *buf;
	char *param_ptr;
	char tchar;
	int32 len;
	char **name_list;
	char **value_list;
	int32 name_size;
	int32 name_cnt;
#ifndef LENIENT_END_TAG
	char *buf_end;
#endif /* LENIENT_END_TAG */

	*names = NULL;
	*values = NULL;

	/*
	 * Punt on obvious error
	 */
	if ((tag == NULL)||(tag->data == NULL))
	{
		return(0);
	}

	PA_LOCK(buf, char *, tag->data);
	len = tag->data_len;

	/*
	 * To simplify enormously, we require that buf[len - 1] is
	 * the tag element terminating character '>'
	 *
	 * If not, punt here.
	 */
	if (buf[len - 1] != '>')
	{
		PA_UNLOCK(tag->data);
		return(0);
	}

	name_list = (char **)XP_ALLOC(NAME_LIST_INC * sizeof(char *));
	if (name_list == NULL)
	{
		PA_UNLOCK(tag->data);
		return(0);
	}
	name_size = NAME_LIST_INC;
	name_cnt = 0;

	value_list = (char **)XP_ALLOC(NAME_LIST_INC * sizeof(char *));
	if (value_list == NULL)
	{
		XP_FREE(name_list);
		PA_UNLOCK(tag->data);
		return(0);
	}

#ifndef LENIENT_END_TAG
	buf_end = (char *)(buf + len - 1);
#endif /* LENIENT_END_TAG */
	while (*buf != '>')
	{
		char *tptr;
		int32 plen;
		int32 vlen;

		/*
		 * Skip whitespace before the next possible
		 * parameter name.  If no more parameters, break
		 * out of the while loop.
		 */
		while ((XP_IS_SPACE(*buf))&&(*buf != '>'))
		{
			buf++;
		}
		if (*buf == '>')
		{
			break;
		}

		/*
		 * Find the end of the parameter name.
		 */
		tptr = buf;
		cnt = 0;
		while ((!XP_IS_SPACE(*tptr))&&(*tptr != '=')&&(*tptr != '>'))
		{
			tptr++;
			cnt++;
		}

		/*
		 * param_ptr points to the start of the name.
		 * plen is its length.
		 */
		param_ptr = buf;
		plen = cnt;

		/*
		 * Move forward to the '=' which delimits the value
		 */
		buf = tptr;
		while (XP_IS_SPACE(*buf))
		{
			buf++;
		}

		/*
		 * clip out the paramater name
		 */
		tptr = (char *)(param_ptr + plen);
		tchar = *tptr;
		*tptr = '\0';

		/*
		 * Stuff the name into the name list, growing
		 * the name list if needed.
		 */
		name_list[name_cnt] = XP_STRDUP(param_ptr);
		name_cnt++;
		if (name_cnt >= name_size)
		{
			name_list = (char **)XP_REALLOC(name_list,
			      (name_size + NAME_LIST_INC) * sizeof(char *));
			if (name_list == NULL)
			{
				PA_UNLOCK(tag->data);
				return(0);
			}
			value_list = (char **)XP_REALLOC(value_list,
			      (name_size + NAME_LIST_INC) * sizeof(char *));
			if (value_list == NULL)
			{
				PA_UNLOCK(tag->data);
				return(0);
			}
			name_size += NAME_LIST_INC;
		}

		/*
		 * Restore character after parameter name
		 */
		*tptr = tchar;

		/*
		 * We have a value associated with this name.
		 */
		if (*buf == '=')
		{
			uint8 quoted; /* 0=none, 1=single, 2=double */
			char *nptr;
			char *vend_ptr;
			char *tmp_value;
			int32 nlen;

			/*
			 * Skip past the '='.
			 */
			buf++;

			/*
			 * skip any white space between '=' and
			 * the value.
			 */
			while (XP_IS_SPACE(*buf))
			{
				buf++;
			}

			/*
			 * Check if this is going to be a quoted
			 * value string.
			 */
			quoted = 0;
			tptr = buf;
			cnt = 0;
			if (*buf == '\'')
			{
				/*
				 * Skip the quote.
				 */
				buf++;
				tptr = buf;
				quoted = 1;
			}
			if (*buf == '\"')
			{
				/*
				 * Skip the quote.
				 */
				buf++;
				tptr = buf;
				quoted = 2;
			}
			if (*buf == '`')
			{
				/*
				 * DON'T skip the quote.
				 */
				tptr = buf+1;
				quoted = 3;
			}

			/*
			 * extract the end and length of the value string.
			 */
			if (quoted == 0)
			{
				while ((!XP_IS_SPACE(*tptr))&&(*tptr != '>'))
				{
					tptr++;
					cnt++;
				}
			}
			else if (quoted == 1)
			{
#ifdef LENIENT_END_TAG
				while ((*tptr != '\'')&&(*tptr != '>'))
				{
					tptr++;
					cnt++;
				}
#else
				/*
				 * Allow '>' inside the quoted value.
				 */
				while ((*tptr != '\'')&&(tptr != buf_end))
				{
					tptr++;
					cnt++;
				}
#endif /* LENIENT_END_TAG */
			}
			else if (quoted == 2)
			{
#ifdef LENIENT_END_TAG
				while ((*tptr != '\"')&&(*tptr != '>'))
				{
					tptr++;
					cnt++;
				}
#else
				/*
				 * Allow '>' inside the quoted value.
				 */
				while ((*tptr != '\"')&&(tptr != buf_end))
				{
					tptr++;
					cnt++;
				}
#endif /* LENIENT_END_TAG */
			}
			else if (quoted == 3)
			{
				/*
				 * Allow '>' inside the quoted value.
				 */
				while ((*tptr != '`')&&(tptr != buf_end))
				{
					tptr++;
					cnt++;
				}
				/* include the end of the back quote when retrieving values. */
				if( *tptr == '`' )
				{
					tptr++;
					cnt++;
				}
			}

			/*
			 * buf is the beginning of the parameter value.
			 * vlen is its length.  tptr is the character
			 * right after the end of the original end which
			 * may have been changed by the amperstand
			 * escapes.
			 */
			vlen = cnt;

			/*
			 * Find the end of the value and terminate
			 * the string.
			 */
			vend_ptr = (char *)(buf + vlen);
			tchar = *vend_ptr;
			*vend_ptr = '\0';

			tmp_value = XP_STRDUP(buf);
			/*
			 * Amperstand escapes are allowed in
			 * parameter values, expand them now.
			 * The last parameter should force no
			 * partial amperstand escapes here.
			 */
			nptr = pa_ExpandEscapes(tmp_value, vlen, &nlen, TRUE, win_csid);
			tmp_value[nlen] = '\0';

			/*
			 * Stuff the value into the list, and restore
			 * the character afer the value string.
			 */
			value_list[name_cnt - 1] = tmp_value;
			*vend_ptr = tchar;

			buf = tptr;
			if ((quoted == 1)&&(*buf == '\''))
			{
				buf++;
			}
			else if ((quoted == 2)&&(*buf == '\"'))
			{
				buf++;
			}

			/*
			 * remove white space trailing the value.
			 */
			while (XP_IS_SPACE(*buf))
			{
				buf++;
			}
		}
		/*
		 * Else a name with no value, just a boolean tag.
		 * For boolean tag set value to NULL.
		 */
		else
		{
			value_list[name_cnt - 1] = NULL;;
		}
	}
	PA_UNLOCK(tag->data);

	*names = name_list;
	*values = value_list;
	return(name_cnt);
}

/*************************************
 * Function: PA_HasMocha
 *
 * Description: This messy function should take the parameter string
 *		that was part of the tag element, and turn
 *		look trhough it for any mocha attributes
 *		It is complicated by
 *		the fact that a value can be a single unquoted word,
 *		or a quoted string.  Also by the fact that some names
 *		may have no matching values, and by the fact that there
 *		can be arbitrary whitespace around the equal sign.
 *		Unmatching quotes in quoted values are assumed to
 *		close at the end of the tag element.
 *
 * Params: Takes a tag element structure containing the parameter string
 *
 * Returns: TRUE if any mocha attributes were found else FALSE
 *************************************/
Bool
PA_HasMocha(PA_Tag *tag)
{
	int32 cnt;
	char *buf;
#ifndef LENIENT_END_TAG
	char *buf_end;
#endif /* LENIENT_END_TAG */
	char *param_ptr;
	char tchar;
	int32 len;
	PA_Block buff;
	char *locked_buff;

	/*
	 * Punt on obvious error
	 */
	if ((tag == NULL)||(tag->data == NULL))
	{
		return(FALSE);
	}

	PA_LOCK(buf, char *, tag->data);
	len = tag->data_len;

	/*
	 * To simplify enormously, we require that buf[len - 1] is
	 * the tag element terminating character '>'
	 *
	 * If not, punt here.
	 */
	if (buf[len - 1] != '>')
	{
		PA_UNLOCK(tag->data);
		return(FALSE);
	}

#ifndef LENIENT_END_TAG
	buf_end = (char *)(buf + len - 1);
#endif /* LENIENT_END_TAG */

	while (*buf != '>')
	{
		char *tptr;
		int32 plen;
		int32 vlen;

		/*
		 * Skip whitespace before the next possible
		 * parameter name.  If no more parameters, break
		 * out of the while loop.
		 */
		while ((XP_IS_SPACE(*buf))&&(*buf != '>'))
		{
			buf++;
		}
		if (*buf == '>')
		{
			break;
		}

		/*
		 * Find the end of the parameter name.
		 */
		tptr = buf;
		cnt = 0;
		while ((!XP_IS_SPACE(*tptr))&&(*tptr != '=')&&(*tptr != '>'))
		{
			tptr++;
			cnt++;
		}

		/*
		 * param_ptr points to the start of the name.
		 * plen is its length.
		 */
		param_ptr = buf;
		plen = cnt;

		/*
		 * Move forward to the '=' which delimits the value
		 */
		buf = tptr;
		while (XP_IS_SPACE(*buf))
		{
			buf++;
		}

		/*
		 * We have a value associated with this name.
		 */
		if (*buf == '=')
		{
			uint8 quoted; /* 0=none, 1=single, 2=double */
			char *pptr;

			/*
			 * Skip past the '='.
			 */
			buf++;

			/*
			 * skip any white space between '=' and
			 * the value.
			 */
			while (XP_IS_SPACE(*buf))
			{
				buf++;
			}

			/*
			 * Check if this is going to be a quoted
			 * value string.
			 */
			quoted = 0;
			tptr = buf;
			cnt = 0;
			if (*buf == '\"')
			{
				/*
				 * Skip the quote.
				 */
				buf++;
				tptr = buf;
				quoted = 2;
			}
			else if (*buf == '\'')
			{
				/*
				 * Skip the quote.
				 */
				buf++;
				tptr = buf;
				quoted = 1;
			}
			else if (*buf == '`')
			{
				/* we want to include the ` as part of the string we return */
				tptr = buf+1;
				cnt = 1;
				quoted = 3;
			}

			/*
			 * extract the end and length of the value string.
			 */
			if (quoted == 0)
			{
				while ((!XP_IS_SPACE(*tptr))&&(*tptr != '>'))
				{
					tptr++;
					cnt++;
				}
			}
			else if (quoted == 1)
			{
#ifdef LENIENT_END_TAG
				while ((*tptr != '\'')&&(*tptr != '>'))
				{
					tptr++;
					cnt++;
				}
#else
				/*
				 * Allow '>' inside the quoted value.
				 */
				while ((*tptr != '\'')&&(tptr != buf_end))
				{
					tptr++;
					cnt++;
				}
#endif /* LENIENT_END_TAG */
			}
			else if (quoted == 2)
			{
#ifdef LENIENT_END_TAG
				while ((*tptr != '\"')&&(*tptr != '>'))
				{
					tptr++;
					cnt++;
				}
#else
				/*
				 * Allow '>' inside the quoted value.
				 */
				while ((*tptr != '\"')&&(tptr != buf_end))
				{
					tptr++;
					cnt++;
				}
#endif /* LENIENT_END_TAG */
			}
			else if (quoted == 3)
			{
				/*
				 * Allow '>' inside the quoted value.
				 */
				while ((*tptr != '`')&&(tptr != buf_end))
				{
					tptr++;
					cnt++;
				}

				if( *tptr == '`' )
				{
					tptr++;
					cnt++;
				}
			}

			/*
			 * buf is the beginning of the parameter value.
			 * vlen is its length.  tptr is the character
			 * right after the end of the original end which
			 * may have been changed by the amperstand
			 * escapes.
			 */

			vlen = cnt;

			/*
			 * clip out the paramater name
			 */
			pptr = (char *)(param_ptr + plen);
			tchar = *pptr;
			*pptr = '\0';

			/*
			 * See if this parameter name is a Mocha attribute name
			 */
			if (pa_TagEqual(PARAM_NAME, param_ptr)        ||
                pa_TagEqual(PARAM_ONFOCUS, param_ptr)     ||
                pa_TagEqual(PARAM_ONBLUR, param_ptr)      ||
                pa_TagEqual(PARAM_ONCHANGE, param_ptr)    ||
                pa_TagEqual(PARAM_ONSELECT, param_ptr)    ||
                pa_TagEqual(PARAM_ONCLICK, param_ptr)     ||
                pa_TagEqual(PARAM_ONSCROLL, param_ptr)    ||
                pa_TagEqual(PARAM_ONSUBMIT, param_ptr)    ||
                pa_TagEqual(PARAM_ONMOUSEOVER, param_ptr) ||
                pa_TagEqual(PARAM_ONMOUSEOUT, param_ptr)  ||
                pa_TagEqual(PARAM_ONRESET, param_ptr)     ||
                pa_TagEqual(PARAM_ONUNLOAD, param_ptr)    ||
                pa_TagEqual(PARAM_ONLOAD, param_ptr)      ||
                pa_TagEqual(PARAM_ONERROR, param_ptr)     ||
                pa_TagEqual(PARAM_ONABORT, param_ptr)     ||
                pa_TagEqual(PARAM_ONHELP, param_ptr)	  ||
                pa_TagEqual(PARAM_ONMOUSEDOWN, param_ptr) ||
                pa_TagEqual(PARAM_ONMOUSEUP, param_ptr)	  ||
                pa_TagEqual(PARAM_ONDBLCLICK, param_ptr)  ||
                pa_TagEqual(PARAM_ONKEYDOWN, param_ptr)	  ||
                pa_TagEqual(PARAM_ONKEYUP, param_ptr)	  ||
                pa_TagEqual(PARAM_ONKEYPRESS, param_ptr)  ||
                pa_TagEqual(PARAM_ONDRAGDROP, param_ptr)  ||
                pa_TagEqual(PARAM_ONMOVE, param_ptr)	  ||
                pa_TagEqual(PARAM_ONRESIZE, param_ptr))

			{
				char *vend_ptr;

				/*
				 * Restore character after parameter name
				 */
				*pptr = tchar;

            	PA_UNLOCK(tag->data);
                return(TRUE);
			}

            /*
			 * Wasn't a meaningful attribute, restore character after 
             *   parameter name
			 */
			*pptr = tchar;

			buf = tptr;
			if ((quoted == 1)&&(*buf == '\''))
			{
				buf++;
			}
			else if ((quoted == 2)&&(*buf == '\"'))
			{
				buf++;
			}
			else if ((quoted == 3)&&(*buf == '`'))
			{
				buf++;
			}

			/*
			 * remove white space trailing the value.
			 */
			while (XP_IS_SPACE(*buf))
			{
				buf++;
			}
		}
		/*
		 * Else a name with no value, just a boolean tag.
		 * For boolean tag set value to '\0' ("").
		 */
		else
		{
			/*
			 * clip out the paramater name
			 */
			tptr = (char *)(param_ptr + plen);
			tchar = *tptr;
			*tptr = '\0';

			/*
			 * Are there any other no-value mocha parameters?
			 */
			if (pa_TagEqual(PARAM_MAYSCRIPT, param_ptr))
			{
                /*
                 * Restore the trailing character
                 */
				*tptr = tchar;

            	PA_UNLOCK(tag->data);
                return(TRUE);
			}

            /*
             * Wasn't useful, restore the trailing character
             */
			*tptr = tchar;
		}
	}
	PA_UNLOCK(tag->data);
	return(FALSE);
}


#ifdef PROFILE
#pragma profile off
#endif

