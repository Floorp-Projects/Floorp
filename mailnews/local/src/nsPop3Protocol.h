/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsPop3Protocol_h__
#define nsPop3Protocol_h__

#include "nsIStreamListener.h"
#include "nsITransport.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsIPop3URL.h"
#include "nsIPop3Sink.h"
#include "nsMsgLineBuffer.h"

#include "rosetta.h"
#include HG09893
#include HG47233

#include "prerror.h"
#include "plhash.h"
#include "xpgetstr.h"

/* A more guaranteed way of making sure that we never get duplicate messages
is to always get each message's UIDL (if the server supports it)
and use these for storing up deletes which were not committed on the
server.  Based on our experience, it looks like we do NOT need to
do this (it has performance tradeoffs, etc.).  To turn it on, three
things need to happen: #define POP_ALWAYS_USE_UIDL_FOR_DUPLICATES, verify that
the uidl's are correctly getting added when the delete response is received,
and change the POP3_QUIT_RESPONSE state to flush the newly committed deletes. */

/* 
 * Cannot have the following line uncommented. It is defined.
 * #ifdef POP_ALWAYS_USE_UIDL_FOR_DUPLICATES will always be evaluated
 * as TRUE.
 *
#define POP_ALWAYS_USE_UIDL_FOR_DUPLICATES 0
 *
 */

// Temporary implementation
#if 0
extern int MK_OUT_OF_MEMORY;
extern int MK_POP3_DELE_FAILURE;
extern int MK_POP3_LIST_FAILURE;
extern int MK_POP3_MESSAGE_WRITE_ERROR;
extern int MK_POP3_NO_MESSAGES;
extern int MK_POP3_OUT_OF_DISK_SPACE;
extern int MK_POP3_PASSWORD_FAILURE;
extern int MK_POP3_PASSWORD_UNDEFINED;
extern int MK_POP3_RETR_FAILURE;
extern int MK_POP3_SERVER_ERROR;
extern int MK_POP3_USERNAME_FAILURE;
extern int MK_POP3_USERNAME_UNDEFINED;
extern int MK_TCP_READ_ERROR;
extern int MK_TCP_WRITE_ERROR;
extern int XP_ERRNO_EWOULDBLOCK;
extern int XP_NO_ANSWER;
extern int XP_THE_POP3_SERVER_DOES_NOT_SUPPORT_UIDL_ETC;
extern int XP_RECEIVING_MESSAGE_OF;
extern int XP_THE_POP3_SERVER_DOES_NOT_SUPPORT_THE_TOP_COMMAND;
extern int XP_THE_PREVIOUSLY_ENTERED_PASSWORD_IS_INVALID_ETC;
extern int XP_CONNECT_HOST_CONTACTED_SENDING_LOGIN_INFORMATION;
extern int XP_PASSWORD_FOR_POP3_USER;
extern int MK_MSG_DOWNLOAD_COUNT;
extern int MK_UNABLE_TO_CONNECT;
extern int MK_CONNECTION_REFUSED;
extern int MK_CONNECTION_TIMED_OUT;
#else
#define MK_OUT_OF_MEMORY -207
#define MK_POP3_DELE_FAILURE -320
#define MK_POP3_LIST_FAILURE -317
#define MK_POP3_MESSAGE_WRITE_ERROR -322
#define MK_POP3_NO_MESSAGES -316
#define MK_POP3_OUT_OF_DISK_SPACE -321
#define MK_POP3_PASSWORD_FAILURE -315
#define MK_POP3_PASSWORD_UNDEFINED -313
#define MK_POP3_RETR_FAILURE -319
#define MK_POP3_SERVER_ERROR -311
#define MK_POP3_USERNAME_FAILURE -314
#define MK_POP3_USERNAME_UNDEFINED -312
#define MK_TCP_READ_ERROR -252
#define MK_TCP_WRITE_ERROR -236
#define XP_ERRNO_EWOULDBLOCK WSAEWOULDBLOCK
#define XP_NO_ANSWER 14401
#define XP_THE_POP3_SERVER_DOES_NOT_SUPPORT_UIDL_ETC 14402
#define XP_RECEIVING_MESSAGE_OF 14403
#define XP_THE_POP3_SERVER_DOES_NOT_SUPPORT_THE_TOP_COMMAND 14404
#define XP_THE_PREVIOUSLY_ENTERED_PASSWORD_IS_INVALID_ETC 14405
#define XP_CONNECT_HOST_CONTACTED_SENDING_LOGIN_INFORMATION 14406
#define XP_PASSWORD_FOR_POP3_USER 14590
#define MK_MSG_DOWNLOAD_COUNT 14734
#define MK_UNABLE_TO_CONNECT -281
#define MK_CONNECTION_REFUSED -242
#define MK_CONNECTION_TIMED_OUT -241
#endif

