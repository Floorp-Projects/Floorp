/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*   jsmsg.c -- javascript reflection of mail messages for filters.
 */

#include "msg.h"
#include "msgmast.h"
#include "msgprefs.h"
#include "prsembst.h"
#include "pmsgsrch.h"
#include "pmsgfilt.h"
#include "maildb.h"
#include "newsdb.h"
#include "mailhdr.h"
#include "newshdr.h"

#include "prefapi.h"

#include "libmime.h" // for the MimeHeaders stuff.

#include "jsapi.h"
#include "xp_core.h"
#include "xp_mcom.h"
#include "msgstrob.h"

#include "ds.h"		/* XXX required by htmldlgs.h */
#include "htmldlgs.h"

#ifdef DEBUG
extern "C" void js_traceon(JSContext*);
#endif

#if defined(XP_MAC) && defined (__MWERKS__)
#pragma require_prototypes off
#endif

static JSObject *filter_obj = NULL;
static JSContext *filter_context = NULL;

static JSBool error_reporter_installed = JS_FALSE;
static JSErrorReporter previous_error_reporter;
static int error_count = 0;

/* tells us when we should recompile the file. */
static JSBool need_compile = JS_TRUE;

/* Unfortunately, news db's don't waste space store the CC fields or msg priority */

/* This is the private instance data associated with a mail message */
struct JSMailMsgData {
	JSContext *js_context;
	JSObject *js_object;
	MSG_Filter *filter;
	
	MailMessageHdr *msgHdr;
	MailDB *mailDB;
	MimeHeaders *mime_headers;
	ParseNewMailState *state;
	XP_Bool msgMoved;
	XP_Bool actionPerformed;
};

/* This is the private instance data associated with a news message */
struct JSNewsMsgData {
	JSContext *js_context;
	JSObject *js_object;
	MSG_Filter *filter;

	DBMessageHdr *msgHdr;
	NewsGroupDB *newsDB;
	XP_Bool actionPerformed;
};

/* The properties of a message that we reflect */
enum msg_slot {
	MSG_SUBJECT = -1,
	MSG_PRI = -2,
	MSG_SENDER = -3,
	MSG_RECIPIENT = -4,
	MSG_CC = -5,
	MSG_READ = -6,
	MSG_FOLDER = -7, /* mail only */
	MSG_GROUP = -8   /* news only */
};

static JSPropertySpec mail_msg_props[] = {
	{ "subject",	MSG_SUBJECT, 	JSPROP_ENUMERATE|JSPROP_READONLY },
	{ "priority",	MSG_PRI,		JSPROP_ENUMERATE },
	{ "sender",		MSG_SENDER,		JSPROP_ENUMERATE|JSPROP_READONLY },
	{ "recipient",	MSG_RECIPIENT,	JSPROP_ENUMERATE|JSPROP_READONLY },
	{ "cc",			MSG_CC,			JSPROP_ENUMERATE|JSPROP_READONLY },
	{ "read",		MSG_READ,		JSPROP_ENUMERATE },
	{ "folder",		MSG_FOLDER,		JSPROP_ENUMERATE },
	{ 0 }
};

static JSPropertySpec news_msg_props[] = {
	{ "subject",	MSG_SUBJECT, 	JSPROP_ENUMERATE|JSPROP_READONLY },
#if news_dbs_waste_space_for_convience_of_js_filters
	{ "priority",	MSG_PRI,		JSPROP_ENUMERATE },
#endif
	{ "sender",		MSG_SENDER,		JSPROP_ENUMERATE|JSPROP_READONLY },
#if news_dbs_waste_space_for_convience_of_js_filters
	{ "recipient",	MSG_RECIPIENT,	JSPROP_ENUMERATE|JSPROP_READONLY },
	{ "cc",			MSG_CC,			JSPROP_ENUMERATE|JSPROP_READONLY },
#endif
	{ "read",		MSG_READ,		JSPROP_ENUMERATE },
	{ "group",		MSG_GROUP,		JSPROP_ENUMERATE|JSPROP_READONLY },
	{ 0 }
};

static char*
msg_convertToMailboxUrl(const char *full_path_to_folder)
{
	char *folder_path;

	PREF_CopyCharPref("mail.directory", &folder_path);

	if (folder_path && !strncmp(full_path_to_folder, folder_path, strlen(folder_path)))
		{
			char *mailbox_path;

			full_path_to_folder += strlen(folder_path);

			XP_FREE(folder_path);

			if (full_path_to_folder[0] == '/')
				full_path_to_folder++;

			mailbox_path = PR_smprintf("mailbox:%s", full_path_to_folder);

			return mailbox_path;
		}
	else
		{
			XP_FREEIF(folder_path);

			return XP_STRDUP(full_path_to_folder);
		}
}

