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
/*
 * Cross platform html dialogs
 *
 *
 */

#include "xp.h"
#include "fe_proto.h"
#include "seccomon.h"
#include "net.h"
#include "htmldlgs.h"
#include "xpgetstr.h"
#include "shist.h"
#include "prlog.h"
#include "libmocha.h"
#include "prclist.h"

extern int XP_SEC_CANCEL;
extern int XP_SEC_OK;
extern int XP_SEC_CONTINUE;
extern int XP_SEC_NEXT;
extern int XP_SEC_BACK;
extern int XP_SEC_FINISHED;
extern int XP_SEC_MOREINFO;
extern int XP_SEC_CONFIG;
extern int XP_SEC_SHOWCERT;
extern int XP_SEC_SHOWORDER;
extern int XP_SEC_SHOWDOCINFO;
extern int XP_SEC_NEXT_KLUDGE;
extern int XP_SEC_BACK_KLUDGE;
extern int XP_SEC_RENEW;
extern int XP_SEC_NEW;
extern int XP_SEC_RELOAD;
extern int XP_SEC_DELETE;
extern int XP_SEC_EDIT;
extern int XP_SEC_LOGIN;
extern int XP_SEC_LOGOUT;
extern int XP_SEC_SETPASSWORD;
extern int XP_SEC_SAVEAS;
extern int XP_SEC_FETCH;
extern int XP_SEC_GRANT;
extern int XP_SEC_DENY;
extern int XP_SEC_DETAILS;
extern int XP_SEC_CERTIFICATE;

extern int XP_DIALOG_CANCEL_BUTTON_STRINGS;
extern int XP_DIALOG_CANCEL_CONTINUE_BUTTON_STRINGS;
extern int XP_DIALOG_CANCEL_OK_BUTTON_STRINGS;
extern int XP_DIALOG_CANCEL_OK_MOREINFO_BUTTON_STRINGS;
extern int XP_DIALOG_FETCH_CANCEL_BUTTON_STRINGS;
extern int XP_DIALOG_CONTINUE_BUTTON_STRINGS;
extern int XP_DIALOG_FOOTER_STRINGS;
extern int XP_DIALOG_HEADER_STRINGS;
extern int XP_DIALOG_MIDDLE_STRINGS;
extern int XP_DIALOG_NULL_STRINGS;
extern int XP_DIALOG_OK_BUTTON_STRINGS;
extern int XP_EMPTY_STRINGS;
extern int XP_PANEL_FIRST_BUTTON_STRINGS;
extern int XP_PANEL_FOOTER_STRINGS;
extern int XP_PANEL_HEADER_STRINGS;
extern int XP_PANEL_LAST_BUTTON_STRINGS;
extern int XP_PANEL_MIDDLE_BUTTON_STRINGS;
extern int XP_PANEL_ONLY_BUTTON_STRINGS;
extern int XP_ALERT_TITLE_STRING;
extern int XP_DIALOG_JS_HEADER_STRINGS;
extern int XP_DIALOG_JS_HEADER_STRINGS_WITH_UTF8_CHARSET;
extern int XP_DIALOG_JS_MIDDLE_STRINGS;
extern int XP_DIALOG_JS_FOOTER_STRINGS;
extern int XP_BANNER_FONTFACE_INFO_STRING;
extern int XP_DIALOG_STYLEINFO_STRING;

typedef struct {
    PRCList link;
    char *data;
    unsigned int len;
} bufferNode;

typedef struct _htmlDialogStream {
    PRCList queuedBuffers;
    PRArenaPool *arena;
    NET_StreamClass *stream;
    URL_Struct *url;
    PRBool buffering;
    char *curbuf;
    unsigned int curlen;
} HTMLDialogStream;

typedef struct {
    void *window;
    void (* deleteWinCallback)(void *arg);
    void *arg;
    PRArenaPool *freeArena;
} deleteWindowCBArg;

static void
deleteWindow(void *closure)
{
    deleteWindowCBArg *state;
    
    state = (deleteWindowCBArg *)closure;
    
    FE_DestroyWindow((MWContext *)state->window);

    if ( state->deleteWinCallback ) {
	(* state->deleteWinCallback)(state->arg);
    }

    if ( state->freeArena != NULL ) {
	PORT_FreeArena(state->freeArena, PR_FALSE);
    }
    
    PORT_Free(state);
    
    return;
}

static SECStatus
writeOrBuffer(HTMLDialogStream *stream)
{
    unsigned int cnt;
    char *tmpdest;
    bufferNode *node;
    int ret;
    
    if ( ! stream->buffering ) {
	cnt = (*(stream->stream)->is_write_ready)
	    (stream->stream);
	/* if stream is clogged, then start buffering */
	if ( cnt < stream->curlen ) {
	    /* we need to copy the current contents of the reusable
	     * buffer to a persistent buffer allocated from the
	     * arena, then free the persistent buffer
	     */
	    tmpdest = (char *)PORT_ArenaAlloc(stream->arena, stream->curlen);
	    if ( tmpdest == NULL ) {
		goto loser;
	    }

	    PORT_Memcpy(tmpdest, stream->curbuf, stream->curlen);
	    PORT_Free(stream->curbuf);
	    stream->curbuf = tmpdest;
	    stream->buffering = PR_TRUE;
	}
    }

    if ( stream->buffering ) {
	node = PORT_ArenaAlloc(stream->arena, sizeof(bufferNode));
	if ( node == NULL ) {
	    goto loser;
	}
	
	PR_APPEND_LINK(&node->link, &stream->queuedBuffers);

	node->data = stream->curbuf;
	node->len = stream->curlen;
    } else {
	ret = (*(stream->stream)->put_block)
	    (stream->stream, stream->curbuf, stream->curlen);
	if ( ret < 0 ) {
	    goto loser;
	}
    }

    return(SECSuccess);
loser:
    return(SECFailure);
}

