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


//
//
// Editor save stuff. LWT 06/01/95
// this should be on the branch
//

#ifdef EDITOR

#include "editor.h"
#ifdef DEBUG

const char* kCommandNames[kCommandIDMax+1] = {
    "Null",
    "Typing", 
    "AddText", 
    "DeleteText", 
    "Cut", 
    "PasteText", 
    "PasteHTML", 
    "PasteHREF", 
    "ChangeAttributes", 
    "Indent", 
    "ParagraphAlign", 
    "MorphContainer", 
    "InsertHorizRule", 
    "SetHorizRuleData", 
    "InsertImage", 
    "SetImageData", 
    "InsertBreak", 
    "ChangePageData", 
    "SetMetaData", 
    "DeleteMetaData", 
    "InsertTarget", 
    "SetTargetData", 
    "InsertUnknownTag", 
    "SetUnknownTagData", 
    "GroupOfChanges",                                           
    "SetListData",
    "InsertTable",
    "DeleteTable",
    "SetTableData",
    "InsertTableCaption",
    "SetTableCaptionData",
    "DeleteTableCaption",
    "InsertTableRow",
    "SetTableRowData",
    "DeleteTableRow",
    "InsertTableColumn",
    "SetTableCellData",
    "DeleteTableColumn",
    "InsertTableCell",
    "DeleteTableCell",
    "InsertMultiColumn",
    "DeleteMultiColumn",
    "SetMultiColumnData",
    "SetSelection"
};

#endif

CEditText::CEditText() { m_pChars = NULL; m_iLength = 0; m_iCount = 0; }
CEditText::~CEditText() { Clear(); }

void CEditText::Clear() {
    if ( m_pChars )
        XP_HUGE_FREE(m_pChars);  // Tied to implementation of CStreamOutMemory
    m_pChars = 0;
    m_iCount = 0;
    m_iLength = 0;
}

char* CEditText::GetChars() { return m_pChars; }
int32 CEditText::Length() { return m_iLength; }
char** CEditText::GetPChars() { return &m_pChars; }
int32* CEditText::GetPLength() { return &m_iLength; }

// CEditCommand

CEditCommand::CEditCommand(CEditBuffer* editBuffer, intn id)
{
    m_id = id;
    m_editBuffer = editBuffer;
}

CEditCommand::~CEditCommand()
{
}

void CEditCommand::Do()
{
}

void CEditCommand::Undo()
{
}

void CEditCommand::Redo()
{
    Do();
}

intn CEditCommand::GetID()
{
    return m_id;
}

#ifdef DEBUG
void CEditCommand::Print(IStreamOut& stream) {
    const char* name = "Unknown";
    if ( m_id >= 0 && m_id <= kCommandIDMax ){
        name = kCommandNames[m_id];
    }
    stream.Printf("%s(%ld) ", name, (long)m_id);
}
#endif

// CEditCommandLog

// We were having problems where we were entering a command while processing the undo of another command.
// This would cause the command log to be trimmed, which would delete the undo log, including the
// command that was being undone. This helps detect that situation.

#ifdef DEBUG

class CEditCommandLogRecursionCheckEntry {
private:
    XP_Bool m_bOldBusy;
    CEditCommandLog* m_log;
public:
    CEditCommandLogRecursionCheckEntry(CEditCommandLog* log) {
        m_log = log;
        m_bOldBusy = m_log->m_bBusy;
        XP_ASSERT(m_log->m_bBusy == FALSE);
        m_log->m_bBusy = TRUE;
    }
    ~CEditCommandLogRecursionCheckEntry() {
        m_log->m_bBusy = m_bOldBusy;
    }
};

#define DEBUG_RECURSION_CHECK CEditCommandLogRecursionCheckEntry debugRecursionCheckEntry(this);
#else
#define DEBUG_RECURSION_CHECK
#endif

CGlobalHistoryGroup* CGlobalHistoryGroup::g_pGlobalHistoryGroup;

CGlobalHistoryGroup* CGlobalHistoryGroup::GetGlobalHistoryGroup(){
    if ( g_pGlobalHistoryGroup == NULL ) {
        g_pGlobalHistoryGroup = new CGlobalHistoryGroup();
    }
    return g_pGlobalHistoryGroup;
}

CGlobalHistoryGroup::CGlobalHistoryGroup(){
    m_pHead = NULL;
}

CGlobalHistoryGroup::~CGlobalHistoryGroup(){
    CEditCommandLog* pLog = m_pHead;
    while(pLog){
        CEditCommandLog* pCurrent = pLog;
        pLog = pLog->m_pNext;
        delete pCurrent;
    }
    m_pHead = NULL;
}

XP_Bool CGlobalHistoryGroup::IsReload(CEditBuffer* pEditBuffer){
    CEditCommandLog* pLog = m_pHead;
    while(pLog){
        if ( pEditBuffer->m_pContext == pLog->m_pContext ) {
            return TRUE;
        }
        pLog = pLog->m_pNext;
    }
    return FALSE;
}

CEditCommandLog* CGlobalHistoryGroup::CreateLog(CEditBuffer* pEditBuffer){
    CEditCommandLog* pLog = m_pHead;
    while(pLog){
        if ( pEditBuffer->m_pContext == pLog->m_pContext ) {
            pLog->m_bIgnoreDelete = FALSE;
            pLog->m_pBuffer = pEditBuffer;
            return pLog;
        }
        pLog = pLog->m_pNext;
    }
    pLog = new CEditCommandLog();
    pLog->m_pContext = pEditBuffer->m_pContext;
    pLog->m_pNext = m_pHead;
    pLog->m_pBuffer = pEditBuffer;
    m_pHead = pLog;
    return pLog;
}

CEditCommandLog* CGlobalHistoryGroup::GetLog(CEditBuffer* pEditBuffer){
    CEditCommandLog* pLog = m_pHead;
    while(pLog){
        if ( pEditBuffer->m_pContext == pLog->m_pContext ) {
            return pLog;
        }
        pLog = pLog->m_pNext;
    }
    XP_ASSERT(FALSE);
    return NULL;
}

