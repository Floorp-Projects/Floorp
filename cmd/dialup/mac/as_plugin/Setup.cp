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

#include "pluginIncludes.h"
#include "MUC.h"

Boolean				openedOurResFileFlag = false;
FSSpec				pluginFSSpec;
TimerUPP			initTimerProcUPP = NULL;
void*				theArray = NULL;			// array of data from Reg Server

void*				lckFileData = NULL;
void*				animationDat = NULL;
void*				animationRes = NULL;
long				lckFileDataLen = 0L;
long				animationDatLen = 0L;
long				animationResLen = 0L;

short				pluginResFile = 0;
Boolean				freePPPAutoConnectStateDetectedFlag = FALSE;
Boolean				freePPPAutoConnectState;
extern Boolean		cursorDirty;
extern Boolean		connectedFlag;
//extern FreePPPPubInterfaceUPP		pppPubUPP;
//extern PPPRequestUPP	pppPluginUPP;
extern Str255		gRegAccountName;



pascal void			initTimer( TMTaskPtr tmTaskPtr );
extern	pascal OSErr __initialize( const CFragInitBlock* theInitBlock );
extern	pascal void __terminate( void );


pascal OSErr initFragment( const CFragInitBlock* block )
{
	OSErr		err = noErr;

	err = __initialize( block );
	if ( err == noErr )
	{
		if ( block )
		{
			if ( block->fragLocator.where == kDataForkCFragLocator )
			{
				if ( block->fragLocator.u.onDisk.fileSpec )
				{
					pluginFSSpec.vRefNum = block->fragLocator.u.onDisk.fileSpec->vRefNum;
					pluginFSSpec.parID = block->fragLocator.u.onDisk.fileSpec->parID;
					BlockMove( block->fragLocator.u.onDisk.fileSpec->name, &pluginFSSpec.name, 1L+(unsigned)(block->fragLocator.u.onDisk.fileSpec->name[ 0 ] ) );
					if ( ( pluginResFile = FSpOpenResFile( &pluginFSSpec, fsRdPerm ) ) != kResFileNotOpened )
					{
						openedOurResFileFlag = true;
					}
				}
			}
		}
		if ( openedOurResFileFlag == false )
		{
			// default to trying to use the current resfile if for some reason we can't open our resfile
			pluginResFile = CurResFile();
		}
	}
	return err;
}



pascal void terminateFragment( void )
{
	__terminate();
	if ( openedOurResFileFlag == true )
	{
		(void)CloseResFile( pluginResFile );
		openedOurResFileFlag = false;
	}
}



//------------------------------------------------------------------------------------
// NPP_Initialize:
//------------------------------------------------------------------------------------
NPError NPP_Initialize(void)
{
	initAsyncCursors();

	// check for FreePPP 
	//if (err=Gestalt(gestaltFreePPPPubInterfaceSelector,(long *)&pppPubUPP))	{
	//	pppPubUPP=NULL;
	//	}
	//if (pppPubUPP != NULL)	{
	//	if (pppPubUPP = NewFreePPPPubInterfaceUPP(pppPubUPP))	{
	//		SETUP_PLUGIN_INFO_STR("\p NPP_Initialize: FreePPP is installed", NULL);
	//		}
	//	else	{
	//		pppPubUPP=NULL;
	//		SETUP_PLUGIN_ERROR("\p NPP_Initialize: NewFreePPPPubInterfaceUPP error;g", err);
	//		}
	//	}
	//else	{
	//	SETUP_PLUGIN_ERROR("\p NPP_Initialize: FreePPP is not installed", err);
	//	}

	// check for FreePPP Config Plugin  (how?  gestaltFreePPPPluginSelector is registered by FreePPP)

	//if (err=Gestalt(gestaltFreePPPPluginSelector,(long *)&pppPluginUPP))	{
	//	pppPluginUPP=NULL;
	//	}
	//if (pppPluginUPP != NULL)	{
	//	if (pppPluginUPP = NewPPPRequestUPP(pppPluginUPP))	{
	//		SETUP_PLUGIN_INFO_STR("\p NPP_Initialize: FreePPP Config Plugin is installed", NULL);
	//		}
	//	else	{
	//		pppPluginUPP=NULL;
	//		SETUP_PLUGIN_ERROR("\p NPP_Initialize: NewPPPRequestUPP error;g", err);
	//		}
	//	}
	//else	{
	//	SETUP_PLUGIN_ERROR("\p NPP_Initialize: FreePPP Config Plugin is not installed", err);
	//	}

	// get Java Environment reference

//	JRIEnv* env = NPN_GetJavaEnv();
//	if( env ) {
//		SetupPlugin::_register( env );			// see NPP_GetJavaClass()
//		netscape_plugin_Plugin::_use( env );		// only "use" other things, not ourself
//		register_SetupPlugin(env);			// don't "register" ourself (it happens elsewhere)
//		use_SetupPlugin(env);
//		register_java_lang_String(env);
//		register_netscape_javascript_JSObject(env);
//		use_netscape_javascript_JSObject(env);

//		use_java_lang_String(env);

//		}
	return NPERR_NO_ERROR;
}