static SECStatus
putStringToStream(HTMLDialogStream *stream, char *string, PRBool quote)
{
    SECStatus rv;
    char *tmpptr;
    char *destptr;
    int curcount;
    unsigned int maxsize;

    if (*string == 0) {
	return(SECSuccess);
    }

    if ( stream->buffering ) {
	/* pick a size that is convenient based on the current behaviour
	 * of the html parser
	 */
	maxsize = 2000; /* XXX - we know what the parser does here!! */
	stream->curbuf = destptr = (char *)PORT_ArenaAlloc(stream->arena,
							   maxsize);
    } else {
	maxsize = (*(stream->stream)->is_write_ready)
	    (stream->stream);
	
	if ( maxsize == 0 ) {
	    /* we are switching to buffering now */
	    maxsize = 2000;
	    stream->curbuf = destptr = (char *)PORT_ArenaAlloc(stream->arena,
							       maxsize);
	    stream->buffering = PR_TRUE;
	} else {
	    stream->curbuf = destptr = (char *)PORT_Alloc( maxsize );
	}
    }

    if ( stream->curbuf == NULL ) {
	return(SECFailure);
    }

    curcount = 0;
    tmpptr = string;

    if ( quote ) {
	while ( *tmpptr ) {
	    if ( *tmpptr == '\'' ) {
		*destptr = '\\';
		destptr++;
		*destptr = *tmpptr;
		destptr++;
		curcount += 2;
	    } else if ( *tmpptr == '\\' ) {
		*destptr = '\\';
		destptr++;
		*destptr = '\\';
		destptr++;
		curcount += 2;
	    } else if ( *tmpptr == '\n' ) {
		*destptr = '\\';
		destptr++;
		*destptr = 'n';
		destptr++;
		curcount += 2;
	    } else if ( *tmpptr == '\r' ) {
		*destptr = '\\';
		destptr++;
		*destptr = 'r';
		destptr++;
		curcount += 2;
	    } else if ( *tmpptr == '/' ) {
		*destptr = '\\';
		destptr++;
		*destptr = '/';
		destptr++;
		curcount += 2;
	    } else {
		*destptr = *tmpptr;
		destptr++;
		curcount++;
	    }

	    tmpptr++;
	    if ( curcount >= ( maxsize - 1 ) ) {
		stream->curlen = curcount;
		rv = writeOrBuffer(stream);
		if ( rv != SECSuccess ) {
		    goto loser;
		}
		
		if ( stream->buffering && ( *tmpptr != '\0' ) ) {
		    maxsize = 2000; /*XXX-we know what the parser does here!!*/
		    stream->curbuf = destptr =
			(char *)PORT_ArenaAlloc(stream->arena, maxsize);
		    if ( stream->curbuf == NULL ) {
			goto loser;
		    }
		} else {
		    destptr = stream->curbuf;
		}
		curcount = 0;
	    }
	}
    } else {
	while ( *tmpptr ) {
	    *destptr = *tmpptr;
	    destptr++;
	    curcount++;
	    tmpptr++;

	    if ( curcount >= ( maxsize - 1 ) ) {
		stream->curlen = curcount;
		rv = writeOrBuffer(stream);
		if ( rv != SECSuccess ) {
		    goto loser;
		}
		
		if ( stream->buffering && ( *tmpptr != '\0') ) {
		    maxsize = 2000; /*XXX-we know what the parser does here!!*/
		    stream->curbuf = destptr =
			(char *)PORT_ArenaAlloc(stream->arena, maxsize);
		    if ( stream->curbuf == NULL ) {
			goto loser;
		    }
		} else {
		    destptr = stream->curbuf;
		}
		curcount = 0;
	    }
	}
    }

    if ( curcount > 0 ) {
	stream->curlen = curcount;
	rv = writeOrBuffer(stream);
	if ( rv != SECSuccess ) {
	    goto loser;
	}
    }

    rv = SECSuccess;
    goto done;
    
loser:
    rv = SECFailure;
done:
    if ( ( !stream->buffering ) && ( stream->curbuf != NULL ) ) {
	PORT_Free(stream->curbuf);
    }

    return(rv);
}

SECStatus
XP_PutDialogStringsToStream(HTMLDialogStream *stream, XPDialogStrings *strs,
			    PRBool quote)
{
    SECStatus rv;
    char *src, *token, *junk;
    int argnum;
    void *proto_win = stream->stream->window_id;    
    src = strs->contents;
    
    while ((token = PORT_Strchr(src, '%'))) {
	*token = 0;
	rv = putStringToStream(stream, src, quote);
	if (rv) {
	    return(SECFailure);
	}
	*token = '%';		/* non-destructive... */
	token++;		/* now points to start of token */
	src = PORT_Strchr(token, '%');
	*src = 0;
	
	argnum = (int)XP_STRTOUL(token, &junk, 10);
	if (junk != token) {	/* A number */
	    PORT_Assert(argnum <= strs->nargs);
	    if (strs->args[argnum]) {
		rv = putStringToStream(stream, strs->args[argnum],
				       quote);
		if (rv) {
 		    return(SECFailure);
		}
	    }
	}
	else if (!PORT_Strcmp(token, "-cont-")) /* continuation */
	    /* Do Nothing */;
	else if (*token == 0) {	/* %% -> % */
	    rv = putStringToStream(stream, "%", quote);
	    if (rv) {
 		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "cancel")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_CANCEL),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "ok")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_OK),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "continue")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_CONTINUE),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "next")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_NEXT),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "back")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_BACK),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "finished")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_FINISHED),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "moreinfo")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_MOREINFO),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "config")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_CONFIG),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "showcert")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_SHOWCERT),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "showorder")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_SHOWORDER),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "showdocinfo")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_SHOWDOCINFO),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "renew")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_RENEW),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "new")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_NEW),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "reload")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_RELOAD),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "delete")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_DELETE),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "edit")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_EDIT),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "login")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_LOGIN),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "logout")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_LOGOUT),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "setpassword")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_SETPASSWORD),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "fetch")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_FETCH),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "banner-sec")) {
	    rv = putStringToStream(stream,
				   "<table width=100%><tr><td bgcolor=#0000ff><img align=left src=about:security?banner-secure></td></tr></table><br>",
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "saveas")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_SAVEAS),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "-styleinfo-")) {
	    rv = putStringToStream(stream,
			XP_GetString(XP_DIALOG_STYLEINFO_STRING), quote);
	    if(rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "sec-banner-begin")) {
	    rv = putStringToStream(stream,
			"<table><tr><td>"
			"<img align=left src=about:security?banner-secure></td>"
			"<td>", quote);
	    if (rv) {
		return(SECFailure);
	    }
	    rv = putStringToStream(stream,
				XP_GetString(XP_BANNER_FONTFACE_INFO_STRING),
				quote);
	    if (rv) {
		return(SECFailure);
	    }
	    rv = putStringToStream(stream, "<B>", quote);
	    if (rv) {
		return(SECFailure);
	    }
	} else if (!PORT_Strcmp(token, "sec-banner-end")) {
	    rv = putStringToStream(stream,
			"</b></font></td></tr></table><br>", quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "grant")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_GRANT),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "deny")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_DENY),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "details")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_DETAILS),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else if (!PORT_Strcmp(token, "certificate")) {
	    rv = putStringToStream(stream,
				   XP_GetString(XP_SEC_CERTIFICATE),
				   quote);
	    if (rv) {
		return(SECFailure);
	    }
	}
	else {
	    PORT_Assert(0);	/* Failure: Unhandled token. */
	}
	
	*src = '%';		/* non-destructive... */
	src++;			/* now points past %..% */
    }
    
    rv = putStringToStream(stream, src, quote);
    if (rv) {
 	return(SECFailure);
    }
    
    return(SECSuccess);
}