void CGlobalHistoryGroup::DeleteLog(CEditBuffer* pEditBuffer){
    CEditCommandLog* pLog = m_pHead;
    CEditCommandLog* pPrev = NULL;
    while(pLog){
        if ( pEditBuffer->m_pContext == pLog->m_pContext ) {
            pLog->m_pBuffer = NULL;
            if ( pLog->m_bIgnoreDelete ) {
                pLog->m_bIgnoreDelete = FALSE;
            }
            else {
                CEditCommandLog* pNext = pLog->m_pNext;
                delete pLog;
                if ( pPrev ) {
                    pPrev->m_pNext = pNext;
                }
                else {
                    m_pHead = pNext;
                    if ( pNext == NULL ) {
                        // Why yes, that is our this pointer....
                        delete g_pGlobalHistoryGroup;
                        g_pGlobalHistoryGroup = NULL;
                    }
                }
            }
            return;
        }
        pPrev = pLog;
        pLog = pLog->m_pNext;
    }
    XP_ASSERT(FALSE);
}

void CGlobalHistoryGroup::IgnoreNextDeleteOf(CEditBuffer* pEditBuffer){
    CEditCommandLog* pLog = GetLog(pEditBuffer);
    if ( pLog ) {
        XP_ASSERT(pLog->m_bIgnoreDelete == FALSE);
        pLog->m_bIgnoreDelete = TRUE;
    }
}

CCommandState::CCommandState(){
    m_commandID = kNullCommandID;
    m_pState = NULL;
}

CCommandState::~CCommandState(){
    Flush();
}

void CCommandState::SetID(intn commandID){
    m_commandID = commandID;
}

intn CCommandState::GetID(){
    return m_commandID;
}

void CCommandState::Record(CEditBuffer* pBufferToRecord){
    Flush();
    m_pState = pBufferToRecord->RecordState();
}

void CCommandState::Restore(CEditBuffer* pBufferToRestore){
  if (m_pState) {
    pBufferToRestore->RestoreState(m_pState);
  }
}

void CCommandState::Flush(){
  delete m_pState;
  m_pState = NULL;
}

#ifdef DEBUG
void CCommandState::Print(IStreamOut& stream) {
  if (m_pState) {
    stream.Printf("id: %d, ", m_commandID);
    m_pState->Print(stream);
  }
}
#endif


CEditCommandLog::CEditCommandLog()
{
    m_pBuffer = NULL;
    m_pUndo = NULL;
    m_pRedo = NULL;
    m_iBatchLevel = 0;
    m_pNext = NULL;
    m_pContext = NULL;
    m_bIgnoreDelete = FALSE;
#ifdef DEBUG
    m_bBusy = FALSE;
#endif
    m_state = 0;
    m_version = 0;
    m_storedVersion = 0;
    m_highestVersion = 0;
#ifdef EDITOR_JAVA
    m_pPlugins = 0;
#endif
    m_pDocTempDir = NULL;
    m_iDocTempFilenameNonce = 1;


// If this is the first CEditBuffer created, delete any 
// temporary document directories from the last time 
// Communicator 4.0 was run.
#if defined(XP_WIN) || defined(XP_MAC) || defined(XP_OS2)
    // Don't do this for UNIX since multiple instances of Communicator
    // may be running.

    // First instance of CEditCommandLog.
    if (m_iDocTempDirNonce == 0) {
      m_iDocTempDirNonce++; // First temp dir will start with "1".
      char *pxpURL = GetAppTempDir();
      if (pxpURL) {
        // recursive delete.
        edt_RemoveDirectoryR(pxpURL);
        XP_FREE(pxpURL);
      }
      else {
        XP_ASSERT(0);
      }
    }
#endif
}

CEditCommandLog::~CEditCommandLog()
{
    Trim();
#ifdef EDITOR_JAVA
    EditorPluginManager_delete(m_pPlugins);
    m_pPlugins = 0;
#endif

    // Clean up the temp files associated with this doc.
    if (m_pDocTempDir) {
      edt_RemoveDirectoryR(m_pDocTempDir);
      XP_FREE(m_pDocTempDir);
    }

}

void CEditCommandLog::StartTyping(intn typing){
    InternalDo(typing);
}

void CEditCommandLog::EndTyping(){
}

void CEditCommandLog::AdoptAndDo(CEditCommand* pCommand)
{
    DEBUG_RECURSION_CHECK

    if ( m_iBatchLevel == 0 ){
        InternalAdoptAndDo(pCommand);
    }
    else
    {
        pCommand->Do();
        delete pCommand;
    }
}

void CEditCommandLog::InternalAdoptAndDo(CEditCommand* command)
{
    InternalDo(command->GetID());
    command->Do();
    delete command;
}

void CEditCommandLog::InternalDo(intn id)
{
    if ( m_state == 1 ) {
        // recovering from an undo/redo
        return;
    }
    delete m_pUndo;
    m_pUndo = new CCommandState();
    m_pUndo->SetID(id);
    m_pUndo->Record(m_pBuffer);

    if ( m_pRedo ) {
        delete m_pRedo;
        m_pRedo = NULL;
    }
    m_state = 0;

    // Set m_version to a version that has never been seen before.
    m_version = ++m_highestVersion;
}

void CEditCommandLog::Undo()
{
    DEBUG_RECURSION_CHECK
    if ( !m_pUndo ) {
        XP_TRACE(("Nothing to undo."));
        return;
    }
    if ( m_pUndo )
    {
        FinishBatchCommands();
        if ( ! m_pRedo ) {
            m_pRedo = new CCommandState();
            m_pRedo->SetID(m_pUndo->GetID());
            m_pRedo->Record(m_pBuffer);
        }
        m_pUndo->Restore(m_pBuffer);
        delete m_pUndo;
        m_pUndo = NULL;
    }
    m_state = 1;
    // CCommandState::Restore() will set m_version.
}