//------------------------------------------------------------------------------------
// NPP_Shutdown:
//------------------------------------------------------------------------------------
void NPP_Shutdown(void)
{
//	patchPopUpMenuSelect(FALSE);
	stopAsyncCursors();
//	disposeCache( FALSE );

	// as onUnload handlers are broken, detect a quit (or window close) by
	// noticing when plugin is unloaded. If a Reggie account has ever been created,
	// and FreePPP is currently connected, then hangup and send Quit AppleEvent
	// to Navigator

	if ( gRegAccountName[ 0 ] )
	{
		if ( native_SetupPlugin_SECURE_0005fIsDialerConnected( NULL, NULL ) == TRUE )
		{
			native_SetupPlugin_SECURE_0005fDialerHangup( NULL, NULL );
			native_SetupPlugin_SECURE_0005fQuitNavigator( NULL, NULL );
		}
	}

	// reset FreePPP's "Allow Apps to open connections" option
	if ( freePPPAutoConnectStateDetectedFlag == TRUE )
		CallMUCPlugin( kSetAutoConnectState, &freePPPAutoConnectState );

	JRIEnv* env = NPN_GetJavaEnv();
	if ( env )
	{
//		SetupPlugin::_unregister( env );		// see NPP_GetJavaClass()
//		netscape_plugin_Plugin::_unuse( env );		// only "unuse" other things, not ourself
//		unuse_SetupPlugin(env);
//		unregister_SetupPlugin(env);			// don't "unregister" ourself (it happens elsewhere)
//		unregister_java_lang_String(env);
//		unuse_netscape_javascript_JSObject( env);
//		unregister_netscape_javascript_JSObject(env);

		unuse_java_lang_String( env );
	}
}



#ifdef	SECURITY_DIALOG_ENABLED

void
blinkButton(DialogPtr theDialog,short itemNum)
{
		Handle			itemH;
		Rect			box;
		short			itemType;
		long			theTick;

	if (theDialog)	{
		GetDItem(theDialog,itemNum,&itemType,&itemH,&box);
		if (itemH)	{
			HiliteControl((ControlHandle)itemH, kInLabelControlPart);
			Delay(10L,&theTick);
			HiliteControl((ControlHandle)itemH, kNoHiliteControlPart);
			}
		}
}



pascal Boolean 
securityDialogFilter(DialogPtr theDialog, EventRecord *theEvent, short *itemHit)
{
		Boolean			retVal=FALSE;
		GrafPtr			savePort;
		Handle			itemH;
		Rect			box;
		short			itemType;

	if (theDialog)	{
		GetPort(&savePort);
		SetPort((GrafPtr)theDialog);
		
		switch(theEvent->what)	{
			case	updateEvt:
			GetDItem(theDialog,ok,&itemType,&itemH,&box);
			InsetRect(&box,-4,-4);
			PenSize(3,3);
			FrameRoundRect(&box,16,16);
			PenNormal();
			break;

			case	keyDown:
			switch(theEvent->message & charCodeMask)	{
				case	0x0D:
				case	0x03:
				blinkButton(theDialog,ok);
				*itemHit=ok;
				retVal=TRUE;
				break;

				case	0x1B:
				blinkButton(theDialog,cancel);
				*itemHit=cancel;
				retVal=TRUE;
				break;

				case	'.':
				if (theEvent->modifiers & cmdKey)	{
					blinkButton(theDialog,cancel);
					*itemHit=cancel;
					retVal=TRUE;
					}
				break;
				}
			break;
			}
		
		SetPort(savePort);
		}
	return(retVal);
}

