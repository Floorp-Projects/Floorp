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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _nsMsgSearchCore_H_
#define _nsMsgSearchCore_H_

#include "MailNewsTypes.h"
#include "nsString2.h"
#include "nsIMsgHeaderParser.h"
#include "nsCOMPtr.h"

class nsIMsgDatabase;
class nsIMsgFolder;
class nsIMsgSearchAdapter;
class nsIMsgDBHdr;

typedef enum
{
    nsMsgSearchScopeMailFolder,
    nsMsgSearchScopeNewsgroup,
    nsMsgSearchScopeLdapDirectory,
    nsMsgSearchScopeOfflineNewsgroup,
	nsMsgSearchScopeAllSearchableGroups
} nsMsgSearchScopeAttribute;

typedef enum
{
    nsMsgSearchAttribSubject = 0,	/* mail and news */
    nsMsgSearchAttribSender,   
    nsMsgSearchAttribBody,	
    nsMsgSearchAttribDate,	

    nsMsgSearchAttribPriority,	    /* mail only */
    nsMsgSearchAttribMsgStatus,	
    nsMsgSearchAttribTo,
    nsMsgSearchAttribCC,
    nsMsgSearchAttribToOrCC,

    nsMsgSearchAttribCommonName,   /* LDAP only */
    nsMsgSearchAttrib822Address,	
    nsMsgSearchAttribPhoneNumber,
    nsMsgSearchAttribOrganization,
    nsMsgSearchAttribOrgUnit,
    nsMsgSearchAttribLocality,
    nsMsgSearchAttribStreetAddress,
    nsMsgSearchAttribSize,
    nsMsgSearchAttribAnyText,      /* any header or body */
	nsMsgSearchAttribKeywords,

    nsMsgSearchAttribDistinguishedName, /* LDAP result elem only */
    nsMsgSearchAttribObjectClass,       
    nsMsgSearchAttribJpegFile,

    nsMsgSearchAttribLocation,          /* result list only */
    nsMsgSearchAttribMessageKey,        /* message result elems */

	nsMsgSearchAttribAgeInDays,    /* for purging old news articles */

	nsMsgSearchAttribGivenName,    /* for sorting LDAP results */
	nsMsgSearchAttribSurname, 

	nsMsgSearchAttribFolderInfo,	/* for "view thread context" from result */

	nsMsgSearchAttribCustom1,		/* custom LDAP attributes */
	nsMsgSearchAttribCustom2,
	nsMsgSearchAttribCustom3,
	nsMsgSearchAttribCustom4,
	nsMsgSearchAttribCustom5,

	nsMsgSearchAttribMessageId, 
	/* the following are LDAP specific attributes */
	nsMsgSearchAttribCarlicense,
	nsMsgSearchAttribBusinessCategory,
	nsMsgSearchAttribDepartmentNumber,
	nsMsgSearchAttribDescription,
	nsMsgSearchAttribEmployeeType,
	nsMsgSearchAttribFaxNumber,
	nsMsgSearchAttribManager,
	nsMsgSearchAttribPostalAddress,
	nsMsgSearchAttribPostalCode,
	nsMsgSearchAttribSecretary,
	nsMsgSearchAttribTitle,
	nsMsgSearchAttribNickname,
	nsMsgSearchAttribHomePhone,
	nsMsgSearchAttribPager,
	nsMsgSearchAttribCellular,

	nsMsgSearchAttribOtherHeader,  /* for mail and news. MUST ALWAYS BE LAST attribute since we can have an arbitrary # of these...*/
    kNumMsgSearchAttributes      /* must be last attribute */
} nsMsgSearchAttribute;

/* NB: If you add elements to this enum, add only to the end, since 
 *     RULES.DAT stores enum values persistently
 */