void CEditCommandLog::Redo()
{
    DEBUG_RECURSION_CHECK
    if ( !m_pRedo ) {
        XP_TRACE(("Nothing to redo."));
        return;
    }
    if ( m_pRedo )
    {
        FinishBatchCommands();
        if ( ! m_pUndo ) {
            m_pUndo = new CCommandState();
            m_pUndo->SetID(m_pRedo->GetID());
            m_pUndo->Record(m_pBuffer);
        }
        m_pRedo->Restore(m_pBuffer);
        delete m_pRedo;
        m_pRedo = NULL;
    }
    m_state = 1;
    // CCommandState::Restore() will set m_version.
}

void CEditCommandLog::FinishBatchCommands()
{
    if ( m_iBatchLevel > 0 ){
        XP_ASSERT(FALSE);
        m_iBatchLevel = 0;
    }
}

void CEditCommandLog::Trim()
{
    delete m_pRedo;
    m_pRedo = NULL;
    delete m_pUndo;
    m_pUndo = NULL;
}

XP_Bool CEditCommandLog::InReload(){
    return m_state != 0;
}
void CEditCommandLog::SetInReload(XP_Bool bInReload){
    m_state = bInReload;
}

XP_Bool CEditCommandLog::IsDirty(){
    return m_version != m_storedVersion;
}

DocumentVersion CEditCommandLog::GetVersion(){
    return m_version;
}

void CEditCommandLog::SetVersion(DocumentVersion version) {
  m_version = version;
}

void CEditCommandLog::DocumentStored(){
    m_storedVersion = m_version;
}

DocumentVersion CEditCommandLog::GetStoredVersion(){
    return m_storedVersion;
}

intn CEditCommandLog::GetCommandHistoryLimit()
{
    return 1;
}

void CEditCommandLog::SetCommandHistoryLimit(intn newLimit) {
    if ( newLimit >= 0 ) {
        Trim();
    }
}

intn CEditCommandLog::GetNumberOfCommandsToUndo()
{
    return m_pUndo ? 1 : 0;
}

intn CEditCommandLog::GetNumberOfCommandsToRedo()
{
    return m_pRedo ? 1 : 0;
}

// Returns NULL if out of range
intn CEditCommandLog::GetUndoCommand(intn index)
{
    if ( m_pUndo == NULL || index < 0 || index >= GetNumberOfCommandsToUndo())
        return kNullCommandID;
    return m_pUndo->GetID();
}

intn CEditCommandLog::GetRedoCommand(intn index)
{
    if ( m_pRedo == NULL || index < 0 || index >= GetNumberOfCommandsToRedo() )
        return kNullCommandID;
    return m_pRedo->GetID();
}

#ifdef DEBUG
void CEditCommandLog::Print(IStreamOut& stream) {
    stream.Printf("state: %d\n", m_state);
    stream.Printf("Undo list: %d commands\n", GetNumberOfCommandsToUndo());
     if ( m_pUndo ) {
        m_pUndo->Print(stream);
     }
   stream.Printf("Redo list: %d commands\n", GetNumberOfCommandsToRedo());
     if ( m_pRedo ) {
        m_pRedo->Print(stream);
     }
}
#endif

void CEditCommandLog::BeginBatchChanges(intn id) {
    if ( m_iBatchLevel < 0 ) {
        XP_ASSERT(FALSE);
        m_iBatchLevel = 0;
    }
    if ( m_iBatchLevel++ == 0 ) {
        InternalDo(id);
    }
}

void CEditCommandLog::EndBatchChanges() {
    if(m_iBatchLevel <= 0) {
        XP_ASSERT(FALSE);
        m_iBatchLevel = 0;
    }
    else {
        m_iBatchLevel--;
    }
}

#ifdef EDITOR_JAVA
EditorPluginManager CEditCommandLog::GetPlugins(){
    if ( m_pPlugins == NULL ) {
        m_pPlugins = EditorPluginManager_new(m_pContext);
    }
    return m_pPlugins;
}
#endif

// Returns xpURL, ends in slash.
char *CEditCommandLog::GetAppTempDir() {
  char *pTempRoot = XP_TempDirName();
  char* pTempURL = XP_PlatformFileToURL(pTempRoot);
  XP_FREEIF(pTempRoot);
  if (!(pTempURL && *pTempURL)) {
    XP_ASSERT(0);
    return NULL;
  }
  // Make sure ends in slash.
  if (pTempURL[XP_STRLEN(pTempURL)-1] != '/') {
    StrAllocCat(pTempURL,"/");
  }

  // Will still end in a slash.
#ifdef XP_UNIX
  // NOTE:  on UNIX we need to provide support for multiple users...
  //
  char *pUserName = getenv("USER");

  if (pUserName != NULL) {
	  StrAllocCat(pTempURL,"nscomm40-");
	  StrAllocCat(pTempURL,pUserName);
	  StrAllocCat(pTempURL,"/");
  }
  else {
	  StrAllocCat(pTempURL,"nscomm40/");
  }
#else
  StrAllocCat(pTempURL,"nscomm40/");
#endif
  // pTempURL is now the application temp dir root.

    // EXTREME DANGER! CANNOT USE THIS IF "#" IS IN THE TEMP DIRECTORY NAME!
    // Bug 83166  -- entire C drive deleted if TEMP=C:\#TEMP

    //char *pTemp_xpURL = NET_ParseURL(pTempURL,GET_PATH_PART);

    // This is (mostly) lifted from NET_ParseURL - just skip over host part
    char *pTemp_xpURL = NULL;
	char * colon = XP_STRCHR(pTempURL, ':'); /* returns a const char */
    if(colon)
    {
        char * slash;
        if(*(colon+1) == '/' && *(colon+2) == '/')
        {
            /* skip host part */
            slash = XP_STRCHR(colon+3, '/');
        }
        else 
        {
            /* path is right after the colon
             */
            slash = colon+1;
        }
        // Copy everything starting with "/" to result string
        pTemp_xpURL = XP_STRDUP(slash);
        XP_FREEIF(pTempURL);
        
    } else {
        // Should never get here, but if no colon,
        //  just return what we started with
        pTemp_xpURL = pTempURL;
    }

    return pTemp_xpURL;
}

