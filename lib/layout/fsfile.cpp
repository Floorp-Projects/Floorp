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


// Implementation of file-based ITapeFileSystem.
// Used by the editor to save to disk.

#ifdef EDITOR

// Work-around for Win16 precompiled header bug -- all .cpp files in
// lib/layout have to include editor.h first. This file
// doesn't even need editor.h.
////// Now it does, edt_StripUsernamePassword.
#include "editor.h"

#include "fsfile.h"

#ifdef XP_MAC
#include "xp_file_mac.h"
#endif

//-----------------------------------------------------------------------------
//  CFileBackup
//
// File backup and restore Object, only used by CTapeFSFile.
//
//-----------------------------------------------------------------------------
class CFileBackup {
private:
    XP_Bool m_bInitOk;
    char *m_pBackupName;
    char *m_pFileName;

public:
    CFileBackup(): m_bInitOk(FALSE), m_pBackupName(0), m_pFileName(0){}

    ~CFileBackup(){
        Reset();
    }

    void Reset();
    XP_Bool InTransaction(){ return m_bInitOk; }
    ED_FileError BeginTransaction( char *pDestURL );
    char* FileName(){ return m_pFileName; }
    void Commit();
    void Rollback();
};


void CFileBackup::Reset(){
    if( m_pBackupName ){
        XP_FREE( m_pBackupName );
        m_pBackupName = 0;
    }
    if( m_pFileName ){
        XP_FREE( m_pFileName );
        m_pFileName = 0;
    }
    m_bInitOk = FALSE;
}

ED_FileError CFileBackup::BeginTransaction( char *pDestURL ){
    XP_StatStruct statinfo;

    if ( pDestURL == NULL || XP_STRLEN(pDestURL) == 0 || 
             !NET_IsLocalFileURL(pDestURL) ) {
        return ED_ERROR_BAD_URL;
    }
    
    // Skip past the "file://" in pDestURL
    m_pFileName = NET_ParseURL(pDestURL,GET_PATH_PART);
    if (!m_pFileName) {
      return ED_ERROR_CREATE_BAKNAME;
    }
    if (XP_Stat(m_pFileName, &statinfo, xpURL) != -1) {
      if ( XP_STAT_READONLY( statinfo ) ){
          return ED_ERROR_READ_ONLY;
      }

      // File exists - rename to backup to protect data
      m_pBackupName = XP_BackupFileName(pDestURL);
      if ( m_pBackupName == NULL ) {
          return ED_ERROR_CREATE_BAKNAME;
      }
      // Delete backup file if it exists
      if ( -1 != XP_Stat(m_pBackupName, &statinfo, xpURL) &&
            statinfo.st_mode & S_IFREG ) {
          if ( 0 != XP_FileRemove(m_pBackupName, xpURL) ) {
              return ED_ERROR_DELETE_BAKFILE;
          }
      }
      if ( 0 != XP_FileRename(m_pFileName, xpURL, 
                              m_pBackupName, xpURL) ){
          return ED_ERROR_FILE_RENAME_TO_BAK;
      }
    }
    // else file doesn't already exist, so no worries.

    m_bInitOk = TRUE;
    return ED_ERROR_NONE;
}


void CFileBackup::Commit() {
    XP_ASSERT( m_bInitOk );

#ifdef XP_UNIX
    XP_StatStruct statinfo;

    if (m_pBackupName != NULL && m_pFileName != NULL &&
		XP_Stat(m_pBackupName, &statinfo, xpURL) != -1) {
		
		/*
		 *    Is there an XP_chmod()? I cannot find one. This will
		 *    work for Unix, which is probably the only place that cares.
		 *    Don't bother to check the return status, as it's too late to
		 *    do anything about, and we are not in dire straights if it fails.
		 *    Bug #28775..djw
		 */
		chmod(m_pFileName, statinfo.st_mode);
	}
#endif

#ifdef XP_MAC
	// Mac needs to duplicate the resource fork of the old file into new location
	// when there is a save or saveas; this preserves CKID rsrc among other things
	if ( m_pBackupName != NULL && m_pFileName != NULL )
		int result = XP_FileDuplicateResourceFork( m_pBackupName, xpURL,
											m_pFileName, xpURL );
#endif

    XP_FileRemove(m_pBackupName, xpURL);
}