typedef enum
{
    nsMsgSearchOpContains = 0,     /* for text attributes              */
    nsMsgSearchOpDoesntContain,
    nsMsgSearchOpIs,               /* is and isn't also apply to some non-text attrs */
    nsMsgSearchOpIsnt, 
    nsMsgSearchOpIsEmpty,

    nsMsgSearchOpIsBefore,         /* for date attributes              */
    nsMsgSearchOpIsAfter,
    
    nsMsgSearchOpIsHigherThan,     /* for priority. nsMsgSearchOpIs also applies  */
    nsMsgSearchOpIsLowerThan,

    nsMsgSearchOpBeginsWith,              
	nsMsgSearchOpEndsWith,

    nsMsgSearchOpSoundsLike,       /* for LDAP phoenetic matching      */
	nsMsgSearchOpLdapDwim,         /* Do What I Mean for simple search */

	nsMsgSearchOpIsGreaterThan,	
	nsMsgSearchOpIsLessThan,

	nsMsgSearchOpNameCompletion,   /* Name Completion operator...as the name implies =) */

    kNumMsgSearchOperators       /* must be last operator            */
} nsMsgSearchOperator;

/* FEs use this to help build the search dialog box */
typedef enum
{
    nsMsgSearchWidgetText,
    nsMsgSearchWidgetDate,
    nsMsgSearchWidgetMenu,
	nsMsgSearchWidgetInt,          /* added to account for age in days which requires an integer field */
    nsMsgSearchWidgetNone
} nsMsgSearchValueWidget;

/* Used to specify type of search to be performed */
typedef enum
{
	nsMsgSearchNone,
	nsMsgSearchRootDSE,
	nsMsgSearchNormal,
	nsMsgSearchLdapVLV,
	nsMsgSearchNameCompletion
} nsMsgSearchType;

typedef enum {nsMsgSearchBooleanOR, nsMsgSearchBooleanAND} nsMsgSearchBooleanOp;

/* Use this to specify the value of a search term */
typedef struct nsMsgSearchValue
{
    nsMsgSearchAttribute attribute;
    union
    {
        char *string;
        nsMsgPriority priority;
        time_t date;
        PRUint32 msgStatus; /* see MSG_FLAG in msgcom.h */
        PRUint32 size;
        nsMsgKey key;
		PRUint32 age; /* in days */
		nsIMsgFolder *folder;
    } u;
} nsMsgSearchValue;

class nsMsgSearchScopeTerm;
struct nsMsgDIRServer;
class nsMsgSearchTerm;

class nsMsgSearchTermArray : public nsVoidArray
{
public:
	nsMsgSearchTerm *ElementAt(PRUint32 i) const { return (nsMsgSearchTerm*) nsVoidArray::ElementAt(i); }
};

class nsMsgSearchValueArray : public nsVoidArray
{
public:
	nsMsgSearchValue *ElementAt(PRUint32 i) const { return (nsMsgSearchValue*) nsVoidArray::ElementAt(i); }
};

class nsMsgSearchScopeTermArray : public nsVoidArray
{
public:
	nsMsgSearchScopeTerm *ElementAt(PRUint32 i) const { return (nsMsgSearchScopeTerm*) nsVoidArray::ElementAt(i); }
};

//-----------------------------------------------------------------------------
// nsMsgSearchBoolExpression is a class added to provide AND/OR terms in search queries. 
//	A nsMsgSearchBoolExpression contains either a search term or two nsMsgSearchBoolExpressions and
//    a boolean operator.
// I (mscott) am placing it here for now....
//-----------------------------------------------------------------------------

/* CBoolExpresion --> encapsulates one or more search terms by internally representing 
       the search terms and their boolean operators as a binary expression tree. Each node 
	   in the tree consists of either (1) a boolean operator and two nsMsgSearchBoolExpressions or 
	   (2) if the node is a leaf node then it contains a search term. With each search term 
	   that is part of the expression we also keep track of either an evaluation value (XP_BOOL)
	   or a character string. Evaluation values are used for offline searching. The character 
	   string is used to store the IMAP/NNTP encoding of the search term. This makes evaluating
	   the expression (for offline) or generating a search encoding (for online) easier.

  For IMAP/NNTP: nsMsgSearchBoolExpression has/assumes knowledge about how AND and OR search terms are
                 combined according to IMAP4 and NNTP protocol. That is the only piece of IMAP/NNTP
				 knowledge it is aware of. 

   Order of Evaluation: Okay, the way in which the boolean expression tree is put together 
		directly effects the order of evaluation. We currently support left to right evaluation. 
		Supporting other order of evaluations involves adding new internal add term methods. 
 */