#define POP3_PORT 110 // The IANA port for Pop3
#define OUTPUT_BUFFER_SIZE 8192 // maximum size of command string

/* structure to hold data pertaining to the active state of
 * a transfer in progress.
 *
 */

enum Pop3CapabilityEnum {
    POP3_CAPABILITY_UNDEFINED = 0x00000000,
    POP3_AUTH_LOGIN_UNDEFINED = 0x00000001,
    POP3_HAS_AUTH_LOGIN		    = 0x00000002,
    POP3_XSENDER_UNDEFINED    = 0x00000004,
    POP3_HAS_XSENDER		      = 0x00000008,
    POP3_GURL_UNDEFINED		    = 0x00000010,
    POP3_HAS_GURL			        = 0x00000020,
    POP3_UIDL_UNDEFINED       = 0x00000040,
    POP3_HAS_UIDL             = 0x00000080,
    POP3_XTND_XLST_UNDEFINED  = 0x00000100,
    POP3_HAS_XTND_XLST        = 0x00000200,
    POP3_TOP_UNDEFINED         = 0x00000400,
    POP3_HAS_TOP              = 0x00000800
};

enum Pop3StatesEnum {
    POP3_READ_PASSWORD,
    POP3_START_CONNECT,
    POP3_FINISH_CONNECT,
    POP3_WAIT_FOR_RESPONSE,
    POP3_WAIT_FOR_START_OF_CONNECTION_RESPONSE,
    POP3_SEND_USERNAME,
    POP3_SEND_PASSWORD,
    POP3_SEND_STAT,
    POP3_GET_STAT,
    POP3_SEND_LIST,
    POP3_GET_LIST,
    POP3_SEND_UIDL_LIST,
    POP3_GET_UIDL_LIST,
    POP3_SEND_XTND_XLST_MSGID,
    POP3_GET_XTND_XLST_MSGID,
    POP3_GET_MSG,
    POP3_SEND_TOP,
    POP3_TOP_RESPONSE,
    POP3_SEND_RETR,
    POP3_RETR_RESPONSE,
    POP3_SEND_DELE,
    POP3_DELE_RESPONSE,
    POP3_SEND_QUIT,
    POP3_DONE,
    POP3_ERROR_DONE,
    POP3_FREE,
    /* The following 3 states support the use of the 'TOP' command instead of UIDL
       for leaving mail on the pop server -km */
    POP3_START_USE_TOP_FOR_FAKE_UIDL, 
    POP3_SEND_FAKE_UIDL_TOP, 
    POP3_GET_FAKE_UIDL_TOP,
    POP3_SEND_AUTH,
    POP3_AUTH_RESPONSE,
    POP3_AUTH_LOGIN,
    POP3_AUTH_LOGIN_RESPONSE,
    POP3_SEND_XSENDER,
    POP3_XSENDER_RESPONSE,
    POP3_SEND_GURL,
    POP3_GURL_RESPONSE,
#ifdef POP_ALWAYS_USE_UIDL_FOR_DUPLICATES
    POP3_QUIT_RESPONSE,
#endif
    POP3_INTERRUPTED
};

enum MsgBiffState {
    MSG_BIFF_UNKNOWN,
    MSG_BIFF_NEWMAIL,
    MSG_BIFF_NOMAIL
};

