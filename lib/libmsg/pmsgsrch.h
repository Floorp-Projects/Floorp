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

// Private API for searching mail and news
//

#ifndef _PMSGSRCH_H
#define _PMSGSRCH_H

#include "msg_opaq.h"
#include "msg_srch.h"
#include "msglpane.h"
#include "dirprefs.h"
#include "newsdb.h"

#ifdef MOZ_LDAP
	typedef struct ldap LDAP;
	typedef struct ldapmsg LDAPMessage;
	typedef struct ldapvirtuallist LDAPVirtualList;
#endif

#ifdef XP_CPLUSPLUS
	struct MSG_ScopeTerm;
	struct MSG_SearchTerm;
	struct MSG_SearchFrame;
#else
	typedef struct MSG_ScopeTerm MSG_ScopeTerm;
	typedef struct MSG_SearchTerm MSG_SearchTerm;
	typedef struct MSG_SearchFrame MSG_SearchFrame;
#endif

class MSG_MessagePane;
class ParseMailboxState;
class LdapModifyBucket;
class MessageDB;
class DBMessageHdr;
struct DIR_Server;
struct MessageHdrStruct;
class MSG_FolderInfoNews;
class XPStringObj;
struct MSG_SearchView;
class AB_Pane;

//---------------------------------------------------------------------------
// Make our pointer arrays as type-safe as possible. 
// Where are templates when you need them?
//---------------------------------------------------------------------------

class MSG_SearchTermArray : public XPPtrArray
{
public:
	MSG_SearchTerm *GetAt(int i) const { return (MSG_SearchTerm*) XPPtrArray::GetAt(i); }
};

class MSG_SearchValueArray : public XPPtrArray
{
public:
	MSG_SearchValue *GetAt(int i) const { return (MSG_SearchValue*) XPPtrArray::GetAt(i); }
};

class MSG_ScopeTermArray : public XPPtrArray
{
public:
	MSG_ScopeTerm *GetAt(int i) const { return (MSG_ScopeTerm*) XPPtrArray::GetAt(i); }
};

class MSG_SearchResultArray : public XPPtrArray
{
public:
	MSG_ResultElement *GetAt(int i) const { return (MSG_ResultElement*) XPPtrArray::GetAt(i); }
};

class MSG_SearchViewArray : public XPPtrArray
{
public:
	MSG_SearchView * GetAt(int i) const { return (MSG_SearchView *) XPPtrArray::GetAt(i);}
};

typedef enum {MSG_SearchBooleanOR, MSG_SearchBooleanAND} MSG_SearchBooleanOp;

// constants used for online searching with IMAP/NNTP encoded search terms.
// the + 1 is to account for null terminators we add at each stage of assembling the expression...
const int sizeOfORTerm = 6+1;  // 6 bytes if we are combining two sub expressions with an OR term
const int sizeOfANDTerm = 1+1; // 1 byte if we are combining two sub expressions with an AND term

//-----------------------------------------------------------------------------
// CBoolExpression is a class added to provide AND/OR terms in search queries. 
//	A CBoolExpression contains either a search term or two CBoolExpressions and
//    a boolean operator.
// I (mscott) am placing it here for now....
//-----------------------------------------------------------------------------

/* CBoolExpresion --> encapsulates one or more search terms by internally representing 
       the search terms and their boolean operators as a binary expression tree. Each node 
	   in the tree consists of either (1) a boolean operator and two CBoolExpressions or 
	   (2) if the node is a leaf node then it contains a search term. With each search term 
	   that is part of the expression we also keep track of either an evaluation value (XP_BOOL)
	   or a character string. Evaluation values are used for offline searching. The character 
	   string is used to store the IMAP/NNTP encoding of the search term. This makes evaluating
	   the expression (for offline) or generating a search encoding (for online) easier.

  For IMAP/NNTP: CBoolExpression has/assumes knowledge about how AND and OR search terms are
                 combined according to IMAP4 and NNTP protocol. That is the only piece of IMAP/NNTP
				 knowledge it is aware of. 

   Order of Evaluation: Okay, the way in which the boolean expression tree is put together 
		directly effects the order of evaluation. We currently support left to right evaluation. 
		Supporting other order of evaluations involves adding new internal add term methods. 
 */

class CBoolExpression 
{
public:

	// create a leaf node expression
	CBoolExpression(MSG_SearchTerm * newTerm, XP_Bool EvaluationValue = TRUE, char * encodingStr = NULL);         
	CBoolExpression(MSG_SearchTerm * newTerm, char * encodingStr);

	// create a non-leaf node expression containing 2 expressions and a boolean operator
	CBoolExpression(CBoolExpression *, CBoolExpression *, MSG_SearchBooleanOp boolOp); 
	
	CBoolExpression();
	~CBoolExpression();  // recursively destroys all sub expressions as well

	// accesors
	CBoolExpression * AddSearchTerm (MSG_SearchTerm * newTerm, XP_Bool EvaluationValue = TRUE);  // Offline
	CBoolExpression * AddSearchTerm (MSG_SearchTerm * newTerm, char * encodingStr); // IMAP/NNTP