static char *
msg_convertFromMailboxUrl(const char *mailbox_url)
{
	char *folder_path;

	PREF_CopyCharPref("mail.directory", &folder_path);

	if (folder_path && !strncmp(mailbox_url, "mailbox:", 8 /* strlen("mailbox:")*/))
		{
			mailbox_url += 8; /* strlen(mailbox:) */

			if (mailbox_url[0] == '/')
				{
					return XP_STRDUP(mailbox_url);
				}
			else
				{
					char *result;

					result = PR_smprintf("%s%s%s",
										 folder_path,
										 folder_path[strlen(folder_path) - 1] == '/' ? "" : "/",
										 mailbox_url);

					return result;
				}
		}
	else
		{
			XP_FREEIF(folder_path);

			return XP_STRDUP(mailbox_url);
		}
}

/* this function is listed as a friend of ParseNewMailState.  The reason
   for this is that we have to call a protected method of that class, 
   MoveIncorporatedMessage.  This is much better in my opinion than the alternative,
   which is for all this code to live in ParseNewMailState. */
void JSMailFilter_MoveMessage(ParseNewMailState *state, 
							  MailMessageHdr *msgHdr, 
							  MailDB *mailDB, 
							  const char *folder, 
							  MSG_Filter *filter,
							  XP_Bool *pMoved)
{
	XP_Bool msgMoved = FALSE;

	if (XP_FILENAMECMP(mailDB->GetFolderName(), (char *) folder))
		{
			XP_Trace ("+Moving message from %s to %s\n", mailDB->GetFolderName(), folder);

			MsgERR err = state->MoveIncorporatedMessage(msgHdr,
														mailDB,
														msg_convertFromMailboxUrl(folder),
														filter);
			if (err == eSUCCESS)
				msgMoved = TRUE;
		}

	if (pMoved)
		*pMoved = msgMoved;
}

/* these should probably be exposed in libmime.h, as they are very useful, at least
   for what I use them for. */
extern "C" int MimeHeaders_parse_line (const char *buffer, int32 size, MimeHeaders *hdrs);
extern "C" MimeHeaders *MimeHeaders_new (void);
extern "C" void MimeHeaders_free(MimeHeaders*);

static JSString *
msg_getHeader(JSMailMsgData *data, char *header_name)
{
	char *header_value;

	if (!data->mime_headers)
		return NULL;
	
	header_value = MimeHeaders_get(data->mime_headers, header_name, TRUE, FALSE);
	
	if (!header_value) header_value = "";
	
	return JS_NewStringCopyZ(data->js_context, (char*)header_value);
}

static JSString *
mailMsg_initializeHeaders(JSMailMsgData *data)
{
	ParseNewMailState *state;
	state = data->state;

	if (state->GetMsgState()->m_headers_size)
		{
			int32 size = state->GetMsgState()->m_headers_fp;
			char *headers = (char*)XP_ALLOC(size + 1);
			char *buf = headers;
			char *buf_end = buf + size;
			JSString *result;

			if (!headers)
				return NULL;

			memcpy(buf, state->GetMsgState()->m_headers, size);
			buf[size] = 0;

			data->mime_headers = MimeHeaders_new();
			if (!data->mime_headers)
				return NULL;

			while (buf < buf_end)
				{
					int len = strlen(buf);
					/*
					** turns out that prsembst takes out the \n's at the ends
					** of the lines, and puts \0's there instead.  So, if we want
					** to build up the mime header stuff, we have to save off the
					** length, put a \n in where the \0 was, and parse it.
					** Why can't we all just get along, and base our code on 
					** something that does the job *quite* well, and that already exists?
					*/
					if (len != 0) // why are there 0 length header lines?
						{
							int result;
							buf[len] = '\n';
							
							result = MimeHeaders_parse_line(buf,
															len + 1,
															data->mime_headers);
							
							if (result < 0)
								break;
						}
					
					buf += len + 1;
				}

			/*
			** after the above loop, headers contains
			** the \n separated list of headers.
			*/
			printf ("Headers are:\n%s", headers);
			result = JS_NewStringCopyZ(data->js_context, headers);
			XP_FREE(headers);
			return result;
		}
	else
		{
			return NULL;
		}
}

