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

/* Please leave outside of ifdef for windows precompiled headers */
#define FORCE_PR_LOG /* Allow logging in the release build (sorry this breaks the PCH) */

#include "mkutils.h"
#include "netutils.h"

#include "nsNNTPProtocol.h"

#include "rosetta.h"
#include HG40855

#include "prtime.h"
#include "prlog.h"
#include "prerror.h"

#include HG09893
#include "prefapi.h"	

/* start turning on XPCOM interfaces here.
 * when they are all turned on, PLEASE remove dead code */

#include "nsIMsgXOVERParser.h"
#include "nsIMsgNewsArticleList.h"
#include "nsIMsgNewsHost.h"

#include "nsIMsgNewsgroup.h"

/*#define CACHE_NEWSGRP_PASSWORD*/

/* for XP_GetString() */
#include "xpgetstr.h"
extern int MK_MALFORMED_URL_ERROR;
extern int MK_NEWS_ERROR_FMT;
extern int MK_NNTP_CANCEL_CONFIRM;
extern int MK_NNTP_CANCEL_DISALLOWED;
extern int MK_NNTP_NOT_CANCELLED;
extern int MK_OUT_OF_MEMORY;
extern int XP_CONFIRM_SAVE_NEWSGROUPS;
extern int XP_HTML_ARTICLE_EXPIRED;
extern int XP_HTML_NEWS_ERROR;
extern int XP_PROGRESS_READ_NEWSGROUPINFO;
extern int XP_PROGRESS_RECEIVE_ARTICLE;
extern int XP_PROGRESS_RECEIVE_LISTARTICLES;
extern int XP_PROGRESS_RECEIVE_NEWSGROUP;
extern int XP_PROGRESS_SORT_ARTICLES;
extern int XP_PROGRESS_READ_NEWSGROUP_COUNTS;
extern int XP_THERMO_PERCENT_FORM;
extern int XP_PROMPT_ENTER_USERNAME;
extern int MK_BAD_NNTP_CONNECTION;
extern int MK_NNTP_AUTH_FAILED;
extern int MK_NNTP_ERROR_MESSAGE;
extern int MK_NNTP_NEWSGROUP_SCAN_ERROR;
extern int MK_NNTP_SERVER_ERROR;
extern int MK_NNTP_SERVER_NOT_CONFIGURED;
HG25431
extern int MK_TCP_READ_ERROR;
extern int MK_TCP_WRITE_ERROR;
extern int MK_NNTP_CANCEL_ERROR;
extern int XP_CONNECT_NEWS_HOST_CONTACTED_WAITING_FOR_REPLY;
extern int XP_PLEASE_ENTER_A_PASSWORD_FOR_NEWS_SERVER_ACCESS;
extern int XP_GARBAGE_COLLECTING;
extern int XP_MESSAGE_SENT_WAITING_NEWS_REPLY;
extern int MK_MSG_DELIV_NEWS;
extern int MK_MSG_COLLABRA_DISABLED;
extern int MK_MSG_EXPIRE_NEWS_ARTICLES;
extern int MK_MSG_HTML_IMAP_NO_CACHED_BODY;

/* Logging stuff */

PRLogModuleInfo* NNTP = NULL;
#define out     PR_LOG_ALWAYS

#define NNTP_LOG_READ(buf) \
if (NNTP==NULL) \
    NNTP = PR_NewLogModule("NNTP"); \
PR_LOG(NNTP, out, ("Receiving: %s", buf)) ;

#define NNTP_LOG_WRITE(buf) \
if (NNTP==NULL) \
    NNTP = PR_NewLogModule("NNTP"); \
PR_LOG(NNTP, out, ("Sending: %s", buf)) ;

#define NNTP_LOG_NOTE(buf) \
if (NNTP==NULL) \
    NNTP = PR_NewLogModule("NNTP"); \
PR_LOG(NNTP, out, buf) ;


/* end logging */

/* Forward declarations */

#define LIST_WANTED     0
#define ARTICLE_WANTED  1
#define CANCEL_WANTED   2
#define GROUP_WANTED    3
#define NEWS_POST       4
#define READ_NEWS_RC    5
#define NEW_GROUPS      6
#define SEARCH_WANTED   7
#define PRETTY_NAMES_WANTED 8
#define PROFILE_WANTED	9
#define IDS_WANTED		10

/* the output_buffer_size must be larger than the largest possible line
 * 2000 seems good for news
 *
 * jwz: I increased this to 4k since it must be big enough to hold the
 * entire button-bar HTML, and with the new "mailto" format, that can
 * contain arbitrarily long header fields like "references".
 *
 * fortezza: proxy auth is huge, buffer increased to 8k (sigh).
 */
#define OUTPUT_BUFFER_SIZE (4096*2)

/* the amount of time to subtract from the machine time
 * for the newgroup command sent to the nntp server
 */
#define NEWGROUPS_TIME_OFFSET 60L*60L*12L   /* 12 hours */

/* Allow the user to open at most this many connections to one news host*/
#define kMaxConnectionsPerHost 3

/* Keep this many connections cached. The cache is global, not per host */
#define kMaxCachedConnections 2

/* globals
 */
PRIVATE XP_List * nntp_connection_list=0;

PRIVATE XP_Bool net_news_last_username_probably_valid=FALSE;
PRIVATE int32 net_NewsChunkSize=-1;  /* default */

PRIVATE int32 net_news_timeout = 170; /* seconds that an idle NNTP conn can live */

#define PUTSTRING(s)      (*cd->stream->put_block) \
                    (cd->stream, s, PL_strlen(s))
#define COMPLETE_STREAM   (*cd->stream->complete)  \
                    (cd->stream)
#define ABORT_STREAM(s)   (*cd->stream->abort) \
                    (cd->stream, s)
#define PUT_STREAM(buf, size)   (*cd->stream->put_block) \
						  (cd->stream, buf, size)

/* states of the machine
 */
typedef enum _StatesEnum {
NNTP_RESPONSE,
#ifdef BLOCK_UNTIL_AVAILABLE_CONNECTION
NNTP_BLOCK_UNTIL_CONNECTIONS_ARE_AVAILABLE,
NNTP_CONNECTIONS_ARE_AVAILABLE,
#endif
NNTP_CONNECT,
NNTP_CONNECT_WAIT,
HG07711
NNTP_LOGIN_RESPONSE,
NNTP_SEND_MODE_READER,
NNTP_SEND_MODE_READER_RESPONSE,
SEND_LIST_EXTENSIONS,
SEND_LIST_EXTENSIONS_RESPONSE,
SEND_LIST_SEARCHES,
SEND_LIST_SEARCHES_RESPONSE,
NNTP_LIST_SEARCH_HEADERS,
NNTP_LIST_SEARCH_HEADERS_RESPONSE,
NNTP_GET_PROPERTIES,
NNTP_GET_PROPERTIES_RESPONSE,
SEND_LIST_SUBSCRIPTIONS,
SEND_LIST_SUBSCRIPTIONS_RESPONSE,
SEND_FIRST_NNTP_COMMAND,
SEND_FIRST_NNTP_COMMAND_RESPONSE,
SETUP_NEWS_STREAM,
NNTP_BEGIN_AUTHORIZE,
NNTP_AUTHORIZE_RESPONSE,
NNTP_PASSWORD_RESPONSE,
NNTP_READ_LIST_BEGIN,
NNTP_READ_LIST,
DISPLAY_NEWSGROUPS,
NNTP_NEWGROUPS_BEGIN,
NNTP_NEWGROUPS,
NNTP_BEGIN_ARTICLE,
NNTP_READ_ARTICLE,
NNTP_XOVER_BEGIN,
NNTP_FIGURE_NEXT_CHUNK,
NNTP_XOVER_SEND,
NNTP_XOVER_RESPONSE,
NNTP_XOVER,
NEWS_PROCESS_XOVER,
NNTP_READ_GROUP,
NNTP_READ_GROUP_RESPONSE,
NNTP_READ_GROUP_BODY,
NNTP_SEND_GROUP_FOR_ARTICLE,
NNTP_SEND_GROUP_FOR_ARTICLE_RESPONSE,
NNTP_PROFILE_ADD,
NNTP_PROFILE_ADD_RESPONSE,
NNTP_PROFILE_DELETE,
NNTP_PROFILE_DELETE_RESPONSE,
NNTP_SEND_ARTICLE_NUMBER,
NEWS_PROCESS_BODIES,
NNTP_PRINT_ARTICLE_HEADERS,
NNTP_SEND_POST_DATA,
NNTP_SEND_POST_DATA_RESPONSE,
NNTP_CHECK_FOR_MESSAGE,
NEWS_NEWS_RC_POST,
NEWS_DISPLAY_NEWS_RC,
NEWS_DISPLAY_NEWS_RC_RESPONSE,
NEWS_START_CANCEL,
NEWS_DO_CANCEL,
NNTP_XPAT_SEND,
NNTP_XPAT_RESPONSE,
NNTP_SEARCH,
NNTP_SEARCH_RESPONSE,
NNTP_SEARCH_RESULTS,
NNTP_LIST_PRETTY_NAMES,
NNTP_LIST_PRETTY_NAMES_RESPONSE,
NNTP_LIST_XACTIVE,
NNTP_LIST_XACTIVE_RESPONSE,
NNTP_LIST_GROUP,
NNTP_LIST_GROUP_RESPONSE,
NEWS_DONE,
NEWS_ERROR,
NNTP_ERROR,
NEWS_FREE
} StatesEnum;

#ifdef DEBUG
char *stateLabels[] = {
"NNTP_RESPONSE",
#ifdef BLOCK_UNTIL_AVAILABLE_CONNECTION
"NNTP_BLOCK_UNTIL_CONNECTIONS_ARE_AVAILABLE",
"NNTP_CONNECTIONS_ARE_AVAILABLE",
#endif
"NNTP_CONNECT",
"NNTP_CONNECT_WAIT",
HG25430
"NNTP_LOGIN_RESPONSE",
"NNTP_SEND_MODE_READER",
"NNTP_SEND_MODE_READER_RESPONSE",
"SEND_LIST_EXTENSIONS",
"SEND_LIST_EXTENSIONS_RESPONSE",
"SEND_LIST_SEARCHES",
"SEND_LIST_SEARCHES_RESPONSE",
"NNTP_LIST_SEARCH_HEADERS",
"NNTP_LIST_SEARCH_HEADERS_RESPONSE",
"NNTP_GET_PROPERTIES",
"NNTP_GET_PROPERTIES_RESPONSE",
"SEND_LIST_SUBSCRIPTIONS",
"SEND_LIST_SUBSCRIPTIONS_RESPONSE",
"SEND_FIRST_NNTP_COMMAND",
"SEND_FIRST_NNTP_COMMAND_RESPONSE",
"SETUP_NEWS_STREAM",
"NNTP_BEGIN_AUTHORIZE",
"NNTP_AUTHORIZE_RESPONSE",
"NNTP_PASSWORD_RESPONSE",
"NNTP_READ_LIST_BEGIN",
"NNTP_READ_LIST",
"DISPLAY_NEWSGROUPS",
"NNTP_NEWGROUPS_BEGIN",
"NNTP_NEWGROUPS",
"NNTP_BEGIN_ARTICLE",
"NNTP_READ_ARTICLE",
"NNTP_XOVER_BEGIN",
"NNTP_FIGURE_NEXT_CHUNK",
"NNTP_XOVER_SEND",
"NNTP_XOVER_RESPONSE",
"NNTP_XOVER",
"NEWS_PROCESS_XOVER",
"NNTP_READ_GROUP",
"NNTP_READ_GROUP_RESPONSE",
"NNTP_READ_GROUP_BODY",
"NNTP_SEND_GROUP_FOR_ARTICLE",
"NNTP_SEND_GROUP_FOR_ARTICLE_RESPONSE",
"NNTP_PROFILE_ADD",
"NNTP_PROFILE_ADD_RESPONSE",
"NNTP_PROFILE_DELETE",
"NNTP_PROFILE_DELETE_RESPONSE",
"NNTP_SEND_ARTICLE_NUMBER",
"NEWS_PROCESS_BODIES",
"NNTP_PRINT_ARTICLE_HEADERS",
"NNTP_SEND_POST_DATA",
"NNTP_SEND_POST_DATA_RESPONSE",
"NNTP_CHECK_FOR_MESSAGE",
"NEWS_NEWS_RC_POST",
"NEWS_DISPLAY_NEWS_RC",
"NEWS_DISPLAY_NEWS_RC_RESPONSE",
"NEWS_START_CANCEL",
"NEWS_DO_CANCEL",
"NNTP_XPAT_SEND",
"NNTP_XPAT_RESPONSE",
"NNTP_SEARCH",
"NNTP_SEARCH_RESPONSE",
"NNTP_SEARCH_RESULTS",
"NNTP_LIST_PRETTY_NAMES",
"NNTP_LIST_PRETTY_NAMES_RESPONSE",
"NNTP_LIST_XACTIVE_RESPONSE",
"NNTP_LIST_XACTIVE",
"NNTP_LIST_GROUP",
"NNTP_LIST_GROUP_RESPONSE",
"NEWS_DONE",
"NEWS_ERROR",
"NNTP_ERROR",
"NEWS_FREE"
};

#endif


static char * last_password = 0;
static char * last_password_hostname = 0;
static char * last_username=0;
static char * last_username_hostname=0;

/////////////////////////////////////////////////////////////////////////////////////////////
// we suppport the nsIStreamListener interface 
////////////////////////////////////////////////////////////////////////////////////////////

// Whenever data arrives from the connection, core netlib notifices the protocol by calling
// OnDataAvailable. We then read and process the incoming data from the input stream. 
NS_IMETHOD nsNNTPProtocol::OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength)
{
	// right now, this really just means turn around and process the url
	Process();

}

NS_IMETHOD nsNNTPProtocol::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
	// extract the appropriate event sinks from the url and initialize them in our protocol data

}

// stop binding is a "notification" informing us that the stream associated with aURL is going away. 
NS_IMETHOD nsNNTPProtocol::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg)
{

}

/////////////////////////////////////////////////////////////////////////////////////////////
// End of nsIStreamListenerSupport
////////////////////////////////////////////////////////////////////////////////////////////


/* gets the response code from the nntp server and the
 * response line
 *
 * returns the TCP return code from the read
 */
