/*
**	Apple Macintosh Developer Technical Support
**
**	Routines for dealing with full pathnames... if you really must.
**
**	by Jim Luther, Apple Developer Technical Support Emeritus
**
**	File:		FullPath.c
**
**	Copyright © 1995-1996 Apple Computer, Inc.
**	All rights reserved.
**
**	You may incorporate this sample code into your applications without
**	restriction, though the sample code has been provided "AS IS" and the
**	responsibility for its operation is 100% yours.  However, what you are
**	not permitted to do is to redistribute the source as "DSC Sample Code"
**	after having made changes. If you're going to re-distribute the source,
**	we require that you make it clear in the source that the code was
**	descended from Apple Sample Code, but that you've made changes.
*/

/* 
 * This code, which was decended from Apple Sample Code, has been modified by 
 * Netscape.
 */
 
#include <Types.h>
#include <Errors.h>
#include <Memory.h>
#include <Files.h>
#include <TextUtils.h>
#include <Aliases.h>

#define	__COMPILINGMOREFILES

#include "FSpCompat.h"
#include "FullPath.h"

/*
	IMPORTANT NOTE:
	
	The use of full pathnames is strongly discouraged. Full pathnames are
	particularly unreliable as a means of identifying files, directories
	or volumes within your application, for two primary reasons:
	
	• 	The user can change the name of any element in the path at virtually
		any time.
	•	Volume names on the Macintosh are *not* unique. Multiple
		mounted volumes can have the same name. For this reason, the use of
		a full pathname to identify a specific volume may not produce the
		results you expect. If more than one volume has the same name and
		a full pathname is used, the File Manager currently uses the first
		mounted volume it finds with a matching name in the volume queue.
	
	In general, you should use a file’s name, parent directory ID, and
	volume reference number to identify a file you want to open, delete,
	or otherwise manipulate.
	
	If you need to remember the location of a particular file across
	subsequent system boots, use the Alias Manager to create an alias record
	describing the file. If the Alias Manager is not available, you can save
	the file’s name, its parent directory ID, and the name of the volume on
	which it’s located. Although none of these methods is foolproof, they are
	much more reliable than using full pathnames to identify files.
	
	Nonetheless, it is sometimes useful to display a file’s full pathname to
	the user. For example, a backup utility might display a list of full
	pathnames of files as it copies them onto the backup medium. Or, a
	utility might want to display a dialog box showing the full pathname of
	a file when it needs the user’s confirmation to delete the file. No
	matter how unreliable full pathnames may be from a file-specification
	viewpoint, users understand them more readily than volume reference
	numbers or directory IDs. (Hint: Use the TruncString function from
	TextUtils.h with truncMiddle as the truncWhere argument to shorten
	full pathnames to a displayable length.)
	
	The following technique for constructing the full pathname of a file is
	intended for display purposes only. Applications that depend on any
	particular structure of a full pathname are likely to fail on alternate
	foreign file systems or under future system software versions.
*/

/*****************************************************************************/

pascal	OSErr	GetFullPath(short vRefNum,
							long dirID,
							StringPtr name,
							short *fullPathLength,
							Handle *fullPath)
{
	OSErr		result;
	FSSpec		spec;
	
	*fullPathLength = 0;
	*fullPath = NULL;
	
	result = FSMakeFSSpecCompat(vRefNum, dirID, name, &spec);
	if ( result == noErr )
	{
		result = FSpGetFullPath(&spec, fullPathLength, fullPath);
	}
	
	return ( result );
}

/*****************************************************************************/