JSBool PR_CALLBACK
mailMsg_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	JSMailMsgData *data;
	enum msg_slot msg_slot;
    JSString * str;
    jsint slot;
	MailMessageHdr *msgHdr;
	MailDB *mailDB;

	data = (JSMailMsgData*)JS_GetPrivate(cx, obj);
	if (!data)
		return JS_TRUE;

    if (!JSVAL_IS_INT(id))
		return JS_TRUE;

    slot = JSVAL_TO_INT(id);

	msgHdr = data->msgHdr;
	mailDB = data->mailDB;
    if (!msgHdr || !mailDB)
		return JS_TRUE;

	msg_slot = (enum msg_slot)slot;

	switch (msg_slot) {
	case MSG_SUBJECT:
		{
			XPStringObj subject;

			msgHdr->GetSubject(subject, TRUE, mailDB->GetDB());
			
			str = JS_NewStringCopyZ(cx, (const char*)subject);
			if (!str)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(str);
			break;
		}
	case MSG_PRI:
		{
			char priority_buf[100];

			MSG_GetUntranslatedPriorityName(msgHdr->GetPriority(), priority_buf, 100);

			str = JS_NewStringCopyZ(cx, priority_buf);
			if (!str)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(str);
			break;
		}
	case MSG_SENDER:
		{
			char author[512];

			msgHdr->GetAuthor(author, sizeof(author), mailDB->GetDB());
			str = JS_NewStringCopyZ(cx, author);
			if (!str)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(str);

			break;
		}
	case MSG_RECIPIENT:
		{
			XPStringObj recipients;

			msgHdr->GetRecipients(recipients, mailDB->GetDB());

			str = JS_NewStringCopyZ(cx, (const char*)recipients);
			if (!str)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(str);
			break;
		}
	case MSG_CC:
		{
			XPStringObj cc;

			msgHdr->GetCCList(cc, mailDB->GetDB());

			str = JS_NewStringCopyZ(cx, (const char*)cc);
			if (!str)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(str);
			break;
		}
	case MSG_READ:
		{
			XP_Bool is_read = (msgHdr->GetFlags() & MSG_FLAG_READ) != 0;

			*vp = BOOLEAN_TO_JSVAL(is_read);

			break;
		}
	case MSG_FOLDER:
		{
			char *folder = msg_convertToMailboxUrl(mailDB->GetFolderName());
			
			str = JS_NewStringCopyZ(cx, folder);
			if (!str)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(str);

			XP_FREE(folder);
			break;
		}
	}
	return JS_TRUE;
}

JSBool PR_CALLBACK
mailMsg_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	JSMailMsgData *data;
	enum msg_slot msg_slot;
	ParseNewMailState *state;
    jsint slot;
	MailMessageHdr *msgHdr;
	MailDB *mailDB;
	MSG_Filter *filter;

    if (!JSVAL_IS_INT(id))
		return JS_TRUE;

    slot = JSVAL_TO_INT(id);

	data = (JSMailMsgData*)JS_GetPrivate(cx, obj);
	if (!data)
		return JS_TRUE;

	msgHdr = data->msgHdr;
	mailDB = data->mailDB;
	filter = data->filter;
	state = data->state;

    if (!msgHdr || !mailDB || !filter || !state)
		return JS_TRUE;

	msg_slot = (enum msg_slot)slot;

	/* make sure that if the user is setting either the priority
	   or folder that we can convert the value to a string. */
	switch (msg_slot) {
	case MSG_PRI:
	case MSG_FOLDER:
		if (!JSVAL_IS_STRING(*vp) &&
			!JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp)) {
			return JS_FALSE;
		}
		break;
	default:;
	}

	switch (msg_slot) {
	case MSG_PRI:
		{
			char *priority = JS_GetStringBytes(JSVAL_TO_STRING(*vp));

			mailDB->SetPriority(msgHdr, MSG_GetPriorityFromString(priority));
			break;
		}
	case MSG_READ:
		{
			JSBool mark_as_read;

			if (!JS_ValueToBoolean(cx, *vp, &mark_as_read))
				return JS_FALSE;

			if (mark_as_read)
				msgHdr->OrFlags(kIsRead);
			else
				msgHdr->AndFlags(~kIsRead);

			break;
		}
	case MSG_FOLDER:
		{
			char *folder_name = JS_GetStringBytes(JSVAL_TO_STRING(*vp));

			JSMailFilter_MoveMessage(state, msgHdr, mailDB, folder_name, filter, &data->msgMoved);

			break;
		}
	default:;
	}

	data->actionPerformed = TRUE;

	return JS_TRUE;
}