void CFileBackup::Rollback(){
    XP_ASSERT( m_bInitOk );
    if ( m_pBackupName ) {
        // Restore previous file
        // If this fails, we're really messed up!
        XP_FileRemove(m_pFileName, xpURL);
        XP_FileRename(m_pBackupName, xpURL, m_pFileName, xpURL);
    }
}

//-------------------------------------------------------------------------------
// CTapeFSFile: File-based version of abstract file system
//-------------------------------------------------------------------------------
CTapeFSFile::CTapeFSFile(char *pDestPathURL,char *pDestURL):
    m_pSrcBaseURL(0)
{
    XP_ASSERT(pDestPathURL && pDestURL);    
    m_pDestPathURL = XP_STRDUP(pDestPathURL);
    m_pDestURL = XP_STRDUP(pDestURL);
    XP_ASSERT(m_pDestPathURL && m_pDestURL);    
}

CTapeFSFile::~CTapeFSFile()
{
    ///// TODO DELETE TEMP FILES

    int i;        
    for (i = 0; i < m_srcURLs.Size(); i++) {
        if (m_srcURLs[i]) 
            XP_FREE(m_srcURLs[i]);        
    }
    for (i = 0; i < m_destFilenames.Size(); i++) {
        if (m_destFilenames[i])
            XP_FREE(m_destFilenames[i]);
    }
    for (i = 0; i < m_streamOuts.Size(); i++) {
        if (m_streamOuts[i])  // really don't need to check for NULL with C++ delete
            delete m_streamOuts[i];
    }
    for (i = 0; i < m_fileBackups.Size(); i++) {
        if (m_fileBackups[i])  // really don't need to check for NULL with C++ delete
            delete m_fileBackups[i];
    }

    if (m_pDestPathURL)
        XP_FREE(m_pDestPathURL);
    if (m_pDestURL)    
        XP_FREE(m_pDestURL);
    if (m_pSrcBaseURL)
        XP_FREE(m_pSrcBaseURL);
}

void CTapeFSFile::CopyURLInfo(intn, const URL_Struct *) 
{ 
}

intn CTapeFSFile::GetType() {
  return ITapeFileSystem::File;
}

void CTapeFSFile::SetSourceBaseURL(char *pURL){
    // Shouldn't pass in NULL, and m_pSrcBaseURL shouldn't already exist.
    if (m_pSrcBaseURL || !pURL) {
        XP_ASSERT(0);  
        return;
    }

    m_pSrcBaseURL = XP_STRDUP(pURL);
}