PRInt nsNNTPProtocol::NewsResponse(nsIInputStream * inputStream, PRUint32 length);
{
	char * line;
	PRInt status;

//    ce->status = NET_BufferedReadLine(ce->socket, &line, &m_dataBuf, 
//                    &m_dataBufSize, (Bool*)&cd->pause_for_read);
	if (inputStream)
		inputStream->Read(

	NNTP_LOG_READ(m_dataBuf);

    if(ce->status == 0)
	{
        m_nextState = NNTP_ERROR;
        ClearFlag(NNTP_PAUSE_FOR_READ);
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
        return(MK_NNTP_SERVER_ERROR);
	}

    /* if TCP error of if there is not a full line yet return */
    if(ce->status < 0)
	{
        ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, 
                                                       PR_GetOSError());

        /* return TCP error
         */
        return MK_TCP_READ_ERROR;
	}
	else if(!line)
         return ce->status;

    ClearFlag(NNTP_PAUSE_FOR_READ);  /* don't pause if we got a line */

	HG43574
    /* almost correct */
    if(ce->status > 1)
	{
        ce->bytes_received += ce->status;
        FE_GraphProgress(ce->window_id, ce->URL_s, ce->bytes_received, ce->status, ce->URL_s->content_length);
	}

    StrAllocCopy(m_responseText, line+4);

	m_previousResponseCode = m_responseCode;

    sscanf(line, "%d", &m_responseCode);

	/* authentication required can come at any time
	 */
#ifdef CACHE_NEWSGRP_PASSWORD
	/*
	 * This is an effort of trying to cache the username/password
	 * per newsgroup. It is extremely hard to make it work along with
	 * nntp voluntary password checking mechanism. We are backing this 
	 * feature out. Instead of touching various of backend msg files
	 * at this late Dogbert 4.0 beta4 game, the infrastructure will
	 * remain in the msg library. We only modify codes within this file.
	 * Maybe one day we will try to do it again. Zzzzz -- jht
	 */

	if(MK_NNTP_RESPONSE_AUTHINFO_REQUIRE == m_responseCode ||
       MK_NTTP_RESPONSE_AUTHINFO_SIMPLE_REQUIRE == m_responseCode || 
	   MK_NNTP_RESPONSE_PERMISSION_DENIED == m_responseCode)
	  {
        m_nextState = NNTP_BEGIN_AUTHORIZE;
		if (MK_NNTP_RESPONSE_PERMISSION_DENIED == m_responseCode) 
		{
			if (MK_NNTP_RESPONSE_TYPE_OK == MK_NNTP_RESPONSE_TYPE(m_previousResponseCode) 
			{
				if (net_news_last_username_probably_valid)
				  net_news_last_username_probably_valid = FALSE;
				else 
				{
                  m_newsgroup->SetUsername(NULL);
                  m_newsgroup->SetPassword(NULL);
				}
			}
			else 
			{
			  net_news_last_username_probably_valid = FALSE;
			  if (NNTP_PASSWORD_RESPONSE == m_nextStateAfterResponse) 
			  {
                  m_newsgroup->SetUsername(NULL);
                  m_newsgroup->SetPassword(NULL);
			  }
			}
		}
	  }
#else
	if (MK_NNTP_RESPONSE_AUTHINFO_REQUIRE == m_responseCode ||
        MK_NNTP_RESPONSE_AUTHINFO_SIMPLE_REQUIRE == m_responseCode) 
	{
		m_nextState = NNTP_BEGIN_AUTHORIZE;
	}
	else if (MK_NNTP_RESPONSE_PERMISSION_DENIED == m_responseCode)
	{
	    net_news_last_username_probably_valid = FALSE;
		return net_display_html_error_state(ce);
	}
#endif
	else
    	m_nextState = m_nextStateAfterResponse;

    return(0);  /* everything ok */
}


HG43072

/* interpret the server response after the connect
 *
 * returns negative if the server responds unexpectedly
 */
 
PRInt nsNNTPProtocol::LoginResponse()
{
    NewsConData * cd = (NewsConData *)ce->con_data;
	XP_Bool postingAllowed = m_responseCode == MK_NNTP_RESPONSE_POSTING_ALLOWED;

    if(MK_NNTP_RESPONSE_TYPE(m_responseCode)!=MK_NNTP_RESPONSE_TYPE_OK)
	{
		ce->URL_s->error_msg  = NET_ExplainErrorDetails(MK_NNTP_ERROR_MESSAGE, m_responseText);

    	m_nextState = NNTP_ERROR;
        cd->control_con->prev_cache = FALSE; /* to keep if from reconnecting */
        return MK_BAD_NNTP_CONNECTION;
	}

	cd->control_con->posting_allowed = postingAllowed; /* ###phil dead code */
    m_newsHost->SetPostingAllowed(postingAllowed);
    
    m_nextState = NNTP_SEND_MODE_READER;
    return(0);  /* good */
}

PRInt nsNNTPProtocol::SendModeReader()
{
    PL_strcpy(cd->output_buffer, "MODE READER" CRLF);

    ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
	NNTP_LOG_WRITE(cd->output_buffer);

    m_nextState = NNTP_RESPONSE;
    m_nextStateAfterResponse = NNTP_SEND_MODE_READER_RESPONSE;
    SetFlag(NNTP_PAUSE_FOR_READ); 
    return(ce->status);
}

PRInt nsNNTPProtocol::SendModeReaderResponse()
{
	cd->mode_reader_performed = TRUE;

	/* ignore the response code and continue
	 */
    PRBool pushAuth;
    nsresult rv = m_newsHost->IsPushAuth(&pushAuth);
    if (NS_SUCCEEDED(rv) && pushAuth)
		/* if the news host is set up to require volunteered (pushed) authentication,
		 * do that before we do anything else
		 */
		m_nextState = NNTP_BEGIN_AUTHORIZE;
	else
		m_nextState = SEND_LIST_EXTENSIONS;

	return(0);
}

PRInt nsNNTPProtocol::SendListExtensions()
{
    PL_strcpy(cd->output_buffer, "LIST EXTENSIONS" CRLF);
    ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
	NNTP_LOG_WRITE(cd->output_buffer);

	m_nextState = NNTP_RESPONSE;
	m_nextStateAfterResponse = SEND_LIST_EXTENSIONS_RESPONSE;
	ClearFlag(NNTP_PAUSE_FOR_READ);
	return ce->status;
}

PRInt nsNNTPProtocol::SendListExtensionsResponse()
{
	if (MK_NNTP_RESPONSE_TYPE(m_responseCode) == MK_NNTP_RESPONSE_TYPE_OK)
	{
		char *line = NULL;
        nsIMsgNewsHost *news_host = m_newsHost;

		ce->status = NET_BufferedReadLine (ce->socket, &line, &m_dataBuf,
			&m_dataBufSize, (Bool*)&cd->pause_for_read);

		if(ce->status == 0)
		{
			m_nextState = NNTP_ERROR;
			ClearFlag(NNTP_PAUSE_FOR_READ);
			ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
			return MK_NNTP_SERVER_ERROR;
		}
		if (!line)
			return ce->status;  /* no line yet */
		if (ce->status < 0)
		{
			ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, SOCKET_ERRNO);
			/* return TCP error */
			return MK_TCP_READ_ERROR;
		}

		if ('.' != line[0])
            news_host->AddExtension(line);
		else
		{
			/* tell libmsg that it's ok to ask this news host for extensions */		
			m_newsHost->SetSupportsExtensions(TRUE);
			/* all extensions received */
			m_nextState = SEND_LIST_SEARCHES;
			ClearFlag(NNTP_PAUSE_FOR_READ);
		}
	}
	else
	{
		/* LIST EXTENSIONS not recognized 
		 * tell libmsg not to ask for any more extensions and move on to
		 * the real NNTP command we were trying to do. */
		 
		 m_newsHost->SetSupportsExtensions(FALSE);
		 m_nextState = SEND_FIRST_NNTP_COMMAND;
	}

	return ce->status;
}

PRInt nsNNTPProtocol::SendListSearches
{  
    nsresult rv;
    PRBool searchable=FALSE;
    
    rv = m_newsHost->QueryExtension("SEARCH",&searchable);
    if (NS_SUCCEEDED(rv) && searchable)
	{
		PL_strcpy(cd->output_buffer, "LIST SEARCHES" CRLF);
		ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
		NNTP_LOG_WRITE(cd->output_buffer);

		m_nextState = NNTP_RESPONSE;
		m_nextStateAfterResponse = SEND_LIST_SEARCHES_RESPONSE;
		SetFlag(NNTP_PAUSE_FOR_READ);
	}
	else
	{
		/* since SEARCH isn't supported, move on to GET */
		m_nextState = NNTP_GET_PROPERTIES;
		ClearFlag(NNTP_PAUSE_FOR_READ);
	}

	return ce->status;
}

PRInt nsNNTPProtocol::SendListSearchesResponse()
{
	char *line = NULL;
	ce->status = NET_BufferedReadLine (ce->socket, &line, &m_dataBuf,
		&m_dataBufSize, (Bool*)&cd->pause_for_read);

	if(ce->status == 0)
	{
		m_nextState = NNTP_ERROR;
		ClearFlag(NNTP_PAUSE_FOR_READ);
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return MK_NNTP_SERVER_ERROR;
	}
	if (!line)
		return ce->status;  /* no line yet */
	if (ce->status < 0)
	{
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, SOCKET_ERRNO);
		/* return TCP error */
		return MK_TCP_READ_ERROR;
	}

	if ('.' != line[0])
	{
		m_newsHost->AddSearchableGroup(line);
	}
	else
	{
		/* all searchable groups received */
		/* LIST SRCHFIELDS is legal if the server supports the SEARCH extension, which */
		/* we already know it does */
		m_nextState = NNTP_LIST_SEARCH_HEADERS;
		ClearFlag(NNTP_PAUSE_FOR_READ); 
	}

	return ce->status;
}

PRInt nsNNTPProtocol::SendListSearchHeaders()
{
    PL_strcpy(cd->output_buffer, "LIST SRCHFIELDS" CRLF);
    ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
	NNTP_LOG_WRITE(cd->output_buffer);

	m_nextState = NNTP_RESPONSE;
	m_nextStateAfterResponse = NNTP_LIST_SEARCH_HEADERS_RESPONSE;
	SetFlag(NNTP_PAUSE_FOR_READ);

	return ce->status;
}

PRInt nsNNTPProtocol::SendListSearchHeadersResponse()
{
    nsIMsgNewsHost* news_host = m_newsHost;

	char *line = NULL;
	ce->status = NET_BufferedReadLine (ce->socket, &line, &m_dataBuf,
		&m_dataBufSize, (Bool*)&cd->pause_for_read);

	if(ce->status == 0)
	{
		m_nextState = NNTP_ERROR;
		ClearFlag(NNTP_PAUSE_FOR_READ);
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return MK_NNTP_SERVER_ERROR;
	}
	if (!line)
		return ce->status;  /* no line yet */
	if (ce->status < 0)
	{
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, SOCKET_ERRNO);
		/* return TCP error */
		return MK_TCP_READ_ERROR;
	}

	if ('.' != line[0])
        news_host->AddSearchableHeader(line);
	else
	{
		m_nextState = NNTP_GET_PROPERTIES;
		ClearFlag(NNTP_PAUSE_FOR_READ);
	}

	return ce->status;
}

PRInt nsNNTPProtocol::GetProperties()
{
    nsresult rv;
    PRBool setget=FALSE;
    
    rv = m_newsHost->QueryExtension("SETGET",&setget);
    if (NS_SUCCEEDED(rv) && setget)
	{
		PL_strcpy(cd->output_buffer, "GET" CRLF);
		ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
		NNTP_LOG_WRITE(cd->output_buffer);

		m_nextState = NNTP_RESPONSE;
		m_nextStateAfterResponse = NNTP_GET_PROPERTIES_RESPONSE;
		SetFlag(NNTP_PAUSE_FOR_READ);
	}
	else
	{
		/* since GET isn't supported, move on LIST SUBSCRIPTIONS */
		m_nextState = SEND_LIST_SUBSCRIPTIONS;
		ClearFlag(NNTP_PAUSE_FOR_READ);
	}
	return ce->status;
}

PRInt nsNNTPProtocol::GetPropertiesResponse()
{
	char *line = NULL;
	ce->status = NET_BufferedReadLine (ce->socket, &line, &m_dataBuf,
		&m_dataBufSize, (Bool*)&cd->pause_for_read);

	if(ce->status == 0)
	{
		m_nextState = NNTP_ERROR;
		ClearFlag(NNTP_PAUSE_FOR_READ);
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return MK_NNTP_SERVER_ERROR;
	}
	if (!line)
		return ce->status;  /* no line yet */
	if (ce->status < 0)
	{
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, SOCKET_ERRNO);
		/* return TCP error */
		return MK_TCP_READ_ERROR;
	}

	if ('.' != line[0])
	{
		char *propertyName = PL_strdup(line);
		if (propertyName)
		{
			char *space = PL_strchr(propertyName, ' ');
			if (space)
			{
				char *propertyValue = space + 1;
				*space = '\0';
                m_newsHost->AddPropertyForGet(propertyName, propertyValue);
			}
			PR_Free(propertyName);
		}
	}
	else
	{
		/* all GET properties received, move on to LIST SUBSCRIPTIONS */
		m_nextState = SEND_LIST_SUBSCRIPTIONS;
		ClearFlag(NNTP_PAUSE_FOR_READ);
	}

	return ce->status;
}

PRInt nsNNTPProtocol::SendListSubscriptions()
{
   
#if 0
    nsresult rv;
    PRBool searchable=FALSE;
    rv = m_newsHost->QueryExtension("LISTSUBSCR",&listsubscr);
    if (NS_SUCCEEDED(rv) && listsubscr)
#else
	if (0)
#endif
	{
		PL_strcpy(cd->output_buffer, "LIST SUBSCRIPTIONS" CRLF);
		ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
		NNTP_LOG_WRITE(cd->output_buffer);

		m_nextState = NNTP_RESPONSE;
		m_nextStateAfterResponse = SEND_LIST_SUBSCRIPTIONS_RESPONSE;
		SetFlag(NNTP_PAUSE_FOR_READ);
	}
	else
	{
		/* since LIST SUBSCRIPTIONS isn't supported, move on to real work */
		m_nextState = SEND_FIRST_NNTP_COMMAND;
		ClearFlag(NNTP_PAUSE_FOR_READ);
	}

	return ce->status;
}