class nsMsgSearchBoolExpression 
{
public:

	// create a leaf node expression
	nsMsgSearchBoolExpression(nsMsgSearchTerm * newTerm, PRBool EvaluationValue = PR_TRUE, char * encodingStr = NULL);         
	nsMsgSearchBoolExpression(nsMsgSearchTerm * newTerm, char * encodingStr);

	// create a non-leaf node expression containing 2 expressions and a boolean operator
	nsMsgSearchBoolExpression(nsMsgSearchBoolExpression *, nsMsgSearchBoolExpression *, nsMsgSearchBooleanOp boolOp); 
	
	nsMsgSearchBoolExpression();
	~nsMsgSearchBoolExpression();  // recursively destroys all sub expressions as well

	// accesors
	nsMsgSearchBoolExpression * AddSearchTerm (nsMsgSearchTerm * newTerm, PRBool EvaluationValue = TRUE);  // Offline
	nsMsgSearchBoolExpression * AddSearchTerm (nsMsgSearchTerm * newTerm, char * encodingStr); // IMAP/NNTP

	PRBool OfflineEvaluate();  // parses the expression tree and all expressions underneath 
								// this node using each EvaluationValue at each leaf to determine 
								// if the end result is TRUE or FALSE.
	PRInt32 CalcEncodeStrSize();  // assuming the expression is for online searches, 
								// determine the length of the resulting IMAP/NNTP encoding string
	PRInt32 GenerateEncodeStr(nsString2 * buffer); // fills pre-allocated memory in buffer with 
															// the IMAP/NNTP encoding for the expression
															// returns # bytes added to the buffer

protected:
	// if we are a leaf node, all we have is a search term and a Evaluation value 
	// for that search term
	nsMsgSearchTerm * m_term;
	PRBool m_evalValue;
	nsString2 m_encodingStr;     // store IMAP/NNTP encoding for the search term if applicable

	// if we are not a leaf node, then we have two other expressions and a boolean operator
	nsMsgSearchBoolExpression * m_leftChild;
	nsMsgSearchBoolExpression * m_rightChild;
	nsMsgSearchBooleanOp m_boolOp;


	// internal methods

	// the idea is to separate the public interface for adding terms to the expression tree from 
	// the order of evaluation which influences how we internally construct the tree. Right now, 
	// we are supporting left to right evaluation so the tree is constructed to represent that by 
	// calling leftToRightAddTerm. If future forms of evaluation need to be supported, add new methods 
	// here for proper tree construction.
	nsMsgSearchBoolExpression * leftToRightAddTerm(nsMsgSearchTerm * newTerm, PRBool EvaluationValue, char * encodingStr); 
};

class nsMsgSearchScopeTerm 
{
public:
	nsMsgSearchScopeTerm (nsMsgSearchScopeAttribute, nsIMsgFolder *);
	nsMsgSearchScopeTerm ();
	virtual ~nsMsgSearchScopeTerm ();

	PRBool IsOfflineNews();
	PRBool IsOfflineMail ();
	PRBool IsOfflineIMAPMail();  // added by mscott 
	const char *GetMailPath();
	nsresult TimeSlice ();

	nsresult InitializeAdapter (nsMsgSearchTermArray &termList);

	char *GetStatusBarName ();

	nsMsgSearchScopeAttribute m_attribute;
	char *m_name;
	nsCOMPtr <nsIMsgFolder> m_folder;
//	XP_File m_file;
	nsCOMPtr <nsIMsgSearchAdapter> m_adapter;
	PRBool m_searchServer;

};


// nsMsgResultElement specifies a single search hit.

//---------------------------------------------------------------------------
// nsMsgResultElement is a list of attribute/value pairs which are used to
// represent a search hit without requiring a message header or server connection
//---------------------------------------------------------------------------

class nsMsgResultElement
{
public:
	nsMsgResultElement (nsIMsgSearchAdapter *);
	virtual ~nsMsgResultElement ();