#define KEEP		'k'			/* If we want to keep this item on server. */
#define DELETE_CHAR	'd'			/* If we want to delete this item. */
#define TOO_BIG		'b'			/* item left on server because it was too big */

typedef struct Pop3AllocedString { /* Need this silliness as a place to
                                      keep the strings that are allocated
                                      for the keys in the hash table. ### */
    char* str;
    struct Pop3AllocedString* next;
} Pop3AllocedString;

typedef struct Pop3UidlHost {
    char* host;
    char* user;
    PLHashTable * hash;
    Pop3AllocedString* strings;
    struct Pop3UidlHost* next;
} Pop3UidlHost;

typedef struct Pop3MsgInfo {
    PRInt32 size;
    char* uidl;
} Pop3MsgInfo;

typedef struct _Pop3ConData {
    
    PRBool leave_on_server;     /* Whether we're supposed to leave messages
                                   on server. */
    PRInt32 size_limit;         /* Leave messages bigger than this on the
                                   server and only download a partial
                                   message. */
    
    Pop3StatesEnum next_state;  /* the next state or action to be taken */
    Pop3StatesEnum next_state_after_response;  
    PRBool pause_for_read;   		/* Pause now for next read? */
    
    PRBool command_succeeded;   /* did the last command succeed? */
    char *command_response;     /* text of last response */
    PRInt32 first_msg;

    char *obuffer;              /* line buffer for output to msglib */
    PRUint32 obuffer_size;
    PRUint32 obuffer_fp;
    
    PRInt32	really_new_messages;
    PRInt32	real_new_counter;
    PRInt32 number_of_messages;
    Pop3MsgInfo	*msg_info;      /* Message sizes and uidls (used only if we
                                   are playing games that involve leaving
                                   messages on the server). */
    PRInt32 last_accessed_msg;
    PRInt32 cur_msg_size;
    char *output_buffer;
    PRBool truncating_cur_msg;  /* are we using top and uidl? */
    PRBool msg_del_started;     /* True if MSG_BeginMailDel...
                                 * called
                                 */
    PRBool only_check_for_new_mail;
  	MsgBiffState biffstate;     /* If just checking for, what the answer is. */
    
    PRBool password_failed;     /* flag for password querying */
    void *msg_closure;
    PRInt32	bytes_received_in_message; 
    PRInt32	total_folder_size;
    
  	PRInt32	total_download_size; /* Number of bytes we're going to
                                    download.  Might be much less
                                    than the total_folder_size. */
    
    PRBool graph_progress_bytes_p; /* whether we should display info about
                                      the bytes transferred (usually we
                                      display info about the number of
                                      messages instead.) */
    
    Pop3UidlHost *uidlinfo;
    PLHashTable *newuidl;
    char *only_uidl;            /* If non-NULL, then load only this UIDL. */
    
    /* the following three fields support the 
       use of the 'TOP' command instead of UIDL
       for leaving mail on the pop server -km */
    
    /* the current message that we are retrieving 
       with the TOP command */
    PRInt32 current_msg_to_top;	
   	
    /* we will download this many in 
       POP3_GET_MSG */							   
    PRInt32 number_of_messages_not_seen_before;
    /* reached when we have TOPped 
       all of the new messages */
    PRBool found_new_message_boundary; 
    
    /* if this is true then we don't stop 
       at new message boundary, we have to 
       TOP em all to delete some */
    PRBool delete_server_message_during_top_traversal;
    PRBool get_url;
    PRBool seenFromHeader;
    char *sender_info;
} Pop3ConData;

class nsPop3Protocol : public nsIStreamListener, public nsMsgLineBuffer
{
public:
    nsPop3Protocol(nsIURL* aURL);
    
    virtual ~nsPop3Protocol();
    
    PRInt32 Load(nsIURL *aURL, nsISupports * aConsumer /* consumer of the url */ = nsnull);
    PRBool IsRunning() { return m_isRunning; };
    
    NS_DECL_ISUPPORTS
    