PRInt nsNNTPProtocol::SendListSubscriptionsResponse()
{
	char *line = NULL;
	ce->status = NET_BufferedReadLine (ce->socket, &line, &m_dataBuf,
		&m_dataBufSize, (Bool*)&cd->pause_for_read);

	if(ce->status == 0)
	{
		m_nextState = NNTP_ERROR;
		ClearFlag(NNTP_PAUSE_FOR_READ);
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return MK_NNTP_SERVER_ERROR;
	}
	if (!line)
		return ce->status;  /* no line yet */
	if (ce->status < 0)
	{
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, SOCKET_ERRNO);
		/* return TCP error */
		return MK_TCP_READ_ERROR;
	}

	if ('.' != line[0])
	{
#if 0
		char *urlScheme;
		HG56946
		char *url = PR_smprintf ("%s//%s/%s", urlScheme, cd->control_con->hostname, line);
		if (url)
			MSG_AddSubscribedNewsgroup (cd->pane, url);
#endif
	}
	else
	{
		/* all default subscriptions received */
		m_nextState = SEND_FIRST_NNTP_COMMAND;
		ClearFlag(NNTP_PAUSE_FOR_READ);
	}

	return ce->status;
}

/* figure out what the first command is and send it
 *
 * returns the status from the NETWrite */

PRInt nsNNTPProtocol::SendFirstNNTPCommand()
{
	char *command=0;

	if (m_typeWanted == ARTICLE_WANTED)
	  {
		const char *group = 0;
		uint32 number = 0;

		nsresult rv;
        nsIMsgNewsgroup *newsgroup;
        rv = m_newsHost->GetNewsGroupAndNumberOfID(m_path,
                                                      &newsgroup, &number);
		if (NS_SUCCEEDED(rv) && newsgroup && number)
		  {
			m_articleNumber = number;
            m_newsgroup = newsgroup;

			if (cd->control_con->current_group && !PL_strcmp (cd->control_con->current_group, group))
			  m_nextState = NNTP_SEND_ARTICLE_NUMBER;
			else
			  m_nextState = NNTP_SEND_GROUP_FOR_ARTICLE;

			ClearFlag(NNTP_PAUSE_FOR_READ);
			return 0;
		  }
	  }

    if(m_typeWanted == NEWS_POST && !ce->URL_s->post_data)
      {
		PR_ASSERT(0);
        return(-1);
      }
    else if(m_typeWanted == NEWS_POST)
      {  /* posting to the news group */
        StrAllocCopy(command, "POST");
      }
    else if(m_typeWanted == READ_NEWS_RC)
      {
		if(ce->URL_s->method == URL_POST_METHOD ||
								PL_strchr(ce->URL_s->address, '?'))
        	m_nextState = NEWS_NEWS_RC_POST;
		else
        	m_nextState = NEWS_DISPLAY_NEWS_RC;
		return(0);
      } 
	else if(m_typeWanted == NEW_GROUPS)
	{
        PRTime last_update;
        nsresult rv;

        rv = m_newsHost->GetLastUpdatedTime(&last_update);
		char small_buf[64];
        PRExplodedTime  expandedTime;

		if(!last_update)
		{	
			ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_NEWSGROUP_SCAN_ERROR);
			m_nextState = NEWS_ERROR;
			return(MK_INTERRUPTED);
		}
	
		/* subtract some hours just to be sure */
		last_update -= NEWGROUPS_TIME_OFFSET;

        {
           int64  secToUSec, timeInSec, timeInUSec;
           LL_I2L(timeInSec, last_update);
           LL_I2L(secToUSec, PR_USEC_PER_SEC);
           LL_MUL(timeInUSec, timeInSec, secToUSec);
           PR_ExplodeTime(timeInUSec, PR_LocalTimeParameters, &expandedTime);
        }
		PR_FormatTimeUSEnglish(small_buf, sizeof(small_buf), 
                               "NEWGROUPS %y%m%d %H%M%S", &expandedTime);
		
        StrAllocCopy(command, small_buf);

	}
    else if(m_typeWanted == LIST_WANTED)
    {
		
		cd->use_fancy_newsgroup_listing = FALSE;
        PRTime last_update;
        nsresult rv = m_newsHost->GetLastUpdatedTime(&last_update);
        
        if (NS_SUCCEEDED(rv) && last_update!=0)
		{
			m_nextState = DISPLAY_NEWSGROUPS;
        	return(0);
	    }
		else
		{
#ifdef BUG_21013
			if(!FE_Confirm(ce->window_id, XP_GetString(XP_CONFIRM_SAVE_NEWSGROUPS)))
	  		  {
				m_nextState = NEWS_ERROR;
				return(MK_INTERRUPTED);
	  		  }
#endif /* BUG_21013 */

    nsresult rv;
    PRBool xactive=FALSE;
    rv = m_newsHost->QueryExtension("XACTIVE",&xactive);
    if (NS_SUCCEEDED(rv) && xactive)
	{
		StrAllocCopy(command, "LIST XACTIVE");
		cd->use_fancy_newsgroup_listing = TRUE;
	}
	else
	{
		StrAllocCopy(command, "LIST");
	}

    else if(m_typeWanted == GROUP_WANTED) 
    {
        /* Don't use MKParse because the news: access URL doesn't follow traditional
         * rules. For instance, if the article reference contains a '#',
         * the rest of it is lost.
         */
        char * slash;
        char * group_name;
        nsresult rv;

        StrAllocCopy(command, "GROUP ");
        rv = m_newsgroup->GetName(&group_name);
        slash = PL_strchr(group_name, '/');
        m_firstArticle = 0;
        m_lastArticle = 0;
        if (slash)
		{
            *slash = '\0';
            (void) sscanf(slash+1, "%d-%d", &m_firstArticle, &m_lastArticle);
		}

        StrAllocCopy (cd->control_con->current_group, group_name);
        StrAllocCat(command, cd->control_con->current_group);
      }
	else if (m_typeWanted == SEARCH_WANTED)
	{
		nsresult rv;
		PRBool searchable=FALSE;
		rv = m_newsHost->QueryExtension("SEARCH", &searchable);
		if (NS_SUCCEEDED(rv) && searchable)
		{
			/* use the SEARCH extension */
			char *slash = PL_strchr (cd->command_specific_data, '/');
			if (slash)
			{
				char *allocatedCommand = MSG_UnEscapeSearchUrl (slash + 1);
				if (allocatedCommand)
				{
					StrAllocCopy (command, allocatedCommand);
					PR_Free(allocatedCommand);
				}
			}
			m_nextState = NNTP_RESPONSE;
			m_nextStateAfterResponse = NNTP_SEARCH_RESPONSE;
		}
		else
		{
            nsresult rv;
            char *group_name;
            
			/* for XPAT, we have to GROUP into the group before searching */
			StrAllocCopy (command, "GROUP ");
            rv = m_newsgroup->GetName(&group_name);
            StrAllocCat (command, group_name);
			m_nextState = NNTP_RESPONSE;
			m_nextStateAfterResponse = NNTP_XPAT_SEND;
		}
	}
	else if (m_typeWanted == PRETTY_NAMES_WANTED)
	{
		nsresult rv;
		PRBool listpretty=FALSE;
		rv = m_newsHost->QueryExtension("LISTPRETTY",&listpretty);
		if (NS_SUCCEEDED(rv) && listpretty)
		{
			m_nextState = NNTP_LIST_PRETTY_NAMES;
			return 0;
		}
		else
		{
			PR_ASSERT(FALSE);
			m_nextState = NNTP_ERROR;
		}
	}
	else if (m_typeWanted == PROFILE_WANTED)
	{
		char *slash = PL_strchr (cd->command_specific_data, '/');
		if (slash)
		{
			char *allocatedCommand = MSG_UnEscapeSearchUrl (slash + 1);
			if (allocatedCommand)
			{
				StrAllocCopy (command, allocatedCommand);
				PR_Free(allocatedCommand);
			}
		}
		m_nextState = NNTP_RESPONSE;
		if (PL_strstr(ce->URL_s->address, "PROFILE NEW"))
			m_nextStateAfterResponse = NNTP_PROFILE_ADD_RESPONSE;
		else
			m_nextStateAfterResponse = NNTP_PROFILE_DELETE_RESPONSE;
	}
	else if (m_typeWanted == IDS_WANTED)
	{
		m_nextState = NNTP_LIST_GROUP;
		return 0;
	}
    else  /* article or cancel */
	{
		if (m_typeWanted == CANCEL_WANTED)
			StrAllocCopy(command, "HEAD ");
		else
			StrAllocCopy(command, "ARTICLE ");
		if (*m_path != '<')
			StrAllocCat(command,"<");
		StrAllocCat(command, m_path);
		if (PL_strchr(command+8, '>')==0) 
			StrAllocCat(command,">");
	}

    StrAllocCat(command, CRLF);
    ce->status = (int) NET_BlockingWrite(ce->socket, command, PL_strlen(command));
	NNTP_LOG_WRITE(command);
    PR_Free(command);

	m_nextState = NNTP_RESPONSE;
	if (m_typeWanted != SEARCH_WANTED && m_typeWanted != PROFILE_WANTED)
		m_nextStateAfterResponse = SEND_FIRST_NNTP_COMMAND_RESPONSE;
	SetFlag(NNTP_PAUSE_FOR_READ);
    return(ce->status);

} /* sent first command */


/* interprets the server response from the first command sent
 *
 * returns negative if the server responds unexpectedly 
 */

PRInt nsNNTPProtocol::SendFirstNNTPCommandResponse()
{
	int major_opcode = MK_NNTP_RESPONSE_TYPE(m_responseCode);

	if((major_opcode == MK_NNTP_RESPONSE_TYPE_CONT &&
        m_typeWanted == NEWS_POST)
	 	|| (major_opcode == MK_NNTP_RESPONSE_TYPE_OK &&
            m_typeWanted != NEWS_POST) )
      {

        m_nextState = SETUP_NEWS_STREAM;
		cd->some_protocol_succeeded = TRUE;

        return(0);  /* good */
      }
    else
      {
		if (m_responseCode == MK_NNTP_RESPONSE_GROUP_NO_GROUP &&
            m_typeWanted == GROUP_WANTED)
            m_newsHost->GroupNotFound(cd->control_con->current_group,
                                         TRUE /* opening */);
		return net_display_html_error_state(ce);
      }

	/* start the graph progress indicator
     */
    FE_GraphProgressInit(ce->window_id, ce->URL_s, ce->URL_s->content_length);
    cd->destroy_graph_progress = TRUE;  /* we will need to destroy it */
    m_originalContentLength = ce->URL_s->content_length;

	return(ce->status);
}

PRInt nsNNTPProtocol::SendGroupForArticle()
{
  nsresult rv;
  PR_FREEIF(cd->control_con->current_group);
  rv = m_newsgroup->GetName(&cd->control_con->current_group);
  PR_ASSERT(NS_SUCCEEEDED(rv));
  
  PR_snprintf(cd->output_buffer, 
			OUTPUT_BUFFER_SIZE, 
			"GROUP %.512s" CRLF, 
			cd->control_con->current_group);

   ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,
									  PL_strlen(cd->output_buffer));
	NNTP_LOG_WRITE(cd->output_buffer);

    m_nextState = NNTP_RESPONSE;
    m_nextStateAfterResponse = NNTP_SEND_GROUP_FOR_ARTICLE_RESPONSE;
    SetFlag(NNTP_PAUSE_FOR_READ);

    return(ce->status);
}

PRInt nsNNTPProtocol::SendGroupForArticleResponse()
{
  /* ignore the response code and continue
   */
  m_nextState = NNTP_SEND_ARTICLE_NUMBER;

  return(0);
}


PRInt nsNNTPProtocol::SendArticleNumber()
{
	PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "ARTICLE %lu" CRLF, 
			m_articleNumber);

  ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,
									  PL_strlen(cd->output_buffer));
	NNTP_LOG_WRITE(cd->output_buffer);

    m_nextState = NNTP_RESPONSE;
    m_nextStateAfterResponse = SEND_FIRST_NNTP_COMMAND_RESPONSE;
    SetFlag(NNTP_PAUSE_FOR_READ);

    return(ce->status);
}


PRInt nsNNTPProtocol::BeginArticle()
{
  if (m_typeWanted != ARTICLE_WANTED &&
	  m_typeWanted != CANCEL_WANTED)
	return 0;

  /*  Set up the HTML stream
   */ 
  FREEIF (ce->URL_s->content_type);
  ce->URL_s->content_type = PL_strdup (MESSAGE_RFC822);

#ifdef NO_ARTICLE_CACHEING
  ce->format_out = CLEAR_CACHE_BIT (ce->format_out);
#endif

  if (m_typeWanted == CANCEL_WANTED)
	{
	  PR_ASSERT(ce->format_out == FO_PRESENT);
	  ce->format_out = FO_PRESENT;
	}

  /* Only put stuff in the fe_data if this URL is going to get
	 passed to MIME_MessageConverter(), since that's the only
	 thing that knows what to do with this structure. */
  if (CLEAR_CACHE_BIT(ce->format_out) == FO_PRESENT)
	{
	  ce->status = net_InitializeNewsFeData (ce);
	  if (ce->status < 0)
		{
		  /* #### what error message? */
		  return ce->status;
		}
	}

  cd->stream = NET_StreamBuilder(ce->format_out, ce->URL_s, ce->window_id);
  PR_ASSERT (cd->stream);
  if (!cd->stream) return -1;

  m_nextState = NNTP_READ_ARTICLE;

  return 0;
}

PRInt nsNNTPProtocol::ReadArticle()
{
	char *line;
	ce->status = NET_BufferedReadLine(ce->socket, &line,
									 &cd->data_buf, 
									 &cd->data_buf_size,
									 (Bool*)&cd->pause_for_read);

	if(ce->status == 0)
	{
		m_nextState = NNTP_ERROR;
		ClearFlag(NNTP_PAUSE_FOR_READ);
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return(MK_NNTP_SERVER_ERROR);
	}

	if(ce->status > 1)
	{
		ce->bytes_received += ce->status;
		//	FE_GraphProgress(ce->window_id, ce->URL_s,
		//					 ce->bytes_received, ce->status,
		//					 ce->URL_s->content_length);
	}

	if(!line)
	  return(ce->status);  /* no line yet or error */
	
	if (m_typeWanted == CANCEL_WANTED && m_responseCode != MK_NNTP_RESPONSE_ARTICLE_HEAD)
	{
		/* HEAD command failed. */
		return MK_NNTP_CANCEL_ERROR;
	}

	if (line[0] == '.' && line[1] == 0)
	{
		if (m_typeWanted == CANCEL_WANTED)
			m_nextState = NEWS_START_CANCEL;
		else
			m_nextState = NEWS_DONE;
		ClearFlag(NNTP_PAUSE_FOR_READ);
	}
	else
	{
		if (line[0] == '.')
			PL_strcpy (cd->output_buffer, line + 1);
		else
			PL_strcpy (cd->output_buffer, line);

		/* When we're sending this line to a converter (ie,
		   it's a message/rfc822) use the local line termination
		   convention, not CRLF.  This makes text articles get
		   saved with the local line terminators.  Since SMTP
		   and NNTP mandate the use of CRLF, it is expected that
		   the local system will convert that to the local line
		   terminator as it is read.
		 */
		PL_strcat (cd->output_buffer, LINEBREAK);
		/* Don't send content-type to mime parser if we're doing a cancel
		  because it confuses mime parser into not parsing.
		  */
		if (m_typeWanted != CANCEL_WANTED || XP_STRNCMP(cd->output_buffer, "Content-Type:", 13))
			ce->status = PUTSTRING(cd->output_buffer);
	}

	return 0;
}