	static nsresult AssignValues (nsMsgSearchValue *src, nsMsgSearchValue *dst);
	nsresult GetValue (nsMsgSearchAttribute, nsMsgSearchValue **) const;
	nsresult AddValue (nsMsgSearchValue*);

	nsresult GetPrettyName (nsMsgSearchValue**);


	const nsMsgSearchValue *GetValueRef (nsMsgSearchAttribute) const;
	nsresult Open (void *window);

	// added as part of the search as view capabilities...
	static int CompareByFolderInfoPtrs (const void *, const void *);  

	static int Compare (const void *, const void *);
	static nsresult DestroyValue (nsMsgSearchValue *value);

	nsMsgSearchValueArray m_valueList;
	nsIMsgSearchAdapter *m_adapter;

protected:
};


inline PRBool IsStringAttribute (nsMsgSearchAttribute a)
{
	return ! (a == nsMsgSearchAttribPriority || a == nsMsgSearchAttribDate || 
		a == nsMsgSearchAttribMsgStatus || a == nsMsgSearchAttribMessageKey ||
		a == nsMsgSearchAttribSize || a == nsMsgSearchAttribAgeInDays ||
		a == nsMsgSearchAttribFolderInfo);
}



//---------------------------------------------------------------------------
// nsMsgSearchTerm specifies one criterion, e.g. name contains phil
//---------------------------------------------------------------------------

// perhaps this should go in its own header file, if this class gets
// its own cpp file, nsMsgSearchTerm.cpp
class nsMsgSearchTerm
{
public:
	nsMsgSearchTerm();
	nsMsgSearchTerm (nsMsgSearchAttribute, nsMsgSearchOperator, nsMsgSearchValue *, PRBool, char * arbitraryHeader); // the bool is true if AND, FALSE if OR
	nsMsgSearchTerm (nsMsgSearchAttribute, nsMsgSearchOperator, nsMsgSearchValue *, nsMsgSearchBooleanOp, char * arbitraryHeader);

	virtual ~nsMsgSearchTerm ();

	void StripQuotedPrintable (unsigned char*);
	PRInt32 GetNextIMAPOfflineMsgLine (char * buf, int bufferSize, int msgOffset, nsIMessage * msg, nsIMsgDatabase * db);


	nsresult MatchBody (nsMsgSearchScopeTerm*, PRUint32 offset, PRUint32 length, const char *charset, nsIMsgDBHdr * msg, nsIMsgDatabase * db);
	nsresult MatchArbitraryHeader (nsMsgSearchScopeTerm *,PRUint32 offset, PRUint32 length, const char *charset, nsIMsgDBHdr * msg, nsIMsgDatabase *db,
											char * headers, /* NULL terminated header list for msgs being filtered. Ignored unless ForFilters */
											PRUint32 headersSize, /* size of the NULL terminated list of headers */
											PRBool ForFilters /* true if we are filtering */);
	nsresult MatchString (nsString2 *, const char *charset, PRBool body = FALSE);
	nsresult MatchDate (time_t);
	nsresult MatchStatus (PRUint32);
	nsresult MatchPriority (nsMsgPriority);
	nsresult MatchSize (PRUint32);
	nsresult MatchRfc822String(const char *, const char *charset);
	nsresult MatchAge (time_t);

	nsresult EnStreamNew (nsString2 &stream);
	nsresult DeStream (char *, PRInt16 length);
	nsresult DeStreamNew (char *, PRInt16 length);

	nsresult GetLocalTimes (time_t, time_t, struct tm &, struct tm &);

	PRBool IsBooleanOpAND() { return m_booleanOp == nsMsgSearchBooleanAND ? PR_TRUE : PR_FALSE;}
	nsMsgSearchBooleanOp GetBooleanOp() {return m_booleanOp;}
	// maybe should return nsString2 &   ??
	const char * GetArbitraryHeader() {return m_arbitraryHeader.GetBuffer();}

	static char *	EscapeQuotesInStr(const char *str);
	PRBool MatchAllBeforeDeciding ();

	nsCOMPtr<nsIMsgHeaderParser> m_headerAddressParser;