intn CTapeFSFile::AddFile(char *pURL, char *, int16) {
    // MIME type and char set are ignored.

    // Make pSrcURL absolute if SetSourceBaseURL() was called.
    char *pSrcURL;
    if (m_pSrcBaseURL)
        pSrcURL =  NET_MakeAbsoluteURL(m_pSrcBaseURL,pURL);
    else
        pSrcURL = XP_STRDUP(pURL);

    if (!pSrcURL)
        return ITapeFileSystem::Error;
    
    // First file added is the root HTML document, it's destination filename was passed in to
    // the constructor of CTapeFSFile.
    XP_Bool use_m_pDestURL = (m_srcURLs.Size() == 0);

    // Compute and store relative destination filename.
    char *pDestFilename;
    if (use_m_pDestURL) {
        // Kind of redundant, here we are stripping the path, only to put it back on in CTapeFSFile::OpenStream().
        pDestFilename = FE_URLToLocalName(m_pDestURL);
    }
    else {
        pDestFilename =  FE_URLToLocalName(pSrcURL);
    }
    if (!pDestFilename) {
        XP_FREE(pSrcURL);
        return ITapeFileSystem::Error;
    }

    // Check if source and destination are the same. 
    // It's ok for the root HTML document to have the same source and dest.
    //
    // pSrcURL is absolute
    if (!use_m_pDestURL && EDT_IsSameURL(pSrcURL,pDestFilename,NULL,m_pDestURL)) {
        XP_FREE(pSrcURL);
        XP_FREE(pDestFilename);
        return ITapeFileSystem::SourceDestSame;
    }

    // See if pSrcURL is already in the list.
    int i = 0;
    while( i < m_srcURLs.Size() ){
        if( EDT_IsSameURL( pSrcURL, m_srcURLs[i], NULL, NULL ) ){
            XP_FREE(pSrcURL);
            XP_FREE(pDestFilename);
            return i;
        }
        i++;
    }

    // Add to lists.
    int ret = m_srcURLs.Add(pSrcURL);
    m_destFilenames.Add(pDestFilename);
    // Keep m_streamOuts and m_fileBackups the same size as the others.
    m_streamOuts.Add(NULL);
    m_fileBackups.Add(NULL);
    return ret;
}

char* CTapeFSFile::GetSourceURL(intn iFileIndex) {
    if (iFileIndex >= 0 && iFileIndex < m_srcURLs.Size()) {
        return XP_STRDUP(m_srcURLs[iFileIndex]);
    }
    else {
        XP_ASSERT(0);
        return NULL;
    }
}

char* CTapeFSFile::GetDestAbsURL() {
  return XP_STRDUP(m_pDestURL);
}

char *CTapeFSFile::GetDestURL(intn iFileIndex) {
    if (iFileIndex >= 0 && iFileIndex < m_destFilenames.Size()) {
        return XP_STRDUP(m_destFilenames[iFileIndex]);
    }
    else {
        XP_ASSERT(0);
        return NULL;
    }
}

char *CTapeFSFile::GetDestPathURL() {
    return XP_STRDUP(m_pDestPathURL);
}

XP_Bool CTapeFSFile::IsLocalPersistentFile(intn) {
    return TRUE;
}

XP_Bool CTapeFSFile::FileExists(intn iFileIndex) {
    // Check iFileIndex
    if (iFileIndex < 0 || iFileIndex >= m_destFilenames.Size()) {
        XP_ASSERT(0);
        return FALSE;    
    }

    char *pLocalName = PR_smprintf( "%s%s", m_pDestPathURL, m_destFilenames[iFileIndex]);
     if (!pLocalName) {
        XP_ASSERT(0);
        return FALSE;
    }

    // Get everything after "file://"
    char *pxpURL = NET_ParseURL(pLocalName,GET_PATH_PART);
    XP_FREE(pLocalName);
    if (!pxpURL) {
      XP_ASSERT(0);
      return FALSE;
    }
    
    XP_StatStruct statinfo;
    Bool result = 
      ( -1 != XP_Stat(pxpURL, &statinfo, xpURL) && statinfo.st_mode & S_IFREG );
    XP_FREE(pxpURL);
    return result;
}

