/* $Id: win-misc.cpp,v 1.1 1998/09/25 18:01:42 ramiro%netscape.com Exp $
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
 * Copyright (C) 1998 Netscape Communications Corporation.  Portions
 * created by Warwick Allison, Kalle Dalheimer, Eirik Eng, Matthias
 * Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav Tvete are
 * Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
 */


#include <xp.h>
#include <io.h>
#include <direct.h>
#include <qstring.h>


extern "C"
char *
WH_FileName (const char *name, XP_FileType)
{
    return name ? XP_STRDUP(name) : 0;
}


extern "C"
char *WH_FilePlatformName(const char *name)
{
    return name ? XP_STRDUP(name) : 0;
}


extern "C"
char *
WH_TempName(XP_FileType, const char * prefix)
{
    return tmpnam((char*)prefix);
}


//
// Return 0 on success, -1 on failure.  Silly unix weenies
//
extern "C"
int 
XP_FileRemove(const char * name, XP_FileType)
{
    return remove( name );
}

extern "C"
XP_File 
XP_FileOpen(const char * name, XP_FileType type, const XP_FilePerm perm)
{
    XP_File fp;
    char *filename = WH_FileName(name, type);
    if(!filename || !(*filename))
	return 0;
    fp = fopen(filename, (char *) perm);
    XP_FREE(filename);
    return(fp);
}


extern "C"
int 
XP_Stat(const char * name, XP_StatStruct * info, XP_FileType type)
{
    char * filename = WH_FileName(name, type);
    int res;

    if(!info || !filename)
        return(-1);
    
    // Strip off final slash on directory names 
    // BUT we will need to make sure we have c:\ NOT c:   
    int len = XP_STRLEN(filename) - 1;
    if(len > 1 && filename[len] == '\\' && filename[len -1] != ':'){
	filename[len] = '\0';
    }

    //  Normal file or directory.
    res = _stat(filename, info);
    XP_FREE(filename);
    return(res);
}


extern "C"
char*
XP_TempDirName(void)
{
    return XP_STRDUP("C:\\TEMP");
}


PUBLIC XP_Dir 
XP_OpenDir(const char * name, XP_FileType type)
{
    XP_Dir dir;
    char * filename = WH_FileName(name, type);
    QString foo;
									  
    if(!filename)
        return NULL;
    dir = (XP_Dir) new DIR;

    // For directory names we need \*.* at the end, if and only if there is an actual name specified.
    foo += filename;
    foo += '\\';
    foo += "*.*";

    dir->directoryPtr = FindFirstFile((const char *) foo, &(dir->data));
    XP_FREE(filename);
    if(dir->directoryPtr == INVALID_HANDLE_VALUE) {
        delete dir;
        return(NULL);
    } else {
        return(dir);
    }
}


extern "C"
void 
XP_CloseDir(XP_Dir dir)
{
    if (dir)
	{
	    if (dir->directoryPtr)
		FindClose (dir->directoryPtr);
	    delete dir;
	}
} 


extern "C"
XP_DirEntryStruct * 
XP_ReadDir(XP_Dir dir)
{                                         
    static XP_DirEntryStruct dirEntry;

    if(dir && dir->directoryPtr) {
        XP_STRCPY(dirEntry.d_name, dir->data.cFileName);
        if(FindNextFile(dir->directoryPtr, &(dir->data)) == FALSE) {
            FindClose(dir->directoryPtr);
            dir->directoryPtr = NULL;
        }
        return(&dirEntry);
    } else {
        return(NULL);
    }
}


extern "C"
int
XP_MakeDirectory(const char* name, XP_FileType type)	
{
    char * pName = WH_FileName(name, type);
    if(pName) {
	int dir;
	dir = mkdir(pName);
	XP_FREE(pName);
	return(dir);
    }
    else
	return(-1);
}


extern "C"
int
XP_RemoveDirectory (const char *name, XP_FileType type)
{
    int ret = -1;
    char *pName = WH_FileName (name, type);
    if (pName)	{
	ret = rmdir (pName);
	XP_FREE(pName);
    }
    return ret;
}


char * XP_PlatformFileToURL (const char *name)
{
    char *prefix = "file://";
    char *s = (char*)XP_ALLOC (XP_STRLEN(name) + XP_STRLEN(prefix) + 1);
    if (s)
	{
	    XP_STRCPY (s, "file://");
	    XP_STRCAT (s, name);
	}
    return s;
}

int XP_FileRename(const char * from, XP_FileType fromtype,
				  const char * to, XP_FileType totype)
{
    char * fromName = WH_FileName(from, fromtype);
    char * toName = WH_FileName(to, totype);
    int res = 0;
    if (fromName && toName)
	res = rename(fromName, toName);
    else
	res = -1;
    if (fromName)
	XP_FREE(fromName);
    if (toName)
	XP_FREE(toName);
    return res;
}
