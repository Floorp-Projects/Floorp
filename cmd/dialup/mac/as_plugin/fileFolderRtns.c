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



extern JRI_PUBLIC_API(struct java_lang_Object *)
native_SetupPlugin_SECURE_0005fReadFile(JRIEnv* env,
		struct SetupPlugin* self,
		struct java_lang_String *file)
{
		CInfoPBRec		cBlock;
		Handle			h;
		FSSpec			theFSSpec;
		OSErr			err=noErr;
		jref			retVal=NULL;
		char			*theFile;
		short			refNum;
		long			dataForkSize,resForkSize;
		long			theCount;

	theFile = (char *)javaLangString2Cstr( env, file );
	if (theFile && (*theFile))	{
		CtoPstr((void *)theFile);
		err=FSMakeFSSpec(0,0L,(void *)theFile,&theFSSpec);
		PtoCstr((void *)theFile);
		if (err)	{
			SETUP_PLUGIN_ERROR("\p ReadFile: FSMakeFSSpec error;g", err);
			return(NULL);
			}

		bzero((char *)&cBlock,sizeof(cBlock));
		cBlock.hFileInfo.ioCompletion=NULL;
		cBlock.hFileInfo.ioNamePtr=(StringPtr)theFSSpec.name;
		cBlock.hFileInfo.ioVRefNum=theFSSpec.vRefNum;
		cBlock.hFileInfo.ioDirID=theFSSpec.parID;
		cBlock.hFileInfo.ioFDirIndex=0;
		if (err=PBGetCatInfoSync(&cBlock))	{
			SETUP_PLUGIN_ERROR("\p ReadFile: PBGetCatInfoSync error;g", err);
			return(NULL);
			}

		useCursor(watchCursor);
		
		dataForkSize = cBlock.hFileInfo.ioFlLgLen;
		resForkSize = cBlock.hFileInfo.ioFlRLgLen;
		
		if (h=NewHandleClear((2L*sizeof(long)) + dataForkSize + resForkSize))	{
			HLock(h);
			BlockMove(&dataForkSize,*h,(long)sizeof(long));
			BlockMove(&resForkSize,(*h + (long)sizeof(long)),(long)sizeof(long));

			if (dataForkSize > 0L)	{
				if (!(err=FSpOpenDF(&theFSSpec, fsRdPerm, &refNum)))	{
					if (!(err=SetFPos(refNum,fsFromStart,0L)))	{
						theCount = dataForkSize;
						if (!(err=FSRead(refNum,&theCount,(*h + (2L*sizeof(long))))))	{
							if (theCount != dataForkSize)	err=readErr;
							}
						else	{
							SETUP_PLUGIN_ERROR("\p ReadFile: FSRead (data fork) error;g", err);
							}
						}
					(void)FSClose(refNum);
					}
				else	{
					SETUP_PLUGIN_ERROR("\p ReadFile: FSpOpenDF error;g", err);
					}
				}
			if ((!err) && (resForkSize > 0L))	{
				if (!(err=FSpOpenRF(&theFSSpec, fsRdPerm, &refNum)))	{
					if (!(err=SetFPos(refNum,fsFromStart,0L)))	{
						theCount = resForkSize;
						if (!(err=FSRead(refNum,&theCount,(*h + (2L*sizeof(long)) + dataForkSize))))	{
							if (theCount != resForkSize)	err=readErr;
							}
						else	{
							SETUP_PLUGIN_ERROR("\p ReadFile: FSRead (res fork) error;g", err);
							}
						}
					(void)FSClose(refNum);
					}
				else	{
					SETUP_PLUGIN_ERROR("\p ReadFile: FSpOpenRF error;g", err);
					}
				}
			if (!err)	{
				retVal = JRI_NewByteArray(env, GetHandleSize(h), *h);
				if (retVal != NULL)	{
					SETUP_PLUGIN_INFO_STR("\p ReadFile: JRI_NewByteArray success", NULL);
					}
				}
			DisposeHandle(h);
			}
		else	{
			err=(err=MemError()) ? err:-1;
			SETUP_PLUGIN_ERROR("\p ReadFile: NewHandleClear error;g", err);
			}
		}
	return(retVal);
}