	XP_Bool OfflineEvaluate();  // parses the expression tree and all expressions underneath 
								// this node using each EvaluationValue at each leaf to determine 
								// if the end result is TRUE or FALSE.
	int32 CalcEncodeStrSize();  // assuming the expression is for online searches, 
								// determine the length of the resulting IMAP/NNTP encoding string
	int32 GenerateEncodeStr(char * buffer, int32 bufSize); // fills pre-allocated memory in buffer with 
															// the IMAP/NNTP encoding for the expression
															// returns # bytes added to the buffer

protected:
	// if we are a leaf node, all we have is a search term and a Evaluation value 
	// for that search term
	MSG_SearchTerm * m_term;
	XP_Bool m_evalValue;
	char * m_encodingStr;     // store IMAP/NNTP encoding for the search term if applicable

	// if we are not a leaf node, then we have two other expressions and a boolean operator
	CBoolExpression * m_leftChild;
	CBoolExpression * m_rightChild;
	MSG_SearchBooleanOp m_boolOp;


	// internal methods

	// the idea is to separate the public interface for adding terms to the expression tree from 
	// the order of evaluation which influences how we internally construct the tree. Right now, 
	// we are supporting left to right evaluation so the tree is constructed to represent that by 
	// calling leftToRightAddTerm. If future forms of evaluation need to be supported, add new methods 
	// here for proper tree construction.
	CBoolExpression * leftToRightAddTerm(MSG_SearchTerm * newTerm, XP_Bool EvaluationValue, char * encodingStr); 
};



//-----------------------------------------------------------------------------
// These Adapter classes contain the smarts to convert search criteria from 
// the canonical structures in msg_srch.h into whatever format is required
// by their protocol. 
//
// There is a separate Adapter class for area (pop, imap, nntp, ldap) to contain
// the special smarts for that protocol.
//-----------------------------------------------------------------------------

class msg_SearchAdapter
{
public:
	msg_SearchAdapter (MSG_ScopeTerm*, MSG_SearchTermArray&);
	virtual ~msg_SearchAdapter ();

	virtual MSG_SearchError ValidateTerms ();
	virtual MSG_SearchError Search () { return SearchError_Success; }
	virtual MSG_SearchError SendUrl () { return SearchError_Success; }
	virtual MSG_SearchError OpenResultElement (MSG_MessagePane *, MSG_ResultElement *);
	virtual MSG_SearchError ModifyResultElement (MSG_ResultElement*, MSG_SearchValue*);
	virtual const char *GetEncoding () { return NULL; }

	MSG_FolderInfo *FindTargetFolder (const MSG_ResultElement*);

	MSG_ScopeTerm *m_scope;
	MSG_SearchTermArray &m_searchTerms;

	virtual int Abort ();
	XP_Bool m_abortCalled;

	static MSG_SearchError EncodeImap (char **ppEncoding, 
									   MSG_SearchTermArray &searchTerms,  
									   int16 src_csid, 
									   int16 dest_csid,
									   XP_Bool reallyRFC977bis = FALSE);
	
	static MSG_SearchError EncodeImapValue(char *encoding, const char *value, XP_Bool useQuotes, XP_Bool reallyRFC977bis);

	static char *TryToConvertCharset(char *sourceStr, int16 src_csid, int16 dest_csid, XP_Bool useMIME2Style);
	static char *GetImapCharsetParam(int16 dest_csid);
	void GetSearchCSIDs(int16& src_csid, int16& dst_csid);

	// This stuff lives in the base class because the IMAP search syntax 
	// is used by the RFC977bis SEARCH command as well as IMAP itself
	static const char *m_kImapBefore;
	static const char *m_kImapBody;
	static const char *m_kImapCC;
	static const char *m_kImapFrom;
	static const char *m_kImapNot;
	static const char *m_kImapOr;
	static const char *m_kImapSince;
	static const char *m_kImapSubject;
	static const char *m_kImapTo;
	static const char *m_kImapHeader;
	static const char *m_kImapAnyText;
	static const char *m_kImapKeyword;
	static const char *m_kNntpKeywords;
	static const char *m_kImapSentOn;
	static const char *m_kImapSeen;
	static const char *m_kImapAnswered;
	static const char *m_kImapNotSeen;
	static const char *m_kImapNotAnswered;
	static const char *m_kImapCharset;
	static const char *m_kImapDeleted;

protected:
	static MSG_SearchError EncodeImapTerm (MSG_SearchTerm *, XP_Bool reallyRFC977bis, int16 src_csid, int16 dest_csid, char **ppOutTerm);
	char *TransformSpacesToStars (const char *);

	MSG_SearchError OpenNewsResultInUnknownGroup (MSG_MessagePane*, MSG_ResultElement*);
};


class msg_SearchOfflineMail : public msg_SearchAdapter
{
public:
	msg_SearchOfflineMail (MSG_ScopeTerm*, MSG_SearchTermArray&);
	virtual ~msg_SearchOfflineMail ();

	virtual MSG_SearchError ValidateTerms ();
	virtual MSG_SearchError Search ();
	static MSG_SearchError  MatchTermsForFilter(DBMessageHdr * msgToMatch,MSG_SearchTermArray &termList, MSG_ScopeTerm *scope, 
												MessageDB * db, char * headers, uint32 headerSize);