SECStatus
XP_PutDialogStringsTagToStream(HTMLDialogStream *stream, int num, PRBool quote)
{
    SECStatus rv;
    XPDialogStrings *strings;
    
    strings = XP_GetDialogStrings(num);
    if ( strings == NULL ) {
	return(SECFailure);
    }
    
    rv = XP_PutDialogStringsToStream(stream, strings, quote);
    
    XP_FreeDialogStrings(strings);
    
    return(rv);
}

char **
cgi_ConvertStringToArgVec(char *str, int length, int *argcp)
{
    char *cp, **avp;
    int argc;
    char c;
    char tmp;
    char *destp;
    
    char **av = 0;
    if (!str) {
	if (argcp) *argcp = 0;
	return 0;
    }

    cp = str + length - 1;
    while ( (*cp == '\n') || (*cp == '\r') ) {
	*cp-- = '\0';
    }
    
    /* 1. Count number of args in input */
    argc = 1;
    cp = str;
    for (;;) {
	cp = PORT_Strchr(cp, '=');
	if (!cp) break;
	cp++;
	argc++;
	cp = PORT_Strchr(cp, '&');
	if (!cp) break;
	cp++;
	argc++;
    }

    /* 2. Allocate space for argvec array and copy of strings */
    *argcp = argc;
    av = (char**) PORT_ZAlloc(((argc + 1) * sizeof(char*)) + strlen(str) + 1);
    destp = ((char *)av) + ( (argc + 1) * sizeof(char *) );
    
    if (!av) goto loser;

    /*
    ** 3. Break up input into each arg chunk. Once we break an entry up,
    ** compress it in place, replacing the magic characters with their
    ** correct value.
    */
    avp = av;
    cp = str;
    *avp++ = destp;

    while( ( c = (*cp++) ) != '\0' ) {
	switch( c ) {
	  case '&':
	  case '=':
	    *destp++ = '\0';
	    *avp++ = destp;
	    break;
	  case '+':
	    *destp++ = ' ';
	    break;
	  case '%':
	    c = *cp++;
	    if ((c >= 'a') && (c <= 'f')) {
		tmp = c - 'a' + 10;
	    } else if ((c >= 'A') && (c <= 'F')) {
		tmp = c - 'A' + 10;
	    } else {
		tmp = c - '0';
	    }
	    tmp = tmp << 4;
	    c = *cp++;
	    if ((c >= 'a') && (c <= 'f')) {
		tmp |= ( c - 'a' + 10 );
	    } else if ((c >= 'A') && (c <= 'F')) {
		tmp |= ( c - 'A' + 10 );
	    } else {
		tmp |= ( c - '0' );
	    }
	    *destp++ = tmp;
	    break;
	  default:
	    *destp++ = c;
	    break;
	}
	*destp = '\0';
    }
    return av;

  loser:
    PORT_Free(av);
    return 0;
}

/*
 * return the value of a name value pair
 */
char *
XP_FindValueInArgs(const char *name, char **av, int ac)
{
    for( ;ac > 0; ac -= 2, av += 2 ) {
	if ( PORT_Strcmp(name, av[0]) == 0 ) {
	    return(av[1]);
	}
    }
    return(0);
}

void
XP_HandleHTMLDialog(URL_Struct *url)
{
    char **av = NULL;
    int ac;
    char *handleString;
    char *buttonString;
    XPDialogState *handle = NULL;
    unsigned int button;
    PRBool ret;
    
    /* collect post data */
    av = cgi_ConvertStringToArgVec(url->post_data, url->post_data_size, &ac);
    if ( av == NULL ) {
	goto loser;
    }
    
    /* get the handle */
    handleString = XP_FindValueInArgs("handle", av, ac);
    if ( handleString == NULL ) {
	goto loser;
    }

    /* get the button value */
    buttonString = XP_FindValueInArgs("button", av, ac);
    if ( buttonString == NULL ) {
	goto loser;
    }

    handle = NULL;
#if defined(__sun) && !defined(SVR4) /* sun 4.1.3 */
    sscanf(handleString, "%lu", &handle);
#else
    sscanf(handleString, "%p", &handle);
 #endif
    if ( handle == NULL ) {
	goto loser;
    }

    if ( handle->deleted ) {
	goto loser;
    }
    
    if ( PORT_Strcasecmp(buttonString, XP_GetString(XP_SEC_OK)) == 0 ) {
	button = XP_DIALOG_OK_BUTTON;
    } else if ( PORT_Strcasecmp(buttonString,
			      XP_GetString(XP_SEC_CANCEL)) == 0 ) {
	button = XP_DIALOG_CANCEL_BUTTON;
    } else if ( PORT_Strcasecmp(buttonString,
			      XP_GetString(XP_SEC_CONTINUE)) == 0 ) {
	button = XP_DIALOG_CONTINUE_BUTTON;
    } else if ( PORT_Strcasecmp(buttonString,
			      XP_GetString(XP_SEC_MOREINFO)) == 0 ) {
	button = XP_DIALOG_MOREINFO_BUTTON;
    } else if ( PORT_Strcasecmp(buttonString,
			      XP_GetString(XP_SEC_FETCH)) == 0 ) {
	button = XP_DIALOG_FETCH_BUTTON;
    } else {
	button = 0;
    }

    /* call the application handler */
    ret = PR_FALSE;
    if ( handle->dialogInfo->handler != NULL ) {
	ret = (* handle->dialogInfo->handler)(handle, av, ac, button);
    }

loser:

    /* free arg vector */
    if ( av != NULL ) {
	PORT_Free(av);
    }

    if ( ( handle != NULL ) && ( ret == PR_FALSE ) && ( !handle->deleted ) ) {
	deleteWindowCBArg *delstate;
	
	/* set callback to destroy the window */
	delstate = (deleteWindowCBArg *)PORT_Alloc(sizeof(deleteWindowCBArg));
	if ( delstate ) {
	    delstate->window = (void *)handle->window;
	    delstate->arg = handle->cbarg;
	    delstate->deleteWinCallback = handle->deleteCallback;
	    delstate->freeArena = handle->arena;
	    (void)FE_SetTimeout(deleteWindow, (void *)delstate, 0);
	}
	
	handle->deleted = PR_TRUE;
    }
    
    return;
}