OSErr
WriteFile(FSSpecPtr initialFSSpecPtr,void *data, long dataForkSize, void *res, long resForkSize)
{
		Boolean			movedToTrash=false;
		FSSpec			theFSSpec,trashFSSpec;
		OSErr			err=paramErr;
		short			refNum,trashVRefNum;
		long			trashDirID,theCount;

	if (initialFSSpecPtr)	{

		useCursor(watchCursor);

		err=FSMakeFSSpec(initialFSSpecPtr->vRefNum,initialFSSpecPtr->parID,initialFSSpecPtr->name,&theFSSpec);
		if (!err)	{
			// file exists;  it might be open/in-use by Communicator (for example, "Custom Animation" file),
			// so might not be able to delete it. If so, try moving it to the trash.
			//
			// (Note: every volume may have a trash folder; look on that volume, not the system volume)

			if (err=FSpDelete(&theFSSpec))	{
				if (!(err=FindFolder(theFSSpec.vRefNum, kWhereToEmptyTrashFolderType, kCreateFolder, &trashVRefNum, &trashDirID)))	{	// kTrashFolderType
					err=FSMakeFSSpec(trashVRefNum,trashDirID,theFSSpec.name,&trashFSSpec);				
					if (err==noErr || err==fnfErr)	{
						(void)FSpDelete(&trashFSSpec);				// if already in Trash, try and delete Trash item
						if (!(err=FSpCatMove(&theFSSpec, &trashFSSpec)))	{
							movedToTrash=true;
							SETUP_PLUGIN_INFO_STR("\p WriteFile: FSpCatMove succesfully moved file to trash folder",NULL);
							}
						else	{
							SETUP_PLUGIN_INFO_STR("\p WriteFile: FSpCatMove on file ;g",theFSSpec.name);
							SETUP_PLUGIN_ERROR("\p WriteFile: FSpCatMove error;g", err);
							}
						}

					}
				else	{
					SETUP_PLUGIN_INFO_STR("\p WriteFile: Unable to find Trash folder",NULL);
					}
				}
			else	{
				SETUP_PLUGIN_INFO_STR("\p WriteFile: FSpDelete file ;g",theFSSpec.name);
				}
			}

		if ((!err) || (err==fnfErr))	{
			(void)FSpCreate(&theFSSpec, NETSCAPE_SIGNATURE, UNKNOWN_SIGNATURE, smSystemScript);
			FSpCreateResFile(&theFSSpec, NETSCAPE_SIGNATURE, UNKNOWN_SIGNATURE, smSystemScript);

			if (!(err=FSpOpenDF(&theFSSpec, fsRdWrPerm, &refNum)))	{
				if (!(err=SetEOF(refNum,dataForkSize)))	{
					if (data != NULL)	{
						if (!(err=SetFPos(refNum,fsFromStart,0L)))	{
							theCount = dataForkSize;
							if (!(err=FSWrite(refNum,&theCount,data)))	{
								if (theCount != dataForkSize)	err=readErr;
								}
							else	{
								SETUP_PLUGIN_ERROR("\p WriteFile: FSWrite (data fork) error;g", err);
								}
							}
						}
					}
				else	{
					SETUP_PLUGIN_ERROR("\p WriteFile: SetEOF (data fork) error;g", err);
					}
				(void)FSClose(refNum);
				}
			else	{
				SETUP_PLUGIN_ERROR("\p WriteFile: FSpOpenDF error;g", err);
				}
			if (!err)	{
				if (!(err=FSpOpenRF(&theFSSpec, fsRdWrPerm, &refNum)))	{
					if (!(err=SetEOF(refNum,resForkSize)))	{
						if (res != NULL)	{
							if (!(err=SetFPos(refNum,fsFromStart,0L)))	{
								theCount = resForkSize;
								if (!(err=FSWrite(refNum,&theCount,res)))	{
									if (theCount != resForkSize)	err=readErr;
									}
								else	{
									SETUP_PLUGIN_ERROR("\p WriteFile: FSWrite (res fork) error;g", err);
									}
								}
							}
						}
					else	{
						SETUP_PLUGIN_ERROR("\p WriteFile: SetEOF (res fork) error;g", err);
						}
					(void)FSClose(refNum);
					}
				else	{
					SETUP_PLUGIN_ERROR("\p WriteFile: FSpOpenRF error;g", err);
					}
				}
			if (err)	{
				SETUP_PLUGIN_INFO_STR("\p WriteFile: Deleting invalid file ;g",theFSSpec.name);
				(void)FSpDelete(&theFSSpec);
				}
			}
		else	{
			SETUP_PLUGIN_ERROR("\p WriteFile: FSMakeFSSpec error;g", err);
			}
		}
	return(err);
}