	static MSG_SearchError MatchTermsForSearch(DBMessageHdr * msgTomatch, MSG_SearchTermArray & termList, MSG_ScopeTerm *scope, MessageDB * db);

	virtual MSG_SearchError BuildSummaryFile ();
	virtual MSG_SearchError OpenSummaryFile ();
	MSG_SearchError SummaryFileError();

	MSG_SearchError AddResultElement (DBMessageHdr *, MessageDB *db);

	virtual int Abort ();

protected:
	static	MSG_SearchError MatchTerms(DBMessageHdr *msgToMatch,MSG_SearchTermArray &termList, MSG_ScopeTerm *scope, 
										MessageDB *db, char * headers, uint32 headerSize, XP_Bool ForFilters);
	struct ListContext *m_cursor;
	MessageDB *m_db;
	struct ListContext *m_listContext;

	enum
	{
		kOpenFolderState,
		kParseMoreState,
		kCloseFolderState,
		kDoneState
	};
	int m_parserState;
	ParseMailboxState *m_mailboxParser;
};

class msg_SearchIMAPOfflineMail : public msg_SearchOfflineMail
{
public:
	msg_SearchIMAPOfflineMail (MSG_ScopeTerm*, MSG_SearchTermArray&);
	virtual ~msg_SearchIMAPOfflineMail ();

	virtual MSG_SearchError ValidateTerms ();
};



class msg_SearchOfflineNews : public msg_SearchOfflineMail
{
public:
	msg_SearchOfflineNews (MSG_ScopeTerm*, MSG_SearchTermArray&);
	virtual ~msg_SearchOfflineNews ();
	virtual MSG_SearchError ValidateTerms ();

	virtual MSG_SearchError OpenSummaryFile ();
};


class msg_SearchOnlineMail : public msg_SearchAdapter
{
public:
	msg_SearchOnlineMail (MSG_ScopeTerm *scope, MSG_SearchTermArray &termList);
	virtual ~msg_SearchOnlineMail ();

	virtual MSG_SearchError ValidateTerms ();
	virtual MSG_SearchError Search ();
	virtual const char *GetEncoding ();

	static MSG_SearchError Encode (char **ppEncoding, MSG_SearchTermArray &searchTerms, int16 src_csid, int16 dest_csid);
	
	MSG_SearchError AddResultElement (DBMessageHdr *, MessageDB *db);

	static void PreExitFunction (URL_Struct *url, int status, MWContext *context);

protected:
	char *m_encoding;

	static const char *m_kSearchTemplate;
};


class  msg_SearchNews : public msg_SearchAdapter
{
public:
	msg_SearchNews (MSG_ScopeTerm *scope, MSG_SearchTermArray &termList);
	virtual ~msg_SearchNews ();

	virtual MSG_SearchError ValidateTerms ();
	virtual MSG_SearchError Search ();
	virtual const char *GetEncoding() { return m_encoding; }

	XP_Bool DuplicateHit(uint32 artNum); // scans m_hits, returns true if artNum already a hit.
	void AddHit (uint32 artNum) { m_candidateHits.Add (artNum); }
	void CollateHits ();
	void ReportHits ();
	void ReportHit (MessageHdrStruct*, const char *);
	void ReportHit (DBMessageHdr*);

	static int CompareArticleNumbers (const void *, const void *);
	static void PreExitFunction (URL_Struct *url, int status, MWContext *context);

	virtual MSG_SearchError Encode (char **outEncoding);
	virtual char *EncodeTerm (MSG_SearchTerm*);

	char *BuildUrlPrefix ();

protected:
	char *m_encoding;
	XPDWordArray m_candidateHits;
	XPDWordArray m_hits;

	XP_Bool m_ORSearch; // set to true if any of the search terms contains an OR for a boolean operator. 

	static const char *m_kNntpFrom;
	static const char *m_kNntpSubject;
	static const char *m_kTermSeparator;
	static const char *m_kUrlPrefix;
};


class msg_SearchNewsEx : public msg_SearchNews
{
public:
	msg_SearchNewsEx (MSG_ScopeTerm *scope, MSG_SearchTermArray &termList);
	virtual ~msg_SearchNewsEx ();

	virtual MSG_SearchError ValidateTerms ();
	virtual MSG_SearchError Search ();
	virtual MSG_SearchError Encode (char **pEncoding /*out*/);

	MSG_SearchError SaveProfile (const char *profileName);

	static void PreExitFunctionEx (URL_Struct *url, int status, MWContext *context);

protected:
	static const char *m_kSearchTemplate;
	static const char *m_kProfileTemplate;
};


class msg_SearchLdap : public msg_SearchAdapter
{
public:
	msg_SearchLdap (MSG_ScopeTerm *scope, MSG_SearchTermArray &termList);
	virtual ~msg_SearchLdap ();

	virtual MSG_SearchError ValidateTerms ();
	virtual MSG_SearchError Search ();
	virtual MSG_SearchError Encode (char **pEncoding /*out*/);
	virtual MSG_SearchError OpenResultElement (MSG_MessagePane *, MSG_ResultElement *);
	virtual MSG_SearchError OpenResultElement (MWContext *, MSG_ResultElement *);
	virtual MSG_SearchError ModifyResultElement (MSG_ResultElement*, MSG_SearchValue*);