void
xp_InitChrome(void *proto_win, Chrome *chrome, int width, int height,
	      PRBool isModal)
{
    PORT_Memset(chrome, 0, sizeof(Chrome));
    chrome->type = MWContextDialog;
    chrome->w_hint = width;
    chrome->h_hint = height;

    chrome->is_modal = TRUE;
    
    /* Try to make the dialog fit on the screen, centered over the proto_win */
    if ( proto_win && chrome->w_hint && chrome->h_hint ) {
        int32 screenWidth = 0;
        int32 screenHeight = 0;
        
        Chrome protoChrome;
    
        PORT_Memset(&protoChrome, 0, sizeof(protoChrome));
        
        /* We want to shrink the dialog to fit on the screen if necessary.
         *
         * The inner dimensions of the dialog have to be specified in the
	 * Chrome. However, we want to OUTER dimensions to fit on the screen.
         * We don't know the difference between the inner and outer dimensions.
         * We could try to find that difference information from the
	 * proto_win's Chrome structure, but the proto_win probably has
	 * a completely different structure from our dialog windows. So we
	 * just guess...
         */
        FE_GetScreenSize ((MWContext *) proto_win, &screenWidth,
			  &screenHeight);
        
        if ( screenWidth && screenHeight ) {
	    /* arbitrary slop around dialog */
            if (chrome->w_hint > screenWidth - 20)
                chrome->w_hint = screenWidth - 20;
            if (chrome->h_hint > screenHeight - 30)
                chrome->h_hint = screenHeight - 30;
        }

        /* Now, try to find out where the proto_win is so we can center on it.
	 */
        FE_QueryChrome((MWContext *)proto_win, &protoChrome);
        
        /* Did we get anything useful? */
        if ( protoChrome.location_is_chrome && protoChrome.w_hint &&
	    protoChrome.h_hint ) {
            chrome->location_is_chrome = TRUE;
            chrome->l_hint = protoChrome.l_hint +
		(protoChrome.w_hint - chrome->w_hint) / 2;
            chrome->t_hint = protoChrome.t_hint +
		(protoChrome.h_hint - chrome->h_hint) / 2;
        }
        
        /* If we moved the dialog, make sure we didn't move it off the screen.
	 */
        if ( chrome->location_is_chrome && screenWidth && screenHeight ) {
            if ( chrome->l_hint < 0) {
                chrome->l_hint = 0;
	    }
            if ( chrome->t_hint < 0) {
                chrome->t_hint = 0;
	    }
            if ( chrome->l_hint > ( screenWidth - chrome->w_hint ) ) {
                chrome->l_hint = screenWidth - chrome->w_hint;
	    }
            if ( chrome->t_hint > ( screenHeight - chrome->h_hint ) ) {
                chrome->t_hint = screenHeight - chrome->h_hint;
	    }
        }
    }
    
    return;
}


XPDialogState *
XP_MakeHTMLDialog(void *proto_win, XPDialogInfo *dialogInfo, 
		  int titlenum, XPDialogStrings *strings, void *arg,
		  PRBool utf8CharSet)
{
    Chrome chrome;

    xp_InitChrome(proto_win, &chrome, dialogInfo->width,
		  dialogInfo->height, PR_TRUE);
    
    return(XP_MakeHTMLDialogWithChrome(proto_win, dialogInfo, titlenum, 
				       strings, &chrome, arg, utf8CharSet));
}

void
freeHTMLDialogStream(HTMLDialogStream *stream)
{
    if ( stream != NULL ) {
	if ( stream->stream != NULL ) {
	    PORT_Free(stream->stream);
	}
    
	if ( stream->url != NULL ) {
	    NET_FreeURLStruct(stream->url);
	}
    
	if ( stream->arena != NULL ) {
	    PORT_FreeArena(stream->arena, PR_FALSE);
	}
    }
    
    return;
}

HTMLDialogStream *
newHTMLDialogStream(void *cx)
{
    HTMLDialogStream *stream;
    PRArenaPool *arena;

    arena = PORT_NewArena(512);
    if ( arena == NULL ) {
	goto loser;
    }

    /* make an html dialog stream */
    stream = (HTMLDialogStream *)PORT_ArenaZAlloc(arena,
						  sizeof(HTMLDialogStream));
    if ( stream == NULL ) {
	goto loser;
    }

    stream->arena = arena;
    
    /* initialize the list of queued buffers */
    PR_INIT_CLIST(&stream->queuedBuffers);
    
    /* create a URL struct */
    stream->url = NET_CreateURLStruct("", NET_DONT_RELOAD);
    if ( stream->url == NULL ) {
	goto loser;
    }
    StrAllocCopy(stream->url->content_type, TEXT_HTML);
    
    /* make a netlib stream to display in the window */
    stream->stream = NET_StreamBuilder(FO_PRESENT, stream->url, cx);
    if ( stream->stream == NULL ) {
	goto loser;
    }
    
    return(stream);
    
loser:

    freeHTMLDialogStream(stream);
    
    return(NULL);
}

void
emptyQueues(void *arg)
{
    HTMLDialogStream *stream;
    bufferNode *node;
    unsigned int cnt;
    int ret;
    
    stream = (HTMLDialogStream *)arg;

    while ( ! PR_CLIST_IS_EMPTY(&stream->queuedBuffers) ) {
	/* get the head node */
	node = (bufferNode *)PR_LIST_HEAD(&stream->queuedBuffers);

	cnt = (*(stream->stream)->is_write_ready)(stream->stream);

	if ( cnt < node->len ) {
	    /* layout hasn't unclogged yet */
	    break;
	}
	
	ret = (*(stream->stream)->put_block)(stream->stream,
					     node->data, node->len);
	if ( ret < 0 ) {
	    goto loser;
	}
	
	/* remove it from the list */
	PR_REMOVE_LINK(&node->link);
    }
    
    if ( PR_CLIST_IS_EMPTY(&stream->queuedBuffers) ) {
	(*stream->stream->complete) (stream->stream);
	freeHTMLDialogStream(stream);
    } else {
	FE_SetTimeout(emptyQueues, (void *)stream, 100);
    }
    
    return;

loser:
    freeHTMLDialogStream(stream);
    return;
}

void *
xp_MakeHTMLDialogWindow(void *proto_win, Chrome *chrome)
{
    MWContext *cx;
    
    /* make the window */
    cx = FE_MakeNewWindow((MWContext *)proto_win, NULL, NULL, chrome);
    if ( cx == NULL ) {
	goto done;
    }

#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
    LM_ForceJSEnabled(cx);
#endif /* MOZ_NGLAYOUT */

    /* XXX - get rid of session history */
    SHIST_EndSession(cx);
    PORT_Memset((void *)&cx->hist, 0, sizeof(cx->hist));

done:
    return(cx);
}