char *CEditCommandLog::GetDocTempDir() {
  // Already created.
  if (m_pDocTempDir) {
    return XP_STRDUP(m_pDocTempDir);
  }

  char *pTemp_xpURL = GetAppTempDir();
  if (!pTemp_xpURL) {
    XP_ASSERT(0);
    return NULL;
  }

  // Create application temporary directory if necessary
  XP_Dir pDir = edt_OpenDir(pTemp_xpURL,xpURL);
  if (pDir) {
    // Already exists, so ok.
    XP_CloseDir(pDir);
  }
  else {
    edt_MakeDirectory(pTemp_xpURL,xpURL);
    pDir = edt_OpenDir(pTemp_xpURL,xpURL);
    if (pDir) {
      // made successfully, ok
      XP_CloseDir(pDir);
    }
    else {
      // can't make it, so fail.
      XP_FREEIF(pTemp_xpURL);
      return NULL;
    }
  }

  // Under application temporary directory, create process-specific
  // temporary directory.  This is only really process-specific for 
  // UNIX.  WIN and MAC has dummy directory name.
  char *pProcessDir = NULL;
  #ifdef XP_UNIX
    pProcessDir = PR_smprintf("%u/",(unsigned)getpid());
  #else
    pProcessDir = XP_STRDUP("tmp/");
  #endif
  StrAllocCat(pTemp_xpURL,pProcessDir);
  if (!pTemp_xpURL || !pProcessDir) {
    XP_ASSERT(0);
    XP_FREEIF(pProcessDir);
    XP_FREEIF(pTemp_xpURL);
    return NULL;
  }


  // pTemp_xpURL now has process-specific name appended.
  pDir = edt_OpenDir(pTemp_xpURL,xpURL);
  if (pDir) {
    // Already exists, so ok.
    XP_CloseDir(pDir);
  }
  else {
    edt_MakeDirectory(pTemp_xpURL,xpURL);
    pDir = edt_OpenDir(pTemp_xpURL,xpURL);
    if (pDir) {
      // made successfully, ok
      XP_CloseDir(pDir);
    }
    else {
      // can't make it, so fail.
      XP_FREEIF(pTemp_xpURL);
      return NULL;
    }
  }


  // Create document-specific temp dir.
  char *pFilename = PR_smprintf("tmp%ld",m_iDocTempDirNonce);
  if (!pFilename) {
    XP_ASSERT(0);
    XP_FREE(pTemp_xpURL);
    return NULL;
  }
  if (XP_STRLEN(pFilename) > 8) {
    // truncate at 8 chars.
    pFilename[8] = '\0';
  }
  StrAllocCat(pTemp_xpURL,pFilename);
  XP_FREE(pFilename);    

  // end in slash.
  StrAllocCat(pTemp_xpURL,"/");

  pDir = edt_OpenDir(pTemp_xpURL,xpURL);
  if (pDir) {
    // Already exists.  Strange, but not really an error.
    XP_CloseDir(pDir);
  }
  else {
    edt_MakeDirectory(pTemp_xpURL,xpURL);
    pDir = edt_OpenDir(pTemp_xpURL,xpURL);
    if (pDir) {
      // made successfully, ok
      XP_CloseDir(pDir);
    }
    else {
      // failure, clear pTemp_xpURL
      XP_FREEIF(pTemp_xpURL);
    }
  }

  if (pTemp_xpURL) {
    // success.
    m_iDocTempDirNonce++;
    m_pDocTempDir = pTemp_xpURL;
    return XP_STRDUP(m_pDocTempDir);  
  }
  else {
    return NULL;
  }
}

int32 CEditCommandLog::m_iDocTempDirNonce = 0;


// Always return a new filename in the temporary directory.
char *CEditCommandLog::CreateDocTempFilename(char *pPrefix,char *pExtension) {
  // Add directory.
  char *pTempF = GetDocTempDir();
  if (!pTempF) {
    return NULL;
  }

  // Add file prefix.
  if (pPrefix) {
    char *pPreCopy = XP_STRDUP(pPrefix);
    if (!pPreCopy) {
      XP_ASSERT(0);
      return NULL; 
    }

    // truncate to 3 chars.
    if (XP_STRLEN(pPreCopy) > 3) {
      pPreCopy[3] = '\0';
    }

    StrAllocCat(pTempF,pPreCopy);
    XP_FREE(pPreCopy);
  }
  else {
    // default to "edt" for prefix.
    StrAllocCat(pTempF,"edt");
  }

  if (!pTempF) {
    return NULL;
  }
  
  char *pTempFilename = PR_smprintf("%s%d.%s",
                                    pTempF,
                                    (int)m_iDocTempFilenameNonce,
                                    (pExtension ? pExtension : "tmp"));
  XP_FREE(pTempF);
  m_iDocTempFilenameNonce++;
  return pTempFilename;
}


// CEditDataSaver

CEditDataSaver::CEditDataSaver(CEditBuffer* pBuffer){
    m_pEditBuffer = pBuffer;
    m_bModifiedTextHasBeenSaved = FALSE;
#ifdef DEBUG
    m_bDoState = 0;
#endif
}

CEditDataSaver::~CEditDataSaver(){
}

void CEditDataSaver::DoBegin(CPersistentEditSelection& original){
#ifdef DEBUG
    XP_ASSERT(m_bDoState == 0);
    m_bDoState++;
#endif
    m_original = original;
    m_pEditBuffer->GetSelection(m_originalDocument);
    CEditSelection selection =
        m_pEditBuffer->PersistentToEphemeral(m_original);
    selection.ExpandToNotCrossStructures();
    m_expandedOriginal = m_pEditBuffer->EphemeralToPersistent(selection);
    m_pEditBuffer->CopyEditText(m_expandedOriginal, m_originalText);
}

void CEditDataSaver::DoEnd(CPersistentEditSelection& modified){
#ifdef DEBUG
    XP_ASSERT(m_bDoState == 1);
    m_bDoState++;
#endif
    m_pEditBuffer->GetSelection(m_modifiedDocument);
    m_expandedModified = m_expandedOriginal;
    m_expandedModified.Map(m_original, modified);
}