PRInt nsNNTPProtocol::BeginAuthorization()
{
	char * command = 0;
	char * username = 0;
	char * cp;

#ifdef CACHE_NEWSGRP_PASSWORD
	/* reuse cached username from newsgroup folder info*/
	if (cd->pane && 
		(!net_news_last_username_probably_valid ||
		 (last_username_hostname && 
		  PL_strcasecmp(last_username_hostname, cd->control_con->hostname)))) 
	{
      m_newsgroup->GetUsername(&username);
	  if (username && last_username &&
		  !PL_strcmp (username, last_username) &&
		  (m_previousResponseCode == MK_NNTP_RESPONSE_AUTHINFO_OK || 
		   m_previousResponseCode == MK_NNTP_RESPONSE_AUTHINFO_SIMPLE_OK ||
		   m_previousResponseCode == MK_NNTP_RESPONSE_GROUP_SELECTED)) {
		FREEIF (username);
        m_newsgroup->SetUsername(NULL);
        m_newsgroup->SetPassword(NULL);
	  }
	}
#endif
	
	if (cd->pane) 
	{
	/* Following a snews://username:password@newhost.domain.com/newsgroup.topic
	 * backend calls MSG_Master::FindNewsHost() to locate the folderInfo and setting 
	 * the username/password to the newsgroup folderInfo
	 */
      m_newsgroup->GetUsername(&username);
	  if (username && *username)
	  {
		StrAllocCopy(last_username, username);
		StrAllocCopy(last_username_hostname, cd->control_con->hostname);
		/* use it for only once */
        m_newsgroup->SetUsername(NULL);
	  }
	  else 
	  {
		  /* empty username; free and clear it so it will work with
		   * our logic
		   */
		  FREEIF(username);
	  }
	}

	/* If the URL/cd->control_con->hostname contains @ this must be triggered
	 * from the bookmark. Use the embed username if we could.
	 */
	if ((cp = PL_strchr(cd->control_con->hostname, '@')) != NULL)
	  {
		/* in this case the username and possibly
		 * the password are in the URL
		 */
		char * colon;
		*cp = '\0';

		colon = PL_strchr(cd->control_con->hostname, ':');
		if(colon)
			*colon = '\0';

		StrAllocCopy(username, cd->control_con->hostname);
		StrAllocCopy(last_username, cd->control_con->hostname);
		StrAllocCopy(last_username_hostname, cp+1);

		*cp = '@';

		if(colon)
			*colon = ':';
	  }
	/* reuse global saved username if we think it is
	 * valid
	 */
    if (!username && net_news_last_username_probably_valid)
	  {
		if( last_username_hostname &&
			!PL_strcasecmp(last_username_hostname, cd->control_con->hostname) )
			StrAllocCopy(username, last_username);
		else
			net_news_last_username_probably_valid = FALSE;
	  }


	if (!username) {
#if defined(CookiesAndSignons)
	  username = SI_Prompt(ce->window_id,
			       XP_GetString(XP_PROMPT_ENTER_USERNAME),
                               "",
			       cd->control_con->hostname);

#else
	  username = FE_Prompt(ce->window_id,
						   XP_GetString(XP_PROMPT_ENTER_USERNAME),
						   username ? username : "");
#endif
	  
	  /* reset net_news_last_username_probably_valid to false */
	  net_news_last_username_probably_valid = FALSE;
	  if(!username) 
	  {
		ce->URL_s->error_msg = 
		  NET_ExplainErrorDetails( MK_NNTP_AUTH_FAILED, "Aborted by user");
		return(MK_NNTP_AUTH_FAILED);
	  }
	  else 
	  {
		StrAllocCopy(last_username, username);
		StrAllocCopy(last_username_hostname, cd->control_con->hostname);
	  }
	}

#ifdef CACHE_NEWSGRP_PASSWORD
    if (NS_SUCCEEDED(m_newsgroup->GetUsername(&username)) {
	  munged_username = HG64643 (username);
      m_newsgroup->SetUsername(munged_username);
	  PR_FreeIF(munged_username);
	}
#endif

	StrAllocCopy(command, "AUTHINFO user ");
	StrAllocCat(command, username);
	StrAllocCat(command, CRLF);

	ce->status = (int) NET_BlockingWrite(ce->socket, command, PL_strlen(command));
	NNTP_LOG_WRITE(command);

	FREE(command);
	FREE(username);

	m_nextState = NNTP_RESPONSE;
	m_nextStateAfterResponse = NNTP_AUTHORIZE_RESPONSE;;

	SetFlag(NNTP_PAUSE_FOR_READ);

	return ce->status;
}

PRInt nsNNTPProtocol::AuthorizeResponse()
{
    if (MK_NNTP_RESPONSE_AUTHINFO_OK == m_responseCode ||
        MK_NNTP_RESPONSE_AUTHINFO_SIMPLE_OK == m_responseCode) 
	  {
		/* successful login */
        nsresult rv;
        PRBool pushAuth;
		/* If we're here because the host demanded authentication before we
		 * even sent a single command, then jump back to the beginning of everything
		 */
        rv = m_newsHost->IsPushAuth(&pushAuth);
        
        if (!cd->mode_reader_performed)
			m_nextState = NNTP_SEND_MODE_READER;
		/* If we're here because the host needs pushed authentication, then we 
		 * should jump back to SEND_LIST_EXTENSIONS
		 */
        else if (NS_SUCCEEDED(rv) && pushAuth)
			m_nextState = SEND_LIST_EXTENSIONS;
		else
			/* Normal authentication */
			m_nextState = SEND_FIRST_NNTP_COMMAND;

		net_news_last_username_probably_valid = TRUE;
		return(0); 
	  }
	else if (MK_NNTP_RESPONSE_AUTHINFO_CONT == m_responseCode)
	  {
		/* password required
		 */	
		char * command = 0;
		char * password = 0;
		char * cp;

		if (cd->pane)
		{
            m_newsgroup->GetPassword(&password);
            password = HG63218 (password);
            m_newsgroup->SetPassword(NULL);
		}

        if (net_news_last_username_probably_valid 
			&& last_password
			&& last_password_hostname
			&& !PL_strcasecmp(last_password_hostname, cd->control_con->hostname))
          {
#ifdef CACHE_NEWSGRP_PASSWORD
			if (cd->pane)
            m_newsgroup->GetPassword(&password);
            password = HG63218 (password);
#else
            StrAllocCopy(password, last_password);
#endif
          }
        else if ((cp = PL_strchr(cd->control_con->hostname, '@')) != NULL)
          {
            /* in this case the username and possibly
             * the password are in the URL
             */
            char * colon;
            *cp = '\0';
    
            colon = PL_strchr(cd->control_con->hostname, ':');
            if(colon)
			  {
                *colon = '\0';
    
            	StrAllocCopy(password, colon+1);
            	StrAllocCopy(last_password, colon+1);
            	StrAllocCopy(last_password_hostname, cp+1);

                *colon = ':';
			  }
    
            *cp = '@';
    
          }
		if (!password) {
		  password = 
#if defined(CookiesAndSignons)
		  password = SI_PromptPassword
		      (ce->window_id,
		      XP_GetString
			  (XP_PLEASE_ENTER_A_PASSWORD_FOR_NEWS_SERVER_ACCESS),
		      cd->control_con->hostname,
		      TRUE, TRUE);
#else
			FE_PromptPassword(ce->window_id, XP_GetString( 
			           XP_PLEASE_ENTER_A_PASSWORD_FOR_NEWS_SERVER_ACCESS ) );
#endif
		  net_news_last_username_probably_valid = FALSE;
		}
		  
		if(!password)	{
		  ce->URL_s->error_msg = 
			NET_ExplainErrorDetails(MK_NNTP_AUTH_FAILED, "Aborted by user");
		  return(MK_NNTP_AUTH_FAILED);
		}
		else {
		  StrAllocCopy(last_password, password);
		  StrAllocCopy(last_password_hostname, cd->control_con->hostname);
		}

#ifdef CACHE_NEWSGRP_PASSWORD
        char *garbage_password;
        nsresult rv;
        rv = m_newsgroup->GetPassword(&garbage_password);
        if (!NS_SUCCEEDED(rv)) {
          PR_Free(garbage_password);
          munged_password = HG64643(password);
          m_newsgroup->SetPassword(munged_password);
		  PR_FreeIF(munged_password);
		}
#endif

		StrAllocCopy(command, "AUTHINFO pass ");
		StrAllocCat(command, password);
		StrAllocCat(command, CRLF);
	
		ce->status = (int) NET_BlockingWrite(ce->socket, command, PL_strlen(command));
		NNTP_LOG_WRITE(command);

		FREE(command);
		FREE(password);

		m_nextState = NNTP_RESPONSE;
		m_nextStateAfterResponse = NNTP_PASSWORD_RESPONSE;
		SetFlag(NNTP_PAUSE_FOR_READ);

		return ce->status;
	  }
	else
	  {
		ce->URL_s->error_msg = NET_ExplainErrorDetails(
									MK_NNTP_AUTH_FAILED,
									m_responseText ? m_responseText : "");
#ifdef CACHE_NEWSGRP_PASSWORD
		if (cd->pane)
          m_newsgroup->SetUsername(NULL);
#endif
		net_news_last_username_probably_valid = FALSE;

        return(MK_NNTP_AUTH_FAILED);
	  }
		
	PR_ASSERT(0); /* should never get here */
	return(-1);

}

PRInt nsNNTPProtocol::PasswordResponse()
{

    if (MK_NNTP_RESPONSE_AUTHINFO_OK == m_responseCode ||
        MK_NNTP_RESPONSE_AUTHINFO_SIMPLE_OK == m_responseCode) 
	  {
        /* successful login */
        nsresult rv = NS_OK;
        PRBool pushAuth;
		/* If we're here because the host demanded authentication before we
		 * even sent a single command, then jump back to the beginning of everything
		 */
        rv = m_newsHost->IsPushAuth(&pushAuth);
        
		if (!cd->mode_reader_performed)
			m_nextState = NNTP_SEND_MODE_READER;
		/* If we're here because the host needs pushed authentication, then we 
		 * should jump back to SEND_LIST_EXTENSIONS
		 */
        else if (NS_SUCCEEDED(rv) && pushAuth)
			m_nextState = SEND_LIST_EXTENSIONS;
		else
			/* Normal authentication */
			m_nextState = SEND_FIRST_NNTP_COMMAND;

		net_news_last_username_probably_valid = TRUE;
        rv = m_xoverParser->Reset();
        return(0);
	  }
	else
	  {
		ce->URL_s->error_msg = NET_ExplainErrorDetails(
									MK_NNTP_AUTH_FAILED,
									m_responseText ? m_responseText : "");
#ifdef CACHE_NEWSGRP_PASSWORD
		if (cd->pane)
          m_newsgroup->SetPassword(NULL);
#endif
        return(MK_NNTP_AUTH_FAILED);
	  }
		
	PR_ASSERT(0); /* should never get here */
	return(-1);
}

PRInt nsNNTPProtocol::DisplayNewsgroups()
{
	m_nextState = NEWS_DONE;
    ClearFlag(NNTP_PAUSE_FOR_READ);

	NNTP_LOG_NOTE(("about to display newsgroups. path: %s",m_path));

#if 0
	/* #### Now ignoring "news:alt.fan.*"
	   Need to open the root tree of the default news host and keep
	   opening one child at each level until we've exhausted the
	   wildcard...
	 */
	if(rv < 0)
       return(rv);  
	else
#endif
       return(MK_DATA_LOADED);  /* all finished */
}

PRInt nsNNTPProtocol::BeginNewsgroups();
{
	m_nextState = NNTP_NEWGROUPS;
	NET_Progress(ce->window_id, XP_GetString(XP_PROGRESS_RECEIVE_NEWSGROUP));

	ce->bytes_received = 0;

	return(ce->status);
}

PRInt nsNNTPProtocol::ProcessNewsgroups()
{
	char *line, *s, *s1=NULL, *s2=NULL, *flag=NULL;
	int32 oldest, youngest;

    ce->status = NET_BufferedReadLine(ce->socket, &line, &m_dataBuf,
                                        &m_dataBufSize, (Bool*)&cd->pause_for_read);

    if(ce->status == 0)
      {
        m_nextState = NNTP_ERROR;
        ClearFlag(NNTP_PAUSE_FOR_READ);
        ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
        return(MK_NNTP_SERVER_ERROR);
      }

    if(!line)
        return(ce->status);  /* no line yet */

    if(ce->status<0)
      {
        ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, SOCKET_ERRNO);

        /* return TCP error
         */
        return MK_TCP_READ_ERROR;
      }

    /* End of list? 
	 */
    if (line[0]=='.' && line[1]=='\0')
	{
		ClearFlag(NNTP_PAUSE_FOR_READ);
	    nsresult rv;
		PRBool xactive=FALSE;
		rv = m_newsHost->QueryExtension("XACTIVE",&xactive);
		if (NS_SUCCEEDED(rv) && xactive)
		{
          nsresult rv;
          rv = m_newsHost->GetFirstGroupNeedingExtraInfo(&m_newsgroup);
		  if (NS_SUCCEEDED(rv) && m_newsgroup)
		  {
				m_nextState = NNTP_LIST_XACTIVE;
#ifdef DEBUG_bienvenu1
				PR_LogPrint("listing xactive for %s\n", m_groupName);
#endif
				return 0;
		  }
		}
		m_nextState = NEWS_DONE;

		if(ce->bytes_received == 0)
		{
			/* #### no new groups */
		}

		if(ce->status > 0)
        	return MK_DATA_LOADED;
		else
        	return ce->status;
    }
    else if (line [0] == '.' && line [1] == '.')
      /* The NNTP server quotes all lines beginning with "." by doubling it. */
      line++;

    /* almost correct
     */
    if(ce->status > 1)
      {
        ce->bytes_received += ce->status;
        FE_GraphProgress(ce->window_id, ce->URL_s, ce->bytes_received, ce->status, ce->URL_s->content_length);
      }

    /* format is "rec.arts.movies.past-films 7302 7119 y"
	 */
	s = PL_strchr (line, ' ');
	if (s)
	  {
		*s = 0;
		s1 = s+1;
		s = PL_strchr (s1, ' ');
		if (s)
		  {
			*s = 0;
			s2 = s+1;
			s = PL_strchr (s2, ' ');
			if (s)
			{
			  *s = 0;
			  flag = s+1;
			}
		  }
	  }
	youngest = s2 ? atol(s1) : 0;
	oldest   = s1 ? atol(s2) : 0;

	ce->bytes_received++;  /* small numbers of groups never seem to trigger this */
	m_newsHost->AddNewNewsgroup(line, oldest, youngest, flag, PR_FALSE);

    nsresult rv;
    PRBool xactive=FALSE;
    rv = m_newsHost->QueryExtension("XACTIVE",&xactive);
    if (NS_SUCCEEDED(rv) && xactive)
	{
      m_newsHost->SetGroupNeedsExtraInfo(line, TRUE);
	}
    return(ce->status);
}