SECStatus
xp_DrawHTMLDialog(void *cx, XPDialogInfo *dialogInfo,
		  int titlenum, XPDialogStrings *strings,
		  void *state, PRBool utf8CharSet)
{
    HTMLDialogStream *stream = NULL;
    SECStatus rv;
    char buf[50];
    XPDialogStrings *dlgstrings;
    int buttontag;
    
    /* make the stream */
    stream = newHTMLDialogStream(cx);
    if ( stream == NULL ) {
	goto loser;
    }
    
    if ( utf8CharSet ) {
	dlgstrings =
	    XP_GetDialogStrings(XP_DIALOG_JS_HEADER_STRINGS_WITH_UTF8_CHARSET);
    } else {
	dlgstrings = XP_GetDialogStrings(XP_DIALOG_JS_HEADER_STRINGS);
    }
    
    if ( dlgstrings == NULL ) {
	goto loser;
    }

    /* put the title in header */
    XP_CopyDialogString(dlgstrings, 0, XP_GetString(titlenum));

    /* send html header stuff */
    rv = XP_PutDialogStringsToStream(stream, dlgstrings, PR_FALSE);
    XP_FreeDialogStrings(dlgstrings);
    
    if ( rv != SECSuccess ) {
	goto loser;
    }

    dlgstrings = XP_GetDialogStrings(XP_DIALOG_HEADER_STRINGS);
    if ( dlgstrings == NULL ) {
	goto loser;
    }

    /* put handle in header */
#if defined(__sun) && !defined(SVR4) /* sun 4.1.3 */
    sprintf(buf, "%lu", state);
#else
    sprintf(buf, "%p", state);
#endif
    XP_SetDialogString(dlgstrings, 0, buf);
    
    /* send html header stuff */
    rv = XP_PutDialogStringsToStream(stream, dlgstrings, PR_TRUE);
    XP_FreeDialogStrings(dlgstrings);
    
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* send caller's message */
    rv = XP_PutDialogStringsToStream(stream, strings, PR_TRUE);

    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* send trailing info */
    rv = XP_PutDialogStringsTagToStream(stream, XP_DIALOG_FOOTER_STRINGS,
					PR_TRUE);

    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* send button strings */
    switch(dialogInfo->buttonFlags) {
      case XP_DIALOG_CANCEL_BUTTON:
	buttontag = XP_DIALOG_CANCEL_BUTTON_STRINGS;
	break;
      case XP_DIALOG_CONTINUE_BUTTON:
	buttontag = XP_DIALOG_CONTINUE_BUTTON_STRINGS;
	break;
      case XP_DIALOG_OK_BUTTON:
	buttontag = XP_DIALOG_OK_BUTTON_STRINGS;
	break;
      case XP_DIALOG_CANCEL_BUTTON | XP_DIALOG_OK_BUTTON:
	buttontag = XP_DIALOG_CANCEL_OK_BUTTON_STRINGS;
	break;
      case XP_DIALOG_CANCEL_BUTTON | XP_DIALOG_CONTINUE_BUTTON:
	buttontag = XP_DIALOG_CANCEL_CONTINUE_BUTTON_STRINGS;
	break;
      case XP_DIALOG_CANCEL_BUTTON | XP_DIALOG_OK_BUTTON |
	  XP_DIALOG_MOREINFO_BUTTON:
	buttontag = XP_DIALOG_CANCEL_OK_MOREINFO_BUTTON_STRINGS;
	break;
      case XP_DIALOG_CANCEL_BUTTON | XP_DIALOG_FETCH_BUTTON:
	buttontag = XP_DIALOG_FETCH_CANCEL_BUTTON_STRINGS;
	break;
      default:
	buttontag = XP_DIALOG_NULL_STRINGS;
	break;
    }

    rv = XP_PutDialogStringsTagToStream(stream, XP_DIALOG_JS_MIDDLE_STRINGS,
					PR_FALSE);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    rv = XP_PutDialogStringsTagToStream(stream, buttontag, PR_TRUE);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    rv = XP_PutDialogStringsTagToStream(stream, XP_DIALOG_JS_FOOTER_STRINGS,
					PR_FALSE);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    if ( PR_CLIST_IS_EMPTY(&stream->queuedBuffers) ) {
	/* complete the stream */
	(*stream->stream->complete) (stream->stream);

	freeHTMLDialogStream(stream);
	return(SECSuccess);
    } else {
	FE_SetTimeout(emptyQueues, (void *)stream, 0);
	return(SECSuccess);
    }
    
loser:    
    /* XXX free URL and CX ???*/

    /* abort the stream */
    if ( stream != NULL ) {
	if ( stream->stream != NULL ) {
	    (*stream->stream->abort)(stream->stream, rv);
	}
    }

    freeHTMLDialogStream(stream);
    
    return(SECFailure);
}

XPDialogState *
XP_MakeHTMLDialogWithChrome(void *proto_win, XPDialogInfo *dialogInfo,
			    int titlenum, XPDialogStrings *strings,
			    Chrome *chrome, void *arg, PRBool utf8CharSet)
{
    void *cx;
    SECStatus rv;
    XPDialogState *state;
    PRArenaPool *arena = NULL;
    
    arena = PORT_NewArena(1024);
    
    if ( arena == NULL ) {
	return(NULL);
    }

    /* allocate the state structure */
    state = (XPDialogState *)PORT_ArenaAlloc(arena, sizeof(XPDialogState));
    if ( state == NULL ) {
	goto loser;
    }
    
    state->deleted = PR_FALSE;
    state->arena = arena;
    state->dialogInfo = dialogInfo;
    state->arg = arg;
    state->deleteCallback = NULL;
    state->cbarg = NULL;
    state->proto_win = proto_win;

    state->window = xp_MakeHTMLDialogWindow(proto_win, chrome);
    if ( state->window == NULL ) {
	goto loser;
    }
    
    rv = xp_DrawHTMLDialog(state->window, dialogInfo, titlenum, strings,
			   (void *)state, utf8CharSet);
    
    if ( rv != SECSuccess ) {
	goto loser;
    }

    return(state);
    
loser:
    PORT_FreeArena(arena, PR_FALSE);
    return(NULL);
}

void *
xp_MakeHTMLDialogPass1(void *proto_win, XPDialogInfo *dialogInfo)
{
    Chrome chrome;

    xp_InitChrome(proto_win, &chrome, dialogInfo->width,
		  dialogInfo->height, PR_TRUE);

    return(xp_MakeHTMLDialogWindow(proto_win, &chrome));
}

