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

#ifdef __cplusplus
extern "C" {
#endif




// Setup.cp prototype(s)
extern void			AppendData( regStream* This, unsigned long len, void* buffer );
extern Boolean		StartDraw( NPWindow* window );
extern void			EndDraw( NPWindow* window );
extern void			DoDraw( PluginInstance* This );
extern void			blinkButton( DialogPtr theDialog, short itemNum );
pascal Boolean 		securityDialogFilter( DialogPtr theDialog, EventRecord* theEvent, short* itemHit );
extern long			countRegItems( Handle regData, Boolean extendedDataFlag );
extern java_lang_String* getRegElement( JRIEnv *env, Handle regData, long elementNum, Boolean extendedDataFlag );

// asyncCursors.c prototype(s)
extern OSErr		initAsyncCursors( void );
void				spinCursor( acurHandle cursorsH );
void				startAsyncCursors( void );
void				stopAsyncCursors( void );

// cacheRtns.c prototype(s)
extern OSErr		addToCache( FSSpecPtr theFSSpecPtr, unsigned long ioFlMdDat, Handle dataH );
extern OSErr		findInCache( FSSpecPtr theFSSpecPtr, Handle *dataH );
extern void			disposeCache( Boolean justPruneFlag );

// dialerRtns.c prototype(s)
extern void*		LoadMUCPlugin();
extern long			CallMUCPlugin( long selector, void* pb );

extern JRI_PUBLIC_API(jbool)	native_SetupPlugin_SECURE_0005fCheckEnvironment( JRIEnv* env, struct SetupPlugin* self );
extern OSErr		checkSystemSoftware( void );
extern JRI_PUBLIC_API(jbool)	native_SetupPlugin_SECURE_0005fDialerConnect( JRIEnv* env, struct SetupPlugin* self );
extern JRI_PUBLIC_API(void)		native_SetupPlugin_SECURE_0005fDialerHangup( JRIEnv* env, struct SetupPlugin* self );
extern JRI_PUBLIC_API(jbool)	native_SetupPlugin_SECURE_0005fIsDialerConnected( JRIEnv* env, struct SetupPlugin* self );
extern OSErr		addPPPConfigRecord( PPPConfigureStruct **pppH, OSType typeCode, void *data );
ip_addr				convertToIPAddr( StringPtr IPaddress );
OSErr				processScriptFile( StringPtr autoSend, StringPtr scriptStr, StringPtr scriptFileName,ScriptLineDef* commands );
extern JRI_PUBLIC_API(void)		native_SetupPlugin_SECURE_0005fDialerConfig( JRIEnv* env, struct SetupPlugin* self, jstringArray dialerData, jbool regMode );
extern JRI_PUBLIC_API(struct java_lang_String *) native_SetupPlugin_SECURE_0005fEncryptString( JRIEnv* env, struct SetupPlugin* self, struct java_lang_String* clearText );
Boolean				MacTCPIsOpen( void );
extern JRI_PUBLIC_API(void)		native_SetupPlugin_SECURE_0005fDesktopConfig( JRIEnv* env,struct SetupPlugin* self, java_lang_String *desktopData, java_lang_String *iconFilename, java_lang_String *acctsetFilename );

// editorRtns.c prototype(s)
extern JRI_PUBLIC_API(struct java_lang_String *) native_SetupPlugin_SECURE_0005fGetExternalEditor(JRIEnv* env, struct SetupPlugin* self);
extern JRI_PUBLIC_API(void) native_SetupPlugin_SECURE_0005fOpenFileWithEditor(JRIEnv* env, struct SetupPlugin* self, struct java_lang_String *app, struct java_lang_String *file);

// errorRtns.c prototype(s)
void				showPluginError( short errStrIndex, Boolean quitNavFlag );

// ExpireBeta.cp prototype(s)
extern Boolean		CheckIfExpired( void );

// fileFolderRtns.c prototype(s)

extern JRI_PUBLIC_API(struct java_lang_Object *)	native_SetupPlugin_SECURE_0005fReadFile(JRIEnv* env, struct SetupPlugin* self, struct java_lang_String *file);
extern OSErr		WriteFile( FSSpecPtr theFile,void *data, long dataLen, void *res, long resLen );
extern JRI_PUBLIC_API(void)		native_SetupPlugin_SECURE_0005fWriteFile(JRIEnv* env, struct SetupPlugin* self, struct java_lang_String *file, struct java_lang_Object *data);
extern JRI_PUBLIC_API(struct java_lang_String *)	native_SetupPlugin_SECURE_0005fGetNameValuePair(JRIEnv* env, struct SetupPlugin* self, struct java_lang_String *file, struct java_lang_String *section, struct java_lang_String *name);
extern JRI_PUBLIC_API(void)		native_SetupPlugin_SECURE_0005fSetNameValuePair(JRIEnv* env, struct SetupPlugin* self, struct java_lang_String *file, struct java_lang_String *section, struct java_lang_String *name, struct java_lang_String *value);
extern JRI_PUBLIC_API(jbool)	native_SetupPlugin_SECURE_0005fSaveTextToFile(JRIEnv* env, struct SetupPlugin* self, struct java_lang_String *suggestedFilename, struct java_lang_String *data, jbool promptFlag);
extern JRI_PUBLIC_API(jstringArray)		native_SetupPlugin_SECURE_0005fGetFolderContents(JRIEnv* env, struct SetupPlugin* self, struct java_lang_String *path, struct java_lang_String *suffix);
extern void			cleanupStartupFolder( void );
extern OSErr		createStartupFolderEntry( FSSpec *startupFile, StringPtr startupName );

// misc.c prototype(s)
void				useCursor( short cursNum );
extern JRI_PUBLIC_API(jbool)	native_SetupPlugin_SECURE_0005fMilan(JRIEnv* env, struct SetupPlugin* self, struct java_lang_String *name, struct java_lang_String *value, jbool pushPullFlag, jbool extendedLengthFlag);
extern const char *	javaLangString2Cstr( JRIEnv *env, struct java_lang_String *string );
extern java_lang_String *	cStr2javaLangString( JRIEnv *env, char *string, long len );
extern void			bzero(char *p,long num);


// modemWizardRtns.c prototype(s)
extern JRI_PUBLIC_API(void)	native_SetupPlugin_SECURE_0005fOpenModemWizard(JRIEnv* env, struct SetupPlugin* self);
extern JRI_PUBLIC_API(void)	native_SetupPlugin_SECURE_0005fCloseModemWizard(JRIEnv* env, struct SetupPlugin* self);
extern JRI_PUBLIC_API(jbool)	native_SetupPlugin_SECURE_0005fIsModemWizardOpen(JRIEnv* env, struct SetupPlugin* self);
extern int			CompareStringWrapper( const void *string1, const void *string2 );
extern JRI_PUBLIC_API(jstringArray)	native_SetupPlugin_SECURE_0005fGetModemList(JRIEnv* env, struct SetupPlugin* self);
extern JRI_PUBLIC_API(java_lang_String *)	native_SetupPlugin_SECURE_0005fGetCurrentModemName(JRIEnv* env, struct SetupPlugin* self);

// navigatorRtns.c prototype(s)
void	patchPopUpMenuSelect( Boolean patchFlag );
pascal long	ourPopUpMenuSelect( MenuRef menu, short top, short left, short popUpItem );
extern JRI_PUBLIC_API(void)	native_SetupPlugin_SECURE_0005fSetKiosk(JRIEnv* env, struct SetupPlugin* self, jbool flag);
extern JRI_PUBLIC_API(void)	native_SetupPlugin_SECURE_0005fQuitNavigator(JRIEnv* env,struct SetupPlugin* self);

// passwordRtns.c prototype(s)
void				ConvertPassword( char *lpBuf, char *lpszPassword );
StringPtr			MozillaSRDecrypt( StringPtr inOutStr );

// processRtnsRtns.c prototype(s)
extern Boolean		isAppRunning( OSType theSig,ProcessSerialNumber *thePSN, ProcessInfoRec *theProcInfo );
extern OSErr		FindAppInCurrentFolder( OSType theSig, FSSpecPtr theFSSpecPtr );
extern OSErr		LaunchApp( OSType theSig, Boolean appInForegroundFlag, AEDesc* launchDesc, Boolean searchAppFolderFlag );
extern OSErr		QuitApp( ProcessSerialNumber* thePSN );
extern JRI_PUBLIC_API(jbool)		native_SetupPlugin_SECURE_0005fNeedReboot( JRIEnv* env,struct SetupPlugin* self );
extern JRI_PUBLIC_API(void)			native_SetupPlugin_SECURE_0005fReboot( JRIEnv* env,struct SetupPlugin* self, java_lang_String *AccountSetupPathname);

// profileRtns.c prototype(s)
extern OSErr		getProfileDirectory( FSSpecPtr theFSSpecPtr );
extern JRI_PUBLIC_API(struct java_lang_String *)	native_SetupPlugin_SECURE_0005fGetCurrentProfileDirectory(JRIEnv* env, struct SetupPlugin* self);
extern JRI_PUBLIC_API(struct java_lang_String *)	native_SetupPlugin_SECURE_0005fGetCurrentProfileName(JRIEnv* env, struct SetupPlugin* self);
extern JRI_PUBLIC_API(void)		native_SetupPlugin_SECURE_0005fSetCurrentProfileName(JRIEnv* env, struct SetupPlugin* self, struct java_lang_String *profileName);

OSErr	findNetscapeUserProfileDatabase( FSSpecPtr theFSSpecPtr );
OSErr	findCurrentUserProfileID( short refNum, short *profileID );
Handle	pathFromFSSpec( FSSpecPtr theFSSpecPtr );

#ifdef __cplusplus
}
#endif