/* Ahhh, this like print's out the headers and stuff
 *
 * always returns 0
 */
	 
PRInt nsNNTPProtocol::BeginReadNewsList()
{
    m_nextState = NNTP_READ_LIST;

	NET_Progress(ce->window_id, XP_GetString(XP_PROGRESS_RECEIVE_NEWSGROUP));
	 
    return(ce->status);
}

/* display a list of all or part of the newsgroups list
 * from the news server
 */

PRInt nsNNTPProtocol::ReadNewsList()
{
    char * line;
    char * description;
    int i=0;
   
    ce->status = NET_BufferedReadLine(ce->socket, &line, &m_dataBuf, 
                       					&m_dataBufSize, (Bool*) &cd->pause_for_read);
    if(ce->status == 0)
    {
        m_nextState = NNTP_ERROR;
        ClearFlag(NNTP_PAUSE_FOR_READ);
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
        return(MK_NNTP_SERVER_ERROR);
    }

    if(!line)
        return(ce->status);  /* no line yet */

    if(ce->status<0)
	{
        ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, SOCKET_ERRNO);

        /* return TCP error
         */
        return MK_TCP_READ_ERROR;
	}

            /* End of list? */
    if (line[0]=='.' && line[1]=='\0')
    {
	    nsresult rv;
		PRBool listpnames=FALSE;
		rv = m_newsHost->QueryExtension("LISTPNAMES",&listpnames);
		if (NS_SUCCEEDED(rv) && listpnames)
			m_nextState = NNTP_LIST_PRETTY_NAMES;
		else
			m_nextState = DISPLAY_NEWSGROUPS;
        ClearFlag(NNTP_PAUSE_FOR_READ);
        return 0;  
    }
	else if (line [0] == '.' && line [1] == '.')
	  /* The NNTP server quotes all lines beginning with "." by doubling it. */
	  line++;

    /* almost correct
     */
    if(ce->status > 1)
    {
    	ce->bytes_received += ce->status;
        FE_GraphProgress(ce->window_id, ce->URL_s, ce->bytes_received, ce->status, ce->URL_s->content_length);
	 }
    
	 /* find whitespace seperator if it exits */
    for(i=0; line[i] != '\0' && !NET_IS_SPACE(line[i]); i++)
        ;  /* null body */

    if(line[i] == '\0')
        description = &line[i];
    else
        description = &line[i+1];

    line[i] = 0; /* terminate group name */

	/* store all the group names 
	 */
    m_newsHost->AddNewNewsgroup(line, 0, 0, "", FALSE);
    return(ce->status);
}

/* start the xover command
 */

PRInt nsNNTPProtocol::BeginReadXover()
{
    int32 count;     /* Response fields */

	/* Make sure we never close and automatically reopen the connection at this
	   point; we'll confuse libmsg too much... */

	cd->some_protocol_succeeded = TRUE;

	/* We have just issued a GROUP command and read the response.
	   Now parse that response to help decide which articles to request
	   xover data for.
	 */
    sscanf(m_responseText,
		   "%d %d %d", 
		   &count, 
		   &m_firstPossibleArticle, 
		   &m_lastPossibleArticle);

	/* We now know there is a summary line there; make sure it has the
	   right numbers in it. */
    nsresult rv;
    char *group_name;
    m_newsgroup->GetName(&group_name);
    
    m_newsHost->DisplaySubscribedGroup(group_name,
                                          m_firstPossibleArticle,
                                          m_lastPossibleArticle,
                                          count, TRUE);
    PR_Free(group_name);
	if (ce->status < 0) return ce->status;

	m_numArticlesLoaded = 0;
	m_numArticlesWanted = net_NewsChunkSize > 0 ? net_NewsChunkSize : 1L << 30;

	m_nextState = NNTP_FIGURE_NEXT_CHUNK;
	ClearFlag(NNTP_PAUSE_FOR_READ);
	return 0;
}

PRInt nsNNTPProtocol::FigureNextChunk
{
    nsresult rv;
	char *host_and_port = NET_ParseURL (ce->URL_s->address, GET_HOST_PART);

	if (!host_and_port) return MK_OUT_OF_MEMORY;

	if (m_firstArticle > 0) 
	{
      nsresult rv;
      /* XXX - parse state stored in MSG_Pane cd->pane */
      rv = m_articleList->AddToKnownArticles(m_firstArticle,
                                                m_lastArticle);
      
	  if (ce->status < 0) 
	  {
		FREEIF (host_and_port);
		return ce->status;
	  }
	}
										 

	if (m_numArticlesLoaded >= m_numArticlesWanted) 
	{
	  FREEIF (host_and_port);
	  m_nextState = NEWS_PROCESS_XOVER;
	  ClearFlag(NNTP_PAUSE_FOR_READ);
	  return 0;
	}


    /* XXX - parse state stored in MSG_Pane cd->pane */
    rv =
      m_articleList->GetRangeOfArtsToDownload(&ce->status,
                                                 m_firstPossibleArticle,
                                                 m_lastPossibleArticle,
                                                 m_numArticlesWanted -
                                                 m_numArticlesLoaded,
                                                 &(m_firstArticle),
                                                 &(m_lastArticle));
	if (ce->status < 0) 
	{
	  FREEIF (host_and_port);
	  return ce->status;
	}


	if (m_firstArticle <= 0 || m_firstArticle > m_lastArticle) 
	{
	  /* Nothing more to get. */
	  FREEIF (host_and_port);
	  m_nextState = NEWS_PROCESS_XOVER;
	  ClearFlag(NNTP_PAUSE_FOR_READ);
	  return 0;
	}

    NNTP_LOG_NOTE(("    Chunk will be (%ld-%ld)", m_firstArticle, m_lastArticle));

	m_articleNumber = m_firstArticle;

    rv = NS_NewMsgXOVERParser(&m_xoverParser,
                                   m_newsHost, m_newsgroup,
                                   m_firstArticle, m_lastArticle,
                                   m_firstPossibleArticle,
                                   m_lastPossibleArticle);
    /* convert nsresult->status */
    ce->status = !NS_SUCCEEDED(rv);
	FREEIF (host_and_port);

	if (ce->status < 0) {
	  return ce->status;
	}

	ClearFlag(NNTP_PAUSE_FOR_READ);
	if (cd->control_con->no_xover) m_nextState = NNTP_READ_GROUP;
	else m_nextState = NNTP_XOVER_SEND;

	return 0;
}

PRInt nsNNTPProtocol::XoverSend()
{
    PR_snprintf(cd->output_buffer, 
				OUTPUT_BUFFER_SIZE,
				"XOVER %ld-%ld" CRLF, 
				m_firstArticle, 
				m_lastArticle);

	/* printf("XOVER %ld-%ld\n", m_firstArticle, m_lastArticle); */

	NNTP_LOG_WRITE(cd->output_buffer);

    m_nextState = NNTP_RESPONSE;
    m_nextStateAfterResponse = NNTP_XOVER_RESPONSE;
    SetFlag(NNTP_PAUSE_FOR_READ);

	NET_Progress(ce->window_id, XP_GetString(XP_PROGRESS_RECEIVE_LISTARTICLES));

    return((int) NET_BlockingWrite(ce->socket, 
								   cd->output_buffer, 
								   PL_strlen(cd->output_buffer)));
	NNTP_LOG_WRITE(cd->output_buffer);

}

/* see if the xover response is going to return us data
 * if the proper code isn't returned then assume xover
 * isn't supported and use
 * normal read_group
 */

PRInt nsNNTPProtocol::ReadXoverResponse()
{
#ifdef TEST_NO_XOVER_SUPPORT
	m_responseCode = MK_NNTP_RESPONSE_CHECK_ERROR; /* pretend XOVER generated an error */
#endif

    if(m_responseCode != MK_NNTP_RESPONSE_XOVER_OK)
    {
        /* If we didn't get back "224 data follows" from the XOVER request,
		   then that must mean that this server doesn't support XOVER.  Or
		   maybe the server's XOVER support is busted or something.  So,
		   in that case, fall back to the very slow HEAD method.

		   But, while debugging here at HQ, getting into this state means
		   something went very wrong, since our servers do XOVER.  Thus
		   the assert.
         */
		/*PR_ASSERT (0);*/
		m_nextState = NNTP_READ_GROUP;
		cd->control_con->no_xover = TRUE;
    }
    else
    {
        m_nextState = NNTP_XOVER;
    }

    return(0);  /* continue */
}

/* process the xover list as it comes from the server
 * and load it into the sort list.  
 */

PRInt nsNNTPProtocol::ReadXover()
{
    char *line;
    nsresult rv;

    ce->status = NET_BufferedReadLine(ce->socket, &line, &m_dataBuf,
									 &m_dataBufSize,
									 (Bool*)&cd->pause_for_read);

    if(ce->status == 0)
    {
		NNTP_LOG_NOTE(("received unexpected TCP EOF!!!!  aborting!"));
        m_nextState = NNTP_ERROR;
        ClearFlag(NNTP_PAUSE_FOR_READ);
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
        return(MK_NNTP_SERVER_ERROR);
    }

    if(!line)
	{
		return(ce->status);  /* no line yet or TCP error */
	}

	if(ce->status<0) 
	{
        ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR,
													  SOCKET_ERRNO);

        /* return TCP error
         */
        return MK_TCP_READ_ERROR;
	  }

    if(line[0] == '.' && line[1] == '\0')
    {
		m_nextState = NNTP_FIGURE_NEXT_CHUNK;
		ClearFlag(NNTP_PAUSE_FOR_READ);
		return(0);
    }
	else if (line [0] == '.' && line [1] == '.')
	  /* The NNTP server quotes all lines beginning with "." by doubling it. */
	  line++;

    /* almost correct
     */
    if(ce->status > 1)
    {
        ce->bytes_received += ce->status;
        FE_GraphProgress(ce->window_id, ce->URL_s, ce->bytes_received, ce->status,
						 ce->URL_s->content_length);
	}

	 rv = m_xoverParser->Process(line, &ce->status);
	 PR_ASSERT(NS_SUCCEEDED(rv));

	m_numArticlesLoaded++;

    return ce->status; /* keep going */
}


/* Finished processing all the XOVER data.
 */

PRInt nsNNTPProtocol::ProcessXover()
{
    nsresult rv;
    /* xover_parse_state stored in MSG_Pane cd->pane */
    rv = m_xoverParser->Finish(0,&ce->status);

	  if (NS_SUCCEEDED(rv) && ce->status < 0) return ce->status;

	m_nextState = NEWS_DONE;

    return(MK_DATA_LOADED);
}

PRInt nsNNTPProtocol::ReadNewsgroup()
{
	if(m_articleNumber > m_lastArticle)
    {  /* end of groups */

		m_nextState = NNTP_FIGURE_NEXT_CHUNK;
		ClearFlag(NNTP_PAUSE_FOR_READ);
		return(0);
    }
    else
    {
        PR_snprintf(cd->output_buffer, 
				   OUTPUT_BUFFER_SIZE,  
				   "HEAD %ld" CRLF, 
				   m_articleNumber++);
        m_nextState = NNTP_RESPONSE;
		m_nextStateAfterResponse = NNTP_READ_GROUP_RESPONSE;

        SetFlag(NNTP_PAUSE_FOR_READ);

		NNTP_LOG_WRITE(cd->output_buffer);
        return((int) NET_BlockingWrite(ce->socket, cd->output_buffer, PL_strlen(cd->output_buffer)));
    }
}

/* See if the "HEAD" command was successful
 */

PRInt nsNNTPProtocol::ReadNewsgroupResponse()
{
  nsresult rv;
  
  if (m_responseCode == MK_NNTP_RESPONSE_ARTICLE_HEAD)
  {     /* Head follows - parse it:*/
	  m_nextState = NNTP_READ_GROUP_BODY;

	  if(cd->message_id)
		*cd->message_id = '\0';

	  /* Give the message number to the header parser. */
      rv = m_xoverParser->ProcessNonXOVER(m_responseText);
      /* convert nsresult->status */
      return !NS_SUCCEEDED(rv);
  }
  else
  {
	  NNTP_LOG_NOTE(("Bad group header found!"));
	  m_nextState = NNTP_READ_GROUP;
	  return(0);
  }
}

/* read the body of the "HEAD" command
 */
PRInt nsNNTPProtocol::ReadNewsgroupBody()
{
  char *line;
  nsresult rv;

  ce->status = NET_BufferedReadLine(ce->socket, &line, &m_dataBuf,
								   &m_dataBufSize, (Bool*)&cd->pause_for_read);

  if(ce->status == 0)
  {
	  m_nextState = NNTP_ERROR;
	  ClearFlag(NNTP_PAUSE_FOR_READ);
	  ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
	  return(MK_NNTP_SERVER_ERROR);
  }

  /* if TCP error of if there is not a full line yet return
   */
  if(!line)
	return ce->status;

  if(ce->status < 0)
  {
	  ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, SOCKET_ERRNO);

	  /* return TCP error
	   */
	  return MK_TCP_READ_ERROR;
  }

  NNTP_LOG_NOTE(("read_group_body: got line: %s|",line));

  /* End of body? */
  if (line[0]=='.' && line[1]=='\0')
  {
	  m_nextState = NNTP_READ_GROUP;
	  ClearFlag(NNTP_PAUSE_FOR_READ);
  }
  else if (line [0] == '.' && line [1] == '.')
	/* The NNTP server quotes all lines beginning with "." by doubling it. */
	line++;

  rv = m_xoverParser->ProcessNonXOVER(line);
  /* convert nsresult->status */
  return !NS_SUCCEEDED(rv);
}