JSBool PR_CALLBACK
mailMsg_Resolve(JSContext *cx, JSObject *obj, jsval id)
{
	JSMailMsgData *data;
	JSString *header_value;
	char *header_name;

	if (!JSVAL_IS_STRING(id))
		return JS_TRUE;

	data = (JSMailMsgData*)JS_GetPrivate(cx, obj);
	if (!data)
		return JS_TRUE;

	header_name = JS_GetStringBytes(JSVAL_TO_STRING(id));

	header_value = msg_getHeader(data, header_name);

	return JS_DefineProperty(cx, obj, header_name,
							 STRING_TO_JSVAL(header_value),
							 NULL, NULL,
							 JSPROP_ENUMERATE | JSPROP_READONLY);
}

void PR_CALLBACK
mailMsg_Finalize(JSContext *cx, JSObject *obj)
{
	JSMailMsgData *msg_data;

	msg_data = (JSMailMsgData*)JS_GetPrivate(cx, obj);
	if (!msg_data)
		return;

	/* free our mimeheaders if we had any. */
	if (msg_data->mime_headers)
		MimeHeaders_free(msg_data->mime_headers);

	XP_FREE(msg_data);
}

JSBool PR_CALLBACK
newsMsg_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	JSNewsMsgData *data;
	enum msg_slot msg_slot;
    JSString * str;
    jsint slot;
	DBMessageHdr *msgHdr;
	NewsGroupDB *newsDB;

	data = (JSNewsMsgData*)JS_GetPrivate(cx, obj);
	if (!data)
		return JS_TRUE;

    if (!JSVAL_IS_INT(id))
		return JS_TRUE;

    slot = JSVAL_TO_INT(id);

	msgHdr = data->msgHdr;
	newsDB = data->newsDB;
    if (!msgHdr || !newsDB)
		return JS_TRUE;

	msg_slot = (enum msg_slot)slot;

	switch (msg_slot) {
	case MSG_SUBJECT:
		{
			XPStringObj subject;

			msgHdr->GetSubject(subject, TRUE, newsDB->GetDB());
			
			str = JS_NewStringCopyZ(cx, (const char*)subject);
			if (!str)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(str);
			break;
		}
#if news_dbs_waste_space_for_convience_of_js_filters
	case MSG_PRI:
		{
			char priority_buf[100];

			MSG_GetUntranslatedPriorityName(msgHdr->GetPriority(), priority_buf, 100);

			str = JS_NewStringCopyZ(cx, priority_buf);
			if (!str)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(str);
			break;
		}
#endif
	case MSG_SENDER:
		{
			char author[512];

			msgHdr->GetAuthor(author, sizeof(author), newsDB->GetDB());
			str = JS_NewStringCopyZ(cx, author);
			if (!str)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(str);

			break;
		}
#if news_dbs_waste_space_for_convience_of_js_filters
	case MSG_RECIPIENT:
		{
			XPStringObj recipients;

			msgHdr->GetRecipients(recipients, newsDB->GetDB());

			str = JS_NewStringCopyZ(cx, (const char*)recipients);
			if (!str)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(str);
			break;
		}
	case MSG_CC:
		{
			XPStringObj cc;

			msgHdr->GetCCList(cc, newsDB->GetDB());

			str = JS_NewStringCopyZ(cx, (const char*)cc);
			if (!str)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(str);
			break;
		}
#endif
	case MSG_READ:
		{
			XP_Bool is_read = (msgHdr->GetFlags() & MSG_FLAG_READ) != 0;

			*vp = BOOLEAN_TO_JSVAL(is_read);

			break;
		}
	case MSG_GROUP:
		{
			char *groupName = NewsGroupDB::GetGroupNameFromURL(newsDB->GetGroupURL());

			if (!groupName)
				return JS_FALSE;
			str = JS_NewStringCopyZ(cx, groupName);
			if (!str)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(str);

			XP_FREE(groupName);
			break;
		}
	}
	return JS_TRUE;
}

JSBool PR_CALLBACK
newsMsg_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	JSNewsMsgData *data;
	enum msg_slot msg_slot;
    jsint slot;
	DBMessageHdr *msgHdr;
	NewsGroupDB *newsDB;
	MSG_Filter *filter;

    if (!JSVAL_IS_INT(id))
		return JS_TRUE;

    slot = JSVAL_TO_INT(id);

	data = (JSNewsMsgData*)JS_GetPrivate(cx, obj);
	if (!data)
		return JS_TRUE;
	
	msgHdr = data->msgHdr;
	newsDB = data->newsDB;
	filter = data->filter;
	
    if (!msgHdr || !newsDB || !filter)
		return JS_TRUE;
	
	msg_slot = (enum msg_slot)slot;
	
	/* make sure that if the user is setting the priority
	   that we can convert the value to a string. */
	if (msg_slot == MSG_PRI)
		if (!JSVAL_IS_STRING(*vp) &&
			!JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp)) {
			return JS_FALSE;
		}

	switch (msg_slot) {
#if news_dbs_waste_space_for_convience_of_js_filters
	case MSG_PRI:
		{
			char *priority = JS_GetStringBytes(JSVAL_TO_STRING(*vp));

			newsDB->SetPriority(msgHdr, MSG_GetPriorityFromString(priority));
			break;
		}
#endif
	case MSG_READ:
		{
			JSBool mark_as_read;

			if (!JS_ValueToBoolean(cx, *vp, &mark_as_read))
				return JS_FALSE;

			if (mark_as_read)
				msgHdr->OrFlags(kIsRead);
			else
				msgHdr->AndFlags(~kIsRead);

			break;
		}
	default:;
	}

	data->actionPerformed = TRUE;

	return JS_TRUE;
}