#endif SECURITY_DIALOG_ENABLED



//------------------------------------------------------------------------------------
// NPP_New:
//------------------------------------------------------------------------------------
NPError NPP_New(NPMIMEType pluginType,
				NPP instance,
				uint16 mode,
				int16 argc,
				char* argn[],
				char* argv[],
				NPSavedData* saved)
{
		CursHandle		theCursH=NULL;
		DialogPtr		theDialog=NULL;
		short			itemHit=0;

#ifdef	SECURITY_DIALOG_ENABLED
		short			saveRefNum;
		ModalFilterUPP		filterUPP;
#endif	SECURITY_DIALOG_ENABLED

	if (instance == NULL)	{
		return NPERR_INVALID_INSTANCE_ERROR;
		}
		
	instance->pdata = NPN_MemAlloc(sizeof(PluginInstance));
	PluginInstance* This = (PluginInstance*) instance->pdata;
	
	if (This != NULL)	{
		if (mode == NP_EMBED)	{
			useCursor(watchCursor);
			cleanupStartupFolder();				// remove any alias to Account Setup in Startup Items folder

			// checkSystemSoftware is now done elsewhere
/*
			if (checkSystemSoftware())	{		// make sure that FreePPP and MacTCP/OpenTransport are installed
				return(NPERR_MODULE_LOAD_FAILED_ERROR);
				}
*/
			// check for expiration - quit if expired
//
//			if (CheckIfExpired())	{
//				showPluginError(BETAEXPIRED_STRINGID, true);
//				return(NPERR_MODULE_LOAD_FAILED_ERROR);
//				}

#ifdef	SECURITY_DIALOG_ENABLED

			// B3 security dialog

			saveRefNum=CurResFile();
			if (pluginResFile!=0)	UseResFile(pluginResFile);
			theDialog=GetNewDialog(PLUGIN_SECURITY_DIALOG_RESID,NULL,(WindowPtr)(-1L));
			UseResFile(saveRefNum);
			if (!theDialog)	{
				return(NPERR_MODULE_LOAD_FAILED_ERROR);
				}
			if (!(filterUPP=NewModalFilterProc(securityDialogFilter)))	{
				return(NPERR_MODULE_LOAD_FAILED_ERROR);
				}
			
			ShowWindow((WindowPtr)theDialog);
			InitCursor();
			while(TRUE)	{
				ModalDialog(filterUPP,&itemHit);
				if (itemHit==ok || itemHit==cancel)	break;
				}
			DisposeDialog(theDialog);
			DisposeRoutineDescriptor(filterUPP);

			if (itemHit==cancel)	{
				native_SetupPlugin_QuitNavigator(NULL,NULL);
				return(NPERR_MODULE_LOAD_FAILED_ERROR);
				}

#endif	SECURITY_DIALOG_ENABLED

			if (theCursH=GetCursor(watchCursor))	{
				HLock((Handle)theCursH);
				SetCursor(*theCursH);
				HUnlock((Handle)theCursH);
				}

/*
			JRIEnv* env = NPN_GetJavaEnv();			// if NP_EMBED, save JavaScript environment reference
			jsWindow = netscape_plugin_Plugin_getWindow(env,NPN_GetJavaPeer(instance));
*/
			}
		This->fWindow = NULL;
		This->regData = NULL;
		This->fMode = mode;
		
		This->data = NULL;
		
		return(NPERR_NO_ERROR);
		}
	else	{
		return(NPERR_OUT_OF_MEMORY_ERROR);
		}
}