IStreamOut *
CTapeFSFile::OpenStream( intn iFileIndex ) {
    // Check iFileIndex
    if (iFileIndex < 0 || iFileIndex >= m_destFilenames.Size()) {
        XP_ASSERT(0);
        return NULL;    
    }

    // Create fileBackup object
    XP_ASSERT( m_fileBackups[iFileIndex] == NULL ); // Shouldn't already exist.
    m_fileBackups[iFileIndex]= new CFileBackup;
    m_fileBackups[iFileIndex]->Reset();
    char *pDestURL = PR_smprintf( "%s%s", m_pDestPathURL, m_destFilenames[iFileIndex]);
    if ( !pDestURL ) {
        XP_ASSERT(0);
        return NULL;
    }
    ED_FileError status = m_fileBackups[iFileIndex]->BeginTransaction( pDestURL );
    if( status != ED_ERROR_NONE ){
        XP_FREE(pDestURL);        
        return NULL;
    }

    // Get path part of URL, e.g. without file://, for everything that uses xpURL
    char *pDestPath = NET_ParseURL(pDestURL,GET_PATH_PART);
    XP_FREE(pDestURL);        
    if (!pDestPath) {
        XP_ASSERT(0);
        return NULL; 
    }
    
    XP_File outFile = 0;
    // First file is text, rest are binary.
    // Right, well now the first file may be text or binary. This is still
    // bogus-- each file should have a flag telling it's time, but I don't
    // have time to redesign the #!$#@!#!@$ editor.
    XP_FilePerm filePerm = ((iFileIndex == 0) && (!IsFirstBinary())) ? 
				XP_FILE_TRUNCATE : XP_FILE_TRUNCATE_BIN;
    outFile = XP_FileOpen(pDestPath, xpURL, filePerm );
    XP_FREE(pDestPath);

    if( outFile == 0 ){
        return NULL;
    }


    // Now open a stream to outFile.
    //    
    // First file is text, rest are binary.
    CStreamOutFile *streamOut = new CStreamOutFile(outFile,iFileIndex == 0 ? FALSE : TRUE);
    XP_ASSERT(streamOut);    

    XP_ASSERT(m_streamOuts[iFileIndex] == NULL);  // not already set
    // Store stream for later.
    m_streamOuts[iFileIndex] = streamOut;

    return streamOut;    
}    

void CTapeFSFile::CloseStream( intn iFileIndex ) {
    if (iFileIndex < 0 || iFileIndex >= m_streamOuts.Size() || !m_streamOuts[iFileIndex]) {
        XP_ASSERT(0);
        return;
    }

    delete m_streamOuts[iFileIndex];
    m_streamOuts[iFileIndex] = NULL;
}

void CTapeFSFile::Complete( XP_Bool bSuccess,
                                                EDT_ITapeFileSystemComplete *pfComplete, void *pArg ) {
    // Commit or rollback all file changes, depending on the value of bSuccess.
    int i;
    for ( i = 0; i < m_fileBackups.Size(); i++ ) {
        if ( !m_fileBackups[i] ) {
            continue;
        }
        if ( m_fileBackups[i]->InTransaction() ) {
            if ( bSuccess )
                m_fileBackups[i]->Commit();
            else
                m_fileBackups[i]->Rollback();
        }
    }    

    if (pfComplete) {
        pfComplete(TRUE,pArg);
    }
    delete this;
}

//-------------------------------------------------------------------------------
// CTapeFSPublish: Remote HTTP publish version of abstract file system
//-------------------------------------------------------------------------------
CTapeFSPublish::CTapeFSPublish(MWContext *pMWContext, char *pRemoteURL,
                    char *pUsername, char *pPassword, char *pTempDir)  :
m_iVerifier(iVerifierKey),
m_pSrcBaseURL(NULL),
m_pRemoteURL(XP_STRDUP(pRemoteURL)),
m_pUsername(pUsername ? XP_STRDUP(pUsername) : 0),
m_pPassword(pPassword ? XP_STRDUP(pPassword) : 0),
m_pArg(0),
m_pfComplete(0),
m_bIsHTTP(FALSE),
m_pTempDir(XP_STRDUP(pTempDir))
{
    m_pMWContext = pMWContext; 
    XP_ASSERT( m_pRemoteURL && pMWContext );    
    // m_pTempDir must end in a slash.
    XP_ASSERT(m_pTempDir && *m_pTempDir && m_pTempDir[XP_STRLEN(m_pTempDir)-1] == '/');

    int type = NET_URL_Type(pRemoteURL);
    if (type == FTP_TYPE_URL) {
        m_bIsHTTP = FALSE;            
    }
    else if (type == HTTP_TYPE_URL || type == SECURE_HTTP_TYPE_URL) {
        m_bIsHTTP = TRUE;            
    }
    else {
        XP_ASSERT(0);
    }
}

