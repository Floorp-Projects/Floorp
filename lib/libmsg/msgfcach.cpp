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

#include "msg.h"
#include "msgfcach.h"
#include "msgfinfo.h"

extern "C"
{
	extern int MK_OUT_OF_MEMORY;
	extern int MK_UNABLE_TO_OPEN_FILE;
}


//-----------------------------------------------------------------------------
// MSG_FolderCacheElement
//
// MSG_FolderCacheElement is like a light FolderInfo. It holds as much
// folder info stuff as we want to put in the folder cache
//-----------------------------------------------------------------------------

class MSG_FolderCacheElement
{
public:
	MSG_FolderCacheElement ();
	virtual ~MSG_FolderCacheElement ();

	void Init (const char *);

//	XP_Bool IsCacheOf (MSG_FolderInfo *);

	static int CompareWithFolder(const void*, const void*); // asymmetric (for searching)
	static int CompareElements(const void* pElem1, const void* pElem2); // symmetric (for insertion)

	char *m_name;
	char *m_folderSpecificStuff;
};


MSG_FolderCacheElement::MSG_FolderCacheElement ()
{
	m_name = NULL;
	m_folderSpecificStuff = NULL;
}


MSG_FolderCacheElement::~MSG_FolderCacheElement ()
{
	XP_FREEIF(m_name);
	XP_FREEIF(m_folderSpecificStuff);
}


void MSG_FolderCacheElement::Init (const char *buf)
{
	int i;
	for (i = 0; buf[i] != '\t'; i++)
		; // do nothing; just count 'em up

	m_name = (char*) XP_ALLOC(i + 1);

	// Get the minimum stuff out of the line to uniquely identify
	// a folder. We're using the pathname now, hopefully the relative path later.
	XP_STRNCPY_SAFE(m_name, buf, i + 1);
	
	// Pull out the rest of the stuff from the line. Since different
	// folderInfo classes can have different stuff in the file, we'll
	// virtualize reading/writing this in MSG_FolderInfo
	m_folderSpecificStuff = XP_STRDUP(&buf[i] + 1);
}

#if 0
XP_Bool MSG_FolderCacheElement::IsCacheOf (MSG_FolderInfo *folder)
{
	const char *relPath = folder->GetRelativePathName();
	if (relPath && !XP_FILENAMECMP(relPath, m_name))
		return TRUE;
	return FALSE;
}
#endif

int MSG_FolderCacheElement::CompareWithFolder(const void* pElemPtr, const void* pFolderPtr)
{
	MSG_FolderCacheElement* elem = *(MSG_FolderCacheElement**)pElemPtr;
	MSG_FolderInfo* folder = *(MSG_FolderInfo**)pFolderPtr;
	const char *relPath = folder->GetRelativePathName();
	if (!relPath)
	{
		XP_ASSERT(FALSE);
		return -1; // sort folders with no name at the end?
	}
	return XP_FILENAMECMP(elem->m_name, relPath);
}

int MSG_FolderCacheElement::CompareElements(const void* pElem1, const void* pElem2)
{
	MSG_FolderCacheElement* elem1 = *(MSG_FolderCacheElement**)pElem1;
	MSG_FolderCacheElement* elem2 = *(MSG_FolderCacheElement**)pElem2;
	return XP_FILENAMECMP(elem1->m_name, elem2->m_name);
}


//-----------------------------------------------------------------------------
// MSG_FolderCache
//
// Apparently, it's too slow to allow the folder tree to open all of its
// databases at initialization time. So this object is a persistent way 
// to store the same stuff as the DBFolderInfo in just one file so it's
// faster to load
//-----------------------------------------------------------------------------


MSG_FolderCache::MSG_FolderCache ()
	: XPSortedPtrArray(MSG_FolderCacheElement::CompareElements)
{
}


MSG_FolderCache::~MSG_FolderCache ()
{
	Depopulate(); // probably done already, but just make sure
}


int MSG_FolderCache::ReadFromDisk ()
{
	int ret = 0;
	XP_File f = XP_FileOpen ("", xpFolderCache, XP_FILE_READ_BIN);
	if (f)
	{
		const int bufSize = 1024; // big enough?
		char *buf = (char*) XP_ALLOC(bufSize);
		if (buf)
		{
			while (XP_FileReadLine (buf, bufSize, f))
			{
				MSG_FolderCacheElement *elem = new MSG_FolderCacheElement;
				if (elem)
				{
					elem->Init (buf);
					Add(elem);
				}
				else
					ret = MK_OUT_OF_MEMORY;
			}
			XP_FREE(buf);
		}
		else
			ret = MK_OUT_OF_MEMORY;

		XP_FileClose (f);

		// Delete folder cache once we've read it. This way, if we use the DBs, then crash,
		// we won't be trusting a folder cache which contains out-of-date information.
		XP_FileRemove ("", xpFolderCache);

	}
	else
		ret = MK_UNABLE_TO_OPEN_FILE;

	return ret;
}


int MSG_FolderCache::WriteToDisk (MSG_FolderInfo *root) const
{
	XP_File f = 0;
	int32 len = 0, count = 0;

	if (root)
	{
		f = XP_FileOpen ("", xpFolderCache, XP_FILE_TRUNCATE_BIN);
		if (f)
		{
			MSG_FolderIterator iter(root);
			MSG_FolderInfo *folder = NULL;
			count = XP_STRLEN(LINEBREAK);
			while ((folder = iter.Next()) != NULL)
			{
				if ((folder->IsMail() || folder->IsNews()) && folder->IsCachable())
				{
					// Write in the mimimal amount of stuff we need to match a cache line
					// with a folderInfo, then ask the folderInfo to add whatever it wants.
					const char *relPath = folder->GetRelativePathName();
					if (relPath)
					{
						XP_FilePrintf (f, "%s\t", relPath); 
						folder->WriteToCache (f);
						len = XP_FileWrite (LINEBREAK, count, f);
						if (len != count)
						{
							XP_FileClose(f);
							return len;
						}
					}
				}
			}
			XP_FileClose (f);
		}
	}

	return 0;
}


XP_Bool MSG_FolderCache::InitializeFolder (MSG_FolderInfo *folder) const
{
#if 1
	int index = FindIndexUsing(0, folder, MSG_FolderCacheElement::CompareWithFolder);
	if (index < 0)
		return FALSE;
	MSG_FolderCacheElement *elem = GetAt(index);
	folder->ReadFromCache(elem->m_folderSpecificStuff);
	return TRUE;
#else
	MSG_FolderCacheElement *elem;
	for (int i = 0; i < GetSize(); i++)
	{
		elem = GetAt(i);
		if (elem->IsCacheOf (folder))
		{
			folder->ReadFromCache (elem->m_folderSpecificStuff);
			return TRUE;
		}
	}
	return FALSE; 
#endif // 1
}


void MSG_FolderCache::Depopulate ()
{
	MSG_FolderCacheElement *elem;
	for (int i = 0; i < GetSize(); i++)
	{
		elem = GetAt(i);
		delete elem;
	}
}


MSG_FolderCacheElement *MSG_FolderCache::GetAt (int i) const
{
	return (MSG_FolderCacheElement*) XPPtrArray::GetAt(i);
}