extern JRI_PUBLIC_API(void)
native_SetupPlugin_SECURE_0005fWriteFile(JRIEnv* env,
		struct SetupPlugin* self,
		struct java_lang_String *file,
		struct java_lang_Object *data)
{
		FSSpec			theFSSpec;
		OSErr			err=noErr;
		Ptr			dataPtr=NULL;
		char			*theFile;
		long			dataLen;
		long			dataForkSize,resForkSize;

	theFile = (char *)javaLangString2Cstr( env, file );
	if (!theFile)			return;
	if (!data)			return;

	dataLen = JRI_GetByteArrayLength(env,data);
	if (!dataLen)			return;
	if (dataLen < (2L*sizeof(long)))	{
		SETUP_PLUGIN_INFO_STR("\p WriteFile: JRI_GetByteArrayLength on data too small", NULL);
		return;
		}
	dataPtr = JRI_GetByteArrayElements(env,data);
	if (!dataPtr)	{
		SETUP_PLUGIN_INFO_STR("\p WriteFile: JRI_GetByteArrayElements on data returned NULL", NULL);
		return;
		}
	
	dataForkSize = *(long *)dataPtr;
	resForkSize = *(long *)(dataPtr+sizeof(long));
	if ((dataForkSize + resForkSize + (2*sizeof(long))) != dataLen)	{
		SETUP_PLUGIN_INFO_STR("\p WriteFile: data/res fork lengths are invalid", NULL);
		return;
		}

	CtoPstr((void *)theFile);
	err=FSMakeFSSpec(0,0L,(void *)theFile,&theFSSpec);
	PtoCstr((void *)theFile);

	err=WriteFile(&theFSSpec,(dataPtr + (2L*sizeof(long))), dataForkSize, (dataPtr + (2L*sizeof(long)) + dataForkSize), resForkSize);
	if (err)	{
		SETUP_PLUGIN_ERROR("\p WriteFile error: ;g", err);
		}
}



/*
	SetNameValuePair: 

	1) read in [file] contents
	2) if [section] exists, find [name=] and set to [value]; if [name=] not found, add [name=value]
	3) if [section] doesn't exist, add it and [name=]

	Note: file must be of type 'TEXT'
*/

extern JRI_PUBLIC_API(void)
native_SetupPlugin_SECURE_0005fSetNameValuePair(JRIEnv* env,
		struct SetupPlugin* self,
		struct java_lang_String *file,
		struct java_lang_String *section,
		struct java_lang_String *name,
		struct java_lang_String *value)
{
		Boolean			sectionFound=FALSE;
		NPP			instance;
		PluginInstance		*This;
		FInfo			fndrInfo;
		OSErr			err=noErr;
		java_lang_String	*sub=NULL;
		char			*theFile,*theName,*theSection,*theValue=NULL;
		char			buffer[512];
		short			len,refNum;
		long			startSectionOffset,sectionEndOffset,startOffset,endOffset,theCount,theOffset,logEOF;

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_SetNameValuePair entered");