	MSG_SearchError EncodeCustomFilter (const char *value, const char *filter, char **outEncoding);
	MSG_SearchError EncodeTerm (MSG_SearchTerm *pTerm, char **ppOutEncoding, int *pEncodingLength);
	MSG_SearchError EncodeDwim (const char *mnemonic, const char *value, 
								char **ppOutEncoding, int *pEncodingLength);
	MSG_SearchError EncodeMnemonic (MSG_SearchTerm *pTerm, const char *whichMnemonic, 
									char **ppOutEncoding, int *pEncodingLength);

#ifdef MOZ_LDAP
	LDAP *m_ldap;
	LDAPMessage *m_message;
#endif /* MOZ_LDAP */

	const char *GetHostName();
	const char *GetSearchBase ();
	const char *GetHostDescription ();
	const char *GetUrlScheme (XP_Bool forAddToAB);
	int GetMaxHits ();
	int GetPort ();
	int GetStandardPort();
	XP_Bool IsSecure();
	XP_Bool GetEnableAuth();
	void DisplayError (int, const char *, int, XP_Bool isTemplate = FALSE);
	void HandleSizeLimitExceeded();

	char *m_filter;	      // search criteria expressed in LDAP syntax
	char *m_password;
	char *m_authDn;
	char *m_valueUsedToFindDn;

	// LDAP state machine
	typedef enum _msg_LdapState
	{
		kInitialize,

		kPreAuthBindRequest,
		kPreAuthBindResponse,
		kPreAuthSearchRequest,
		kPreAuthSearchResponse,

		kAuthenticatedBindRequest,
		kAnonymousBindRequest,
		kBindResponse,
		kSearchRequest,
		kSearchVLVSearchRequest,
		kSearchVLVIndexRequest,
		kSearchResponse,
		kSearchVLVSearchResponse,
		kSearchVLVIndexResponse,
		kModifyRequest,
		kModifyResponse,
		kUnbindRequest
	} msg_LdapState;

	// LDAP VLV search information
	typedef struct _msg_LdapPair
	{
		char *cn;
		char *filter;
		char *sort;
	} msg_LdapPair;

	int PollConnection ();
#ifdef MOZ_LDAP
	void AddPair (LDAPMessage *messageChain);
	void AddSortToPair (msg_LdapPair *pair, LDAPMessage *messageChain);
	void FreePair (msg_LdapPair *pair);
	void ParsePairList ();
	void AddMessageToResults (LDAPMessage *);
    LDAPVirtualList *GetLdapVLVData ();
    MSG_SearchType GetSearchType ();
    MSG_SearchError SetSearchParam(MSG_SearchType type = searchNormal, void *param = NULL);
	XP_Bool IsValidVLVSearch();
	MSG_SearchError RestartFailedVLVSearch ();
	void ProcessVLVResults (LDAPMessage *);
#endif
	msg_LdapState m_nextState;
	int m_currentMessage;
	virtual int Abort ();

#ifdef MOZ_LDAP
	MSG_SearchError Initialize ();

	MSG_SearchError CollectUserCredentials ();
	MSG_SearchError SaveCredentialsToPrefs ();

	MSG_SearchError PreAuthBindRequest ();
	MSG_SearchError PreAuthBindResponse ();
	MSG_SearchError PreAuthSearchRequest ();
	MSG_SearchError PreAuthSearchResponse (); 

	MSG_SearchError AuthenticatedBindRequest ();
	MSG_SearchError AnonymousBindRequest ();
	MSG_SearchError BindRequest (const char *dn, const char *password);

	MSG_SearchError BindResponse ();
	MSG_SearchError SearchRootDSERequest ();
	MSG_SearchError SearchVLVSearchRequest ();
	MSG_SearchError SearchVLVIndexRequest ();
	MSG_SearchError SearchRequest ();
	MSG_SearchError SearchRootDSEResponse ();
	MSG_SearchError SearchVLVSearchResponse ();
	MSG_SearchError SearchVLVIndexResponse ();
	MSG_SearchError SearchResponse ();
	MSG_SearchError ModifyRequest ();
	MSG_SearchError ModifyResponse ();
	MSG_SearchError UnbindRequest ();

#endif /* MOZ_LDAP */

	MSG_SearchError BuildUrl (const char *dn, char **outUrl, XP_Bool forAddToAB);

#ifdef MOZ_LDAP
	// Data initialized before modify URL is fired, but used in modify operation
	LdapModifyBucket *m_modifyBucket;

	int m_currentPair;
	XPPtrArray m_pairList;
#endif

	DIR_Server *GetDirServer ();
};


//---------------------------------------------------------------------------
// MSG_ResultElement is a list of attribute/value pairs which are used to
// represent a search hit without requiring a DBMessageHdr or server connection
//---------------------------------------------------------------------------

struct MSG_ResultElement : public msg_OpaqueObject 
{
public:
	MSG_ResultElement (msg_SearchAdapter *);
	virtual ~MSG_ResultElement ();