PRInt nsNNTPProtocol::PostData()
{
    /* returns 0 on done and negative on error
     * positive if it needs to continue.
     */
    ce->status = NET_WritePostData(ce->window_id, ce->URL_s,
                                  ce->socket,
								  &cd->write_post_data_data,
								  TRUE);

    SetFlag(NNTP_PAUSE_FOR_READ);

    if(ce->status == 0)
    {
        /* normal done
         */
        PL_strcpy(cd->output_buffer, CRLF "." CRLF);
        NNTP_LOG_WRITE(cd->output_buffer);
        ce->status = (int) NET_BlockingWrite(ce->socket,
                                            cd->output_buffer,
                                            PL_strlen(cd->output_buffer));
		NNTP_LOG_WRITE(cd->output_buffer);

		NET_Progress(ce->window_id,
					XP_GetString(XP_MESSAGE_SENT_WAITING_NEWS_REPLY));

        NET_ClearConnectSelect(ce->window_id, ce->socket);
#ifdef XP_WIN
		if(cd->calling_netlib_all_the_time)
		{
			cd->calling_netlib_all_the_time = FALSE;
#if 0
          /* this should be handled by NET_ClearCallNetlibAllTheTime */
			net_call_all_the_time_count--;
			if(net_call_all_the_time_count == 0)
#endif
				NET_ClearCallNetlibAllTheTime(ce->window_id,"mknews");
		}
#endif
        NET_SetReadSelect(ce->window_id, ce->socket);
		ce->con_sock = 0;

        m_nextState = NNTP_RESPONSE;
        m_nextStateAfterResponse = NNTP_SEND_POST_DATA_RESPONSE;
        return(0);
      }

    return(ce->status);

}


/* interpret the response code from the server
 * after the post is done
 */   
PRInt nsNNTPProtocol::PostDataResponse()
{
	if (m_responseCode != MK_NNTP_RESPONSE_POST_OK) 
	{
	  ce->URL_s->error_msg =
		NET_ExplainErrorDetails(MK_NNTP_ERROR_MESSAGE, 
								m_responseText ? m_responseText : "");
	  if (m_responseCode == MK_NNTP_RESPONSE_POST_FAILED 
		  && MSG_GetPaneType(cd->pane) == MSG_COMPOSITIONPANE
		  && MSG_IsDuplicatePost(cd->pane) &&
		  MSG_GetCompositionMessageID(cd->pane)) {
		/* The news server won't let us post.  We suspect that we're submitting
		   a duplicate post, and that's why it's failing.  So, let's go see
		   if there really is a message out there with the same message-id.
		   If so, we'll just silently pretend everything went well. */
		PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "STAT %s" CRLF,
					MSG_GetCompositionMessageID(cd->pane));
		m_nextState = NNTP_RESPONSE;
		m_nextStateAfterResponse = NNTP_CHECK_FOR_MESSAGE;
		NNTP_LOG_WRITE(cd->output_buffer);
		return (int) NET_BlockingWrite(ce->socket, cd->output_buffer,
									   PL_strlen(cd->output_buffer));
	  }

	  MSG_ClearCompositionMessageID(cd->pane); /* So that if the user tries
													  to just post again, we
													  won't immediately decide
													  that this was a duplicate
													  message and ignore the
													  error. */
	  m_nextState = NEWS_ERROR;
	  return(MK_NNTP_ERROR_MESSAGE);
	}
    m_nextState = NEWS_ERROR; /* even though it worked */
	ClearFlag(NNTP_PAUSE_FOR_READ);
    return(MK_DATA_LOADED);
}

PRInt nsNNTPProtocol::CheckForArticle()
{
  m_nextState = NEWS_ERROR;
  if (m_responseCode >= 220 && m_responseCode <= 223) {
	/* Yes, this article is already there, we're all done. */
	return MK_DATA_LOADED;
  } 
  else 
  {
	/* The article isn't there, so the failure we had earlier wasn't due to
	   a duplicate message-id.  Return the error from that previous
	   posting attempt (which is already in ce->URL_s->error_msg). */
	MSG_ClearCompositionMessageID(cd->pane);
	return MK_NNTP_ERROR_MESSAGE;
  }
}

#define NEWS_GROUP_DISPLAY_FREQ		20

PRInt nsNNTPProtocol::DisplayNewsRC()
{
    nsresult rv;
	if(!TestFlag(NNTP_NEWSRC_PERFORMED))
	{
		SetFlag(NNTP_NEWSRC_PERFORMED);
        rv = m_newsHost->GetNumGroupsNeedingCounts(&m_newsRCListCount);
	}
	
	PL_FREEIF(cd->control_con->current_group);
    rv = m_newsHost->GetFirstGroupNeedingCounts(&cd->control_con->current_group);


	if(NS_SUCCEEDED(rv) && cd->control_con->current_group)
    {
		/* send group command to server
		 */
		int32 percent;

		PR_snprintf(NET_Socket_Buffer, OUTPUT_BUFFER_SIZE, "GROUP %.512s" CRLF,
					cd->control_con->current_group);
    	ce->status = (int) NET_BlockingWrite(ce->socket, NET_Socket_Buffer,
											PL_strlen(NET_Socket_Buffer));
		NNTP_LOG_WRITE(NET_Socket_Buffer);

		percent = (m_newsRCListCount) ?
					(int32) (100.0 * ( (double)m_newsRCListIndex / (double)m_newsRCListCount )) :
					0;
		FE_SetProgressBarPercent (ce->window_id, percent);
		
		/* only update every 20 groups for speed */
		if ((m_newsRCListCount <= NEWS_GROUP_DISPLAY_FREQ) || (m_newsRCListIndex % NEWS_GROUP_DISPLAY_FREQ) == 0 ||
									(m_newsRCListIndex == m_newsRCListCount))
		{
			char thisGroup[20];
			char totalGroups[20];
			char *statusText;
			
			PR_snprintf (thisGroup, sizeof(thisGroup), "%ld", (long) m_newsRCListIndex);
			PR_snprintf (totalGroups, sizeof(totalGroups), "%ld", (long) m_newsRCListCount);
			statusText = PR_smprintf (XP_GetString(XP_THERMO_PERCENT_FORM), thisGroup, totalGroups);
			if (statusText)
			{
				FE_Progress (ce->window_id, statusText);
				PR_Free(statusText);
			}
		}
		
		m_newsRCListIndex++;

		SetFlag(NNTP_PAUSE_FOR_READ);
		m_nextState = NNTP_RESPONSE;
		m_nextStateAfterResponse = NEWS_DISPLAY_NEWS_RC_RESPONSE;
    }
	else
	{
		if (m_newsRCListCount)
		{
			FE_SetProgressBarPercent (ce->window_id, -1);
			m_newsRCListCount = 0;
		}
		else if (m_responseCode == MK_NNTP_RESPONSE_LIST_OK)  
		{
			/*
			 * 5-9-96 jefft 
			 * If for some reason the news server returns an empty 
			 * newsgroups list with a nntp response code MK_NNTP_RESPONSE_LIST_OK -- list of
			 * newsgroups follows. We set ce->status to MK_EMPTY_NEWS_LIST
			 * to end the infinite dialog loop.
			 */
			ce->status = MK_EMPTY_NEWS_LIST;
		}
		m_nextState = NEWS_DONE;
	
		if(ce->status > -1)
		  return MK_DATA_LOADED; 
		else
		  return(ce->status);
	}

	return(ce->status); /* keep going */

}

/* Parses output of GROUP command */
PRInt nsNNTPProtocol::DisplayNewsRCResponse()
{
    NewsConData * cd = (NewsConData *) ce->con_data;

    if(m_responseCode == MK_NNTP_RESPONSE_GROUP_SELECTED)
    {
		char *num_arts = 0, *low = 0, *high = 0, *group = 0;
		int32 first_art, last_art;

		/* line looks like:
		 *     211 91 3693 3789 comp.infosystems
		 */

		num_arts = m_responseText;
		low = PL_strchr(num_arts, ' ');

		if(low)
		{
			first_art = atol(low);
			*low++ = '\0';
			high= PL_strchr(low, ' ');
		}
		if(high)
		{
			*high++ = '\0';
			group = PL_strchr(high, ' ');
		}
		if(group)
		{
			*group++ = '\0';
			/* the group name may be contaminated by "group selected" at
			   the end.  This will be space separated from the group name.
			   If a space is found in the group name terminate at that
			   point. */
			strtok(group, " ");
			last_art = atol(high);
		}

        m_newsHost->DisplaySubscribedGroup(group,
                                              low ? atol(low) : 0,
                                              high ? atol(high) : 0,
                                              atol(num_arts), FALSE);
		if (ce->status < 0)
		  return ce->status;
	  }
	  else if (m_responseCode == MK_NNTP_RESPONSE_GROUP_NO_GROUP)
	  {
          m_newsHost->GroupNotFound(cd->control_con->current_group, FALSE);
	  }
	  /* it turns out subscribe ui depends on getting this displaysubscribedgroup call,
	     even if there was an error.
	  */
	  if(m_responseCode != MK_NNTP_RESPONSE_GROUP_SELECTED)
	  {
		/* only on news server error or when zero articles
		 */
        m_newsHost->DisplaySubscribedGroup(cd->control_con->current_group,
	                                             0, 0, 0, FALSE);
	  }

	m_nextState = NEWS_DISPLAY_NEWS_RC;
		
	return 0;
}

PRInt nsNNTPProtocol::StartCancel()
{
  char *command = "POST" CRLF;

  ce->status = (int) NET_BlockingWrite(ce->socket, command, PL_strlen(command));
  NNTP_LOG_WRITE(command);

  m_nextState = NNTP_RESPONSE;
  m_nextStateAfterResponse = NEWS_DO_CANCEL;
  SetFlag(NNTP_PAUSE_FOR_READ);
  return (ce->status);
}

PRInt nsNNTPProtocol::Cancel()
{
	int status = 0;
	char *id, *subject, *newsgroups, *distribution, *other_random_headers, *body;
	char *from, *old_from, *news_url;
	int L;
	#ifdef USE_LIBMSG
	MSG_CompositionFields *fields = NULL;
	#endif 

  /* #### Should we do a more real check than this?  If the POST command
	 didn't respond with "MK_NNTP_RESPONSE_POST_SEND_NOW Ok", then it's not ready for us to throw a
	 message at it...   But the normal posting code doesn't do this check.
	 Why?
   */
  PR_ASSERT (m_responseCode == MK_NNTP_RESPONSE_POST_SEND_NOW);

  /* These shouldn't be set yet, since the headers haven't been "flushed" */
  PR_ASSERT (!m_cancelID &&
			 !m_cancelFromHdr &&
			 !m_cancelNewsgroups &&
			 !m_cancelDistribution);

  /* Write out a blank line.  This will tell mimehtml.c that the headers
	 are done, and it will call news_generate_html_header_fn which will
	 notice the fields we're interested in.
   */
  PL_strcpy (cd->output_buffer, CRLF); /* CRLF used to be LINEBREAK. 
  										 LINEBREAK is platform dependent
  										 and is only <CR> on a mac. This
										 CRLF is the protocol delimiter 
										 and not platform dependent  -km */
  ce->status = PUTSTRING(cd->output_buffer);
  if (ce->status < 0) return ce->status;

  /* Now news_generate_html_header_fn should have been called, and these
	 should have values. */
  id = m_cancelID;
  old_from = m_cancelFromHdr;
  newsgroups = m_cancelNewsgroups;
  distribution = m_cancelDistribution;

  PR_ASSERT (id && newsgroups);
  if (!id || !newsgroups) return -1; /* "unknown error"... */

  m_cancelNewsgroups = 0;
  m_cancelDistribution = 0;
  m_cancelFromHdr = 0;
  m_cancelID = 0;

  L = PL_strlen (id);

  from = MIME_MakeFromField ();
  subject = (char *) PR_Malloc (L + 20);
  other_random_headers = (char *) PR_Malloc (L + 20);
  body = (char *) PR_Malloc (PL_strlen (XP_AppCodeName) + 100);

  /* Make sure that this loser isn't cancelling someone else's posting.
	 Yes, there are occasionally good reasons to do so.  Those people
	 capable of making that decision (news admins) have other tools with
	 which to cancel postings (like telnet.)
	 Don't do this if server tells us it will validate user. DMB 3/19/97
   */
  nsresult rv;
  PRBool cancelchk=FALSE;
  rv = m_newsHost->QueryExtension("CANCELCHK",&cancelchk);
  if (NS_SUCCEEDED(rv) && cancelchk)
  {
	char *us = MSG_ExtractRFC822AddressMailboxes (from);
	char *them = MSG_ExtractRFC822AddressMailboxes (old_from);
	XP_Bool ok = (us && them && !PL_strcasecmp (us, them));
	FREEIF(us);
	FREEIF(them);
	if (!ok)
	{
		status = MK_NNTP_CANCEL_DISALLOWED;
		ce->URL_s->error_msg = PL_strdup (XP_GetString(status));
		m_nextState = NEWS_ERROR; /* even though it worked */
		ClearFlag(NNTP_PAUSE_FOR_READ);
		goto FAIL;
	}
  }

  /* Last chance to cancel the cancel.
   */
  if (!FE_Confirm (ce->window_id, XP_GetString(MK_NNTP_CANCEL_CONFIRM)))
  {
	  status = MK_NNTP_NOT_CANCELLED;
	  goto FAIL;
   }

  news_url = ce->URL_s->address;  /* we can just post here. */

  if (!from || !subject || !other_random_headers || !body)
	{
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}

  PL_strcpy (subject, "cancel ");
  PL_strcat (subject, id);

  PL_strcpy (other_random_headers, "Control: cancel ");
  PL_strcat (other_random_headers, id);
  PL_strcat (other_random_headers, CRLF);
  if (distribution)
	{
	  PL_strcat (other_random_headers, "Distribution: ");
	  PL_strcat (other_random_headers, distribution);
	  PL_strcat (other_random_headers, CRLF);
	}

  PL_strcpy (body, "This message was cancelled from within ");
  PL_strcat (body, XP_AppCodeName);
  PL_strcat (body, "." CRLF);

#ifdef USE_LIBMSG
  fields = MSG_CreateCompositionFields(from, 0, 0, 0, 0, 0, newsgroups,
									   0, 0, subject, id, other_random_headers,
									   0, 0, news_url);
#endif 
  
/* so that this would compile - will probably change later */
#if 0
									   FALSE,
									   FALSE  
									   );
#endif

#ifdef USE_LIBMSG
  if (!fields)
  {
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
  }
  HG38712
#endif 

  m_cancelStatus = 0;