//------------------------------------------------------------------------------------
// NPP_Destroy:
//------------------------------------------------------------------------------------
NPError NPP_Destroy(NPP instance, NPSavedData** save)
{
	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	PluginInstance* This = (PluginInstance*) instance->pdata;

	if (This != NULL)
	{
		if (This->regData) {
			::DisposeHandle(This->regData);
		}
		NPN_MemFree(instance->pdata);
		instance->pdata = NULL;
		
		if (This->data)	{
			::DisposeHandle(This->data);
		}
	}

	return NPERR_NO_ERROR;
}



//------------------------------------------------------------------------------------
// NPP_SetWindow:
//------------------------------------------------------------------------------------
NPError NPP_SetWindow(NPP instance, NPWindow* window)
{

	InitCursor();

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	PluginInstance* This = (PluginInstance*) instance->pdata;

	//
	// *Developers*: Before setting fWindow to point to the
	// new window, you may wish to compare the new window
	// info to the previous window (if any) to note window
	// size changes, etc.
	//
	
	This->fWindow = window;

	return NPERR_NO_ERROR;
}



//------------------------------------------------------------------------------------
// NPP_NewStream:
//------------------------------------------------------------------------------------
NPError NPP_NewStream( NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype )
{
	if ( instance == NULL )
	{
		return NPERR_INVALID_INSTANCE_ERROR;
	}

	PluginInstance* This = (PluginInstance*)instance->pdata;

	regStream* newStream = (regStream*)NPN_MemAlloc( sizeof( regStream ) );

	if ( newStream != NULL )
	{
		newStream->data = NULL;
		newStream->dataLen = 0;
		stream->pdata = newStream;
		if ( type )	
		{
			newStream->extendedDataFlag = ( !strcmp( type, REG_STREAM_TYPE_V2 ) ) ? TRUE : FALSE;
		}
	}
	else
	{
		return NPERR_OUT_OF_MEMORY_ERROR;
	}
	return NPERR_NO_ERROR;
}




//
// *Developers*: 
// These next 2 functions are directly relevant in a plug-in which handles the
// data in a streaming manner.  If you want zero bytes because no buffer space
// is YET available, return 0.  As long as the stream has not been written
// to the plugin, Navigator will continue trying to send bytes.  If the plugin
// doesn't want them, just return some large number from NPP_WriteReady(), and
// ignore them in NPP_Write().  For a NP_ASFILE stream, they are still called
// but can safely be ignored using this strategy.
//

int32 STREAMBUFSIZE = 0X0FFFFFFF;   // If we are reading from a file in NPAsFile
                                    // mode so we can take any size stream in our
                                    // write call (since we ignore it)

//------------------------------------------------------------------------------------
// NPP_WriteReady:
//------------------------------------------------------------------------------------
int32 NPP_WriteReady( NPP instance, NPStream* stream )
{
	if ( instance != NULL )
	{
		PluginInstance* This = (PluginInstance*)instance->pdata;
	
	}

	return STREAMBUFSIZE;   // Number of bytes ready to accept in NPP_Write()
}



//------------------------------------------------------------------------------------
// NPP_Write:
//------------------------------------------------------------------------------------
int32 NPP_Write( NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer )
{
	if ( instance != NULL )
	{
		if ( stream->pdata != NULL )
		{
			regStream* pluginStream = (regStream*)stream->pdata;
			AppendData( pluginStream, len, buffer );
			return len; // The number of bytes accepted
		}
	}

	return -1; 			// Something went wrong. Stop the stream.
}




/*
	countRegItems:	count the number of elements of Registration data
*/


long
countRegItems(Handle regData, Boolean extendedDataFlag)
{
		unsigned short		len;
		unsigned long		lenLong;
		long			numRegItems=0L,theSize;
		char			*p;

	theSize=GetHandleSize(regData);
	p=*regData;

	while (theSize>0)	{
		if (extendedDataFlag == TRUE)	{
			BlockMove(p,&lenLong,sizeof(lenLong));
			p+=sizeof(lenLong);
			theSize-=sizeof(lenLong);
			if (theSize<lenLong)	break;
			p+=lenLong;
			theSize-=lenLong;
			}
		else	{
			BlockMove(p,&len,sizeof(len));
			p+=sizeof(len);
			theSize-=sizeof(len);
			if (theSize<len)	break;
			p+=len;
			theSize-=len;
			}
		if (theSize<0)		break;

		++numRegItems;
		}
	numRegItems /= 2L;

	return(numRegItems);
}



