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
/*
   mhtmlstm.h --- generation of MIME HTML from the editor.
 */

#ifndef LIBMSG_MHTMLSTM_H
#define LIBMSG_MHTMLSTM_H

// Comment out the next line if you want to turn off the sending of multipart/related info.
#define MSG_SEND_MULTIPART_RELATED

//#ifdef MSG_SEND_MULTIPART_RELATED

#include "msg.h"
#include "itapefs.h"
#include "ptrarray.h"
#include "xp_file.h"
#include "ntypes.h"
#include "msgcpane.h"
#include "msgsendp.h"
#include "itapefs.h"

class MSG_MimeRelatedStreamIn;
class MSG_MimeRelatedStreamOut;
class MSG_MimeRelatedSubpart;
class MSG_MimeRelatedSaver;
class MSG_SendMimeDeliveryState;

typedef void (*DeliveryDoneCallback) (MWContext *context,
									  void *fe_data,
									  int status,
									  const char *error_message);

// A record which represents a part of this multipart.
// Encapsulates the stuff necessary to convert MIME part data
// into message text.
class MSG_MimeRelatedSubpart : public MSG_SendPart
{
public:
	char *m_pOriginalURL;          // Original URL for this lump
	char *m_pLocalURL;             // Local relative URL for this lump
	MSG_MimeRelatedSaver *m_pParentFS; // The parent FS that includes us
	char *m_pContentID;            // Content ID of this lump
	char *m_pEncoding;             // Content transfer encoding
	char *m_pContentName;		   // Content name (as conveyed by CopyURLInfo)
	MSG_MimeRelatedStreamOut *m_pStreamOut; // Current output (to file) stream
	XP_Bool m_rootPart;				// Are we the first part of the multipart?

	MSG_MimeRelatedSubpart(MSG_MimeRelatedSaver *parent,
						   char *pContentID,
						   char *pOriginal,
						   char *pLocal,
						   char *pMime,
						   int16 part_csid,
						   char *pRootFileName = NULL);

	~MSG_MimeRelatedSubpart(void);

	char                   *GetContentID(XP_Bool bAddMIMEPrefix);
	int                    GetStreamOut(IStreamOut **pReturn);
	int                    CloseStreamOut(void);
	int                    Write(void);

	// Copy information out of the URL used to fetch its data
	void					CopyURLInfo(const URL_Struct *pURL);

	// Function used by the mime encoder when writing to message file
	static int WriteEncodedMessageBody(const char *buf, int32 size, void *pPart);

	// Generate any additional header strings for this part
	// (such as the "Content-ID:" string)
	char *                 GenerateCIDHeader(void);
	char *				   GenerateEncodingHeader(void);
};

class MSG_MimeRelatedSaver;

class MSG_MimeRelatedParentPart : public MSG_SendPart
{
public:
	MSG_MimeRelatedParentPart(int16 part_csid);
	virtual ~MSG_MimeRelatedParentPart();

	//virtual int Write();

    //virtual int SetFile(const char* filename, XP_FileType filetype);
    //virtual int SetBuffer(const char* buffer);
    //virtual int SetOtherHeaders(const char* other);
	//virtual int AppendOtherHeaders(const char* moreother);
    //virtual int AddChild(MSG_SendPart* child);
};

class MSG_MimeRelatedSaver : public ITapeFileSystem
{
public:
	MWContext *    m_pContext; // Context
	char *         m_pBaseURL; // Base URL of this lump
	char *         m_pMessageID; // Message ID that we will use to generate
                               // unique names for each content ID
	char *         m_rootFilename; // Filename for root object (created at const. time)
  char *         m_pSourceBaseURL; // Only used to return absolute URLs for GetSourceURL()
	
	MSG_SendPart *m_pPart;

	// All these are parameters to be passed to StartMessageDelivery
	MSG_CompositionPane *m_pPane;
	MSG_CompositionFields *m_pFields;
	XP_Bool        m_digest;
	MSG_Deliver_Mode m_deliverMode;
	const char     *m_pBody;
	uint32         m_bodyLength;
	MSG_AttachedFile *m_pAttachedFiles;
	DeliveryDoneCallback m_cbDeliveryDone;
    void           (*m_pEditorCompletionFunc)(XP_Bool success, void *data);
    void           *m_pEditorCompletionArg;

	void ClearAllParts(void);

public:
    MSG_MimeRelatedSaver(MSG_CompositionPane *pane, MWContext *context, 
						 MSG_CompositionFields *fields,
						 XP_Bool digest_p, MSG_Deliver_Mode deliver_mode,
						 const char *body, uint32 body_length,
						 MSG_AttachedFile *attachedFiles,
						 DeliveryDoneCallback cb,
						 char **ppOriginalRootURL);
	virtual ~MSG_MimeRelatedSaver();


  virtual intn GetType();

    // This function is called before anything else.  It tells the file
    //  system the base url it is going to see.
    virtual void SetSourceBaseURL( char* pURL );

	virtual char* GetSourceURL(intn iFileIndex);

    // DESCRIPTION:
    //
    // Add a name to the file system.  It is up to the file system to localize
    //  the name.  For example, I could add 'http://home.netscape.com/'
    //  and the file system might decide that it should be called 'index.html'
    //  if the file system were DOS, the url might be converted to INDEX.HTML
    //
    // RETURNS: index of the file (0 based)
    //   
    virtual intn AddFile( char* pURL, char *pMimeType, int16 iDocCharSetID );

	// Count the number of files we know about.
	virtual intn GetNumFiles(void);

  virtual char* GetDestAbsURL();

    // Gets the name of the RELATIVE url to place in the file.  String is 
    //  allocated with XP_STRDUP();
    //  
    virtual char* GetDestURL( intn iFileIndex );

    // String is allocated with XP_STRDUP().
    virtual char* GetDestPathURL();

    //
    // Returns the name to display when saving the file, can be the same as 
    //  GetURLName. String is allocated with XP_STRDUP();
    //
    virtual char* GetHumanName( intn iFileIndex );

    virtual XP_Bool IsLocalPersistentFile(intn iFileIndex);

    // Does the file referenced by iFileIndex already exist?
    // For the MHTML version, this will always return FALSE.
    virtual XP_Bool FileExists(intn iFileIndex);

    //
    // Opens the output stream. Returns a stream that can be written to.  All
    //  'AddFile's occur before the first OpenStream.
    //
    virtual IStreamOut * OpenStream( intn iFileIndex );

    virtual void CloseStream( intn iFileIndex );

	// ### mwelch Added so that multipart/related message saver can properly construct
	//            messages using quoted/forwarded part data.
	// Tell the tape file system the mime type of a particular part.
	// (Calling this overrides any previously determined mime type for this part.)
	virtual void CopyURLInfo(intn iFileIndex, const URL_Struct *pURL);

    //
    // Called on completion, TRUE if completed successfully, FALSE if it failed.
    //  
    virtual void Complete( Bool bSuccess, EDT_ITapeFileSystemComplete *pfComplete, void *pArg );
	// Has this file system received a true multipart or just some HTML?
	char *GetMimeType(void);

	MWContext *GetContext(void) const { return m_pContext; }

	char *GetMessageID(void) const { return m_pMessageID; }

	MSG_MimeRelatedSubpart *GetSubpart( intn iFileIndex );

  // Used when the mail system is done sending the message.
  // Causes the editor to clean up from the save.
  static void UrlExit(MWContext *context, void *fe_data, int status, 
					  const char *error_message);
};

//#endif

#endif