    /////////////////////////////////////////////////////////////////////////////
    // nsIStreamListener interface
    ////////////////////////////////////////////////////////////////////////////
    NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo)   { return NS_OK;};	// for now
    NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream* aInputStream, PRUint32 aLength);
    NS_IMETHOD OnStartBinding(nsIURL* aURL, const char* aContentType);
    NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg);
    
    NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 pProgress, PRUint32 aProgressMax)
    { return NS_OK; };
    
	NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg) 
    { return NS_OK; };

public:

    const char* GetUsername() { return m_username; };
    void SetUsername(const char* name);

    const char* GetPassword() { return m_password; };
    void SetPassword(const char* password);

private:

    PRUint32 m_pop3CapabilityFlags;
    char* m_username;
    char* m_password;
    Pop3ConData* m_pop3ConData;
    nsIPop3URL* m_nsIPop3URL;
    nsITransport* m_transport;
    nsIOutputStream* m_outputStream; // from the transport
    nsIStreamListener* m_outputConsumer; // from the transport
	nsMsgLineStreamBuffer   * m_lineStreamBuffer; // used to efficiently extract lines from the incoming data stream
    nsIPop3Sink* m_nsIPop3Sink;
    PRBool m_isRunning;

    /**********************************************************************
     * sounds like the following do not need to be included in the protocol
     * instance 
    static PRInt8 uidl_cmp (const void* obj1, const void* obj2);
    static void put_hash(Pop3UidlHost* host, PLHashTable* table, const char*
                         key, char value);
    static PRBool hash_empty_mapper(PLHashTable* hash, const void* key, void*
                                    value, void* closure);
    static PRBool hash_empty(PLHashTable* hash);

    Pop3UidlHost* net_pop3_load_state(const char* host, const char* user);
    PRBool net_pop3_write_mapper(PLHashTable* hash, const void* key, void*
                                 value, void* closure);

    void net_pop3_write_state(Pop3UidlHost* host);
    ***********************************************************************/
    void FreeMsgInfo();
    PRInt32 WaitForStartOfConnectionResponse(nsIInputStream* inputStream, 
                                             PRUint32 length);
    PRInt32 WaitForResponse(nsIInputStream* inputStream, 
                            PRUint32 length);
    PRInt32 Error(int err_code);
    PRInt32 SendCommand(const char * command);
    PRInt32 SendAuth();
    PRInt32 AuthResponse(nsIInputStream* inputStream, 
                         PRUint32 length);
    PRInt32 AuthLogin();
    PRInt32 AuthLoginResponse();
    PRInt32 SendUsername();
    PRInt32 SendPassword();
    PRInt32 SendStatOrGurl(PRBool sendStat);
    PRInt32 SendStat();
    PRInt32 GetStat();
    PRInt32 SendGurl();
    PRInt32 GurlResponse();
    PRInt32 SendList();
    PRInt32 GetList(nsIInputStream* inputStream, PRUint32 length);
    PRInt32 SendFakeUidlTop();
    PRInt32 StartUseTopForFakeUidl();
    PRInt32 GetFakeUidlTop(nsIInputStream* inputStream, PRUint32 length);
    PRInt32 SendXtndXlstMsgid();
    PRInt32 GetXtndXlstMsgid(nsIInputStream* inputStream, PRUint32 length);
    PRInt32 SendUidlList();
    PRInt32 GetUidlList(nsIInputStream* inputStream, PRUint32 length);
    PRInt32 GetMsg();
    PRInt32 SendTop();
    PRInt32 SendXsender();
    PRInt32 XsenderResponse();
    PRInt32 SendRetr();
    PRInt32 HandleLine(char *line, PRUint32 line_length);

    PRInt32 RetrResponse(nsIInputStream* inputStream, PRUint32 length);
    PRInt32 TopResponse(nsIInputStream* inputStream, PRUint32 length);
    PRInt32 SendDele();
    PRInt32 DeleResponse();
    PRInt32 CommitState(PRBool remove_last_entry);
    PRInt32 ProcessPop3State(nsIURL* aURL, nsIInputStream* aInputStream, PRUint32 aLength); 

	void Initialize(nsIURL * aURL);
};

#endif /* nsPop3Protocol_h__ */