/*
	getRegElement:	look for a variable in the Registration data, and return the value if found, otherwise NULL
*/

java_lang_String *
getRegElement(JRIEnv *env,Handle regData,long elementNum, Boolean extendedDataFlag)
{
		unsigned short		lenShort;
		unsigned long		len;
		long			theSize;
		char			*p;
		java_lang_String	*data=NULL;

		char			*nameBuffer=NULL,*buffer=NULL;
		unsigned long		nameBufferLen;

	theSize=GetHandleSize(regData);
	p=*regData;

	while (elementNum>0 && theSize>0)	{
		if (extendedDataFlag==TRUE)	{
			BlockMove(p,&len,sizeof(len));
			p+=sizeof(len);
			theSize-=sizeof(len);
			}
		else	{
			BlockMove(p,&lenShort,sizeof(lenShort));
			p+=sizeof(lenShort);
			theSize-=sizeof(lenShort);
			len=(unsigned long)lenShort;
			}
		if (theSize<len)	break;
		p+=len;
		theSize-=len;
		if (theSize<0)		break;

		if (extendedDataFlag==TRUE)	{
			BlockMove(p,&len,sizeof(len));
			p+=sizeof(len);
			theSize-=sizeof(len);
			}
		else	{
			BlockMove(p,&lenShort,sizeof(lenShort));
			p+=sizeof(lenShort);
			theSize-=sizeof(lenShort);
			len=(unsigned long)lenShort;
			}
		if (theSize<len)	break;
		p+=len;
		theSize-=len;
		if (theSize<0)		break;

		--elementNum;
		}
	if (!elementNum && theSize>0)	{
		if (extendedDataFlag==TRUE)	{
			BlockMove(p,&len,sizeof(len));
			p+=sizeof(len);
			theSize-=sizeof(len);
			}
		else	{
			BlockMove(p,&lenShort,sizeof(lenShort));
			p+=sizeof(lenShort);
			theSize-=sizeof(lenShort);
			len=(unsigned long)lenShort;
			}
		if (theSize<len){
			return(data);
			}
		nameBufferLen=len*2;
		if (!(nameBuffer=NewPtr(nameBufferLen)))	return(data);

		//BlockMove( (Ptr)p, (Ptr)nameBuffer, len );
		//nameBuffer[ len + 1 ] = '=';
		//nameBuffer[ len + 2 ] = 0;		
		sprintf(nameBuffer,"%.*s=",len,p);

		p+=len;
		theSize-=len;
		if (theSize<0)	{
			return(data);
			}
		if (extendedDataFlag==TRUE)	{
			BlockMove(p,&len,sizeof(len));
			p+=sizeof(len);
			theSize-=sizeof(len);
			}
		else	{
			BlockMove(p,&lenShort,sizeof(lenShort));
			p+=sizeof(lenShort);
			theSize-=sizeof(lenShort);
			len=(unsigned long)lenShort;
			}
		if (theSize<len)	{
			return(data);
			}
		if (!strcmp(nameBuffer,"LCK_FILE="))	{
			lckFileData = p;
			lckFileDataLen = len;
			len=0;
			}
		else if (!strcmp(nameBuffer,"ANIMATION_DAT="))	{
			animationDat = p;
			animationDatLen = len;
			len=0;
			}
		else if (!strcmp(nameBuffer,"ANIMATION_RES="))	{
			animationRes = p;
			animationResLen = len;
			len=0;
			}
		else if (!strcmp(nameBuffer,"ICON=")) {
			long x = 0;
			long y = 1;
			long c = x + y;
			len = 0;
			}
		else if (buffer=NewPtr(nameBufferLen + len))	{
			strcpy(buffer,nameBuffer);
			strncat(buffer,p,len);
//			data=JRI_NewStringUTF(env,buffer,strlen(buffer));
			//data = NULL;
			data=cStr2javaLangString(env,buffer,strlen(buffer));			
			DisposePtr(buffer);
			}
		if (nameBuffer)	{
			DisposePtr(nameBuffer);
			}
		}
	
	return(data);
}



