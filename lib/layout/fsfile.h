/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#ifndef FSFILE_H
#define FSFILE_H

#ifdef EDITOR

#include "xp.h"
#include "xp_file.h"
#include "streams.h"
#include "bits.h"  // mac seems to need it for garray.h to work. hardts
#include "garray.h"

// Same as TXP_GrowableArray_pChar 
Declare_GrowableArray(char,char*)    // TXP_GrowableArray_char

Declare_GrowableArray(CStreamOutFile,CStreamOutFile *) // TXP_GrowableArray_CStreamOutFile

class CFileBackup;
Declare_GrowableArray(CFileBackup,CFileBackup *) // TXP_GrowableArray_CFileBackup

//-----------------------------------------------------------------------
// File based version of ITapeFileSystem
//-----------------------------------------------------------------------


class CTapeFSFile: public ITapeFileSystem {
public:
    // Give the directory where everything will be saved.
    CTapeFSFile(char *pDestPathURL,char *pDestURL);
    virtual ~CTapeFSFile();

    virtual intn GetType();

    virtual void SetSourceBaseURL( char* pURL );

    // See ITapeFileSystem::AddFile().
    virtual intn AddFile( char* pURL, char *pMIMEType, int16 iDocCharSetID );
    
    virtual intn GetNumFiles() {return m_srcURLs.Size();}    

    // Simply returns the URL given in AddFile().  Allocated with XP_STRDUP().
    virtual char* GetSourceURL(intn iFileIndex);

    virtual char* GetDestAbsURL();

    // iFileIndex must be in {0...GetNumFiles()-1}
    // Returns URL relative to document, i.e. just the stripped local file name.
    virtual char* GetDestURL( intn iFileIndex );
    virtual char* GetHumanName( intn iFileIndex ) {return GetDestURL(iFileIndex);}

    virtual char* GetDestPathURL();

    virtual XP_Bool IsLocalPersistentFile(intn iFileIndex);

    virtual XP_Bool FileExists(intn iFileIndex);

    // Save old file first.
    virtual IStreamOut *OpenStream( intn iFileIndex );

    virtual void CloseStream( intn iFileIndex );
    
    virtual void Complete( XP_Bool bSuccess,
                                    EDT_ITapeFileSystemComplete *complete, void *pArg  );

	void CopyURLInfo(intn iFileIndex, const URL_Struct *pURL);
	
protected:
    // Can be NULL.    
    char *m_pSrcBaseURL;
    // These are absolute URLs.
    TXP_GrowableArray_char m_srcURLs;
     // All dest filenames relative to pDestPathURL, which is a directory.
    char *m_pDestPathURL;       
    // The destination filename for the root HTML document.  Should be file:// url.
    char *m_pDestURL;    
    // relative filenames   
    TXP_GrowableArray_char m_destFilenames;
    
    TXP_GrowableArray_CStreamOutFile m_streamOuts;
    TXP_GrowableArray_CFileBackup m_fileBackups;
};

//-----------------------------------------------------------------------
// HTTP/FTP publish based version of ITapeFileSystem
// Similar to CTapeFSFile because it writes to temporary files,
// then publishes.
//-----------------------------------------------------------------------
class CTapeFSPublish: public ITapeFileSystem {
public:
    // Important: If pRemoteURL is a directory, it must end with a slash.
    // pUsername and pPassword may be NULL.
    CTapeFSPublish(MWContext *, char *pRemoteURL, char *pUsername, char *pPassword, 
                    char *pTempDir); // where to put temp files, in xpURL format, ends in slash.
    virtual ~CTapeFSPublish();

    virtual intn GetType();

    virtual void SetSourceBaseURL( char* pURL );

    // See ITapeFileSystem::AddFile().
    virtual intn AddFile( char* pURL, char *pMIMEType, int16 iDocCharSetID );
    
    virtual intn GetNumFiles() {return m_srcURLs.Size();}    

    // Simply returns the URL given in AddFile().  Allocated with XP_STRDUP().
    virtual char* GetSourceURL(intn iFileIndex);

    virtual char* GetDestAbsURL();

    char *GetUsername();
    char *GetPassword();

    // iFileIndex must be in {0...GetNumFiles()-1}
    // Returns URL relative to document, i.e. just the stripped local file name.
    virtual char* GetDestURL( intn iFileIndex );
    virtual char* GetHumanName( intn iFileIndex ) {return GetDestURL(iFileIndex);}

    // Returns the directory on the remote server (with trailing slash) where the document will
    // be published.  Does not contain username or password info.  
    // E.g. http://the_machine/the_directory/  not http://username:password@the_machine/the_directory/
    virtual char* GetDestPathURL();

    virtual XP_Bool IsLocalPersistentFile(intn iFileIndex);

    virtual XP_Bool FileExists(intn /* iFileIndex */ ) {return FALSE;}
    
    // Save old file first.
    virtual IStreamOut *OpenStream( intn iFileIndex );

    virtual void CloseStream( intn iFileIndex );
    
    // Calls Net_PublishFiles().
    virtual void Complete( XP_Bool bSuccess,
                                     EDT_ITapeFileSystemComplete *pfComplete, void *pArg  );
    
    XP_Bool Verify() {return m_iVerifier == iVerifierKey;}

	void CopyURLInfo(intn iFileIndex, const URL_Struct *pURL);

public:
    // These are just here to pass data to edt_CTapeFSExit().
    EDT_ITapeFileSystemComplete *m_pfComplete;
    void *m_pArg;
    
private:
    int32 m_iVerifier;
    static int32 iVerifierKey;
    
    // Makes a new url pointing to a file in the same directory as baseURL, baseURL may also be a 
    // directory, e.g. http://home.netscape.com/     
    // Uses srcURL to choose the name and returns an absolute URL.
    char *makeLocal(char *baseURL, char *srcURL);

    // For call to NET_PublishFiles.
    MWContext *m_pMWContext;
    
    // Can be NULL.    
    char *m_pSrcBaseURL;

    // These are absolute URLs.
    TXP_GrowableArray_char m_srcURLs;

    // final location of the HTML file
    char *m_pRemoteURL;    // {http,ftp}://location
    char *m_pUsername;
    char *m_pPassword;

    // final locations of all the files.  Note that element 0 will be a copy of m_pRemoteURL.
    // These contain username:password information.
    TXP_GrowableArray_char m_remoteURLs;

    char *m_pTempDir;  // xpURL format.
    // Absolute platform specific filenames.  The temp files.
    TXP_GrowableArray_char m_tempFilenames;

    // Streams to write to temp files.
    TXP_GrowableArray_CStreamOutFile m_streamOuts;

    // TRUE if http, FALSE if ftp
    XP_Bool m_bIsHTTP;
};

#endif // EDITOR
#endif