	if (file==NULL)		return;									// these parameters are required
	if (section==NULL)	return;
	if (name==NULL)		return;

	if ((theFile = (char *)javaLangString2Cstr( env, file )) == NULL)		return;		// and must convert to C strings
	if ((theSection = (char *)javaLangString2Cstr( env, section )) == NULL)	return;
	if ((theName = (char *)javaLangString2Cstr( env, name )) == NULL)		return;

	if (!(*theFile))	return;									// and must not be empty strings
	if (!(*theSection))	return;
	if (!(*theName))	return;

	sprintf(buffer,"%s",theFile);
	CtoPstr(buffer);

	instance = (NPP) (netscape_plugin_Plugin_getPeer(env,(struct netscape_plugin_Plugin *)self));
	This = (PluginInstance*)instance->pdata;
	if (This==NULL)	return;

	if (This->data)	{
		DisposeHandle(This->data);
		This->data=NULL;
		}

	if (This->data==NULL)	{
		(void)create(theFile, 0, SIMPLETEXT_SIGNATURE, TEXTFILE_TYPE);					// make sure file exists
		if (err=getfinfo(theFile,0, &fndrInfo))	return;							// only make changes to 'TEXT' files
		if (fndrInfo.fdType != TEXTFILE_TYPE)		return;

		useCursor(watchCursor);
		
		// read in file contents

		if (err=HOpenDF(0, 0, (unsigned char *)buffer, fsRdWrPerm, &refNum))	return;
		if (!(err=GetEOF(refNum,&logEOF)))	{
			if (!(err=SetFPos(refNum,fsFromStart,0L)))	{
				if (This->data = NewHandle(logEOF))	{
					HNoPurge(This->data);
					HLock(This->data);
					theCount=logEOF;
					if (!(err=FSRead(refNum,&theCount,*(This->data))))	{
						if (theCount!=logEOF)	err=readErr;
						}
					HUnlock(This->data);

					// convert CRLF runs to CRs

					theOffset=0L;
					while ((theOffset=Munger(This->data,theOffset,"\r\n",2L,"\r",1L))>=0L)	{
						++theOffset;			// replace 2 chars (CRLF) with 1 (CR) so add (2-1) to theOffset
						}

					// convert LFs to CRs

					theOffset=0L;
					while ((theOffset=Munger(This->data,theOffset,"\n",1L,"\r",1L))>=0L)	{
						++theOffset;
						}
					}
				}
			}
		}