XPDialogState *
xp_MakeHTMLDialogPass2(void *proto_win, void *cx, XPDialogInfo *dialogInfo,
		       int titlenum, XPDialogStrings *strings,
		       void *arg, PRBool utf8CharSet)
{
    SECStatus rv;
    XPDialogState *state;
    PRArenaPool *arena = NULL;
    
    arena = PORT_NewArena(1024);
    
    if ( arena == NULL ) {
	return(NULL);
    }

    /* allocate the state structure */
    state = (XPDialogState *)PORT_ArenaAlloc(arena, sizeof(XPDialogState));
    if ( state == NULL ) {
	goto loser;
    }
    
    state->deleted = PR_FALSE;
    state->arena = arena;
    state->dialogInfo = dialogInfo;
    state->arg = arg;
    state->deleteCallback = NULL;
    state->cbarg = NULL;
    state->proto_win = proto_win;

    state->window = cx;
    
    rv = xp_DrawHTMLDialog(state->window, dialogInfo, titlenum, strings,
			   (void *)state, utf8CharSet);
    
    if ( rv != SECSuccess ) {
	goto loser;
    }

    return(state);
    
loser:
    PORT_FreeArena(arena, PR_FALSE);
    return(NULL);
}

int
XP_RedrawRawHTMLDialog(XPDialogState *state,
		       XPDialogStrings *strings,
		       int handlestring)
{
    HTMLDialogStream *stream = NULL;
    SECStatus rv;
    char buf[50];

    stream = newHTMLDialogStream(state->window);
    if ( stream == NULL ) {
	goto loser;
    }
    
    /* put handle in header */
#if defined(__sun) && !defined(SVR4) /* sun 4.1.3 */
    sprintf(buf, "%lu", state);
#else
    sprintf(buf, "%p", state);
#endif
    XP_SetDialogString(strings, handlestring, buf);
    
    /* send caller's message */
    rv = XP_PutDialogStringsToStream(stream, strings, PR_FALSE);

    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* complete the stream */
    if ( PR_CLIST_IS_EMPTY(&stream->queuedBuffers) ) {
	/* complete the stream */
	(*stream->stream->complete) (stream->stream);

	freeHTMLDialogStream(stream);
	return((int)SECSuccess);
    } else {
	FE_SetTimeout(emptyQueues, (void *)stream, 0);
	return((int)SECSuccess);
    }

loser:    
    /* XXX free URL and CX ???*/

    /* abort the stream */
    if ( stream != NULL ) {
	if ( stream->stream != NULL ) {
	    (*stream->stream->abort)(stream->stream, rv);
	}
    }

    freeHTMLDialogStream(stream);

    return((int)SECFailure);
}