CTapeFSPublish::~CTapeFSPublish() {
    XP_FREEIF(m_pSrcBaseURL);

    int i;        
    for (i = 0; i < m_srcURLs.Size(); i++) {
       XP_FREEIF(m_srcURLs[i]);        
    }

    XP_FREEIF(m_pRemoteURL);
    XP_FREEIF(m_pUsername);
    XP_FREEIF(m_pPassword);

    for (i = 0; i < m_remoteURLs.Size(); i++) {
      XP_FREEIF(m_remoteURLs[i]);        
    }

    XP_FREEIF(m_pTempDir);
    for (i = 0; i < m_tempFilenames.Size(); i++) {
        if (m_tempFilenames[i]) {
            // Delete temp files.
            XP_FileRemove(m_tempFilenames[i], xpFileToPost);
            XP_FREE(m_tempFilenames[i]);
        }
    }

    for (i = 0; i < m_streamOuts.Size(); i++) {
        if (m_streamOuts[i])  // really don't need to check for NULL with C++ delete
            delete m_streamOuts[i];
    }
}

void CTapeFSPublish::CopyURLInfo(intn, const URL_Struct *) 
{ 
}

intn CTapeFSPublish::GetType() {
  return ITapeFileSystem::Publish;
}

// Some random value.
int32 CTapeFSPublish::iVerifierKey = 0xABACADAE;

void CTapeFSPublish::SetSourceBaseURL(char *pURL){
    // Shouldn't pass in NULL, and m_pSrcBaseURL shouldn't already exist.
    if (m_pSrcBaseURL || !pURL) {
        XP_ASSERT(0);  
        return;
    }
    m_pSrcBaseURL = XP_STRDUP(pURL);
}

intn CTapeFSPublish::AddFile(char *pURL, char *, int16) {
    // MIME type and char set are ignored.

    // Make pSrcURL absolute if SetSourceBaseURL() was called.
    char *pSrcURL;
    if (m_pSrcBaseURL)
        pSrcURL =  NET_MakeAbsoluteURL(m_pSrcBaseURL,pURL);
    else
        pSrcURL = XP_STRDUP(pURL);

    if (!pSrcURL)
        return ITapeFileSystem::Error;
    
    // See if pSrcURL is already in the list.
    int i = 0;
    while( i < m_srcURLs.Size() ){
        if( EDT_IsSameURL( pSrcURL, m_srcURLs[i], NULL, NULL)){
            XP_FREE(pSrcURL);
            return i;
        }
        i++;
    }

    // Note that the way pNewRemoteURL is constructed has the desirable
    // side effect of NOT copying in any username/password information 
    // in pURL.
    char *pNewRemoteURL = NULL;
    if (m_srcURLs.Size() == 0)
        // First file added is the root HTML document.    
        pNewRemoteURL = XP_STRDUP(m_pRemoteURL);

    else {
        // All others are images that will be placed in the directory of the root doc.    
        pNewRemoteURL = makeLocal(m_pRemoteURL,pSrcURL);

    }
    if ( !pNewRemoteURL ) {
        XP_FREE(pSrcURL);
        return ITapeFileSystem::Error;
    }

    // Check if would be copying from one location to itself.
    if (m_srcURLs.Size() > 0 && // Make sure not the root document.
        EDT_IsSameURL(pSrcURL,pNewRemoteURL,NULL,m_pRemoteURL)) {
      XP_FREE(pSrcURL);
      XP_FREE(pNewRemoteURL);
      return ITapeFileSystem::SourceDestSame;
    }

    // Add to lists.
    int ret = m_srcURLs.Add(pSrcURL);
    m_remoteURLs.Add(pNewRemoteURL);
    // Keep the same size as the other arrays..
    m_tempFilenames.Add(NULL);
    m_streamOuts.Add(NULL);
    return ret;
}