	if (!err && (This->data))	{

		// look for start of [section]

		startSectionOffset=0L;
		sprintf(buffer,"[%s]",theSection);			
		len=strlen(buffer);
		if ((startSectionOffset=Munger(This->data,startSectionOffset,buffer,len,NULL,0L))>=0L)	{
			startSectionOffset+=(long)len;
			if ((sectionEndOffset=Munger(This->data,startSectionOffset,"\r[",2L,NULL,0L))>=0L)	{
				++sectionEndOffset;					// include first CR in section
				}
			else	{
				sectionEndOffset=GetHandleSize(This->data);
				}
			}
		else	{
			PtrAndHand("\r\r",This->data,2L);				// if section not found, append [section]\r
			PtrAndHand(buffer,This->data,(long)len);
			startSectionOffset = sectionEndOffset = GetHandleSize(This->data);
			}

		// look for [name=] in this section (must be inside of section)

		sprintf(buffer,"\r%s=",theName);
		len=strlen(buffer);

		if (value!=NULL)	{
			theValue = (char *)javaLangString2Cstr( env, value );
			}
		
		startOffset=startSectionOffset;
		if ((startOffset=Munger(This->data,startOffset,buffer,(long)len,NULL,0L))>=0L)	{
			if (startOffset<sectionEndOffset)	{
				startOffset+=(long)len;
				endOffset=startOffset;
				if ((endOffset=Munger(This->data,startOffset,"\r",1L,NULL,0L))<0L)	{	// find end of line
					endOffset=GetHandleSize(This->data);
					}
				sprintf(buffer,"%s",(theValue!=NULL) ? theValue:"");				// replace line
				Munger(This->data,startOffset,NULL,endOffset-startOffset,buffer,(long)strlen(buffer));
				}
			else if (startOffset == sectionEndOffset)	{
				sprintf(buffer,"%s",(theValue!=NULL) ? theValue:"");				// insert line
				Munger(This->data,startOffset,NULL,0L,buffer,(long)strlen(buffer));
				}
			else	{										// insert line
				sprintf(buffer,"\r%s=%s\r",theName,(theValue!=NULL) ? theValue:"");
				Munger(This->data,startSectionOffset,NULL,0L,buffer,(long)strlen(buffer));
				}
			}
		else	{							// if [name=] not found, insert [name=value]\r
			sprintf(buffer,"\r%s=%s",theName,(theValue!=NULL) ? theValue:"");
			Munger(This->data,startSectionOffset,NULL,0L,buffer,(long)strlen(buffer));
			}

		// update file contents
		
		if (!(err = SetFPos(refNum, fsFromStart, 0L)))	{			// move to file beginning
			err = SetEOF(refNum, 0L);					// empty out file
			}
		if (!err)	{
			HLock(This->data);
			theCount = GetHandleSize(This->data);
			err=FSWrite(refNum, &theCount, *(This->data));			// write data
			if (!err && (theCount != GetHandleSize(This->data)))	{	// ensure ALL data was written
				err=writErr;
				}
			}
		}

	(void)FSClose(refNum);
	if (This->data)	{
		DisposeHandle(This->data);
		This->data=NULL;
		}

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_SetNameValuePair exiting");
}



/*
	SaveTextToFile: save a long text string to a file
	
	Note: The file is saved as a SimpleText file, and a 'styl' resource
		is added to force a monospaced font: "Monaco 9"
*/