pascal	OSErr	FSpGetFullPath(const FSSpec *spec,
							   short *fullPathLength,
							   Handle *fullPath)
{
	OSErr		result;
	FSSpec		tempSpec;
	CInfoPBRec	pb;
	
	*fullPathLength = 0;
	*fullPath = NULL;
	
	/* Make a copy of the input FSSpec that can be modified */
	BlockMoveData(spec, &tempSpec, sizeof(FSSpec));
	
	if ( tempSpec.parID == fsRtParID )
	{
		/* The object is a volume */
		
		/* Add a colon to make it a full pathname */
		++tempSpec.name[0];
		tempSpec.name[tempSpec.name[0]] = ':';
		
		/* We're done */
		result = PtrToHand(&tempSpec.name[1], fullPath, tempSpec.name[0]);
	}
	else
	{
		/* The object isn't a volume */
		
		/* Is the object a file or a directory? */
		pb.dirInfo.ioNamePtr = tempSpec.name;
		pb.dirInfo.ioVRefNum = tempSpec.vRefNum;
		pb.dirInfo.ioDrDirID = tempSpec.parID;
		pb.dirInfo.ioFDirIndex = 0;
		result = PBGetCatInfoSync(&pb);
		if ( result == noErr )
		{
			/* if the object is a directory, append a colon so full pathname ends with colon */
			if ( (pb.hFileInfo.ioFlAttrib & ioDirMask) != 0 )
			{
				++tempSpec.name[0];
				tempSpec.name[tempSpec.name[0]] = ':';
			}
			
			/* Put the object name in first */
			result = PtrToHand(&tempSpec.name[1], fullPath, tempSpec.name[0]);
			if ( result == noErr )
			{
				/* Get the ancestor directory names */
				pb.dirInfo.ioNamePtr = tempSpec.name;
				pb.dirInfo.ioVRefNum = tempSpec.vRefNum;
				pb.dirInfo.ioDrParID = tempSpec.parID;
				do	/* loop until we have an error or find the root directory */
				{
					pb.dirInfo.ioFDirIndex = -1;
					pb.dirInfo.ioDrDirID = pb.dirInfo.ioDrParID;
					result = PBGetCatInfoSync(&pb);
					if ( result == noErr )
					{
						/* Append colon to directory name */
						++tempSpec.name[0];
						tempSpec.name[tempSpec.name[0]] = ':';
						
						/* Add directory name to beginning of fullPath */
						(void) Munger(*fullPath, 0, NULL, 0, &tempSpec.name[1], tempSpec.name[0]);
						result = MemError();
					}
				} while ( (result == noErr) && (pb.dirInfo.ioDrDirID != fsRtDirID) );
			}
		}
	}
	if ( result == noErr )
	{
		/* Return the length */
		*fullPathLength = GetHandleSize(*fullPath);
	}
	else
	{
		/* Dispose of the handle and return NULL and zero length */
		if ( *fullPath != NULL )
		{
			DisposeHandle(*fullPath);
		}
		*fullPath = NULL;
		*fullPathLength = 0;
	}
	
	return ( result );
}

/*****************************************************************************/

pascal OSErr FSpLocationFromFullPath(short fullPathLength,
									 const void *fullPath,
									 FSSpec *spec)
{
	AliasHandle	alias;
	OSErr		result;
	Boolean		wasChanged;
	Str32		nullString;
	
	/* Create a minimal alias from the full pathname */
	nullString[0] = 0;	/* null string to indicate no zone or server name */
	result = NewAliasMinimalFromFullPath(fullPathLength, fullPath, nullString, nullString, &alias);
	if ( result == noErr )
	{
		/* Let the Alias Manager resolve the alias. */
		result = ResolveAlias(NULL, alias, spec, &wasChanged);
		
		DisposeHandle((Handle)alias);	/* Free up memory used */
	}
	return ( result );
}

/*****************************************************************************/

pascal OSErr LocationFromFullPath(short fullPathLength,
								  const void *fullPath,
								  short *vRefNum,
								  long *parID,
								  Str31 name)
{
	OSErr	result;
	FSSpec	spec;
	
	result = FSpLocationFromFullPath(fullPathLength, fullPath, &spec);
	if ( result == noErr )
	{
		*vRefNum = spec.vRefNum;
		*parID = spec.parID;
		BlockMoveData(&spec.name[0], &name[0], spec.name[0] + 1);
	}
	return ( result );
}

/*****************************************************************************/