void PR_CALLBACK
newsMsg_Finalize(JSContext *cx, JSObject *obj)
{
	JSNewsMsgData *msg_data;

	msg_data = (JSNewsMsgData*)JS_GetPrivate(cx, obj);
	if (!msg_data)
		return;

	XP_FREE(msg_data);
}

/* So we can possibly add functions "global" to filters... */
static JSClass global_class = {
    "MessageFilters", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

static JSClass js_mail_msg_class = {
    "MailMessage", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, mailMsg_getProperty, mailMsg_setProperty,
    JS_EnumerateStub, mailMsg_Resolve, JS_ConvertStub, mailMsg_Finalize
};

static JSClass js_news_msg_class = {
	"NewsMessage", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, newsMsg_getProperty, newsMsg_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, newsMsg_Finalize
};

/* message.trash() -- moves the message to the trash folder and marks it as read. */
JSBool PR_CALLBACK
mailMsg_delete(JSContext *cx, JSObject *obj, uint /*argc*/, jsval *argv, jsval * /*rval*/)
{
	JSMailMsgData *data;
	MailMessageHdr *msgHdr;
	ParseNewMailState *state;
	MailDB *mailDB;
	MSG_Filter *filter;
	char *trash_folder;

    if (!(data = (JSMailMsgData*)JS_GetInstancePrivate(cx, obj, &js_mail_msg_class, argv)))
        return JS_FALSE;

	msgHdr = data->msgHdr;
	state = data->state;
	mailDB = data->mailDB;
	filter = data->filter;

    if (!msgHdr || !mailDB || !state || !filter)
		return JS_FALSE;

	/* copy the behaviour from prsembst.cpp */
	trash_folder = state->GetMaster()->GetPrefs()->MagicFolderName(MSG_FOLDER_FLAG_TRASH);
	msgHdr->OrFlags(kIsRead); // mark read in trash.

	JSMailFilter_MoveMessage(state, msgHdr, mailDB, trash_folder, filter, &data->msgMoved);

	data->actionPerformed = TRUE;
	return JS_TRUE;
}

/* message.killThread() -- marks the thread containing this message as ignored. */
JSBool PR_CALLBACK
mailMsg_killthread(JSContext *cx, JSObject *obj, uint /*argc*/, jsval *argv, jsval * /*rval*/)
{
	JSMailMsgData *data;
	MailMessageHdr *msgHdr;

    if (!(data = (JSMailMsgData*)JS_GetInstancePrivate(cx, obj, &js_mail_msg_class, argv)))
        return JS_FALSE;

	msgHdr = data->msgHdr;

    if (!msgHdr)
		return JS_FALSE;

	/* copy the behaviour from prsembst.cpp */
	msgHdr->OrFlags(kIgnored);

	data->actionPerformed = TRUE;
	return JS_TRUE;
}

/* message.watchThread() -- marks the thread containing this message as watched. */
JSBool PR_CALLBACK
mailMsg_watchthread(JSContext *cx, JSObject *obj, uint /*argc*/, jsval *argv, jsval * /*rval*/)
{
	JSMailMsgData *data;
	MailMessageHdr *msgHdr;

    if (!(data = (JSMailMsgData*)JS_GetInstancePrivate(cx, obj, &js_mail_msg_class, argv)))
        return JS_FALSE;

	msgHdr = data->msgHdr;

    if (!msgHdr)
		return JS_FALSE;

	/* copy the behaviour from prsembst.cpp */
	msgHdr->OrFlags(kWatched);

	data->actionPerformed = TRUE;
	return JS_TRUE;
}

/* message.killThread() -- marks the thread containing this message as ignored. */
JSBool PR_CALLBACK
newsMsg_killthread(JSContext *cx, JSObject *obj, uint /*argc*/, jsval *argv, jsval * /*rval*/)
{
	JSNewsMsgData *data;

    if (!(data = (JSNewsMsgData*)JS_GetInstancePrivate(cx, obj, &js_news_msg_class, argv)))
        return JS_FALSE;

    data->msgHdr->OrFlags(kIgnored);

	data->actionPerformed = TRUE;
	return JS_TRUE;
}

/* message.watchThread() -- marks the thread containing this message as watched. */
JSBool PR_CALLBACK
newsMsg_watchthread(JSContext *cx, JSObject *obj, uint /*argc*/, jsval *argv, jsval * /*rval*/)
{
	JSNewsMsgData *data;

    if (!(data = (JSNewsMsgData*)JS_GetInstancePrivate(cx, obj, &js_news_msg_class, argv)))
        return JS_FALSE;

    data->msgHdr->OrFlags(kWatched);

	data->actionPerformed = TRUE;
	return JS_TRUE;
}

#if DEBUG
/* trace() -- outputs spew to stderr.  Actually, this (or something like it
   that perhaps outputs to the same file that the rest of the filter logging code
   writes to) would probably be very useful in the normal course of writing filters. */
JSBool PR_CALLBACK
filter_trace(JSContext *cx, JSObject * /*obj*/, uint argc, jsval *argv, jsval * /*rval*/)
{

	if (argc > 0)
		{
			JSString *str;
			const char *trace_str;
			if (!(str = JS_ValueToString(cx, argv[0])))
				return JS_FALSE;

			trace_str = JS_GetStringBytes(str);
			if (*trace_str != '\0')
				{
					fprintf (stderr, "jsfilter trace: %s\n", trace_str);
				}
			return JS_TRUE;
		}

	return JS_FALSE;
}
#endif

static JSFunctionSpec mail_msg_methods[] = {
	{ "trash",	mailMsg_delete,	0 },
	{ "killThread", mailMsg_killthread, 0 },
	{ "watchThread", mailMsg_watchthread, 0 },
	{ 0 }
};

static JSFunctionSpec news_msg_methods[] = {
	{ "killThread", newsMsg_killthread, 0 },
	{ "watchThread", newsMsg_watchthread, 0 },
	{ 0 }
};

static JSFunctionSpec filter_methods[] = {
#ifdef DEBUG
	{ "trace", filter_trace, 1 },
#endif
	{ 0 }
};

static void
destroyJSFilterStuff()
{
	filter_obj = NULL;
}

static JSContext *
initializeJSFilterStuff(MWContext *context)
{
	PREF_GetConfigContext(&filter_context);

	/* we're totally hosed.  Let's bail */
	if (!filter_context)
		return NULL;

	/* we're already initialized */
	if (filter_obj)
		return filter_context;

	/* create our "global" object.  We make the message object a child of this */
	filter_obj = JS_NewObject(filter_context, &global_class, NULL, NULL);
	
	if (!filter_obj
		|| !JS_SetPrivate(filter_context, filter_obj, context)
		|| !JS_InitStandardClasses(filter_context, filter_obj)
		|| !JS_DefineFunctions(filter_context, filter_obj, filter_methods))
		{
			destroyJSFilterStuff();
			return NULL;
		}

	return filter_context;
}

static JSObject *
newMailMsgObject()
{
	JSObject *message_obj;
	JSMailMsgData *msg_data;

	/* define the javascript object that is going to reflect all the message
	   properties we allow.  The name in quotes is the word that in the script
	   that will represent this object -- "message.trash()" */
	message_obj = JS_DefineObject(filter_context, filter_obj,
								  "mailMessage", &js_mail_msg_class,
								  NULL, JSPROP_ENUMERATE);
	
	if (!message_obj)
		return NULL;
	
	msg_data = XP_NEW_ZAP(JSMailMsgData);
	
	if (!msg_data)
		return NULL;
	
	if (!JS_SetPrivate(filter_context, message_obj, msg_data)
		|| !JS_DefineProperties(filter_context, message_obj, mail_msg_props)
		|| !JS_DefineFunctions(filter_context, message_obj, mail_msg_methods))
		{
			return NULL;
		}
	
	return message_obj;
}

static JSObject *
newNewsMsgObject()
{
	JSObject *message_obj;
	JSNewsMsgData *msg_data;

	/* define the javascript object that is going to reflect all the message
	   properties we allow.  The name in quotes is the word that in the script
	   that will represent this object -- "message.trash()" */
	message_obj = JS_DefineObject(filter_context, filter_obj,
								  "newsMessage", &js_news_msg_class,
								  NULL, JSPROP_ENUMERATE);
	
	if (!message_obj)
		return NULL;
	
	msg_data = XP_NEW_ZAP(JSNewsMsgData);
	
	if (!msg_data)
		return NULL;
	
	if (!JS_SetPrivate(filter_context, message_obj, msg_data)
		|| !JS_DefineProperties(filter_context, message_obj, news_msg_props)
		|| !JS_DefineFunctions(filter_context, message_obj, news_msg_methods))
		{
			return NULL;
		}
	
	return message_obj;
}

void
jsmsg_ErrorReporter(JSContext *cx, const char *message,
					JSErrorReport *report)
{
	char *last = NULL;
    int i, j, k, n;
    const char *s, *t;
	MWContext *context = (MWContext*)JS_GetPrivate(cx, filter_obj);

	if (!context)
		return;

	// for now we only put up one dialog.
	if (error_count)
		return;

	error_count++;

	last = PR_sprintf_append(0,
							 "<FONT SIZE=4>\n<B>JavaScript Mail Filter Error:</B><BR>");
	last = PR_sprintf_append(last,
							 "Messages that would have ordinarily been modified by your JavaScript<BR>"
							 "Filters have been placed, unmodified, in your Inbox.<BR><BR>");
	
    if (!report) {
	last = PR_sprintf_append(last, "<B>%s:</B>\n", message);
    } else {
	if (report->filename)
	    last = PR_sprintf_append(last, "<A HREF=\"%s\">%s</A>, ",
				     report->filename, report->filename);
	if (report->lineno)
	    last = PR_sprintf_append(last, "<B>line %u:</B>", report->lineno);
	last = PR_sprintf_append(last,
				 "<BR><BR>%s.</FONT><PRE><FONT SIZE=4>",
				 message);
	if (report->linebuf) {
	    for (s = t = report->linebuf; *s != '\0'; s = t) {
		for (; t != report->tokenptr && *t != '<' && *t != '\0'; t++)
		    ;
		last = PR_sprintf_append(last, "%.*s", t - s, s);
		if (*t == '\0')
		    break;
		if (t == report->tokenptr) {
		    last = PR_sprintf_append(last,
					     "</FONT>"
					     "<FONT SIZE=4 COLOR=#ff2020>");
		}
		last = PR_sprintf_append(last, (*t == '<') ? "&lt;" : "%c", *t);
		t++;
	    }
	    last = PR_sprintf_append(last, "</FONT><FONT SIZE=4>\n");
	    n = report->tokenptr - report->linebuf;
	    for (i = j = 0; i < n; i++) {
		if (report->linebuf[i] == '\t') {
		    for (k = (j + 8) & ~7; j < k; j++)
			last = PR_sprintf_append(last, ".");
		    continue;
		}
		last = PR_sprintf_append(last, ".");
		j++;
	    }
	    last = PR_sprintf_append(last, "<B>^</B>");
	}
	last = PR_sprintf_append(last, "\n</FONT></PRE>");
    }

	if (last)
		{
			XP_MakeHTMLAlert(context, last);

			XP_FREE(last);
		}
}

static JSBool
compileJSFilters()
{
	static time_t m_time = 0; // the last modification time of filters.js
	static JSBool ret_val = JS_FALSE;
	char *filename;
	XP_File fp;
	XP_StatStruct stats;
	MWContext *context = (MWContext*)JS_GetPrivate(filter_context, filter_obj);

	if (!need_compile)
		return ret_val;

	filename = WH_FileName("", xpJSMailFilters);

	XP_Trace("+Filename for script filter is %s\n", filename);

#ifdef XP_WIN
    _stat(filename, &stats);
#else
	stat(filename, &stats);
#endif

	if (stats.st_mtime > m_time || need_compile)
		{
			long fileLength;
			char *buffer;
			
			m_time = stats.st_mtime;
			
			fileLength = stats.st_size;
			if (fileLength <= 1)
				{
					ret_val = JS_FALSE;
					return ret_val;
				}
			
			fp = fopen(filename, "r");
			
			buffer = (char*)malloc(fileLength);
			if (!buffer)
				{
					fclose(fp);
					ret_val = JS_FALSE;
					return ret_val;
				}
			
			fileLength = XP_FileRead(buffer, fileLength, fp);

			XP_FileClose(fp);

			XP_Trace("+Compiling filters.js...\n");

			jsval rval;
			ret_val = JS_EvaluateScript(filter_context, filter_obj, buffer, fileLength,
										filename, 1, &rval);

			XP_Trace("+Done.\n");
			
			XP_FREE(buffer);

			need_compile = JS_FALSE;
		}

	return ret_val;
}

MSG_SearchError
JSMailFilter_execute(ParseNewMailState *state, 
					 MSG_Filter *filter, 
					 MailMessageHdr *msgHdr, 
					 MailDB *mailDB,
					 XP_Bool *pMoved)
{
	jsval result;
	jsval filter_arg; /* we will this in with the message object we create. */
	JSObject *message_obj;
	JSString *regexp_input;
	JSMailMsgData *msg_data;
	char *script_name;

	/* initialize the filter stuff, and bomb out early if it fails */
	if (!initializeJSFilterStuff(state->GetContext()))
		return SearchError_OutOfMemory; // XXX

	if (!error_reporter_installed)
		{
			error_reporter_installed = JS_TRUE;
			previous_error_reporter = JS_SetErrorReporter(filter_context,
														  jsmsg_ErrorReporter);
		}

	/* reload the filters.js file if necessary, and compile it. */
	
	if (!compileJSFilters())
		return SearchError_OutOfMemory; // XXX

	message_obj = newMailMsgObject();
	if (!message_obj)
		return SearchError_OutOfMemory; // XXX

	/* fill in the fields of the msg_data that allow us to 
	   get at all the state we need */
	msg_data = (JSMailMsgData*)JS_GetPrivate(filter_context, message_obj);
	msg_data->msgHdr = msgHdr;
	msg_data->mailDB = mailDB;
	msg_data->filter = filter;
	msg_data->state = state;
	msg_data->js_context = filter_context;
	msg_data->js_object = message_obj;

	/* initialize the regular expression machinery, setting $_ to
	   the message headers */
	regexp_input = mailMsg_initializeHeaders(msg_data);
	JS_SetRegExpInput(filter_context, regexp_input, JS_TRUE /* multiline */);

	filter_arg = OBJECT_TO_JSVAL(message_obj);
	filter->GetFilterScript(&script_name);
	if (script_name)
		JS_CallFunctionName(filter_context, filter_obj, script_name,
							1, &filter_arg, &result);

	/* clean up the regexp stuff. */
	JS_ClearRegExpStatics(filter_context);

	if (pMoved)
		*pMoved = msg_data->msgMoved;
	
	/* msg_data->actionPerformed is set in the js methods and setProperty() above.
	   XXX we should also probably take into account the return value of the script.*/
	if (msg_data->actionPerformed)
		return SearchError_Success;
	else
		return SearchError_NotAMatch;
}

MSG_SearchError
JSNewsFilter_execute(MSG_Filter* filter, 
					 DBMessageHdr* msgHdr, 
					 NewsGroupDB* newsDB)
{
	jsval result;
	jsval filter_arg; /* we will this in with the message object we create. */
	JSObject *message_obj;
	JSNewsMsgData *msg_data;
	char *script_name;

	/* initialize the filter stuff, and bomb out early if it fails */
	if (!initializeJSFilterStuff(NULL))
		return SearchError_OutOfMemory; // XXX

	if (!error_reporter_installed)
		{
			error_reporter_installed = JS_TRUE;
			previous_error_reporter = JS_SetErrorReporter(filter_context,
														  jsmsg_ErrorReporter);
		}

	/* reload the filters.js file if necessary, and compile it. */
	
	if (!compileJSFilters())
		return SearchError_OutOfMemory; // XXX

	message_obj = newNewsMsgObject();
	if (!message_obj)
		return SearchError_OutOfMemory; // XXX

	/* fill in the fields of the msg_data that allow us to 
	   get at all the state we need */
	msg_data = (JSNewsMsgData*)JS_GetPrivate(filter_context, message_obj);
	msg_data->msgHdr = msgHdr;
	msg_data->newsDB = newsDB;
	msg_data->filter = filter;
	msg_data->js_context = filter_context;
	msg_data->js_object = message_obj;

	filter_arg = OBJECT_TO_JSVAL(message_obj);
	filter->GetFilterScript(&script_name);
	if (script_name)
		JS_CallFunctionName(filter_context, filter_obj, script_name,
							1, &filter_arg, &result);

	/* msg_data->actionPerformed is set in the js methods and setProperty() above.
	   XXX we should also probably take into account the return value of the script.*/
	if (msg_data->actionPerformed)
		return SearchError_Success;
	else
		return SearchError_NotAMatch;
}

void
JSFilter_cleanup()
{
	XP_Trace("+Cleaning up JS Filters");

	error_count = 0;

	need_compile = JS_TRUE;

	if (filter_context)
	{
		if (error_reporter_installed)
			{
				error_reporter_installed = JS_FALSE;
				JS_SetErrorReporter(filter_context, previous_error_reporter);
			}

		JS_GC(filter_context);

		destroyJSFilterStuff();
	}
}