void CEditDataSaver::Undo(){
#ifdef DEBUG
    XP_ASSERT(m_bDoState == 2);
    m_bDoState++;
#endif
    m_pEditBuffer->SetSelection(m_expandedModified);
    if ( ! m_bModifiedTextHasBeenSaved ) {
        m_pEditBuffer->CutEditText(m_modifiedText);
        m_bModifiedTextHasBeenSaved = TRUE;
    }
    else {
        m_pEditBuffer->DeleteSelection();
    }
    m_pEditBuffer->PasteEditText(m_originalText);
    m_pEditBuffer->SetSelection(m_originalDocument);
}

void CEditDataSaver::Redo(){
#ifdef DEBUG
    XP_ASSERT(m_bDoState == 3);
    m_bDoState--;
#endif
    m_pEditBuffer->SetSelection(m_expandedOriginal);
    m_pEditBuffer->DeleteSelection();
    m_pEditBuffer->PasteEditText(m_modifiedText);
    m_pEditBuffer->SetSelection(m_modifiedDocument);
}


// CDeleteTableCommand
CDeleteTableCommand::CDeleteTableCommand(CEditBuffer* buffer, intn id)
    : CEditCommand(buffer, id)
{
	m_pTable = NULL;
    GetEditBuffer()->GetSelection(m_originalSelection);
    CEditInsertPoint ip;
    GetEditBuffer()->GetTableInsertPoint(ip);
    GetEditBuffer()->ClearTableAndCellSelection();
    m_pTable = ip.m_pElement->GetTableIgnoreSubdoc();
	if ( m_pTable ) {
		CEditElement* pRefreshStart = m_pTable->GetFirstMostChild()->PreviousLeaf();
		CEditInsertPoint replacePoint(m_pTable->GetLastMostChild()->NextLeaf(), 0);
		GetEditBuffer()->SetInsertPoint(replacePoint);
		m_pTable->Unlink();
		m_replacePoint = GetEditBuffer()->EphemeralToPersistent(replacePoint);
		GetEditBuffer()->Relayout(pRefreshStart, 0, replacePoint.m_pElement);
	}
}

CDeleteTableCommand::~CDeleteTableCommand()
{
    delete m_pTable;
}

void CDeleteTableCommand::Do() {
}

// CInsertTableCaptionCommand
CInsertTableCaptionCommand::CInsertTableCaptionCommand(CEditBuffer* buffer,
  EDT_TableCaptionData* pData, intn id)
    : CEditCommand(buffer, id)
{
    m_pOldCaption = NULL;
    GetEditBuffer()->GetSelection(m_originalSelection);
    CEditInsertPoint ip;
    GetEditBuffer()->GetTableInsertPoint(ip);
    CEditTableElement* pTable = ip.m_pElement->GetTableIgnoreSubdoc();
    if ( pTable )
    {
        CEditCaptionElement* pCaption = new CEditCaptionElement();
        pCaption->SetData(pData);
        pTable->SetCaption(pCaption);
        pTable->FinishedLoad(GetEditBuffer());
        // CLM: Don't move insert point if we have a selected cell
        //      (We use selection to indicate current cell in property dialogs)
        if( !GetEditBuffer()->IsSelected() &&
            ip.m_pElement->GetTableCellIgnoreSubdoc() != NULL )
        {
            // Put cursor at end of caption
            ip.m_pElement = pTable->GetCaption()->GetLastMostChild()->Leaf();
            ip.m_iPos = ip.m_pElement->GetLen();
            GetEditBuffer()->SetInsertPoint(ip);
        }
        GetEditBuffer()->Relayout(pTable, 0);
    }
}

CInsertTableCaptionCommand::~CInsertTableCaptionCommand()
{
    delete m_pOldCaption;
}

void CInsertTableCaptionCommand::Do() {
    // All done in constructor
}

// CDeleteTableCaptionCommand
CDeleteTableCaptionCommand::CDeleteTableCaptionCommand(CEditBuffer* buffer, intn id)
    : CEditCommand(buffer, id),
    m_pOldCaption(NULL)
{
    GetEditBuffer()->GetSelection(m_originalSelection);
    CEditInsertPoint ip;
    GetEditBuffer()->GetTableInsertPoint(ip);
    CEditTableElement* pTable = ip.m_pElement->GetTableIgnoreSubdoc();
    if ( pTable )
    {
        m_pOldCaption = pTable->GetCaption();
        if ( m_pOldCaption )
        {
            CEditTableCellElement* pTableCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
            //cmanske - Set cursor only if we are not in the caption being deleted
            CEditCaptionElement *pCaption = ip.m_pElement->GetCaption();

            m_pOldCaption->Unlink();
            pTable->FinishedLoad(GetEditBuffer());
            if( pCaption /*!pTableCell*/ )
            {
                // Set cursor to someplace that still exists
                // TODO: NEED TO FIX THIS TO PLACE CURSOR CLOSER TO WHERE CAPTION WAS
                CEditTableCellElement * pCell = pTable->GetFirstCell();
                int32 X = 0;
                int32 Y = 0;
                if( pCell )
                {
                    X = pCell->GetX();
                    Y = pCell->GetY();
                }
                GetEditBuffer()->StartSelection(X,Y);
            }
            GetEditBuffer()->Relayout(pTable, 0);
        }
    }
}

CDeleteTableCaptionCommand::~CDeleteTableCaptionCommand()
{
    delete m_pOldCaption;
}

void CDeleteTableCaptionCommand::Do()
{
}

// CInsertTableRowCommand
CInsertTableRowCommand::CInsertTableRowCommand(CEditBuffer* buffer,
  EDT_TableRowData* /* pData */, XP_Bool bAfterCurrentRow, intn number, intn id)
    : CEditCommand(buffer, id)
{
    m_number = number;
    GetEditBuffer()->GetSelection(m_originalSelection);
    if( GetEditBuffer()->IsSelected() )
    {
        GetEditBuffer()->ClearSelection();
    }
    CEditInsertPoint ip;
    GetEditBuffer()->GetTableInsertPoint(ip);
    CEditTableCellElement* pTableCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    if ( pTableCell)
    {
        CEditTableElement* pTable = pTableCell->GetTable();
        if ( pTable )
        {
            int32 X = pTableCell->GetX();
            int32 Y = pTableCell->GetY();
            int32 iNewY = Y + (bAfterCurrentRow ? pTableCell->GetHeight() : 0);
            // Try to locate cursor in new cell
            int32 iCaretY = iNewY + (bAfterCurrentRow ? pTable->GetInterCellSpace() : 0);
            pTable->InsertRows(Y, iNewY, number);
            pTable->FinishedLoad(GetEditBuffer());
            GetEditBuffer()->Relayout(pTable, 0);
            GetEditBuffer()->MoveToExistingCell(pTable, X+2, iCaretY+2);
        }
    }
}