	static MSG_SearchError AssignValues (MSG_SearchValue *src, MSG_SearchValue *dst);
	MSG_SearchError GetValue (MSG_SearchAttribute, MSG_SearchValue **) const;
	MSG_SearchError AddValue (MSG_SearchValue*);

	MSG_SearchError GetPrettyName (MSG_SearchValue**);

	MWContextType GetContextType();

	const MSG_SearchValue *GetValueRef (MSG_SearchAttribute) const;
	MSG_SearchError Open (void *window);

	// added as part of the search as view capabilities...
	static int CompareByFolderInfoPtrs (const void *, const void *);  

	static int Compare (const void *, const void *);
	static MSG_SearchError DestroyValue (MSG_SearchValue *value);

	MSG_SearchValueArray m_valueList;
	msg_SearchAdapter *m_adapter;

protected:
	virtual uint32 GetExpectedMagic ();
	static uint32 m_expectedMagic;
};

inline uint32 MSG_ResultElement::GetExpectedMagic ()
{ return m_expectedMagic; }


//---------------------------------------------------------------------------
// MSG_ScopeTerm specifies a container to search in, e.g. Inbox
//---------------------------------------------------------------------------

struct MSG_ScopeTerm : public msg_OpaqueObject
{
public:
	MSG_ScopeTerm (MSG_SearchFrame *, MSG_ScopeAttribute, MSG_FolderInfo *);
	MSG_ScopeTerm (MSG_SearchFrame *, DIR_Server *);
	virtual ~MSG_ScopeTerm ();

	XP_Bool IsOfflineMail ();
	XP_Bool IsOfflineIMAPMail();  // added by mscott 
	const char *GetMailPath();
	MSG_SearchError TimeSlice ();

	MSG_SearchError InitializeAdapter (MSG_SearchTermArray &termList);

	char *GetStatusBarName ();

	MSG_ScopeAttribute m_attribute;
	char *m_name;
	MSG_FolderInfo *m_folder;
	XP_File m_file;
	msg_SearchAdapter *m_adapter;
	MSG_SearchFrame *m_frame;
	DIR_Server *m_server;
	XP_Bool m_searchServer;

protected:
	virtual uint32 GetExpectedMagic ();
	static uint32 m_expectedMagic;
};

inline uint32 MSG_ScopeTerm::GetExpectedMagic ()
{ return m_expectedMagic; }


//---------------------------------------------------------------------------
// MSG_BodyHandler: used to retrive lines from POP and IMAP offline messages.
// This is a helper class used by MSG_SearchTerm::MatchBody
//---------------------------------------------------------------------------
class MSG_BodyHandler
{
public:
	MSG_BodyHandler (MSG_ScopeTerm *, uint32 offset, uint32 length, DBMessageHdr * msg, MessageDB * db);
	
	// we can also create a body handler when doing arbitrary header filtering...we need the list of headers and the header size as well
	// if we are doing filtering...if ForFilters is false, headers and headersSize is ignored!!!
	MSG_BodyHandler (MSG_ScopeTerm *, uint32 offset, uint32 length, DBMessageHdr * msg, MessageDB * db,
					 char * headers /* NULL terminated list of headers */, uint32 headersSize, XP_Bool ForFilters);

	virtual ~MSG_BodyHandler();

	// Returns nextline 
	int32 GetNextLine(char * buf, int bufSize);    // returns next message line in buf, up to bufSize bytes.

	// Transformations
	void SetStripHtml (XP_Bool strip) { m_stripHtml = strip; }
	void SetStripHeaders (XP_Bool strip) { m_stripHeaders = strip; }

protected:
	void Initialize();  // common initialization code

	// filter related methods. For filtering we always use the headers list instead of the database...
	XP_Bool m_Filtering;
	int32 GetNextFilterLine(char * buf, int bufSize);
	char * m_headers;  // pointer into the headers list in the original message hdr db...
	uint32 m_headersSize;
	uint32 m_headerBytesRead;

	// local / POP related methods
    void OpenLocalFolder();
	int32 GetNextLocalLine(char * buf, int bufSize);      // goes through the mail folder 

	MSG_ScopeTerm *m_scope;

	// local file state
	XP_File	*m_localFile;
	int32 m_localFileOffset;       // current offset into the mail folder file
	uint32 m_numLocalLines;

	// Offline IMAP related methods & state
	int32 GetNextIMAPLine(char * buf, int bufSize);     // goes through the MessageDB 
	DBMessageHdr * m_msgHdr;
	MessageDB * m_db;
	int32 m_IMAPMessageOffset;
	XP_Bool m_OfflineIMAP;		 // TRUE if we are in Offline IMAP mode, FALSE otherwise

	// News related methods & state
	int32 m_NewsArticleOffset;
	int32 GetNextNewsLine (NewsGroupDB * newsDB, char * buf, int bufSize);  // goes through the NewsDB

	// Transformations
	XP_Bool m_stripHeaders;		// TRUE if we're supposed to strip of message headers
	XP_Bool m_stripHtml;		// TRUE if we're supposed to strip off HTML tags
	XP_Bool m_passedHeaders;	// TRUE if we've already skipped over the headers
	XP_Bool m_messageIsHtml;	// TRUE if the Content-type header claims text/html
	int32 ApplyTransformations (char *buf, int32 length, XP_Bool &returnThisLine);
	void StripHtml (char *buf);
};


