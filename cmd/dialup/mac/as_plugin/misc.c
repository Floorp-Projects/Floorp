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


extern void*	theArray;			// array of data from Reg Server
extern void*	lckFileData;
extern void*	animationDat;
extern void*	animationRes;
extern long		lckFileDataLen;
extern long		animationDatLen;
extern long		animationResLen;

Handle			milanData = NULL;
Boolean			cursorDirty = FALSE;

void useCursor( short cursNum )
{
	CursHandle		theCursH = NULL;
	static	long	theTick = 0L;

	if ( cursNum > 0 && ( theCursH = GetCursor( cursNum ) ) )
	{
		HLock( (Handle)theCursH );
		SetCursor( *theCursH );
		HUnlock( (Handle)theCursH );
		cursorDirty = TRUE;
		theTick = TickCount()+60L;
	}
	else if ( TickCount() > theTick )
	{
		cursorDirty = FALSE;
		InitCursor();
	}
}



/*
	javaLangString2Cstr: convert from java_lang_String to a C string

	Note: was using JRI_GetStringUTFChars(), which only worked for UTF7 characters  -  "ISO-LATIN-1"
*/
const char* javaLangString2Cstr( JRIEnv* env, struct java_lang_String* string )
{
	const char*		c = NULL;

	if ( string )
	{
		c = JRI_GetStringPlatformChars( env, string, NULL, 0 );
		// have to do this so JRI continues to work
		JRI_ExceptionClear( env );
	}
	return c;
}



/*
	cStr2javaLangString: convert from C string to a java_lang_String
*/
java_lang_String* cStr2javaLangString( JRIEnv* env, char* string, long len )
{
	java_lang_String*	c = NULL;

	if ( string )
	{
		c = JRI_NewStringPlatform( env, string, len, NULL, 0L );
		if ( JRI_ExceptionOccurred( env ) )	
		{
//			JRI_ExceptionDescribe(env);
			// have to do this so JRI continues to work
			JRI_ExceptionClear( env );
		}
	}
	return c;
}



/*
	bzero:	zero out an array
*/
void bzero( char* p,long num )
{
	while ( num-- )
		*p++=0;
}



/*
	Milan:	QA support for generating Milan data
*/