char* CTapeFSPublish::GetSourceURL(intn iFileIndex) {
    if (iFileIndex >= 0 && iFileIndex < m_srcURLs.Size()) {
        return XP_STRDUP(m_srcURLs[iFileIndex]);
    }
    else {
        XP_ASSERT(0);
        return NULL;
    }
}

char* CTapeFSPublish::GetDestAbsURL() {
  return XP_STRDUP(m_pRemoteURL);
}

char *CTapeFSPublish::GetUsername() {
  return m_pUsername ? XP_STRDUP(m_pUsername) : 0;
}

char *CTapeFSPublish::GetPassword() {
  return m_pPassword ? XP_STRDUP(m_pPassword) : 0;
}

char *CTapeFSPublish::GetDestURL(intn iFileIndex) {
    if (iFileIndex >= 0 && iFileIndex < m_remoteURLs.Size()) {
        // Find last slash.
        char *slash = XP_STRRCHR(m_remoteURLs[iFileIndex],'/');
        if (!slash) {
            XP_ASSERT(0);
            return NULL;
        }
        // Copy everything after the slash.
        return XP_STRDUP(slash + 1);
    }
    else {
        XP_ASSERT(0);
        return NULL;
    }
}

char *CTapeFSPublish::GetDestPathURL() {
    // Grab everything before and including the last slash in m_pRemoteURL.
    char *slash = XP_STRRCHR(m_pRemoteURL,'/');
    if (!slash) {
        XP_ASSERT(0);
        return NULL;
    }

    char tmp = *(slash + 1);
    *(slash + 1) = '\0';
    char *ret = XP_STRDUP(m_pRemoteURL);
    *(slash + 1) = tmp;
    return ret;
}

XP_Bool CTapeFSPublish::IsLocalPersistentFile(intn /* iFileIndex */) {
    return FALSE;
}

IStreamOut *
CTapeFSPublish::OpenStream( intn iFileIndex ) {
    // Check iFileIndex
    if (iFileIndex < 0 || iFileIndex >= m_tempFilenames.Size()) {
        XP_ASSERT(0);
        return NULL;    
    }


    // WARNING: We are pulling a fast one here, m_pTempDir is xpURL and we treat it as xpFileToPost.
    // Since there are no routines to convert a filename to xpFileToPost, I just take advantage of 
    // it being the same as xpURL.

    // Get filename of temporary file.
    // Use the temporary file of the current document.
    // Start filename with "pub" so it doesn't collide with CEditCommandLog::CreateDocTempFilename()
    // which starts all filenames with "edt" by default.  
    char *pTempFilename = PR_smprintf("%spubl%d.tmp",m_pTempDir,(int)iFileIndex); // m_pTempDir ends in slash.
    if (!pTempFilename) {
      return NULL;
    }
    m_tempFilenames[iFileIndex] = pTempFilename;


    XP_File outFile = 0;
    // First file is text, rest are binary.
    // Right, well now the first file may be text or binary. This is still
    // bogus-- each file should have a flag telling it's time, but I don't
    // have time to redesign the #!$#@!#!@$ editor.
    XP_FilePerm filePerm = ((iFileIndex == 0) && (!IsFirstBinary())) ? 
				XP_FILE_TRUNCATE : XP_FILE_TRUNCATE_BIN;
     outFile = XP_FileOpen(m_tempFilenames[iFileIndex],
                 xpFileToPost, filePerm );

    XP_TRACE(("XP_FileOpen handle = %d", outFile));
    
    if( outFile == 0 ){
        XP_TRACE(("Failed to opened file %s", m_tempFilenames[iFileIndex]));

        // Delete and clear temp filename, this serves as a flag to 
        // Complete() that this entry should not be published.
        XP_FREEIF(m_tempFilenames[iFileIndex]);
        return NULL;
    }

    // Now open a stream to outFile.
    //    
    // First file is text, rest are binary.
    CStreamOutFile *streamOut = new CStreamOutFile(outFile,iFileIndex == 0 ? FALSE : TRUE);
    if (!streamOut) {
      // Out of memory.
      XP_ASSERT(0);
      return NULL;
    }

    XP_ASSERT(m_streamOuts[iFileIndex] == NULL);  // not already set
    // Store stream for later.
    m_streamOuts[iFileIndex] = streamOut;

    return streamOut;    
}    