//---------------------------------------------------------------------------
// MSG_SearchTerm specifies one criterion, e.g. name contains phil
//---------------------------------------------------------------------------

struct MSG_SearchTerm : public msg_OpaqueObject
{
public:
	MSG_SearchTerm();
	MSG_SearchTerm (MSG_SearchAttribute, MSG_SearchOperator, MSG_SearchValue *, XP_Bool, char * arbitraryHeader); // the bool is true if AND, FALSE if OR
	MSG_SearchTerm (MSG_SearchAttribute, MSG_SearchOperator, MSG_SearchValue *, MSG_SearchBooleanOp, char * arbitraryHeader);

	virtual ~MSG_SearchTerm ();

	void StripQuotedPrintable (unsigned char*);
	int32 GetNextIMAPOfflineMsgLine (char * buf, int bufferSize, int msgOffset, DBMessageHdr * msg, MessageDB * db);


	MSG_SearchError MatchBody (MSG_ScopeTerm*, uint32 offset, uint32 length, int16 csid, DBMessageHdr * msg, MessageDB * db);
	MSG_SearchError MatchArbitraryHeader (MSG_ScopeTerm *,uint32 offset, uint32 length, int16 csid, DBMessageHdr * msg, MessageDB *db,
											char * headers, /* NULL terminated header list for msgs being filtered. Ignored unless ForFilters */
											uint32 headersSize, /* size of the NULL terminated list of headers */
											XP_Bool ForFilters /* true if we are filtering */);
	MSG_SearchError MatchString (const char *, int16 csid, XP_Bool body = FALSE);
	MSG_SearchError MatchDate (time_t);
	MSG_SearchError MatchStatus (uint32);
	MSG_SearchError MatchPriority (MSG_PRIORITY);
	MSG_SearchError MatchSize (uint32);
	MSG_SearchError MatchRfc822String(const char *, int16 csid);
	MSG_SearchError MatchAge (time_t);

	MSG_SearchError EnStreamNew (char **, int16 *length);
	MSG_SearchError DeStream (char *, int16 length);
	MSG_SearchError DeStreamNew (char *, int16 length);

	MSG_SearchError GetLocalTimes (time_t, time_t, struct tm &, struct tm &);

	XP_Bool IsBooleanOpAND() { return m_booleanOp == MSG_SearchBooleanAND ? TRUE : FALSE;}
	MSG_SearchBooleanOp GetBooleanOp() {return m_booleanOp;}
	char * GetArbitraryHeader() {return m_arbitraryHeader;}

	static char *	EscapeQuotesInStr(const char *str);
	XP_Bool MatchAllBeforeDeciding ();
	MSG_SearchAttribute m_attribute;
	MSG_SearchOperator m_operator;
	MSG_SearchValue m_value;
	MSG_SearchBooleanOp m_booleanOp;  // boolean operator to be applied to this search term and the search term which precedes it.
	char * m_arbitraryHeader;         // user specified string for the name of the arbitrary header to be used in the search
									  // only has a value when m_attribute = attribOtherHeader!!!!
protected:
	MSG_SearchError		OutputValue(XPStringObj &outputStr);
	MSG_SearchAttribute ParseAttribute(char *inStream);
	MSG_SearchOperator	ParseOperator(char *inStream);
	MSG_SearchError		ParseValue(char *inStream);
	virtual uint32 GetExpectedMagic ();
	static uint32 m_expectedMagic;
};

inline uint32 MSG_SearchTerm::GetExpectedMagic ()
{ return m_expectedMagic; }


typedef enum {MSG_SearchAsViewCopy,MSG_SearchAsViewDelete, MSG_SearchAsViewDeleteNoTrash, MSG_SearchAsViewMove} SearchAsViewCmd;
typedef struct MSG_SearchView
{
	MSG_FolderInfo * folder;
	MessageDBView * view;
} MSG_SearchView;

//-----------------------------------------------------------------------------
// MSG_SearchFrame is a search session. There is one pane object per running
// customer of search (mail/news, ldap, rules, etc.)
//-----------------------------------------------------------------------------

struct MSG_SearchFrame : public msg_OpaqueObject 
{
public:
	MSG_SearchFrame (MSG_LinedPane *);
	virtual ~MSG_SearchFrame ();

	// bottleneck this cast to do type checking
	static MSG_SearchFrame *FromContext (MWContext*);
	static MSG_SearchFrame *FromPane (MSG_Pane*);

	// public API implementation
	MSG_SearchError AddSearchTerm (MSG_SearchAttribute, MSG_SearchOperator, MSG_SearchValue *, XP_Bool BooleanAND, char * arbitraryHeader);
	MSG_SearchError AddScopeTerm (MSG_ScopeAttribute, MSG_FolderInfo*);
	MSG_SearchError AddScopeTerm (DIR_Server*);
	MSG_SearchError AddAllScopes (MSG_Master*, MSG_ScopeAttribute);
	MSG_SearchError GetResultElement (MSG_ViewIndex idx, MSG_ResultElement **result);
	MSG_SearchError SortResultList (MSG_SearchAttribute, XP_Bool);
	MSG_SearchError GetLdapObjectClasses (char **, uint16 *);
	MSG_SearchError SetLdapObjectClass (char *);
	MSG_SearchError AddLdapResultsToAddressBook (MSG_ViewIndex*, int);
	MSG_SearchError ComposeFromLdapResults (MSG_ViewIndex*, int);