CInsertTableRowCommand::~CInsertTableRowCommand()
{
}

void CInsertTableRowCommand::Do() {
    // All done in constructor
}


// CDeleteTableRowCommand
CDeleteTableRowCommand::CDeleteTableRowCommand(CEditBuffer* buffer, intn rows, intn id)
    : CEditCommand(buffer, id),
    m_table(0,0)
{
    GetEditBuffer()->GetSelection(m_originalSelection);
    //The code from Redo moved here:
    CEditInsertPoint ip;
    GetEditBuffer()->GetTableInsertPoint(ip);
    CEditTableCellElement* pTableCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    if ( pTableCell )
    {
        CEditTableElement* pTable = pTableCell->GetTable();
        if ( pTable )
        {
            int32 X = pTableCell->GetX();
            int32 Y = pTableCell->GetY();
            //TODO: FIGURE THIS OUT 
            m_bDeletedWholeTable = FALSE; //m_row == 0 && m_rows >= pTable->GetRows();

            pTable->DeleteRows(Y, rows);
            pTable->FinishedLoad(GetEditBuffer());
            // Move to a safe location so Relayout() doesn't assert
            GetEditBuffer()->Relayout(pTable, 0, NULL, RELAYOUT_NOCARET);
            // Try to move to whereever we deleted
            GetEditBuffer()->MoveToExistingCell(pTable, X, Y);
        }
    }
}

CDeleteTableRowCommand::~CDeleteTableRowCommand()
{
}

void CDeleteTableRowCommand::Do()
{
    // All done in constructor
}


// CInsertTableColumnCommand
CInsertTableColumnCommand::CInsertTableColumnCommand(CEditBuffer* buffer,
  EDT_TableCellData* /* pData */, XP_Bool bAfterCurrentCell, intn number, intn id)
    : CEditCommand(buffer, id)
{
//    m_number = number;
    GetEditBuffer()->GetSelection(m_originalSelection);
    if( GetEditBuffer()->IsSelected() ){
        GetEditBuffer()->ClearSelection();
    }
    CEditInsertPoint ip;
    GetEditBuffer()->GetTableInsertPoint(ip);
    CEditTableCellElement* pTableCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    if ( pTableCell)
    {
        CEditTableElement* pTable = pTableCell->GetTable();
        if ( pTable )
        {
            int32 X = pTableCell->GetX();
            int32 Y = pTableCell->GetY();
            int32 iNewX = X + (bAfterCurrentCell ? pTableCell->GetWidth() : 0);
            // Try to place cursor in inserted cell
            int32 iCaretX = iNewX + (bAfterCurrentCell ? pTable->GetInterCellSpace() : 0);
            pTable->InsertColumns(X, iNewX, number);
            pTable->FinishedLoad(GetEditBuffer());
            GetEditBuffer()->Relayout(pTable, 0);
            GetEditBuffer()->MoveToExistingCell(pTable, iCaretX, Y);
        }
    }
}

CInsertTableColumnCommand::~CInsertTableColumnCommand()
{
}

void CInsertTableColumnCommand::Do() {
    // All done in constructor
}

// CDeleteTableColumnCommand
CDeleteTableColumnCommand::CDeleteTableColumnCommand(CEditBuffer* buffer, intn columns, intn id)
    : CEditCommand(buffer, id),
    m_table(0,0)
{
//	m_columns = columns;
    GetEditBuffer()->GetSelection(m_originalSelection);
    CEditInsertPoint ip;
    GetEditBuffer()->GetTableInsertPoint(ip);
    CEditTableCellElement* pTableCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    if ( pTableCell )
    {
        CEditTableElement* pTable = pTableCell->GetTable();
        if ( pTable )
        {
            //TODO: FIGURE THIS OUT 
            m_bDeletedWholeTable = FALSE; //m_column == 0 && m_columns >= pTable->GetColumns();
            int32 X = pTableCell->GetX();
            int32 Y = pTableCell->GetY();
            pTable->DeleteColumns(X, columns, &m_table);
            pTable->FinishedLoad(GetEditBuffer());
            // Move to a safe location so Relayout() doesn't assert
            GetEditBuffer()->Relayout(pTable, 0, NULL, RELAYOUT_NOCARET);
            // Try to move to whereever we deleted
            GetEditBuffer()->MoveToExistingCell(pTable, X, Y);
        }
    }
}

CDeleteTableColumnCommand::~CDeleteTableColumnCommand()
{
}

void CDeleteTableColumnCommand::Do()
{
//#endif
}


// CInsertTableCellCommand
CInsertTableCellCommand::CInsertTableCellCommand(CEditBuffer* buffer,
  XP_Bool bAfterCurrentCell, intn number, intn id)
    : CEditCommand(buffer, id)
{
    m_number = number;
    GetEditBuffer()->GetSelection(m_originalSelection);
    if( GetEditBuffer()->IsSelected() ){
        GetEditBuffer()->ClearSelection();
    }
    CEditInsertPoint ip;
    GetEditBuffer()->GetTableInsertPoint(ip);
    CEditTableCellElement* pTableCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    if ( pTableCell)
    {
        CEditTableRowElement* pTableRow = pTableCell->GetTableRow();
        CEditTableElement* pTable = pTableCell->GetTable();
        if ( pTable && pTableRow )
        {
            int32 X = pTableCell->GetX();
            int32 Y = pTableCell->GetY();
            int32 iNewX = X + (bAfterCurrentCell ? pTableCell->GetWidth() : 0);
            // Try to move cursor to new column
            int32 iCaretX = bAfterCurrentCell ? (iNewX+pTable->GetInterCellSpace()) : X;
            pTableRow->InsertCells(X, iNewX, number);
            pTable->FinishedLoad(GetEditBuffer());
            GetEditBuffer()->Relayout(pTable, 0);
            GetEditBuffer()->MoveToExistingCell(pTable, iCaretX, Y);
        }
    }
}