void CTapeFSPublish::CloseStream( intn iFileIndex ) {
    if (iFileIndex < 0 || iFileIndex >= m_streamOuts.Size() || !m_streamOuts[iFileIndex]) {
        XP_ASSERT(0);
        return;
    }

    if (m_streamOuts[iFileIndex]->Status() != IStreamOut::EOS_NoError) {
        // Delete and clear temp filename, this serves as a flag to 
        // Complete() that this entry should not be published.
        XP_FREEIF(m_tempFilenames[iFileIndex]);
    }

    delete m_streamOuts[iFileIndex];
    m_streamOuts[iFileIndex] = NULL;
}

#if defined(XP_OS2)
extern "C"
#else
PRIVATE
#endif
void edt_CTapeFSExit( URL_Struct *pURL, int status, MWContext * ) {
    CTapeFSPublish *tapeFS = (CTapeFSPublish *)pURL->fe_data;
    // Make sure that fe_data can be trusted.
    if (!(tapeFS && tapeFS->Verify())) {
        XP_ASSERT(0);
        return;
    }

    // Call complete function passed into CTapeFSPublish::Complete().
    if (tapeFS->m_pfComplete) {
        XP_Bool bSuccess = (status >= 0 && 
            (pURL->server_status == 204 || pURL->server_status == 201 ||  // from HTTP
             pURL->server_status == 0)); // from FTP.
        tapeFS->m_pfComplete(bSuccess,tapeFS->m_pArg);
    }
    
    // Kill the tapeFS.
    delete tapeFS;
 }