//------------------------------------------------------------------------------------
// NPP_DestroyStream:
//------------------------------------------------------------------------------------



NPError NPP_DestroyStream(NPP instance, NPStream *stream, NPError reason)
{
		JRIGlobalRef		globalRef=NULL;
		Boolean			extendedDataFlag=FALSE;
		FSSpec			profileSpec;
		OSErr			err=noErr;

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;
	PluginInstance* This = (PluginInstance*) instance->pdata;
	regStream* pluginStream = (regStream*) stream->pdata;
	
	if (pluginStream->data != NULL)	{
		if( This->regData != NULL )	{		// If there was already a picture being displayed,
			::DisposeHandle( This->regData );	// dispose of it.
			}
		
		Handle regData = pluginStream->data;		
		extendedDataFlag = pluginStream->extendedDataFlag;
		pluginStream->data = NULL;			// Dispose of the stream structure
		NPN_MemFree( pluginStream );
		stream->pdata = NULL;

		JRIEnv* env = NPN_GetJavaEnv();
		if (regData && env)	{			// save regData in global for GetRegInfo call later
			HLock(regData);

			lckFileData = animationDat = animationRes = NULL;
			lckFileDataLen = animationDatLen = animationResLen = 0L;

			long numItems = countRegItems(regData,extendedDataFlag);

			// using the intermediate refToArray so we can prevent this from being garbage collected.
			theArray = JRI_NewObjectArray( env, numItems, class_java_lang_String(env), NULL );
			globalRef=JRI_NewGlobalRef(env, theArray);	// locks the array
			for (long x=0; x<numItems; x++)	{
				java_lang_String* langString = getRegElement( env, regData, x, extendedDataFlag );
				JRI_SetObjectArrayElement( env, theArray, x, langString );
				}
			if (globalRef)	JRI_DisposeGlobalRef(env, globalRef);

			if (!(err=getProfileDirectory(&profileSpec)))	{
				if (lckFileData != NULL)	{
					BlockMove(CFG_FILENAME,profileSpec.name,1L+(unsigned)CFG_FILENAME[0]);
					WriteFile(&profileSpec,lckFileData,lckFileDataLen,NULL,0L);
					}
				if ((animationDat != NULL) || (animationRes != NULL))	{
					BlockMove(ANIMATION_FILENAME,profileSpec.name,1L+(unsigned)ANIMATION_FILENAME[0]);
					WriteFile(&profileSpec,animationDat,animationDatLen,animationRes,animationResLen);
					}
				}

/*
			if (!jsWindow)	debugstr("bad... jsWindow is NULL for callback");
			struct java_lang_Class* clazz = JRI_FindClass(env, classname_SetupPlugin);
			SetupPlugin_RegistrationComplete(env, clazz, jsWindow, theArray );
*/
			DisposeHandle(regData);
			}
		}
	return NPERR_NO_ERROR;
}



extern JRI_PUBLIC_API(jstringArray)
native_SetupPlugin_SECURE_0005fGetRegInfo(JRIEnv* env,struct SetupPlugin* self, jbool flushDataFlag)
{
		void		*data;

	data=theArray;
	if (flushDataFlag == true)	{
		theArray=NULL;						
		}
	return((jstringArray)data);
}



//------------------------------------------------------------------------------------
// NPP_StreamAsFile:
//------------------------------------------------------------------------------------
void NPP_StreamAsFile(NPP instance, NPStream *stream, const char* fname)
{
// We donÕt support files
}