CInsertTableCellCommand::~CInsertTableCellCommand()
{
}

void CInsertTableCellCommand::Do() {
    // All done in constructor
}


// CDeleteTableCellCommand
CDeleteTableCellCommand::CDeleteTableCellCommand(CEditBuffer* buffer, intn columns, intn id)
    : CEditCommand(buffer, id)
{
//	m_columns = columns;
    GetEditBuffer()->GetSelection(m_originalSelection);
    CEditInsertPoint ip;
    GetEditBuffer()->GetTableInsertPoint(ip);
    CEditTableCellElement* pTableCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
    if ( pTableCell )
    {
        CEditTableRowElement* pTableRow = pTableCell->GetTableRowIgnoreSubdoc();
        CEditTableElement* pTable = pTableCell->GetTableIgnoreSubdoc();
        if ( pTable && pTableRow )
        {
            // TODO: FIGURE THIS OUT
            m_bDeletedWholeTable = FALSE; // m_column == 0 && m_columns >= pTableRow->GetCells();

            int32 X = pTableCell->GetX();
            int32 Y = pTableCell->GetY();
            pTableRow->DeleteCells(X, columns /*, &m_tableRow*/);
            pTable->FinishedLoad(GetEditBuffer());
            // Move to a safe location so Relayout() doesn't assert
            GetEditBuffer()->Relayout(pTable, 0, NULL, RELAYOUT_NOCARET);
            // Try to move to whereever we deleted
            GetEditBuffer()->MoveToExistingCell(pTable, X, Y);
        }
    }
}

CDeleteTableCellCommand::~CDeleteTableCellCommand()
{
}

void CDeleteTableCellCommand::Do()
{
}


// CEditCommandGroup

CEditCommandGroup::CEditCommandGroup(CEditBuffer* pEditBuffer, int id)
    : CEditCommand( pEditBuffer, id){
}

CEditCommandGroup::~CEditCommandGroup(){
    for ( int i = 0; i < m_commands.Size(); i++ ) {
        CEditCommand* item = m_commands[i];
        delete item;
    }
}

void CEditCommandGroup::AdoptAndDo(CEditCommand* pCommand){
    pCommand->Do();
    //TODO: IS THIS EVER FREED???
    m_commands.Add(pCommand);
}

void CEditCommandGroup::Undo(){
    for ( int i = m_commands.Size() - 1; i >= 0 ; i-- ) {
        CEditCommand* item = m_commands[i];
        item->Undo();
    }
}

void CEditCommandGroup::Redo(){
    for ( int i = 0; i < m_commands.Size(); i++ ) {
        CEditCommand* item = m_commands[i];
        item->Redo();
    }
}

intn CEditCommandGroup::GetNumberOfCommands(){
    return m_commands.Size();
}

#ifdef DEBUG
void CEditCommandGroup::Print(IStreamOut& stream){
    CEditCommand::Print(stream);
    stream.Printf(" %d commands\n", m_commands.Size());
    for ( int i = 0; i < m_commands.Size(); i++ ) {
        CEditCommand* item = m_commands[i];
        stream.Printf("    [%d] 0x%08x ", i, item);
        item->Print(stream);
        stream.Printf("\n");
    }
}
#endif

// CSetListDataCommand
CSetListDataCommand::CSetListDataCommand(CEditBuffer* pBuffer, EDT_ListData& listData, intn id)
    : CEditCommand(pBuffer, id)
{
    m_newData = listData;
    m_pOldData = NULL;
}

CSetListDataCommand::~CSetListDataCommand(){
    if ( m_pOldData ){
        CEditListElement::FreeData( m_pOldData );
    }
}

void CSetListDataCommand::Do(){
    m_pOldData = GetEditBuffer()->GetListData();
    XP_ASSERT(m_pOldData);
    GetEditBuffer()->SetListData(&m_newData);
}

void CSetListDataCommand::Undo(){
    if ( m_pOldData ){
        GetEditBuffer()->SetListData(m_pOldData);
    }
}

void CSetListDataCommand::Redo(){
    GetEditBuffer()->SetListData(&m_newData);
}

// CChangePageDataCommand

CChangePageDataCommand::CChangePageDataCommand(CEditBuffer* buffer, intn id)
    : CEditCommand(buffer, id)
{
    m_oldData = GetEditBuffer()->GetPageData();
    m_newData = NULL;
}

CChangePageDataCommand::~CChangePageDataCommand(){
    if ( m_oldData ) {
        GetEditBuffer()->FreePageData(m_oldData);
    }
    if ( m_newData ) {
        GetEditBuffer()->FreePageData(m_newData);
    }
}

void CChangePageDataCommand::Undo(){
    if ( ! m_newData ) {
        m_newData = GetEditBuffer()->GetPageData();
    }
    GetEditBuffer()->SetPageData(m_oldData);
}

void CChangePageDataCommand::Redo(){
    GetEditBuffer()->SetPageData(m_newData);
}

// CSetMetaDataCommand

CSetMetaDataCommand::CSetMetaDataCommand(CEditBuffer* buffer, EDT_MetaData *pMetaData, XP_Bool bDelete, intn id)
    : CEditCommand(buffer, id){
    m_bDelete = bDelete;
    int existingIndex = GetEditBuffer()->FindMetaData( pMetaData);
    m_bNewItem = existingIndex < 0;
    if ( m_bNewItem ) {
        m_pOldData = 0;
    }
    else {
        m_pOldData = GetEditBuffer()->GetMetaData(existingIndex);
    }
    if ( m_bDelete ) {
        GetEditBuffer()->DeleteMetaData(pMetaData);
        m_pNewData = 0;
    }
    else {
        GetEditBuffer()->SetMetaData(pMetaData);
        m_pNewData = GetEditBuffer()->GetMetaData(GetEditBuffer()->FindMetaData(pMetaData));
    }
}

