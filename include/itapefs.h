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

#ifndef ITAPEFS_H
#define ITAPEFS_H

#ifdef EDITOR
//
// Abstract output file stream.
//
class IStreamOut {
public:
    IStreamOut();
    virtual ~IStreamOut();

    virtual void Write( char *pBuffer, int32 iCount )=0;

    // NOTICE: the implementation is not pure.  There is a default
    //  implementation that implements this function interms of 
    //  'Write'.
    virtual int Printf( char * pFormat, ... );

    enum EOutStreamStatus {
        EOS_NoError,
        EOS_DeviceFull,
        EOS_FileError
    };

    virtual EOutStreamStatus Status(){ return EOS_NoError; }

    // implemented in terms of the interface.
    void WriteInt( int32 i ){ Write( (char*)&i, sizeof( int32 ) ); }
    void WriteZString( char* pString);
    void WritePartialZString( char* pString, int32 start, int32 end);
private:
    char* stream_buffer; // used to implement Printf
};


//-----------------------------------------------------------------------
// Abstract File System
//-----------------------------------------------------------------------
typedef void
EDT_ITapeFileSystemComplete( XP_Bool bSuccess, void *pArg );

class ITapeFileSystem {
    PRBool m_FirstBinary; // is the first file really binary, not text?
public:
    ITapeFileSystem() { m_FirstBinary = PR_FALSE; }
    // ITapeFileSystem::File, ITapeFileSystem::Publish, or
    // ITapeFileSystem::MailSend, 
    enum {File,Publish,MailSend};
    virtual intn GetType() = 0;

    // This function is called before anything else.  It tells the file
    //  system the base url for the URLs added in AddFile().
    // An actual file, not a directory.
    virtual void SetSourceBaseURL( char* pURL )=0;

    // DESCRIPTION:
    //
    // Add a name to the file system.  It is up to the file system to localize
    //  the name.  For example, I could add 'http://home.netsacpe.com/
    //  and the file system might decide that it should be called 'index.html'
    //  if the file system were DOS, the url might be converted to INDEX.HTML
    //
    // pMIMEType may be NULL.  In this case if the tape file system needs the
    // MIME type, it must figure it out by itself.
    //
    // RETURNS: index of the file (0 based), OR
    // ITapeFileSystem::Error if an error adding name, OR
    // ITapeFileSystem::SourceDestSame if adding
    // this name would result in the source and destination being the same, and thus 
    // no point in copying the file.  
    //   
    // The first file added must be the root HTML document. (It is ok for the root
    // document to have the same source and dest URL).
    //
    virtual intn AddFile( char* pURL, char *pMIMEType, int16 iCharSetID)=0;

    // Return the number of files added to the file system.
    virtual intn GetNumFiles()=0;

    // Returns the absolute version of the URL given in AddFile(), using the 
    // URL given in SetSourceBaseURL() as the base.
    // Allocated with XP_STRDUP().
    virtual char* GetSourceURL(intn iFileIndex)=0;

    // Return the absolute destination of the HTML doc if meaningful, else return 
    // NULL.  Almost the same as "GetDestPathURL()+GetDestURL(0)" except that this call
    // will work before file 0 has been added to the file system.
    virtual char* GetDestAbsURL()=0;

    // Gets the name of the RELATIVE url to place in the file.  String is 
    //  allocated with XP_STRDUP();
    //  
    virtual char* GetDestURL( intn iFileIndex )=0;

    // Return the path URL associated with the ITapeFilesystem or NULL if there is none.
    // If NULL is returned, all URLs returned by GetDestURL() must be absolute.
    //
    // i.e. for a file or remote HTTP based ITapeFileSystem, this is the directory where the images are 
    // stored.  For a MHTML ITapeFileSystem this is NULL.
    //
    // String is allocated with XP_STRDUP().
    virtual char* GetDestPathURL() = 0;

    //
    // Returns the name to display when saving the file, can be the same as 
    //  GetURLName. String is allocated with XP_STRDUP();
    //
    virtual char* GetHumanName( intn iFileIndex )=0;

    enum {
        Error = -1, SourceDestSame = -2
    };

    // Does the file referenced by iFileIndex already exist?
    // E.g. for the MHTML version, this will always return FALSE.
    virtual XP_Bool FileExists(intn iFileIndex) = 0;

    // Will we be creating a new non-temporary file on the local machine.
    // Used to update SiteManager.
    virtual XP_Bool IsLocalPersistentFile(intn iFileIndex) = 0;

	// ### mwelch Added so that multipart/related message saver can properly construct
	//            messages using quoted/forwarded part data.
	// Tell the tape file system the mime type of a particular part.
	// Calling this overrides any previously determined mime type for this part.
	virtual void CopyURLInfo(intn iFileIndex, const URL_Struct *pURL) = 0;
	
    //
    // Opens the output stream. Returns a stream that can be written to or NULL if error.  All
    //  'AddFile's occur before the first OpenStream.
    // Do not delete the returned stream, just call CloseStream() when done.
    //
    virtual IStreamOut *OpenStream( intn iFileIndex )=0;

    virtual void CloseStream( intn iFileIndex )=0;

    // Called on completion, TRUE if completed successfully, FALSE if it failed.
    // The caller should not reference the ITapeFileSystem after calling Complete().
    // Caller does not free up memory for ITapeFileSystem, Complete() causes file system to delete itself.
    //  
    // The tape file system will call pfComplete with pArg and with whether the ITapeFileSystem 
    // completed successfully.  Note: the ITapeFileSystem may call pfComplete() with TRUE even if
    // ITapeFileSystem::Complete() was given FALSE.
    // pfComplete may be NULL.  Call to pfComplete may be synchronous or asynchronous.  
    //
    // The ITapeFileSystem will call pfComplete(success,pArg) before deleting itself.  I.e. the ITapeFileSystem is still valid 
    // when it calls pfComplete().
    virtual void Complete( Bool bSuccess, EDT_ITapeFileSystemComplete *pfComplete, void *pArg )=0;

    inline PRBool IsFirstBinary(void) { return m_FirstBinary; }
    inline void SetFirstBinary(void) { m_FirstBinary = PR_TRUE; }
    inline void ResetFirstBinary(void) { m_FirstBinary = PR_FALSE; }
};

#endif // EDITOR

#endif