#ifdef USE_LIBMSG
  MSG_StartMessageDelivery (cd->pane, (void *) ce,
							fields,
							FALSE, /* digest_p */
							TRUE,  /* dont_deliver_p */
							TEXT_PLAIN, body, PL_strlen (body),
							0, /* other attachments */
							NULL, /* multipart/related chunk */
							net_cancel_done_cb);

  /* Since there are no attachments, MSG_StartMessageDelivery will run
	 net_cancel_done_cb right away (it will be called before the return.) */

  if (!m_cancelMessageFile)
	{
	  status = m_cancelStatus;
	  PR_ASSERT (status < 0);
	  if (status >= 0) status = -1;
	  goto FAIL;
	}

  /* Now send the data - do it blocking, who cares; the message is known
	 to be very small.  First suck the whole file into memory.  Then delete
	 the file.  Then do a blocking write of the data.

	 (We could use file-posting, maybe, but I couldn't figure out how.)
   */
  {
	char *data;
	uint32 data_size, data_fp;
	XP_StatStruct st;
	int nread = 0;
	XP_File file = XP_FileOpen (m_cancelMessageFile,
								xpFileToPost, XP_FILE_READ);
	if (! file) return -1; /* "unknown error"... */
	XP_Stat (m_cancelMessageFile, &st, xpFileToPost);

	data_fp = 0;
	data_size = st.st_size + 20;
	data = (char *) PR_Malloc (data_size);
	if (! data)
	  {
		status = MK_OUT_OF_MEMORY;
		goto FAIL;
	  }

	while ((nread = XP_FileRead (data + data_fp, data_size - data_fp, file))
		   > 0)
	  data_fp += nread;
	data [data_fp] = 0;
	XP_FileClose (file);
	XP_FileRemove (m_cancelMessageFile, xpFileToPost);

	PL_strcat (data, CRLF "." CRLF CRLF);
	status = NET_BlockingWrite(ce->socket, data, PL_strlen(data));
	NNTP_LOG_WRITE(data);
	PR_Free (data);
    if (status < 0)
	  {
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_WRITE_ERROR,
													  status);
		goto FAIL;
	  }

    SetFlag(NNTP_PAUSE_FOR_READ);
	m_nextState = NNTP_RESPONSE;
	m_nextStateAfterResponse = NNTP_SEND_POST_DATA_RESPONSE;
  }
#else

  {
    /* NET_BlockingWrite() should go away soon? I think. */
    /* The following are what we really need to cancel a posted message */
    char *data;
    data = PR_smprintf("From: %s" CRLF
                       "Newsgroups: %s" CRLF
                       "Subject: %s" CRLF
                       "References: %s" CRLF
                       "%s" CRLF /* other_random_headers */
                       "%s"     /* body */
                       CRLF "." CRLF CRLF, /* trailing SMTP "." */
                       from, newsgroups, subject, id,
                       other_random_headers, body);
    
    status = NET_BlockingWrite(ce->socket, data, PL_strlen(data));
    NNTP_LOG_WRITE(data);
    PR_Free (data);
    if (status < 0)
	  {
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_WRITE_ERROR,
													  status);
		goto FAIL;
	  }

    SetFlag(NNTP_PAUSE_FOR_READ);
	m_nextState = NNTP_RESPONSE;
	m_nextStateAfterResponse = NNTP_SEND_POST_DATA_RESPONSE;
  }

#endif 

 FAIL:
  FREEIF (id);
  FREEIF (from);
  FREEIF (old_from);
  FREEIF (subject);
  FREEIF (newsgroups);
  FREEIF (distribution);
  FREEIF (other_random_headers);
  FREEIF (body);
  FREEIF (m_cancelMessageFile);

#ifdef USE_LIBMSG
  if (fields)
	  MSG_DestroyCompositionFields(fields);
#endif 

  return status;
}

PRInt nsNNTPProtocol::XPATSend()
{
	int status = 0;
	char *thisTerm = NULL;

	if (cd->current_search &&
		(thisTerm = PL_strchr(cd->current_search, '/')) != NULL)
	{
		/* extract the XPAT encoding for one query term */
/*		char *next_search = NULL; */
		char *command = NULL;
		char *unescapedCommand = NULL;
		char *endOfTerm = NULL;
		StrAllocCopy (command, ++thisTerm);
		endOfTerm = PL_strchr(command, '/');
		if (endOfTerm)
			*endOfTerm = '\0';
		StrAllocCat (command, CRLF);
	
		unescapedCommand = MSG_UnEscapeSearchUrl(command);

		/* send one term off to the server */
		NNTP_LOG_WRITE(command);
		status = NET_BlockingWrite(ce->socket, unescapedCommand, PL_strlen(unescapedCommand));
		NNTP_LOG_WRITE(unescapedCommand);

		m_nextState = NNTP_RESPONSE;
		m_nextStateAfterResponse = NNTP_XPAT_RESPONSE;
	    SetFlag(NNTP_PAUSE_FOR_READ);

		PR_Free(command);
		PR_Free(unescapedCommand);
	}
	else
	{
		m_nextState = NEWS_DONE;
		status = MK_DATA_LOADED;
	}
	return status;
}

PRInt nsNNTPProtocol::XPATResponse()
{
	char *line;
	NewsConData * cd = (NewsConData *)ce->con_data;

	if (m_responseCode != MK_NNTP_RESPONSE_XPAT_OK)
	{
		ce->URL_s->error_msg  = NET_ExplainErrorDetails(MK_NNTP_ERROR_MESSAGE, m_responseText);
    	m_nextState = NNTP_ERROR;
		ClearFlag(NNTP_PAUSE_FOR_READ);
		return MK_NNTP_SERVER_ERROR;
	}

	ce->status = NET_BufferedReadLine(ce->socket, &line,
									 &m_dataBuf, 
									 &m_dataBufSize,
									 (Bool*)&cd->pause_for_read);
	NNTP_LOG_READ(line);

	if(ce->status == 0)
	{
		m_nextState = NNTP_ERROR;
		ClearFlag(NNTP_PAUSE_FOR_READ);
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return(MK_NNTP_SERVER_ERROR);
	}

	if (line)
	{
		if (line[0] != '.')
		{
			long articleNumber;
			sscanf(line, "%ld", &articleNumber);
			MSG_AddNewsXpatHit (ce->window_id, (uint32) articleNumber);
		}
		else
		{
			/* set up the next term for next time around */
			char *nextTerm = PL_strchr(cd->current_search, '/');
			if (nextTerm)
				cd->current_search = ++nextTerm;
			else
				cd->current_search = NULL;

			m_nextState = NNTP_XPAT_SEND;
			ClearFlag(NNTP_PAUSE_FOR_READ);
			return 0;
		}
	}
	return 0;
}

PRInt nsNNTPProtocol::ListPrettyNames()
{

    char *group_name;
    nsresult rv = m_newsgroup->GetName(&group_name);
	PR_snprintf(cd->output_buffer, 
			OUTPUT_BUFFER_SIZE, 
			"LIST PRETTYNAMES %.512s" CRLF,
            NS_SUCCEEDED(rv) ? group_name : "");
    
    ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
	NNTP_LOG_WRITE(cd->output_buffer);
#ifdef DEBUG_bienvenu1
	PR_LogPrint(cd->output_buffer);
#endif
	m_nextState = NNTP_RESPONSE;
	m_nextStateAfterResponse = NNTP_LIST_PRETTY_NAMES_RESPONSE;

	return ce->status;
}

PRInt nsNNTPProtocol::ListPrettyNamesResponse()
{
	char *line;
	char *prettyName;

	if (m_responseCode != MK_NNTP_RESPONSE_LIST_OK)
	{
		m_nextState = DISPLAY_NEWSGROUPS;
/*		m_nextState = NEWS_DONE; */
		ClearFlag(NNTP_PAUSE_FOR_READ);
		return 0;
	}

	ce->status = NET_BufferedReadLine(ce->socket, &line,
									 &m_dataBuf, 
									 &m_dataBufSize,
									 (Bool*)&cd->pause_for_read);
	NNTP_LOG_READ(line);

	if(ce->status == 0)
	{
		m_nextState = NNTP_ERROR;
		ClearFlag(NNTP_PAUSE_FOR_READ);
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return(MK_NNTP_SERVER_ERROR);
	}

	if (line)
	{
		if (line[0] != '.')
		{
			int i;
			/* find whitespace seperator if it exits */
			for (i=0; line[i] != '\0' && !NET_IS_SPACE(line[i]); i++)
				;  /* null body */

			if(line[i] == '\0')
				prettyName = &line[i];
			else
				prettyName = &line[i+1];

			line[i] = 0; /* terminate group name */
			if (i > 0)
              m_newsHost->AddPrettyName(line,prettyName);
#ifdef DEBUG_bienvenu1
			PR_LogPrint("adding pretty name %s\n", prettyName);
#endif
		}
		else
		{
			m_nextState = DISPLAY_NEWSGROUPS;	/* this assumes we were doing a list */
/*			m_nextState = NEWS_DONE;	 */ /* ### dmb - don't really know */
			ClearFlag(NNTP_PAUSE_FOR_READ);
			return 0;
		}
	}
	return 0;
}

PRInt nsNNTPProtocol::ListXActive();
{ 
	char *group_name;
    nsresult rv = m_newsgroup->GetName(&group_name);
    
	PR_snprintf(cd->output_buffer, 
			OUTPUT_BUFFER_SIZE, 
			"LIST XACTIVE %.512s" CRLF,
            group_name);
    ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
	NNTP_LOG_WRITE(cd->output_buffer);

	m_nextState = NNTP_RESPONSE;
	m_nextStateAfterResponse = NNTP_LIST_XACTIVE_RESPONSE;

	return ce->status;
}

PRInt nsNNTPProtocol::ListXActiveResponse()
{
	char *line;

	PR_ASSERT(m_responseCode == MK_NNTP_RESPONSE_LIST_OK);
	if (m_responseCode != MK_NNTP_RESPONSE_LIST_OK)
	{
		m_nextState = DISPLAY_NEWSGROUPS;
/*		m_nextState = NEWS_DONE; */
		ClearFlag(NNTP_PAUSE_FOR_READ);
		return MK_DATA_LOADED;
	}

	ce->status = NET_BufferedReadLine(ce->socket, &line,
									 &m_dataBuf, 
									 &m_dataBufSize,
									 (Bool*)&cd->pause_for_read);
	NNTP_LOG_READ(line);

	if(ce->status == 0)
	{
		m_nextState = NNTP_ERROR;
		ClearFlag(NNTP_PAUSE_FOR_READ);
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return(MK_NNTP_SERVER_ERROR);
	}

	 /* almost correct
	*/
	if(ce->status > 1)
	{
		ce->bytes_received += ce->status;
		FE_GraphProgress(ce->window_id, ce->URL_s, ce->bytes_received, ce->status, ce->URL_s->content_length);
	}

	if (line)
	{
		if (line[0] != '.')
		{
			char *s = line;
			/* format is "rec.arts.movies.past-films 7302 7119 csp"
			 */
			while (*s && !NET_IS_SPACE(*s))
				s++;
			if (s)
			{
				char flags[32];	/* ought to be big enough */
				*s = 0;
				sscanf(s + 1,
					   "%d %d %31s", 
					   &m_firstPossibleArticle, 
					   &m_lastPossibleArticle,
					   flags);

                m_newsHost->AddNewNewsgroup(line,
                                          m_firstPossibleArticle,
                                          m_lastPossibleArticle, flags, TRUE);
				/* we're either going to list prettynames first, or list
                   all prettynames every time, so we won't care so much
                   if it gets interrupted. */
#ifdef DEBUG_bienvenu1
				PR_LogPrint("got xactive for %s of %s\n", line, flags);
#endif
				/*	This isn't required, because the extra info is
                    initialized to false for new groups. And it's
                    an expensive call.
				*/
				/* MSG_SetGroupNeedsExtraInfo(cd->host, line, FALSE); */
			}
		}
		else
		{
          nsresult rv;
          PRBool xactive=FALSE;
          rv = m_newsHost->QueryExtension("XACTIVE",&xactive);
          if (m_typeWanted == NEW_GROUPS &&
              NS_SUCCEEDED(rv) && xactive)
			{
                nsIMsgNewsgroup* old_newsgroup = m_newsgroup;
                m_newsHost->GetFirstGroupNeedingExtraInfo(&m_newsgroup);
                if (old_newsgroup && m_newsgroup &&
                    old_newsgroup != m_newsgroup)
                /* make sure we're not stuck on the same group */
                {
                    NS_RELEASE(old_newsgroup);
#ifdef DEBUG_bienvenu1
					PR_LogPrint("listing xactive for %s\n", m_groupName);
#endif
					m_nextState = NNTP_LIST_XACTIVE;
			        ClearFlag(NNTP_PAUSE_FOR_READ); 
					return 0;
				}
				else
				{
                    NS_RELEASE(old_newsgroup);
                    m_newsgroup = NULL;
				}
			}
            PRBool listpname;
            rv = m_newsHost->QueryExtension("LISTPNAME",&listpname);
            if (NS_SUCCEEDED(rv) && listpname)
				m_nextState = NNTP_LIST_PRETTY_NAMES;
			else
				m_nextState = DISPLAY_NEWSGROUPS;	/* this assumes we were doing a list - who knows? */
/*			m_nextState = NEWS_DONE;	 */ /* ### dmb - don't really know */
			ClearFlag(NNTP_PAUSE_FOR_READ);
			return 0;
		}
	}
	return 0;
}

PRInt nsNNTPProtocol::ListGroup()
{
    nsresult rv;
    char *group_name;
    rv = m_newsgroup->GetName(&group_name);
    
	PR_snprintf(cd->output_buffer, 
			OUTPUT_BUFFER_SIZE, 
			"listgroup %.512s" CRLF,
                group_name);
    
    rv = NS_NewMsgNewsArticleList(&m_articleList,
                                  m_newsHost, m_newsgroup);
    ce->status = (int) NET_BlockingWrite(ce->socket,cd->output_buffer,PL_strlen(cd->output_buffer));
	NNTP_LOG_WRITE(cd->output_buffer);

	m_nextState = NNTP_RESPONSE;
	m_nextStateAfterResponse = NNTP_LIST_GROUP_RESPONSE;

	return ce->status;
}