/*	***** What a hack!!! Make the security advisor modeless on the Mac. ***** */
#ifdef XP_MAC
extern int XP_SECURITY_ADVISOR_TITLE_STRING;
#endif
XPDialogState *
XP_MakeRawHTMLDialog(void *proto_win, XPDialogInfo *dialogInfo,
		     int titlenum, XPDialogStrings *strings,
		     int handlestring, void *arg)
{
    MWContext *cx = NULL;
    SECStatus rv;
    XPDialogState *state;
    PRArenaPool *arena = NULL;
    Chrome chrome;
    PORT_Memset(&chrome, 0, sizeof(chrome));
    chrome.type = MWContextDialog;
    chrome.w_hint = dialogInfo->width;
    chrome.h_hint = dialogInfo->height;
/*	***** What a hack!!! Make the security advisor modeless on the Mac. ***** */
#ifdef XP_MAC
    chrome.is_modal = (titlenum == XP_SECURITY_ADVISOR_TITLE_STRING) ? FALSE : TRUE;
#else
    chrome.is_modal = TRUE;
#endif
    
    arena = PORT_NewArena(1024);
    
    if ( arena == NULL ) {
	return(NULL);
    }

    /* allocate the state structure */
    state = (XPDialogState *)PORT_ArenaAlloc(arena, sizeof(XPDialogState));
    if ( state == NULL ) {
	goto loser;
    }
    
    state->deleted = PR_FALSE;
    state->arena = arena;
    state->dialogInfo = dialogInfo;
    state->arg = arg;
    state->deleteCallback = NULL;
    state->cbarg = NULL;
    
    /* make the window */
/*	***** What a hack!!! Make the security advisor modeless on the Mac. ***** */
#ifdef XP_MAC
	if (titlenum == XP_SECURITY_ADVISOR_TITLE_STRING)
    	cx = FE_MakeNewWindow((MWContext *)proto_win, NULL, XP_GetString(titlenum), &chrome);
    else
#else
    cx = FE_MakeNewWindow((MWContext *)proto_win, NULL, NULL, &chrome);
#endif
    if ( cx == NULL ) {
	goto loser;
    }
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
    LM_ForceJSEnabled(cx);
#endif /* MOZ_NGLAYOUT */
    
    state->window = (void *)cx;
    state->proto_win = proto_win;

    /* XXX - get rid of session history */
    SHIST_EndSession(cx);
    PORT_Memset((void *)&cx->hist, 0, sizeof(cx->hist));

    rv = (SECStatus)XP_RedrawRawHTMLDialog(state, strings, handlestring);

    if ( rv != SECSuccess ) {
	goto loser;
    }
    return(state);

loser:
    /* free the arena */
    if ( arena != NULL ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(NULL);
}

static void
displayPanelCB(void *arg)
{
    XPPanelState *state;
    HTMLDialogStream *stream;
    XPDialogStrings *contentstrings;
    SECStatus rv;
    char buf[50];
    XPDialogStrings *dlgstrings;
    int buttontag;
    
    state = (XPPanelState *)arg;
    
    stream = newHTMLDialogStream(state->window);
    if (stream == NULL) {
	goto loser;
    }

    dlgstrings = XP_GetDialogStrings(XP_DIALOG_JS_HEADER_STRINGS);
    
    if ( dlgstrings == NULL ) {
	goto loser;
    }

    /* put the title in header */
    XP_CopyDialogString(dlgstrings, 0, XP_GetString(state->titlenum));

    /* send html header stuff */
    rv = XP_PutDialogStringsToStream(stream, dlgstrings, PR_FALSE);
    XP_FreeDialogStrings(dlgstrings);
    
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* get header strings */
    dlgstrings = XP_GetDialogStrings(XP_PANEL_HEADER_STRINGS);
    if ( dlgstrings == NULL ) {
	goto loser;
    }

    /* put handle in header */
#if defined(__sun) && !defined(SVR4) /* sun 4.1.3 */
    sprintf(buf, "%lu", state);
#else
    sprintf(buf, "%p", state);
#endif
    XP_SetDialogString(dlgstrings, 0, buf);
    
    /* send html header stuff */
    rv = XP_PutDialogStringsToStream(stream, dlgstrings, PR_TRUE);

    /* free the strings */
    XP_FreeDialogStrings(dlgstrings);
    
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* send panel message */
    contentstrings = (* state->panels[state->curPanel].content)(state);
    if ( contentstrings == NULL ) {
	goto loser;
    }
    
    state->curStrings = contentstrings;
    
    rv = XP_PutDialogStringsToStream(stream, contentstrings, PR_TRUE);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    XP_FreeDialogStrings(contentstrings);

    /* send html middle stuff */
    rv = XP_PutDialogStringsTagToStream(stream, XP_DIALOG_FOOTER_STRINGS,
					PR_TRUE);
    if ( rv != SECSuccess ) {
	goto loser;
    }
    if ( state->panels[state->curPanel].flags & XP_PANEL_FLAG_ONLY ) {
	/* if its the only panel */
	buttontag = XP_PANEL_ONLY_BUTTON_STRINGS;
    } else if ( ( state->curPanel == ( state->panelCount - 1 ) ) ||
	( state->panels[state->curPanel].flags & XP_PANEL_FLAG_FINAL ) ) {
	/* if its the last panel or has the final flag set */
	buttontag = XP_PANEL_LAST_BUTTON_STRINGS;
    } else if ( ( state->curPanel == 0 ) ||
	( state->panels[state->curPanel].flags & XP_PANEL_FLAG_FIRST ) ) {
	buttontag = XP_PANEL_FIRST_BUTTON_STRINGS;
    } else {
	buttontag = XP_PANEL_MIDDLE_BUTTON_STRINGS;
    }
    
    rv = XP_PutDialogStringsTagToStream(stream, XP_DIALOG_JS_MIDDLE_STRINGS,
					PR_FALSE);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* send button strings */
    rv = XP_PutDialogStringsTagToStream(stream, buttontag, PR_TRUE);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    rv = XP_PutDialogStringsTagToStream(stream, XP_DIALOG_JS_FOOTER_STRINGS,
					PR_FALSE);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    /* complete the stream */
    if ( PR_CLIST_IS_EMPTY(&stream->queuedBuffers) ) {
	/* complete the stream */
	(*stream->stream->complete) (stream->stream);

	freeHTMLDialogStream(stream);
	return;
    } else {
	FE_SetTimeout(emptyQueues, (void *)stream, 0);
	return;
    }
    return;

loser:    
    /* abort the stream */
    if ( stream != NULL ) {
	if ( stream->stream != NULL ) {
	    (*stream->stream->abort)(stream->stream, rv);
	}
    }

    freeHTMLDialogStream(stream);

    return;
}

static void
displayPanel(XPPanelState *state)
{
    (void)FE_SetTimeout(displayPanelCB, (void *)state, 0);
}

void
XP_MakeHTMLPanel(void *proto_win, XPPanelInfo *panelInfo,
		 int titlenum, void *arg)
{
    PRArenaPool *arena;
    XPPanelState *state = NULL;
    Chrome chrome;
    MWContext *cx;
    
    arena = PORT_NewArena(1024);
    if ( arena == NULL ) {
	return;
    }

    state = (XPPanelState *)PORT_ArenaAlloc(arena, sizeof(XPPanelState));
    if ( state == NULL ) {
	return;
    }
    
    state->deleted = PR_FALSE;
    state->arena = arena;
    state->panels = panelInfo->desc;
    state->panelCount = panelInfo->panelCount;
    state->curPanel = 0;
    state->arg = arg;
    state->finish = panelInfo->finishfunc;
    state->info = panelInfo;
    state->titlenum = titlenum;
    state->deleteCallback = NULL;
    state->cbarg = NULL;
    
    /* make the window */
    PORT_Memset(&chrome, 0, sizeof(chrome));
    chrome.type = MWContextDialog;
    chrome.w_hint = panelInfo->width;
    chrome.h_hint = panelInfo->height;
    chrome.is_modal = TRUE;
    cx = FE_MakeNewWindow((MWContext *)proto_win, NULL, NULL, &chrome);
    state->window = (void *)cx;
    
    if ( state->window == NULL ) {
	PORT_FreeArena(arena, PR_FALSE);
	return;
    }
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
    LM_ForceJSEnabled(cx);
#endif /* MOZ_NGLAYOUT */

    /* XXX - get rid of session history */
    SHIST_EndSession(cx);
    PORT_Memset((void *)&cx->hist, 0, sizeof(cx->hist));
    
    /* display the first panel */
    displayPanel(state);
    
    return;
}

void
XP_HandleHTMLPanel(URL_Struct *url)
{
    char **av = NULL;
    int ac;
    char *handleString;
    char *buttonString;
    XPPanelState *state = NULL;
    unsigned int button;
    int nextpanel;
    
    /* collect post data */
    av = cgi_ConvertStringToArgVec(url->post_data, url->post_data_size, &ac);
    if ( av == NULL ) {
	goto loser;
    }
    
    /* get the handle */
    handleString = XP_FindValueInArgs("handle", av, ac);
    if ( handleString == NULL ) {
	goto loser;
    }

    /* get the button value */
    buttonString = XP_FindValueInArgs("button", av, ac);
    if ( buttonString == NULL ) {
	goto loser;
    }

    /* extract a handle pointer from the string */
    state = NULL;
#if defined(__sun) && !defined(SVR4) /* sun 4.1.3 */
    sscanf(handleString, "%lu", &state);
#else
    sscanf(handleString, "%p", &state);
#endif
    if ( state == NULL ) {
	goto loser;
    }

    if ( state->deleted ) {
	goto loser;
    }
    
    /* figure out which button was pressed */
    if ( PORT_Strcasecmp(buttonString,
		       XP_GetString(XP_SEC_NEXT_KLUDGE)) == 0 ) {
	button = XP_DIALOG_NEXT_BUTTON;
    } else if ( PORT_Strcasecmp(buttonString,
			      XP_GetString(XP_SEC_CANCEL)) == 0 ) {
	button = XP_DIALOG_CANCEL_BUTTON;
    } else if ( PORT_Strcasecmp(buttonString,
			      XP_GetString(XP_SEC_BACK_KLUDGE)) == 0 ) {
	button = XP_DIALOG_BACK_BUTTON;
    } else if ( PORT_Strcasecmp(buttonString,
			      XP_GetString(XP_SEC_FINISHED)) == 0 ) {
	button = XP_DIALOG_FINISHED_BUTTON;
    } else if ( PORT_Strcasecmp(buttonString,
			      XP_GetString(XP_SEC_MOREINFO)) == 0 ) {
	button = XP_DIALOG_MOREINFO_BUTTON;
    } else {
	button = 0;
    }

    /* call the application handler */
    if ( state->panels[state->curPanel].handler != NULL ) {
	nextpanel = (* state->panels[state->curPanel].handler)(state, av, ac,
							       button);
    } else {
	nextpanel = 0;
    }

    if ( button == XP_DIALOG_CANCEL_BUTTON ) {
	if ( state->finish ) {
	    (* state->finish)(state, PR_TRUE);
	}
	goto done;
    }
    
    if ( nextpanel != 0 ) {
	state->curPanel = nextpanel - 1;
    } else {
	switch ( button ) {
	  case XP_DIALOG_BACK_BUTTON:
	    PORT_Assert(state->curPanel > 0);
	    state->curPanel = state->curPanel - 1;
	    break;
	  case XP_DIALOG_NEXT_BUTTON:
	    PORT_Assert(state->curPanel < ( state->panelCount - 1 ) );
	    state->curPanel = state->curPanel + 1;
	    break;
	  case XP_DIALOG_FINISHED_BUTTON:
	    if ( state->finish ) {
		(* state->finish)(state, PR_FALSE);
	    }
	    goto done;
	}
    }
    
    displayPanel(state);

    /* free arg vector */
    PORT_Free(av);
    return;
    
loser:
done:

    /* free arg vector */
    if ( av != NULL ) {
	PORT_Free(av);
    }

    if ( ( state != NULL ) && ( !state->deleted ) ) {
	/* destroy the window */
	deleteWindowCBArg *delstate;
	
	/* set callback to destroy the window */
	delstate = (deleteWindowCBArg *)PORT_Alloc(sizeof(deleteWindowCBArg));
	if ( delstate ) {
	    delstate->window = (void *)state->window;
	    delstate->arg = state->cbarg;
	    delstate->deleteWinCallback = state->deleteCallback;
	    delstate->freeArena = state->arena;
	    (void)FE_SetTimeout(deleteWindow, (void *)delstate, 0);
	}

	state->deleted = PR_TRUE;
    }

    return;
}

/*
 * fetch a string from the dialog strings database
 */
XPDialogStrings *
XP_GetDialogStrings(int stringnum)
{
    XPDialogStrings *header = NULL;
    PRArenaPool *arena = NULL;
    char *dst, *src;
    int n, size, len, done = 0;
    
    /* get a new arena */
    arena = PORT_NewArena(XP_STRINGS_CHUNKSIZE);
    if ( arena == NULL ) {
	return(NULL);
    }

    /* allocate the header structure */
    header = (XPDialogStrings *)PORT_ArenaAlloc(arena, sizeof(XPDialogStrings));
    if ( header == NULL ) {
	goto loser;
    }

    /* init the header */
    header->arena = arena;
    header->basestringnum = stringnum;

    src = XP_GetString(stringnum);
    len = PORT_Strlen(src);
    size = len + 1;
    dst = header->contents =
	(char *)PORT_ArenaAlloc(arena, sizeof(char) * size);
    if (dst == NULL)
	goto loser;
    
    while (!done) {		/* Concatenate pieces to form message */
	PORT_Memcpy(dst, src, len+1);
	done = 1;
	if (XP_STRSTR(src, "%-cont-%")) { /* Continuation */
	    src = XP_GetString(++stringnum);
	    len = PORT_Strlen(src);
	    header->contents =
		(char *)PORT_ArenaGrow(arena,
				     header->contents, size, size + len);
	    if (header->contents == NULL)
		goto loser;
	    dst = header->contents + size - 1;
	    size += len;
	    done = 0;
	}
    }

    /* At this point we should have the complete message in
       header->contents, including like %-cont-%, which will be
       ignored later. */

    /* Count the arguments in the message */
    header->nargs = -1;		/* Support %0% as lowest token */
    src = header->contents;
    while ((src = PORT_Strchr(src, '%'))) {
	src++;
	n = (int)XP_STRTOUL(src, &dst, 10);
	if (dst == src)	{	/* Integer not found... */
	    src = PORT_Strchr(src, '%') + 1; /* so skip this %..% */
	    PORT_Assert(NULL != src-1); /* Unclosed %..% ? */
	    continue;
	}
	
	if (header->nargs < n)
	    header->nargs = n;
	src = dst + 1;
    }

    if (++(header->nargs) > 0)  /* Allocate space for arguments */
	header->args = (char **)PORT_ArenaZAlloc(arena, sizeof(char *) *
					      header->nargs);

    return(header);
    
loser:
    PORT_FreeArena(arena, PR_FALSE);
    return(NULL);
}

/*
 * Set a dialog string to the given string.
 * The source string must be writable(not a static string), and will
 *  not be copied, so it is the responsibility of the caller to make
 *  sure it is freed after use.
 */
void
XP_SetDialogString(XPDialogStrings *strings, int argNum, char *string)
{
    /* make sure we are doing it right */
    PORT_Assert(argNum < strings->nargs);
    PORT_Assert(argNum >= 0);
    PORT_Assert(strings->args[argNum] == NULL);
    
    /* set the string */
    strings->args[argNum] = string;
    
    return;
}

/*
 * Copy a string to the dialog string
 */
void
XP_CopyDialogString(XPDialogStrings *strings, int argNum, const char *string)
{
    int len;
    
    /* make sure we are doing it right */
    PORT_Assert(argNum < strings->nargs);
    PORT_Assert(argNum >= 0);
    PORT_Assert(strings->args[argNum] == NULL);
    
    /* copy the string */
    len = PORT_Strlen(string) + 1;
    strings->args[argNum] = (char *)PORT_ArenaAlloc(strings->arena, len);
    if ( strings->args[argNum] != NULL ) {
	PORT_Memcpy(strings->args[argNum], string, len);
    }
    
    return;
}

/*
 * free the dialog strings
 */
void
XP_FreeDialogStrings(XPDialogStrings *strings)
{
    PORT_FreeArena(strings->arena, PR_FALSE);
    
    return;
}

static XPDialogInfo alertDialog = {
    XP_DIALOG_OK_BUTTON,
    NULL,
    600,
    224
};

void
XP_MakeHTMLAlert(void *proto_win, char *string)
{
    XPDialogStrings *strings;
    
    /* get empty strings */
    strings = XP_GetDialogStrings(XP_EMPTY_STRINGS);
    if ( strings == NULL ) {
	return;
    }
    
    XP_CopyDialogString(strings, 0, string);

    /* put up the dialog */
    XP_MakeHTMLDialog(proto_win, &alertDialog, XP_ALERT_TITLE_STRING,
		      strings, NULL, PR_FALSE);

    return;
}

/*
 * destroy an HTML dialog window that has not had anything written to it yet
 */
void
XP_DestroyHTMLDialogWindow(void *window)
{
    deleteWindowCBArg *delstate;
	
    /* set callback to destroy the window */
    delstate = (deleteWindowCBArg *)PORT_Alloc(sizeof(deleteWindowCBArg));
    if ( delstate ) {
	delstate->window = window;
	delstate->arg = NULL;
	delstate->deleteWinCallback = NULL;
	delstate->freeArena = NULL;
	(void)FE_SetTimeout(deleteWindow, (void *)delstate, 0);
    }
    return;
}