	nsMsgSearchAttribute m_attribute;
	nsMsgSearchOperator m_operator;
	nsMsgSearchValue m_value;
	nsMsgSearchBooleanOp m_booleanOp;  // boolean operator to be applied to this search term and the search term which precedes it.
	nsString2 m_arbitraryHeader;         // user specified string for the name of the arbitrary header to be used in the search
									  // only has a value when m_attribute = attribOtherHeader!!!!
protected:
	nsresult		OutputValue(nsString2 &outputStr);
	nsMsgSearchAttribute ParseAttribute(char *inStream);
	nsMsgSearchOperator	ParseOperator(char *inStream);
	nsresult		ParseValue(char *inStream);
	nsresult		InitHeaderAddressParser();

};


/* Use this to help build menus in the search dialog. See APIs below */
#define kMsgSearchMenuLength 64
typedef struct nsMsgSearchMenuItem 
{
    int16 attrib;
    char name[kMsgSearchMenuLength];
    PRBool isEnabled;
} nsMsgSearchMenuItem;


//---------------------------------------------------------------------------
// MSG_BodyHandler: used to retrive lines from POP and IMAP offline messages.
// This is a helper class used by nsMsgSearchTerm::MatchBody
//---------------------------------------------------------------------------
class nsMsgBodyHandler
{
public:
	nsMsgBodyHandler (nsMsgSearchScopeTerm *, PRUint32 offset, PRUint32 length, nsIMsgDBHdr * msg, nsIMsgDatabase * db);
	
	// we can also create a body handler when doing arbitrary header filtering...we need the list of headers and the header size as well
	// if we are doing filtering...if ForFilters is false, headers and headersSize is ignored!!!
	nsMsgBodyHandler (nsMsgSearchScopeTerm *, PRUint32 offset, PRUint32 length, nsIMsgDBHdr * msg, nsIMsgDatabase * db,
					 char * headers /* NULL terminated list of headers */, PRUint32 headersSize, PRBool ForFilters);

	virtual ~nsMsgBodyHandler();

	// Returns nextline 
	PRInt32 GetNextLine(char * buf, int bufSize);    // returns next message line in buf, up to bufSize bytes.

	// Transformations
	void SetStripHtml (PRBool strip) { m_stripHtml = strip; }
	void SetStripHeaders (PRBool strip) { m_stripHeaders = strip; }

protected:
	void Initialize();  // common initialization code

	// filter related methods. For filtering we always use the headers list instead of the database...
	PRBool m_Filtering;
	PRInt32 GetNextFilterLine(char * buf, int bufSize);
	char * m_headers;  // pointer into the headers list in the original message hdr db...
	PRUint32 m_headersSize;
	PRUint32 m_headerBytesRead;

	// local / POP related methods
    void OpenLocalFolder();
	PRInt32 GetNextLocalLine(char * buf, int bufSize);      // goes through the mail folder 

	nsMsgSearchScopeTerm *m_scope;

	// local file state
//	XP_File	*m_localFile;
	// need a file stream here, I bet.
	PRInt32 m_localFileOffset;       // current offset into the mail folder file
	PRUint32 m_numLocalLines;

	// Offline IMAP related methods & state
	PRInt32 GetNextIMAPLine(char * buf, int bufSize);     // goes through the MessageDB 
	nsIMsgDBHdr * m_msgHdr;
	nsIMsgDatabase * m_db;
	PRInt32 m_IMAPMessageOffset;
	PRBool m_OfflineIMAP;		 // TRUE if we are in Offline IMAP mode, FALSE otherwise

	// News related methods & state
	PRInt32 m_NewsArticleOffset;
	PRInt32 GetNextNewsLine (nsIMsgDatabase * newsDB, char * buf, int bufSize);  // goes through the NewsDB

	// Transformations
	PRBool m_stripHeaders;		// TRUE if we're supposed to strip of message headers
	PRBool m_stripHtml;		// TRUE if we're supposed to strip off HTML tags
	PRBool m_passedHeaders;	// TRUE if we've already skipped over the headers
	PRBool m_messageIsHtml;	// TRUE if the Content-type header claims text/html
	PRInt32 ApplyTransformations (char *buf, PRInt32 length, PRBool &returnThisLine);
	void StripHtml (char *buf);
};





#endif // _nsMsgSearchCore_H_