PRInt nsNNTPProtocol::ListGroupResponse()
{
	char *line;

	PR_ASSERT(m_responseCode == MK_NNTP_RESPONSE_GROUP_SELECTED);
	if (m_responseCode != MK_NNTP_RESPONSE_GROUP_SELECTED)
	{
		m_nextState = NEWS_DONE; 
		ClearFlag(NNTP_PAUSE_FOR_READ);
		return MK_DATA_LOADED;
	}

	ce->status = NET_BufferedReadLine(ce->socket, &line,
									 &m_dataBuf, 
									 &m_dataBufSize,
									 (Bool*)&cd->pause_for_read);
	NNTP_LOG_READ(line);

	if(ce->status == 0)
	{
		m_nextState = NNTP_ERROR;
		ClearFlag(NNTP_PAUSE_FOR_READ);
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return(MK_NNTP_SERVER_ERROR);
	}

	if (line)
	{
		if (line[0] != '.')
		{
			long found_id = MSG_MESSAGEKEYNONE;
            nsresult rv;
			sscanf(line, "%ld", &found_id);
            
            rv = m_articleList->AddArticleKeyToGroup(found_id);
		}
		else
		{
			m_nextState = NEWS_DONE;	 /* ### dmb - don't really know */
			ClearFlag(NNTP_PAUSE_FOR_READ); 
			return 0;
		}
	}
	return 0;
}


PRInt nsNNTPProtocol::Search()
{
	PR_ASSERT(FALSE);
	return 0;
}

PRInt nsNNTPProtocol::SearchResponse()
{
    if (MK_NNTP_RESPONSE_TYPE(m_responseCode) == MK_NNTP_RESPONSE_TYPE_OK)
		m_nextState = NNTP_SEARCH_RESULTS;
	else
		m_nextState = NEWS_DONE;
	ClearFlag(NNTP_PAUSE_FOR_READ);
	return 0;
}

PRInt nsNNTPProtocol::SearchResults()
{
	char *line = NULL;
	ce->status = NET_BufferedReadLine (ce->socket, &line, &m_dataBuf,
		&m_dataBufSize, (Bool*)&cd->pause_for_read);

	if(ce->status == 0)
	{
		m_nextState = NNTP_ERROR;
		ClearFlag(NNTP_PAUSE_FOR_READ); 
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_NNTP_SERVER_ERROR);
		return MK_NNTP_SERVER_ERROR;
	}
	if (!line)
		return ce->status;  /* no line yet */
	if (ce->status < 0)
	{
		ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, SOCKET_ERRNO);
		/* return TCP error */
		return MK_TCP_READ_ERROR;
	}

	if ('.' != line[0])
		MSG_AddNewsSearchHit (ce->window_id, line);
	else
	{
		/* all overview lines received */
		m_nextState = NEWS_DONE;
		ClearFlag(NNTP_PAUSE_FOR_READ);
	}

	return ce->status;
}



#ifdef PROFILE
#pragma profile off
#endif

#endif /* MOZILLA_CLIENT */


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// The following method is used for processing the news state machine. 
// It returns a negative number (mscott: we'll change this to be an enumerated type which we'll coordinate
// with the netlib folks?) when we are done processing.
//////////////////////////////////////////////////////////////////////////////////////////////////////////
PRInt nsNNTPProtocol::ProcessNewsState(nsIInputStream * inputStream, PRUint32 length)
{
    if (m_offlineNewsState != NULL)
	{
		return NET_ProcessOfflineNews(ce, cd);
	}
    
	ClearFlag(NNTP_PAUSE_FOR_READ); 

    while(!TestFlag(NNTP_PAUSE_FOR_READ))
	{

#if DEBUG
        NNTP_LOG_NOTE(("Next state: %s",stateLabels[m_nextState])); 
#endif
		// examine our current state and call an appropriate handler for that state.....
        switch(m_nextState)
        {
            case NNTP_RESPONSE:
                ce->status = NewsResponse(inputStream, length);
                break;

			// mscott: I've removed the states involving connections on the assumption
			// that core netlib will now be managing that information.

			HG42871

            case NNTP_LOGIN_RESPONSE:
                ce->status = LoginResponse();
                break;

			case NNTP_SEND_MODE_READER:
                ce->status = SendModeReader(); 
                break;

			case NNTP_SEND_MODE_READER_RESPONSE:
                ce->status = SendModeReaderResponse(); 
                break;

			case SEND_LIST_EXTENSIONS:
				ce->status = SendListExtensions(); 
				break;
			case SEND_LIST_EXTENSIONS_RESPONSE:
				ce->status = SendListExtensionsResponse();
				break;
			case SEND_LIST_SEARCHES:
				ce->status = SendListSearches(); 
				break;
			case SEND_LIST_SEARCHES_RESPONSE:
				ce->status = SendListSearchesResponse(); 
				break;
			case NNTP_LIST_SEARCH_HEADERS:
				ce->status = SendListSearchHeaders();
				break;
			case NNTP_LIST_SEARCH_HEADERS_RESPONSE:
				ce->status = SendListSearchHeadersResponse(); 
				break;
			case NNTP_GET_PROPERTIES:
				ce->status = GetProperties();
				break;
			case NNTP_GET_PROPERTIES_RESPONSE:
				ce->status = GetPropertiesResponse();
				break;				
			case SEND_LIST_SUBSCRIPTIONS:
				ce->status = SendListSubscriptions();
				break;
			case SEND_LIST_SUBSCRIPTIONS_RESPONSE:
				ce->status = SendListSubscriptionsResponse();
				break;

            case SEND_FIRST_NNTP_COMMAND:
                ce->status = SendFirstNNTPCommand();
                break;
            case SEND_FIRST_NNTP_COMMAND_RESPONSE:
                ce->status = SendFirstNNTPCommandResponse();
                break;

            case NNTP_SEND_GROUP_FOR_ARTICLE:
                ce->status = SendGroupForArticle();
                break;
            case NNTP_SEND_GROUP_FOR_ARTICLE_RESPONSE:
                ce->status = SendGroupForArticleResponse();
                break;
            case NNTP_SEND_ARTICLE_NUMBER:
                ce->status = SendArticleNumber();
                break;

            case SETUP_NEWS_STREAM:
                ce->status = net_setup_news_stream(ce);
                break;

			case NNTP_BEGIN_AUTHORIZE:
				ce->status = BeginAuthorization(); 
                break;

			case NNTP_AUTHORIZE_RESPONSE:
				ce->status = AuthorizationResponse(); 
                break;

			case NNTP_PASSWORD_RESPONSE:
				ce->status = NewsPasswordResponse();
                break;
    
			// read list
            case NNTP_READ_LIST_BEGIN:
                ce->status = BeginReadNewsList(); 
                break;
            case NNTP_READ_LIST:
                ce->status = ReadNewsList();
                break;

			// news group
			case DISPLAY_NEWSGROUPS:
				ce->status = DisplayNewsgroups(); 
                break;
			case NNTP_NEWGROUPS_BEGIN:
				ce->status = BeginNewsgroups();
                break;
   			case NNTP_NEWGROUPS:
				ce->status = ProcessNewsgroups();
                break;
    
			// article specific
            case NNTP_BEGIN_ARTICLE:
				ce->status = BeginArticle(); 
                break;

            case NNTP_READ_ARTICLE:
				ce->status = ReadArticle();
				break;
			
            case NNTP_XOVER_BEGIN:
			    NET_Progress(ce->window_id, XP_GetString(XP_PROGRESS_READ_NEWSGROUPINFO));
			    ce->status = BeginReadXover();
                break;

			case NNTP_FIGURE_NEXT_CHUNK:
				ce->status = FigureNextChunk(); 
				break;

			case NNTP_XOVER_SEND:
				ce->status = XoverSend();
				break;
    
            case NNTP_XOVER:
                ce->status = ReadXover();
                break;

            case NNTP_XOVER_RESPONSE:
                ce->status = ReadXoverResponse();
                break;

            case NEWS_PROCESS_XOVER:
		    case NEWS_PROCESS_BODIES:
                NET_Progress(ce->window_id, XP_GetString(XP_PROGRESS_SORT_ARTICLES));
				ce->status = ProcessXover();
                break;

            case NNTP_READ_GROUP:
                ce->status = ReadNdwsgroup();
                break;
    
            case NNTP_READ_GROUP_RESPONSE:
                ce->status = ReadnewsgroupResponse();
                break;

            case NNTP_READ_GROUP_BODY:
                ce->status = ReadNewsgroupResponse();
                break;

	        case NNTP_SEND_POST_DATA:
	            ce->status = PostData();
	            break;
	        case NNTP_SEND_POST_DATA_RESPONSE:
	            ce->status = PostDataResponse();
	            break;

			case NNTP_CHECK_FOR_MESSAGE:
				ce->status = CheckForArticle();
				break;

			case NEWS_NEWS_RC_POST:
		        ce->status = net_NewsRCProcessPost(ce);
		        break;

            case NEWS_DISPLAY_NEWS_RC:
		        ce->status = DisplayNewsRC(); 
				break;
            case NEWS_DISPLAY_NEWS_RC_RESPONSE:
		        ce->status = DisplayNewsRCResponse();
		        break;

			// cancel
            case NEWS_START_CANCEL:
		        ce->status = StartCancel();
		        break;

            case NEWS_DO_CANCEL:
		        ce->status = Cancel();
		        break;

			// XPAT
			case NNTP_XPAT_SEND:
				ce->status = XPATSend();
				break;
			case NNTP_XPAT_RESPONSE:
				ce->status = XPATResponse();
				break;

			// search
			case NNTP_SEARCH:
				ce->status = Seach();
				break;
			case NNTP_SEARCH_RESPONSE:
				ce->status = SearchResponse();
				break;
			case NNTP_SEARCH_RESULTS:
				ce->status = SearchResults();
				break;

			
			case NNTP_LIST_PRETTY_NAMES:
				ce->status = ListPrettyNames();
				break;
			case NNTP_LIST_PRETTY_NAMES_RESPONSE:
				ce->status = ListPrettyNamesResponse();
				break;
			case NNTP_LIST_XACTIVE:
				ce->status = ListXActive();
				break;
			case NNTP_LIST_XACTIVE_RESPONSE:
				ce->status = ListXActiveResponse();
				break;
			case NNTP_LIST_GROUP:
				ce->status = ListGroup();
				break;
			case NNTP_LIST_GROUP_RESPONSE:
				ce->status = ListGroupResponse();
				break;
	        case NEWS_DONE:
			  /* call into libmsg and see if the article counts
			   * are up to date.  If they are not then we
			   * want to do a "news://host/group" URL so that we
			   * can finish up the article counts.
			   */
#if 0   // mscott 01/04/99. This should be temporary until I figure out what to do with this code.....
			  if (cd->stream)
				COMPLETE_STREAM;

	            cd->next_state = NEWS_FREE;
                /* set the connection unbusy
     	         */
    		    cd->control_con->busy = FALSE;
                NET_TotalNumberOfOpenConnections--;

				NET_ClearReadSelect(ce->window_id, cd->control_con->csock);
				NET_RefreshCacheFileExpiration(ce->URL_s);
#endif
	            break;

	        case NEWS_ERROR:
	            if(cd->stream)
		             ABORT_STREAM(ce->status);
	            m_nextState = NEWS_FREE;
    	        /* set the connection unbusy
     	         */
    		    cd->control_con->busy = FALSE;
                NET_TotalNumberOfOpenConnections--;

				if(cd->control_con->csock != NULL)
				  {
					NET_ClearReadSelect(ce->window_id, cd->control_con->csock);
				  }
	            break;

	        case NNTP_ERROR:
	            if(cd->stream)
				  {
		            ABORT_STREAM(ce->status);
					cd->stream=0;
				  }
    
				if(cd->control_con && cd->control_con->csock != NULL)
				  {
					NNTP_LOG_NOTE(("Clearing read and connect select on socket %d",
															cd->control_con->csock));
					NET_ClearConnectSelect(ce->window_id, cd->control_con->csock);
					NET_ClearReadSelect(ce->window_id, cd->control_con->csock);
#ifdef XP_WIN
					if(cd->calling_netlib_all_the_time)
					{
						cd->calling_netlib_all_the_time = FALSE;
						NET_ClearCallNetlibAllTheTime(ce->window_id,"mknews");
					}
#endif /* XP_WIN */

#if defined(XP_WIN) || (defined(XP_UNIX)&&defined(UNIX_ASYNC_DNS))
                    NET_ClearDNSSelect(ce->window_id, cd->control_con->csock);
#endif /* XP_WIN || XP_UNIX */
				    net_nntp_close (cd->control_con, ce->status);  /* close the
																  socket */ 
					NET_TotalNumberOfOpenConnections--;
					ce->socket = NULL;
				  }

                /* check if this connection came from the cache or if it was
                 * a new connection.  If it was not new lets start it over
				 * again.  But only if we didn't have any successful protocol
				 * dialog at all.
                 */

				// mscott: I've removed the code that used to be here because it involved connection
				// management which should now be handled by the netlib module.
				m_nextState = NEWS_FREEE;
				break;
    
            case NEWS_FREE:
				ce->status = CloseConnection();
				break;

            default:
                /* big error */
                return(-1);
          
		} // end switch

        if(ce->status < 0 && m_nextState != NEWS_ERROR &&
           m_nextState != NNTP_ERROR && m_nextState != NEWS_FREE)
        {
			m_nextState = NNTP_ERROR;
            ClearFlag(NNTP_PAUSE_FOR_READ);
        }
      
	} /* end big while */

	return(0); /* keep going */
}

PRInt nsNNTPProtocol::CloseConnection()
{
  /* do we need to know if we're parsing xover to call finish xover? 
  /* yes, I think we do! Why did I think we should??? */
  /* If we've gotten to NEWS_FREE and there is still XOVER
     data, there was an error or we were interrupted or
     something.  So, tell libmsg there was an abnormal
     exit so that it can free its data. */
            
	if (m_xoverParser != NULL)
	{
		int status;
        nsresult rv;
       /* XXX - how/when to Release() this? */
        rv = m_xoverParser->Finish(ce->status,&status);
		PR_ASSERT(NS_SUCCEEDED(rv));
		if (NS_SUCCEEDED(rv))
			NS_RELEASE(m_xoverParser);
		if (NS_SUCCEEDED(rv) && ce->status >= 0 && status < 0)
					  ce->status = status;
	}
	else
	{
		/* XXX - state is stored in the MSG_Pane cd->pane */
        NS_RELEASE(m_articleList);
	}

	if (cd->control_con)
		cd->control_con->last_used_time = XP_TIME();

    FREEIF(m_path);
    FREEIF(m_responseText);
    FREEIF(m_dataBuffer);
	FREEIF(m_outputBuffer);

    NS_RELEASE(cd->newsgroup);

	FREEIF (m_cancelID);
	FREEIF (m_cancelFromHdr);
	FREEIF (m_cancelNewsgroups); 
	FREEIF (m_cancelDistribution);

//    if(cd->destroy_graph_progress)
//      FE_GraphProgressDestroy(ce->window_id, 
//                             ce->URL_s, 
//                             cd->original_content_length,
//		         			   ce->bytes_received);
      

	return(-1); /* all done */
}