//------------------------------------------------------------------------------------
// NPP_Print:
//------------------------------------------------------------------------------------
void NPP_Print(NPP instance, NPPrint* printInfo)
{
	if (instance != NULL)
	{
		PluginInstance* This = (PluginInstance*) instance->pdata;
	
		if (printInfo->mode == NP_FULL)
		{
			//
			// If weÕre fullscreen, we donÕt want to take over printing,
			// so return false.  NPP_Print will be called again with
			// mode == NP_EMBED.
			//
			printInfo->print.fullPrint.pluginPrinted = false;
		}
		else	// If not fullscreen, we must be embedded
		{
			NPWindow* printWindow = &(printInfo->print.embedPrint.window);
			if (StartDraw(printWindow))
			{
				DoDraw(This);
				EndDraw(printWindow);
			}
		}
	}

}


//------------------------------------------------------------------------------------
// NPP_HandleEvent:
// Mac-only.
//------------------------------------------------------------------------------------
int16 NPP_HandleEvent(NPP instance, void* event)
{
	Boolean					eventHandled = false;
	OSErr					err;
	static Boolean			spinningFlag = false;
	static unsigned long	staticReceivedPacketCount = 0L;	
	unsigned long			receivedPacketCount = 0L;
	static long				lastChangeTick = 0L;

	if ( connectedFlag == TRUE )
	{
		err = CallMUCPlugin( kGetReceivedIPPacketCount, &receivedPacketCount ); 
		if ( !err )
		{
			if ( staticReceivedPacketCount != receivedPacketCount )
			{
				staticReceivedPacketCount = receivedPacketCount;
				lastChangeTick = TickCount();
				if ( spinningFlag == false )
				{
					spinningFlag = true;
					startAsyncCursors();
				}
			}
		}
		if ( TickCount() > ( lastChangeTick + SPIN_TIMEOUT ) )
		{
			if ( spinningFlag == true )
			{
				spinningFlag = false;
				stopAsyncCursors();
			}
		}
	}
	else if ( spinningFlag == true )
	{
		spinningFlag = false;
		stopAsyncCursors();
	}

	if ( ( spinningFlag == false ) && ( cursorDirty == TRUE ) )
		useCursor( 0 );

	if ( instance == NULL )
		return eventHandled;
		
	PluginInstance* This = (PluginInstance*) instance->pdata;
	if (This != NULL && event != NULL)
	{
		EventRecord* ev = (EventRecord*) event;
		switch (ev->what)
		{
			//
			// Draw ourselves on update events
			//
			case updateEvt:
//				DoDraw(This);
				eventHandled = true;
				break;
				
			default:
				break;
		}
			
	}
	
	return eventHandled;
}

//------------------------------------------------------------------------------------
// NPP_URLNotify: Currently just a stub for compatibility with the new API
//------------------------------------------------------------------------------------
void NPP_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{
}

//------------------------------------------------------------------------------------
// NPP_GetJavaClass: Currently just a stub for compatibility with the new API
//------------------------------------------------------------------------------------
jref NPP_GetJavaClass( void )
{
	JRIEnv*		env;
	jref		theRef = NULL;

	if ( env = NPN_GetJavaEnv() )
	{
//		theRef = SetupPlugin::_register( env );
		theRef = register_SetupPlugin( env );
//		netscape_plugin_Plugin::_use( env );
	
		use_java_lang_String( env ); 	 // ::_use( env );			// new
		use_netscape_plugin_Plugin( env ); // ::_use( env );			// new
	}
	return theRef;
}


//------------------------------------------------------------------------------------
// AppendData:
//------------------------------------------------------------------------------------
void AppendData( regStream* This, unsigned long len, void* buffer )
{
	if ( This->data == NULL )
	{
		This->data = ::NewHandle( len );
		This->dataLen = 0;
	}
	else
	{
		SetHandleSize( This->data, GetHandleSize( This->data ) + len );
	}
	BlockMove( buffer, (*This->data) + This->dataLen, len );
	This->dataLen += len;
}


//------------------------------------------------------------------------------------
// StartDraw:
//------------------------------------------------------------------------------------
Boolean StartDraw(NPWindow* window)
{
	return true;
}


//------------------------------------------------------------------------------------
// EndDraw:
//------------------------------------------------------------------------------------
void EndDraw(NPWindow* window)
{
}


//------------------------------------------------------------------------------------
// DoDraw:
//------------------------------------------------------------------------------------
void DoDraw(PluginInstance* This)
{
}