extern JRI_PUBLIC_API(jbool)
native_SetupPlugin_SECURE_0005fMilan(JRIEnv* env,struct SetupPlugin* self,struct java_lang_String *name,struct java_lang_String *value,jbool pushPullFlag,jbool extendedLengthFlag)
{
		Boolean			extendedDataFlag;
		FSSpec			profileSpec;
		OSErr			err=noErr;
		SFTypeList		typeList;
		StandardFileReply	theReply;
		jbool			retVal=FALSE;
		const char		*nameStr;
		const char		*valueStr;
		short			refNum;
		long			numItems,theCount,logEOF,x; //y;
		unsigned short		nameLen,valueLen;
		unsigned long		nameLenLong,valueLenLong;
		Str255			fileString;
		Point			centerPoint = {-1,-1};
		java_lang_String*	tempStringObj;
		void *	refToArray;	//WARNING - we need to reference our array with a stack based java object
										//so that it does not get garbage collected!  
	
SETUP_PLUGIN_TRACE("\p native_SetupPlugin_Milan entered");

	if (pushPullFlag == FALSE)	{
	
		// saving Milan data
	
		if (name && value)	{
			nameStr = javaLangString2Cstr(env,name);
			valueStr = javaLangString2Cstr(env,value);

			if (nameStr && valueStr)	{
				if (extendedLengthFlag==TRUE)	{
					nameLenLong=(unsigned long)strlen(nameStr);
					valueLenLong=(unsigned long)strlen(valueStr);
					}
				else	{
					nameLen=(unsigned short)strlen(nameStr);
					valueLen=(unsigned short)strlen(valueStr);
					}
				
				if (milanData == NULL)	{
					if (!(milanData=NewHandle(0)))	return(retVal);
					HNoPurge(milanData);
					HUnlock(milanData);
					}
				
				// write out name length
				
				if (extendedLengthFlag==TRUE)	{
					if (err=PtrAndHand(&nameLenLong,milanData,(long)sizeof(nameLenLong)))	{
						SETUP_PLUGIN_ERROR("\p Milan nameLenLong PtrAndHand error;g", err);
						}
					}
				else	{
					if (err=PtrAndHand(&nameLen,milanData,(long)sizeof(nameLen)))	{
						SETUP_PLUGIN_ERROR("\p Milan nameLen PtrAndHand error;g", err);
						}
					}

				// write out name

				if (!err)	{
					if (extendedLengthFlag==TRUE)	{
						if (err=PtrAndHand(nameStr,milanData,nameLenLong))	{
							SETUP_PLUGIN_ERROR("\p Milan nameStr PtrAndHand error;g", err);
							}
						}
					else if (err=PtrAndHand(nameStr,milanData,(long)nameLen))	{
						SETUP_PLUGIN_ERROR("\p Milan nameStr PtrAndHand error;g", err);
						}
					}

				// write out value length

				if (!err)	{
					if (extendedLengthFlag==TRUE)	{
						if (err=PtrAndHand(&valueLenLong,milanData,(long)sizeof(valueLenLong)))	{
							SETUP_PLUGIN_ERROR("\p Milan valueLenLong PtrAndHand error;g", err);
							}
						}
					else if (err=PtrAndHand(&valueLen,milanData,(long)sizeof(valueLen)))	{
						SETUP_PLUGIN_ERROR("\p Milan valueLen PtrAndHand error;g", err);
						}
					}

				// write out value

				if (!err)	{
					if (extendedLengthFlag==TRUE)	{
						if (err=PtrAndHand(valueStr,milanData,valueLenLong))	{
							SETUP_PLUGIN_ERROR("\p Milan valueStr PtrAndHand error;g", err);
							}
						}
					else if (err=PtrAndHand(valueStr,milanData,(long)valueLen))	{
						SETUP_PLUGIN_ERROR("\p Milan valueStr PtrAndHand error;g", err);
						}
					}

				// if any errors, flush any milan data
				
				if (err)	{
					if (milanData)	DisposeHandle(milanData);
					milanData=NULL;
					}
				}
			}
		else if (name==NULL && value==NULL)	{
			if (milanData != NULL)	{

				// save Milan Data

				StandardPutFile("\pSave Milan Data:", "\pDATA.MLN", &theReply);
				if (theReply.sfGood)	{
					(void)FSpCreate(&theReply.sfFile, UNKNOWN_SIGNATURE, (extendedLengthFlag==TRUE) ? MILAN_TYPE_EXT:MILAN_TYPE, theReply.sfScript);
					FSpCreateResFile(&theReply.sfFile, UNKNOWN_SIGNATURE, (extendedLengthFlag==TRUE) ? MILAN_TYPE_EXT:MILAN_TYPE, theReply.sfScript);
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
							theCount = GetHandleSize(milanData);
							HLock(milanData);
							err=FSWrite(refNum, &theCount, *milanData);
							}
						FSClose(refNum);
						if (!err)	retVal=TRUE;
						}
					}
				DisposeHandle(milanData);
				milanData=NULL;
				}
			}
		}
	else	{
	
		// retrieving Milan data

		if (milanData)	DisposeHandle(milanData);
		milanData=NULL;
		

		typeList[0] = MILAN_TYPE;
		typeList[1] = MILAN_TYPE_EXT;
		GetIndString(fileString, CUSTOMGETFILE_RESID, CHOOSEMILAN_STRINGID);
		ParamText(fileString,"\p", "\p", "\p");
		CustomGetFile(NULL, 2, typeList, &theReply, CUSTOMGETFILE_RESID, centerPoint, nil, nil, nil, nil, nil);							// call StandardFile, tell it to list milan files

		//StandardGetFile(NULL, 2, typeList, &theReply);							// call StandardFile, tell it to list milan files
		if (theReply.sfGood == TRUE)	{

			// open file, read in contents

			if (!(err=FSpOpenDF(&theReply.sfFile, fsRdPerm, &refNum)))	{
				if (!(err=GetEOF(refNum,&logEOF)))	{
					if (!(err=SetFPos(refNum,fsFromStart,0L)))	{
						if (milanData = NewHandle(logEOF))	{
							HNoPurge(milanData);
							HLock(milanData);
							theCount=logEOF;
							if (!(err=FSRead(refNum,&theCount,*(milanData))))	{
								if (theCount!=logEOF)	err=readErr;
								}
							HUnlock(milanData);
							}
						else	{
							err=(err=MemError()) ? err:-1;
							SETUP_PLUGIN_ERROR("\p Milan: NewHandle error;g", err);
							}
						}
					else	{
						SETUP_PLUGIN_ERROR("\p Milan: SetFPos error;g", err);
						}
					}
				else	{
					SETUP_PLUGIN_ERROR("\p Milan: GetEOF error;g", err);
					}
				(void)FSClose(refNum);
				}
			else	{
				SETUP_PLUGIN_ERROR("\p Milan: FSpOpenDF error;g", err);
				}

			// if no error, parse data and save in global for GetRegInfo call later

			if (!err)	{
				extendedDataFlag=(theReply.sfType == MILAN_TYPE_EXT) ? TRUE:FALSE;
				HLock(milanData);

				lckFileData = animationDat = animationRes = NULL;
				lckFileDataLen = animationDatLen = animationResLen = 0L;

				numItems = countRegItems(milanData,extendedDataFlag);
				
				refToArray = JRI_NewObjectArray( env, numItems, class_java_lang_String(env), NULL );
				JRI_NewGlobalRef(env, refToArray);
				theArray = refToArray;
				for (x=0; x<numItems; x++)	{
					tempStringObj = getRegElement(env,milanData,x,extendedDataFlag);
					JRI_SetObjectArrayElement(env, theArray, x, tempStringObj);
					}

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

				retVal=TRUE;
				}
			if (milanData)	{
				DisposeHandle(milanData);
				milanData=NULL;
				}
			}
		}

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_Milan exiting");

	return(retVal);
}