    void            *GetSearchParam();
    MSG_SearchType  GetSearchType();
    MSG_SearchError SetSearchParam(MSG_SearchType type = searchNormal, void *param = NULL);

	// implementation helpers
	MSG_SearchError Initialize ();
	MSG_SearchError BuildUrlQueue(Net_GetUrlExitFunc*);
	MSG_SearchError GetUrl ();
	MSG_SearchError AddResultElement (MSG_ResultElement *);
	MSG_SearchError DeleteResultElementsInFolder (MSG_FolderInfo *);
	MSG_SearchError AddScopeTree (MSG_ScopeAttribute, MSG_FolderInfo*,XP_Bool deep = TRUE);
	void UpdateStatusBar (int message);
	MSG_SearchError BeginSearching();

	MSG_SearchError EncodeRFC977bisScopes (char **ppOutEncoding);

	// libnet async support
	int TimeSlice ();
	int m_idxRunningScope;
	msg_SearchAdapter *GetRunningAdapter ();
	MSG_ScopeTerm *GetRunningScope();

	// support for searching multiple scopes in serial
	int TimeSliceSerial ();

	// support for searching multiple scopes in parallel
	XP_Bool m_parallel;
	MSG_ScopeTermArray m_parallelScopes;
	int TimeSliceParallel ();



	// Search As View API for the search pane to call
	MsgERR MoveMessages (const MSG_ViewIndex *indices,
		int32 numIndices,
		MSG_FolderInfo * destFolder);

	MsgERR CopyMessages (const MSG_ViewIndex *indices, 
		int32 numIndices, 
		MSG_FolderInfo * destFolder,
		XP_Bool deleteAfterCopy);

	MSG_DragEffect DragMessagesStatus(const MSG_ViewIndex *indices,
									int32 numIndices,
									MSG_FolderInfo * destFolder,
									MSG_DragEffect request);


	MsgERR TrashMessages (const MSG_ViewIndex *indices,int32 numIndices);

	MsgERR DeleteMessages(const MSG_ViewIndex * indices, int32 numIndices);

	MsgERR ProcessSearchAsViewCmd (const MSG_ViewIndex * indices, int32 numIndices, MSG_FolderInfo * destFolder, /* might be NULL */ SearchAsViewCmd cmdType);

	MSG_SearchError CopyResultElements (MSG_FolderInfo * srcFolder, MSG_FolderInfo * destFolder, IDArray * ids, XP_Bool deleteAfterCopy);
	MSG_SearchError TrashResultElements  (MSG_FolderInfo * srcFolder, IDArray * ids);
	MSG_SearchError DeleteResultElements (MSG_FolderInfo * srcFolder, IDArray * ids);
	MSG_SearchResultArray * IndicesToResultElements (const MSG_ViewIndex *indices, int32 numIndices); // user must deallocate result array

	void AddViewToList (MSG_FolderInfo * folder, MessageDBView * view);
	void CloseView(MSG_FolderInfo * folder);
	MessageDBView * GetView (MSG_FolderInfo * folder);


	MSG_ScopeTermArray m_scopeList;
	MSG_SearchTermArray m_termList;
	MSG_SearchResultArray m_resultList;

	void DestroyTermList ();
	void DestroyScopeList ();
	void DestroyResultList ();
	void DestroySearchViewList();

	MSG_SearchAttribute m_sortAttribute;
	XP_Bool m_descending;

	char *m_ldapObjectClass;
	const char *GetLdapObjectClass ();

	// notification of results runs through the linedPane back to the FE
	MSG_LinedPane * GetPane () { return m_pane;}
	MSG_LinedPane *m_pane;
	MWContext *GetContext () { return m_pane->GetContext(); }
	XP_Bool m_calledStartingUpdate;
	XP_Bool m_handlingError;

	URL_Struct *m_urlStruct;

	XP_Bool GetSaveProfileStatus ();
	msg_SearchNewsEx *GetProfileAdapter ();

	XP_Bool GetGoToFolderStatus (MSG_ViewIndex *indices, int32 numIndices);

	void BeginCylonMode ();
	void EndCylonMode ();

	void IncrementOfflineProgress ();

protected:
	// this array stores JUST the views that have been opened in order to perform a 
	// search as view operation. Routines call back to the search frame when the action 
	// is completed at which point the view can be closed. This array does NOT contain every view
	// in the search result elements list!! IN the future, do not allow it as this would imply that
	// we could have MANY DBs open at the same time. 
	MSG_SearchViewArray m_viewList; // list of all currently open views in the search frame (along with their folders)

	virtual uint32 GetExpectedMagic ();
	static uint32 m_expectedMagic;