extern JRI_PUBLIC_API(jbool)
native_SetupPlugin_SECURE_0005fSaveTextToFile(JRIEnv* env, 
				struct SetupPlugin* self,
				struct java_lang_String *suggestedFilename,
				struct java_lang_String *data,
				jbool promptFlag)
{
		FSSpec			navFSSpec;
		OSErr			err=noErr;
		Point			where={0,0};
		ProcessInfoRec		netscapeProcInfo; 
		ProcessSerialNumber	netscapePSN;
		StandardFileReply	theReply;
		Str255			defaultName={0};
		short			saveResFile,refNum;
		long			count,logEOF;
		_styleDataH		styleH;
		char			*theData;
		jbool			retVal = false;

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_SaveTextToFile entered");

	if (!data)	return(retVal);

	if (suggestedFilename)	{
		const char *theData = javaLangString2Cstr( env, suggestedFilename );
		if (theData)	{
			strcpy((char *)defaultName,theData);			// XXX should check string length
			CtoPstr((char *)defaultName);
			}
		}

	if (promptFlag)	{

		// if using Standard File, set initial location to be folder containing the running process (Netscape)

		netscapeProcInfo.processInfoLength = sizeof(ProcessInfoRec);
		netscapeProcInfo.processName = NULL;
		netscapeProcInfo.processLocation = NULL;
		netscapeProcInfo.processAppSpec = &navFSSpec;
		if (isAppRunning(NETSCAPE_SIGNATURE, &netscapePSN, &netscapeProcInfo) == true)	{
			LMSetSFSaveDisk(-navFSSpec.vRefNum);			// note: set to negative of vRefNum
			LMSetCurDirStore(navFSSpec.parID);
			}

		InitCursor();
		StandardPutFile("\pSave Account Information:", defaultName, &theReply);
		if (!theReply.sfGood)	{
			err=userCanceledErr;
			}
		}
	else if (defaultName[0])	{
		err=FSMakeFSSpec(0,0L,defaultName,&theReply.sfFile);
		if (err==fnfErr)	err=noErr;
		}
	else	{
		err=paramErr;
		}

	theData = (char *)javaLangString2Cstr( env, data );
	if ((!err) && theData)	{
//		if (theReply.sfReplacing == true)	{
			/* err= */ (void)FSpDelete(&theReply.sfFile);
//			}
		if (!err)	{
			(void)FSpCreate(&theReply.sfFile, SIMPLETEXT_SIGNATURE, (promptFlag) ? SIMPLETEXT_TYPE:TEXTFILE_TYPE, theReply.sfScript);
			FSpCreateResFile(&theReply.sfFile, SIMPLETEXT_SIGNATURE, (promptFlag) ? SIMPLETEXT_TYPE:TEXTFILE_TYPE, theReply.sfScript);

			if (!(err=FSpOpenDF(&theReply.sfFile, fsRdWrPerm, &refNum)))	{
				if (!(err=GetEOF(refNum, &logEOF)))	{
					if (logEOF>0L)	{
						err=SetEOF(refNum,0L);
						}
					}
				if (!err)	{
					err=SetFPos(refNum, fsFromStart,0L);
					}
				if (!err)	{
					count = strlen(theData);
					err=FSWrite(refNum, &count, theData);
					}
				FSClose(refNum);

				retVal=true;
				}
			if (!err)	{
				if (styleH = (_styleDataH)NewHandleClear((long)sizeof(_styleData)))	{
					HLock((Handle)styleH);

					(**styleH).numRuns=1;
					(**styleH).lineHeight=11;
					(**styleH).fontAscent=9;
					(**styleH).fontFamily = kFontIDMonaco;
					(**styleH).fontSize = 9;

					saveResFile = CurResFile();
					if ((refNum=FSpOpenResFile(&theReply.sfFile, fsRdWrPerm)) != kResFileNotOpened)	{
						UseResFile(refNum);
						AddResource((Handle)styleH, 'styl', 128, "\p");
						WriteResource((Handle)styleH);
						CloseResFile(refNum);
						}
					else	{
						DisposeHandle((Handle)styleH);
						}
					UseResFile(saveResFile);
					}
				}
			}
		}

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_SaveTextToFile exiting");

	return(retVal);
}



/*
	GetFolderContents:	returns a string array, each element being the name
				of a visible 'TEXT' file matching a given suffix in
				the specified directory
*/
#if 0
extern JRI_PUBLIC_API(jstringArray)
native_SetupPlugin_SECURE_0005fGetFolderContents(JRIEnv* env,
		struct SetupPlugin* self,
		struct java_lang_String *path,
		struct java_lang_String *suffix)
{
		CInfoPBRec		cBlock;
//		FInfo			fndrInfo;
		FSSpec			theFSSpec;
		JRIGlobalRef		globalRef=NULL;
		OSErr			err=noErr;
		Str255			theName={0};
		_fileArrayPtr		fileArray=NULL,*nextFile=&fileArray, tempFilePtr;
		short			ioFDirIndex,loop,numFilesFound=0,vRefNum;
		long			theDirID=0L;
		java_lang_String	*sub=NULL;
		const char		*theFolder,*theSuffix=NULL;
		void			*theArray=NULL;

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_GetFolderContents entered");

	theFolder = javaLangString2Cstr( env, path );
	if (!theFolder)		return(NULL);
	if (!theFolder[0])	return(NULL);

	strcpy((char *)theName,theFolder);
	CtoPstr((char *)theName);
	
	if (suffix)	{
		theSuffix = javaLangString2Cstr( env, suffix );
		}

	// translate pathname to vRefNum/dirID

	if (err=FSMakeFSSpec(0,0L,theName,&theFSSpec))	{
		SETUP_PLUGIN_ERROR("\p GetFolderContents: FSMakeFSSpec error;g", err);
		return(NULL);
		}

