
#ifndef __nsMsgBodyHandler_h
#define __nsMsgBodyHandler_h

#include "nsMsgSearchScopeTerm.h"

//---------------------------------------------------------------------------
// MSG_BodyHandler: used to retrive lines from POP and IMAP offline messages.
// This is a helper class used by nsMsgSearchTerm::MatchBody
//---------------------------------------------------------------------------
class nsMsgBodyHandler
{
public:
	nsMsgBodyHandler (nsMsgSearchScopeTerm *,
                      PRUint32 offset,
                      PRUint32 length,
                      nsIMsgDBHdr * msg,
                      nsIMsgDatabase * db);
	
	// we can also create a body handler when doing arbitrary header
    // filtering...we need the list of headers and the header size as well
	// if we are doing filtering...if ForFilters is false, headers and
    // headersSize is ignored!!!
	nsMsgBodyHandler (nsMsgSearchScopeTerm *, PRUint32 offset, PRUint32 length, nsIMsgDBHdr * msg, nsIMsgDatabase * db,
					 const char * headers /* NULL terminated list of headers */, PRUint32 headersSize, PRBool ForFilters);

	virtual ~nsMsgBodyHandler();

    // returns next message line in buf, up to bufSize bytes.
	PRInt32 GetNextLine(char * buf, int bufSize);    

	// Transformations
	void SetStripHtml (PRBool strip) { m_stripHtml = strip; }
	void SetStripHeaders (PRBool strip) { m_stripHeaders = strip; }

protected:
	void Initialize();  // common initialization code

	// filter related methods. For filtering we always use the headers
    // list instead of the database...
	PRBool m_Filtering;
	PRInt32 GetNextFilterLine(char * buf, int bufSize);
    // pointer into the headers list in the original message hdr db...
	char * m_headers;  
	PRUint32 m_headersSize;
	PRUint32 m_headerBytesRead;

	// local / POP related methods
    void OpenLocalFolder();
    
    // goes through the mail folder 
	PRInt32 GetNextLocalLine(char * buf, int bufSize);      

	nsMsgSearchScopeTerm *m_scope;

	// local file state
//	XP_File	*m_localFile;
	// need a file stream here, I bet

    // current offset into the mail folder file.
	PRInt32 m_localFileOffset;       
	PRUint32 m_numLocalLines;

	// Offline IMAP related methods & state
    
    // goes through the MessageDB 
	PRInt32 GetNextIMAPLine(char * buf, int bufSize);
    
	nsIMsgDBHdr * m_msgHdr;
	nsIMsgDatabase * m_db;
	PRInt32 m_IMAPMessageOffset;

    // PR_TRUE if we are in Offline IMAP mode, PR_FALSE otherwise
	PRBool m_OfflineIMAP;		 

	// News related methods & state
	PRInt32 m_NewsArticleOffset;

    // goes through the NewsDB
	PRInt32 GetNextNewsLine (nsIMsgDatabase * newsDB, char * buf, int bufSize);  

	// Transformations
	PRBool m_stripHeaders;	// PR_TRUE if we're supposed to strip of message headers
	PRBool m_stripHtml;		// PR_TRUE if we're supposed to strip off HTML tags
	PRBool m_passedHeaders;	// PR_TRUE if we've already skipped over the headers
	PRBool m_messageIsHtml;	// PR_TRUE if the Content-type header claims text/html
	PRInt32 ApplyTransformations (char *buf, PRInt32 length, PRBool &returnThisLine);
	void StripHtml (char *buf);
};
#endif