void CTapeFSPublish::Complete(XP_Bool bSuccess, 
       EDT_ITapeFileSystemComplete *pfComplete, void *pArg ) {
    m_pfComplete = pfComplete;
    m_pArg = pArg;


    // Count number of files we're actually publishing, may be less than 
    // m_tempFilenames.Size() if errors occurred.
    int numFilesToPost = 0;
    for (int n = 0; n < m_tempFilenames.Size(); n++) {
      if (m_tempFilenames[n])
        numFilesToPost++;
    }


    // Publish temp files.  Temp files deleted in destructor.
    if (bSuccess && numFilesToPost ) {
        // One extra for trailing NULL.
        char **filesToPost = (char **)XP_ALLOC((numFilesToPost + 1) * sizeof(char*));
        if ( !filesToPost ) {
            XP_ASSERT(0);
            return;
        }

        // Where to put the files on the server.
        // FTP publish will ignore the path info in postTo and will just use 
        // the filename after the last slash.
        char **postTo = NULL; 
        postTo = (char **)XP_ALLOC((numFilesToPost + 1) * sizeof(char*));
        if ( !postTo ) {
            XP_ASSERT(0);
            XP_FREE(filesToPost);
            return;
        }

        // Not NULL-terminated.
        XP_Bool *addCRLF = (XP_Bool *)XP_ALLOC((numFilesToPost) * sizeof(XP_Bool));
        if (!addCRLF) {
            XP_ASSERT(0);
            XP_FREE(filesToPost);
            XP_FREE(postTo);
            return;
        }

        // Copying filenames, reversing order or list and removing blanks at
        // the same time.
        int n,m;        
        for ( n = 0, m = numFilesToPost - 1; 
              n < m_tempFilenames.Size(); n++ ) {
            // Insert into list backwards, because NET_PublishFilesTo publishes in reverse order.
            // So, HTML file will be published first.
            if (m_tempFilenames[n]) {
              filesToPost[m] = XP_STRDUP(m_tempFilenames[n]);
              postTo[m] = XP_STRDUP(m_remoteURLs[n]);

              // Only first file is treated as text, rest are whatever netlib decides.
	      // well actually only if it's not a pre-encrypted file!
              addCRLF[m] = (n == 0) && !IsFirstBinary(); 
              m--;
            }
        }        
        // Otherwise we counted numFilesToPost wrong.
        XP_ASSERT(m == -1);

         // Trailing NULL.
        filesToPost[numFilesToPost] = NULL;
        postTo[numFilesToPost] = NULL;



        // Temporarily remove characters after final slash
        // from m_pRemoteURL, so pRemoteFullURL is a directory.
        char *lastSlash = XP_STRRCHR(m_pRemoteURL,'/');
        char saved;
        if (lastSlash) {
          saved = *(lastSlash + 1);
          *(lastSlash + 1) = '\0';
        }

        char *pRemoteFullURL = NULL;
        if (!NET_MakeUploadURL(&pRemoteFullURL,m_pRemoteURL,m_pUsername,m_pPassword)) {
          pRemoteFullURL = NULL; // Just to be sure.
        }

        // Restore m_pRemoteURL.
        *(lastSlash + 1) = saved;

        if (!pRemoteFullURL) {
          return;
        }

        if (m_bIsHTTP) {
          // m_pRemoteURL may or may not be a directory.
          // username and password are passed in separately, not through the
          // URL, to avoid ever being displayed on screen to the user.
          NET_PublishFilesTo(m_pMWContext, filesToPost, postTo, addCRLF,
                             m_pRemoteURL,m_pUsername,m_pPassword,
                             edt_CTapeFSExit, (void *)this);
        }
        else {  
          // pRemoteFullURL must be a directory for FTP publishing.
          // pRemoteFullURL contains username/password information, maybe we 
          // should modify ns/lib/libnet/mkftp.c to deal with username/password
          // in the URL struct like mkhttp.c does.
          NET_PublishFilesTo(m_pMWContext, filesToPost, postTo, addCRLF,
                            pRemoteFullURL,NULL,NULL,
                            edt_CTapeFSExit, (void *)this);
        }
        // edt_CTapeFSExit will call pfComplete() and delete this.

        XP_FREEIF(pRemoteFullURL);
    }
    else {
      if (m_pfComplete) {
        // Pass in TRUE because CTapeFSComplete() did the right thing, even 
        // though there may have been errors earlier on.
        m_pfComplete(TRUE,m_pArg);
      }
      delete this;
    }
}

char *CTapeFSPublish::makeLocal(char *baseURL,char *srcURL) {
    // Return value.
    char *ret = NULL;  
    
    // Set ret to be directory to place files in.
    int len = XP_STRLEN(baseURL);
    if (len < 1) {
        XP_ASSERT(0);
        return NULL;
    }

    if (baseURL[len-1] == '/') {
        // baseURL is already a directory.
        ret = XP_STRDUP(baseURL);
    }
    else {
        char *trail = XP_STRRCHR(baseURL,'/');
        if (trail == NULL) {
            XP_ASSERT(0);
            return NULL;
        }
        // Temporarily set the char after the last slash to NULL and copy everything before and 
        // including the last slash.  We know the '/' is not the last character, since we checked for that 
        // above.
        char tmp = *(trail + 1);
        *(trail + 1) = '\0';
        ret = XP_STRDUP(baseURL);        
        *(trail + 1) = tmp;
    }    

    char *localName = FE_URLToLocalName(srcURL);
    if (localName == NULL) {
        XP_ASSERT(0);
        return NULL;
    }

    if (!ret) {
        XP_ASSERT(0);
        return NULL;
    }

    ret = XP_AppendStr(ret,localName);
    XP_ASSERT(ret); 
    XP_FREE(localName);

    return ret;
}

#endif // EDITOR