	bzero((char *)&cBlock,sizeof(cBlock));
	cBlock.hFileInfo.ioCompletion=NULL;
	cBlock.hFileInfo.ioNamePtr=(StringPtr)theFSSpec.name;
	cBlock.hFileInfo.ioVRefNum=theFSSpec.vRefNum;
	cBlock.hFileInfo.ioDirID=theFSSpec.parID;
	cBlock.hFileInfo.ioFDirIndex=0;
	if (err=PBGetCatInfoSync(&cBlock))	{
		SETUP_PLUGIN_ERROR("\p GetFolderContents: PBGetCatInfoSync error;g", err);
		return(NULL);
		}
	
	// XXX should verify that its a directory (i.e. not a file)

	vRefNum=cBlock.hFileInfo.ioVRefNum;
	theDirID=cBlock.hFileInfo.ioDirID;

	// enumerate directory contents;

	ioFDirIndex=1;
	bzero((char *)&cBlock,sizeof(cBlock));
	cBlock.hFileInfo.ioCompletion=NULL;
	cBlock.hFileInfo.ioNamePtr=(StringPtr)theName;
	cBlock.hFileInfo.ioVRefNum=vRefNum;
	while(!err)	{
		cBlock.hFileInfo.ioDirID=theDirID;
		cBlock.hFileInfo.ioFDirIndex=ioFDirIndex++;
		if (err=PBGetCatInfoSync(&cBlock))	break;

/*
		// Make sure that the file is a 'TEXT' file and (XXX) visible

		if (FSMakeFSSpec(vRefNum,theDirID,theName,&theFSSpec))	continue;
		if (FSpGetFInfo(&theFSSpec,&fndrInfo))	continue;
		if (fndrInfo.fdType != TEXTFILE_TYPE)	continue;
*/

		// if a suffix has been specified, check for a match

		PtoCstr(theName);
		if (theSuffix)	{
			if (strlen(theSuffix) > strlen((char *)theName))	continue;
			if (!equalstring((char *)theSuffix, (char *)&theName[strlen((char *)theName)-strlen((char *)theSuffix)], false,false))	continue;
			}

		if (!(*nextFile=(_fileArrayPtr)NPN_MemAlloc(sizeof(_fileArray))))	{
			err=-1;
			break;
			}
		bzero((char *)*nextFile,sizeof(fileArray));
		strcpy((char *)(*nextFile)->name,(char *)theName);
		nextFile=&(*nextFile)->next;
		++numFilesFound;
		}
	if (err==fnfErr)	err=noErr;					// if fnfErr, we're done

	// create string array of filenames if no error

	if (!err)	{
		theArray = JRI_NewObjectArray( env, numFilesFound, class_java_lang_String(env), NULL );
		if (theArray)	{
			globalRef=JRI_NewGlobalRef(env, theArray);			// locks the array
			nextFile=&fileArray;
			for (loop=0; loop<numFilesFound,(*nextFile != NULL); loop++,nextFile=&(*nextFile)->next)	{
//				sub=JRI_NewStringUTF(env,(char *)(*nextFile)->name,strlen((char *)(*nextFile)->name));
				sub=cStr2javaLangString(env,(char *)(*nextFile)->name,strlen((char *)(*nextFile)->name));
				JRI_SetObjectArrayElement(env, theArray, loop, sub);
				}
			if (globalRef)	JRI_DisposeGlobalRef(env, globalRef);
			}
		}

	// free filename array using NPN_MemFree
		
	while (fileArray)	{
		tempFilePtr=fileArray->next;
		NPN_MemFree((Ptr)fileArray);
		fileArray=tempFilePtr;
		}

	return((struct java_lang_String *)theArray);

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_GetFolderContents exiting");
}

#endif


/*
	cleanupStartupFolder: when Account Setup starts, it will remove any aliases that
				were created by Account Setup before requiring a reboot
*/