CSetMetaDataCommand::~CSetMetaDataCommand(){
    if ( m_pOldData ) {
        GetEditBuffer()->FreeMetaData(m_pOldData);
    }
    if ( m_pNewData ) {
        GetEditBuffer()->FreeMetaData(m_pNewData);
    }
}

void CSetMetaDataCommand::Undo(){
    if ( m_bNewItem  ) {
        if ( m_pNewData ) {
            GetEditBuffer()->DeleteMetaData(m_pNewData);
        }
    }
    else {
        if ( m_pOldData ) {
            GetEditBuffer()->SetMetaData(m_pOldData);
        }
    }
}

void CSetMetaDataCommand::Redo(){
    if ( m_bDelete ) {
        if ( m_pOldData ) {
            GetEditBuffer()->DeleteMetaData(m_pOldData);
        }
    }
    else {
        if ( m_pNewData ) {
            GetEditBuffer()->SetMetaData(m_pNewData);
        }
    }
}

// CSetTableDataCommand

CSetTableDataCommand::CSetTableDataCommand(CEditBuffer* buffer, EDT_TableData* pTableData, intn id)
    : CEditCommand(buffer, id){
    m_pOldData = GetEditBuffer()->GetTableData();
    GetEditBuffer()->SetTableData(pTableData);
    m_pNewData = GetEditBuffer()->GetTableData();
}

CSetTableDataCommand::~CSetTableDataCommand(){
    CEditTableElement::FreeData(m_pOldData);
    CEditTableElement::FreeData(m_pNewData);
}

void CSetTableDataCommand::Undo(){
    GetEditBuffer()->SetTableData(m_pOldData);
}

void CSetTableDataCommand::Redo(){
    GetEditBuffer()->SetTableData(m_pNewData);
}

CSetTableCaptionDataCommand::CSetTableCaptionDataCommand(CEditBuffer* buffer, EDT_TableCaptionData* pTableData, intn id)
    : CEditCommand(buffer, id){
    m_pOldData = GetEditBuffer()->GetTableCaptionData();
    GetEditBuffer()->SetTableCaptionData(pTableData);
    m_pNewData = GetEditBuffer()->GetTableCaptionData();
}

CSetTableCaptionDataCommand::~CSetTableCaptionDataCommand(){
    CEditCaptionElement::FreeData(m_pOldData);
    CEditCaptionElement::FreeData(m_pNewData);
}

void CSetTableCaptionDataCommand::Undo(){
    GetEditBuffer()->SetTableCaptionData(m_pOldData);
}

void CSetTableCaptionDataCommand::Redo(){
    GetEditBuffer()->SetTableCaptionData(m_pNewData);
}

CSetTableRowDataCommand::CSetTableRowDataCommand(CEditBuffer* buffer, EDT_TableRowData* pTableData, intn id)
    : CEditCommand(buffer, id){
    m_pOldData = GetEditBuffer()->GetTableRowData();
    GetEditBuffer()->SetTableRowData(pTableData);
    m_pNewData = GetEditBuffer()->GetTableRowData();
}

CSetTableRowDataCommand::~CSetTableRowDataCommand(){
    CEditTableRowElement::FreeData(m_pOldData);
    CEditTableRowElement::FreeData(m_pNewData);
}

void CSetTableRowDataCommand::Undo(){
    GetEditBuffer()->SetTableRowData(m_pOldData);
}

void CSetTableRowDataCommand::Redo(){
    GetEditBuffer()->SetTableRowData(m_pNewData);
}

CSetTableCellDataCommand::CSetTableCellDataCommand(CEditBuffer* buffer, EDT_TableCellData* pTableData, intn id)
    : CEditCommand(buffer, id){
    m_pOldData = GetEditBuffer()->GetTableCellData();
    GetEditBuffer()->SetTableCellData(pTableData);
    m_pNewData = GetEditBuffer()->GetTableCellData();
}

CSetTableCellDataCommand::~CSetTableCellDataCommand(){
    CEditTableCellElement::FreeData(m_pOldData);
    CEditTableCellElement::FreeData(m_pNewData);
}

void CSetTableCellDataCommand::Undo(){
    GetEditBuffer()->SetTableCellData(m_pOldData);
}

void CSetTableCellDataCommand::Redo(){
    GetEditBuffer()->SetTableCellData(m_pNewData);
}

#ifdef SUPPORT_MULTICOLUMN

CSetMultiColumnDataCommand::CSetMultiColumnDataCommand(CEditBuffer* buffer, EDT_MultiColumnData* pMultiColumnData, intn id)
    : CEditCommand(buffer, id){
    m_pOldData = GetEditBuffer()->GetMultiColumnData();
    GetEditBuffer()->SetMultiColumnData(pMultiColumnData);
    m_pNewData = GetEditBuffer()->GetMultiColumnData();
}

CSetMultiColumnDataCommand::~CSetMultiColumnDataCommand(){
    CEditMultiColumnElement::FreeData(m_pOldData);
    CEditMultiColumnElement::FreeData(m_pNewData);
}

void CSetMultiColumnDataCommand::Undo(){
    GetEditBuffer()->SetMultiColumnData(m_pOldData);
}

void CSetMultiColumnDataCommand::Redo(){
    GetEditBuffer()->SetMultiColumnData(m_pNewData);
}
#endif

// CSetSelectionCommand

CSetSelectionCommand::CSetSelectionCommand(CEditBuffer* buffer, CEditSelection& selection, intn id)
    : CEditCommand(buffer, id){
    m_NewSelection = GetEditBuffer()->EphemeralToPersistent(selection);
}

CSetSelectionCommand::~CSetSelectionCommand(){
}

void CSetSelectionCommand::Do(){
    GetEditBuffer()->GetSelection(m_OldSelection);
    GetEditBuffer()->SetSelection(m_NewSelection);
}

void CSetSelectionCommand::Undo(){
    GetEditBuffer()->SetSelection(m_OldSelection);
}

void CSetSelectionCommand::Redo(){
    GetEditBuffer()->SetSelection(m_NewSelection);
}

#endif