	XP_Bool m_inCylonMode;
	void *m_cylonModeTimer;
	static void CylonModeCallback (void *closure);

	int32 m_offlineProgressTotal;
	int32 m_offlineProgressSoFar;

	MSG_SearchType m_searchType;
	void *m_pSearchParam;
};

inline uint32 MSG_SearchFrame::GetExpectedMagic ()
{ return m_expectedMagic; }


//-----------------------------------------------------------------------------
// Validity checking for attrib/op pairs. We need to know what operations are
// legal in three places:
//   1. when the FE brings up the dialog box and needs to know how to build
//      the menus and enable their items
//   2. when the FE fires off a search, we need to check their lists for
//      correctness
//   3. for on-the-fly capability negotion e.g. with XSEARCH-capable news 
//      servers
//-----------------------------------------------------------------------------

class msg_SearchValidityTable 
{
public:
	msg_SearchValidityTable ();
	void SetAvailable (int attrib, int op, XP_Bool);
	void SetEnabled (int attrib, int op, XP_Bool);
	void SetValidButNotShown (int attrib, int op, XP_Bool);
	XP_Bool GetAvailable (int attrib, int op);
	XP_Bool GetEnabled (int attrib, int op);
	XP_Bool GetValidButNotShown (int attrib, int op);
	MSG_SearchError ValidateTerms (MSG_SearchTermArray&);
	int GetNumAvailAttribs();   // number of attribs with at least one available operator
								  
protected:
	int m_numAvailAttribs;        // number of rows with at least one available operator
	typedef struct vtBits
	{
		uint16 bitEnabled : 1;
		uint16 bitAvailable : 1;
		uint16 bitValidButNotShown : 1;
	} vtBits;
	vtBits m_table [kNumAttributes][kNumOperators];
};

// Using getters and setters seems a little nicer then dumping the 2-D array
// syntax all over the code
inline void msg_SearchValidityTable::SetAvailable (int a, int o, XP_Bool b)
{ m_table [a][o].bitAvailable = b; }
inline void msg_SearchValidityTable::SetEnabled (int a, int o, XP_Bool b)
{ m_table [a][o].bitEnabled = b; }
inline void msg_SearchValidityTable::SetValidButNotShown (int a, int o, XP_Bool b)
{ m_table [a][o].bitValidButNotShown = b; }

inline XP_Bool msg_SearchValidityTable::GetAvailable (int a, int o)
{ return m_table [a][o].bitAvailable; }
inline XP_Bool msg_SearchValidityTable::GetEnabled (int a, int o)
{ return m_table [a][o].bitEnabled; }
inline XP_Bool msg_SearchValidityTable::GetValidButNotShown (int a, int o)
{ return m_table [a][o].bitValidButNotShown; }

class msg_SearchValidityManager
{
public:
	msg_SearchValidityManager ();
	~msg_SearchValidityManager ();

	MSG_SearchError GetTable (int, msg_SearchValidityTable**);
  
	MSG_SearchError PostProcessValidityTable (MSG_NewsHost*);
	MSG_SearchError PostProcessValidityTable (DIR_Server*);

	enum {
	  onlineMail,
	  onlineMailFilter,
	  offlineMail,
	  localNews,
	  news,
	  newsEx,
	  Ldap
	};

protected:

	// There's one global validity manager that everyone uses. You *could* do 
	// this with static members of the adapter classes, but having a dedicated
	// object makes cleanup of these tables (at shutdown-time) automagic.

	msg_SearchValidityTable *m_offlineMailTable;
	msg_SearchValidityTable *m_onlineMailTable;
	msg_SearchValidityTable *m_onlineMailFilterTable;
	msg_SearchValidityTable *m_newsTable;
	msg_SearchValidityTable *m_newsExTable;
	msg_SearchValidityTable *m_ldapTable;
	msg_SearchValidityTable *m_localNewsTable; // used for local news searching or offline news searching...

	MSG_SearchError NewTable (msg_SearchValidityTable **);

	MSG_SearchError InitOfflineMailTable ();
	MSG_SearchError InitOnlineMailTable ();
	MSG_SearchError InitOnlineMailFilterTable ();
	MSG_SearchError InitNewsTable ();
	MSG_SearchError InitLocalNewsTable(); 
	MSG_SearchError InitNewsExTable (MSG_NewsHost *host = NULL);
	MSG_SearchError InitLdapTable (DIR_Server *server = NULL);

	void EnableLdapAttribute (MSG_SearchAttribute, XP_Bool enabled = TRUE);
};

extern msg_SearchValidityManager gValidityMgr;


inline XP_Bool IsStringAttribute (MSG_SearchAttribute a)
{
	return ! (a == attribPriority || a == attribDate || 
		a == attribMsgStatus || a == attribMessageKey ||
		a == attribSize || a == attribAgeInDays ||
		a == attribFolderInfo);
}


#define SEARCH_API    extern "C"

#define LDAP_VLV_BASE        "cn=config,cn=ldbm"
#define LDAP_VLV_SEARCHCLASS "(objectClass=vlvSearch)"
#define LDAP_VLV_INDEXCLASS  "(objectClass=vlvIndex)"

#endif // _PMSGSRCH_H 