void
cleanupStartupFolder()
{
		OSErr			err;
		short			startupVRefNum;
		long			startupDirID;

SETUP_PLUGIN_TRACE("\p cleanupStartupFolder entered");

	if (!(err=FindFolder(kOnSystemDisk, kStartupFolderType, kDontCreateFolder, &startupVRefNum, &startupDirID)))	{
		(void)HDelete(startupVRefNum, startupDirID, NAV_STARTUP_ALIAS_NAME);
		(void)HDelete(startupVRefNum, startupDirID, ACCOUNT_SETUP_STARTUP_ALIAS_NAME);
		}

SETUP_PLUGIN_TRACE("\p cleanupStartupFolder exiting");
}



/*
	createStartupFolderEntry:	create an alias file (text file owned by Netscape) in the Startup Items folder
*/

OSErr
createStartupFolderEntry(FSSpec *startupFile, StringPtr startupName)
{
		AliasHandle		theAliasH=NULL;
		FInfo			fndrInfo;
		FSSpec			aliasFileSpec;
		Handle			h;
		OSErr			err;
		short			refNum,saveResFile,startupVRefNum;
		long			startupDirID;

SETUP_PLUGIN_TRACE("\p createStartupFolderEntry entered");

	if (!(err=NewAliasMinimal(startupFile, &theAliasH)))	{
		HLock((Handle)theAliasH);
		if (!(err=FindFolder(kOnSystemDisk, kStartupFolderType, kDontCreateFolder, &startupVRefNum, &startupDirID)))	{
			err = FSMakeFSSpec(startupVRefNum, startupDirID, startupName, &aliasFileSpec);
			if (err==fnfErr)	err=noErr;

			if (!err)	{				// create the file & set the Finder's kIsAlias bit for the file in the Startup Items folder
				FSpCreateResFile(&aliasFileSpec, NETSCAPE_SIGNATURE, TEXTFILE_TYPE, smRoman);
				if (!(err=FSpGetFInfo(&aliasFileSpec,&fndrInfo)))	{
					fndrInfo.fdFlags |= kIsAlias;
					err=FSpSetFInfo(&aliasFileSpec,&fndrInfo);
					}
				else	{
					SETUP_PLUGIN_ERROR("\p createStartupFolderEntry: FSpGetFInfo error;g", err);
					}
				
				saveResFile = CurResFile();		// add alias resource
				if (!err && ((refNum=FSpOpenResFile(&aliasFileSpec, fsRdWrPerm)) != kResFileNotOpened))	{
					UseResFile(refNum);

					SetResLoad(FALSE);
					h=Get1Resource(rAliasType,0);	// if it already exists, remove original before adding new alias
					SetResLoad(TRUE);
					if (h)	{
						RemoveResource(h);
						UpdateResFile(refNum);
						}
					AddResource((Handle)theAliasH, rAliasType, 0, "\p");
					WriteResource((Handle)theAliasH);
					CloseResFile(refNum);
					theAliasH=NULL;
					}
				else	{
					SETUP_PLUGIN_ERROR("\p createStartupFolderEntry: FSpOpenResFile error;g", err);
					}
				UseResFile(saveResFile);
				
				if (err)	{			// if any error occurred, delete any reference to startup file
					(void)FSpDelete(&aliasFileSpec);
					}
				}
			else	{
				SETUP_PLUGIN_ERROR("\p createStartupFolderEntry: FSMakeFSSpec error;g", err);
				}
			}
		else	{
			SETUP_PLUGIN_ERROR("\p createStartupFolderEntry: FindFolder (Startup Items folder) error;g", err);
			}
		if (theAliasH)	{
			DisposeHandle((Handle)theAliasH);
			}
		}
	else	{
		SETUP_PLUGIN_ERROR("\p createStartupFolderEntry: NewAliasMinimal error;g", err);
		}

SETUP_PLUGIN_TRACE("\p createStartupFolderEntry exiting");

	return(err);
}
