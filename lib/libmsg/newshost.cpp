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

#include "rosetta.h"
#include "msg.h"
#include "newshost.h"

#include "newsset.h"
#include "msgfinfo.h"
#include "msgfpane.h"
#include "hosttbl.h"
#include "grec.h"
#include "prefapi.h"

#ifdef XP_UNIX
static const char LINEBREAK_START = '\012';
#else
static const char LINEBREAK_START = '\015';
#endif
MSG_NewsHost* MSG_NewsHost::M_FileOwner = NULL;

extern "C" {
	extern int MK_OUT_OF_MEMORY;
	extern int MK_UNABLE_TO_OPEN_NEWSRC;
	extern int MK_MIME_ERROR_WRITING_FILE;
	HG28366
	extern int MK_MSG_CANT_MOVE_FOLDER;
}

MSG_NewsHost::MSG_NewsHost(MSG_Master* master, const char* name,
						   XP_Bool xxx, int32 port)
{
	m_master = master;
	m_hostname = new char [XP_STRLEN(name) + 1];
	XP_STRCPY(m_hostname, name);
	HG18763
	XP_ASSERT(port);
	if (port == 0) port = HG28736 NEWS_PORT;
	m_port = port;

	m_searchableGroupCharsets = XP_HashTableNew(20, XP_StringHash,
												(XP_HashCompFunction) strcmp);

	m_nameAndPort = NULL;
	m_fullUIName = NULL;
	m_optionLines = NULL;
	m_filename = NULL;
	m_groups = NULL;
	m_dbfilename = NULL;
	m_dirty = 0;
	m_writetimer = NULL;
	m_postingAllowed = TRUE;
	m_supportsExtensions = FALSE;
	m_pushAuth = FALSE;
	m_groupSucceeded = FALSE;
}

MSG_NewsHost::~MSG_NewsHost()
{
	if (m_dirty) WriteNewsrc();
	if (m_groupTreeDirty) SaveHostInfo();
	FREEIF(m_optionLines);
	delete [] m_filename;
	delete [] m_hostname;
	FREEIF(m_nameAndPort);
	FREEIF(m_fullUIName);
	delete m_groups;
	delete [] m_dbfilename;
	delete m_groupTree;
	if (m_block)
		delete [] m_block;
	if (m_groupFile)
		XP_FileClose (m_groupFile);
	FREEIF(m_groupFilePermissions);
	if (M_FileOwner == this) M_FileOwner = NULL;
	FREEIF(m_hostinfofilename);
	int i;
	for (i = 0; i < m_supportedExtensions.GetSize(); i++)
		XP_FREE((char*) m_supportedExtensions.GetAt(i));
	for (i = 0; i < m_searchableGroups.GetSize(); i++)
		XP_FREE((char*) m_searchableGroups.GetAt(i));
	for (i = 0; i < m_searchableHeaders.GetSize(); i++)
		XP_FREE((char*) m_searchableHeaders.GetAt(i));
	for (i = 0; i < m_propertiesForGet.GetSize(); i++)
		XP_FREE((char*) m_propertiesForGet.GetAt(i));
	for (i = 0; i < m_valuesForGet.GetSize(); i++)
		XP_FREE((char*) m_valuesForGet.GetAt(i));
	if (m_searchableGroupCharsets)
	{
		// We do NOT free the individual key/value pairs,
		// because deleting m_searchableGroups above has
		// already caused this to happen.
		XP_HashTableDestroy(m_searchableGroupCharsets);
		m_searchableGroupCharsets = NULL;
	}
}


void
MSG_NewsHost::OpenGroupFile(const XP_FilePerm permissions)
{
	XP_ASSERT(permissions);
	if (!permissions) return;
	if (m_groupFile) {
		if (m_groupFilePermissions &&
			  XP_STRCMP(m_groupFilePermissions, permissions) == 0) {
			return;
		}
		XP_FileClose(m_groupFile);
		m_groupFile = NULL;
	}
	if (M_FileOwner && M_FileOwner != this && M_FileOwner->m_groupFile) {
		XP_FileClose(M_FileOwner->m_groupFile);
		M_FileOwner->m_groupFile = NULL;
	}
	M_FileOwner = this;
	FREEIF(m_groupFilePermissions);
	m_groupFilePermissions = XP_STRDUP(permissions);
	m_groupFile = XP_FileOpen(m_hostinfofilename, xpXoverCache, permissions);
}



void MSG_NewsHost::ClearNew()
{
	m_firstnewdate = time(0) + 1;
}



void MSG_NewsHost::dump()
{
	// ###tw  Write me...
}


int32 MSG_NewsHost::getPort()
{
	return m_port;
}


HG28361

time_t MSG_NewsHost::getLastUpdate()
{
	return m_lastgroupdate;
}

void MSG_NewsHost::setLastUpdate(time_t date)
{
	m_lastgroupdate = date;
}

const char* MSG_NewsHost::getStr()
{
	return m_hostname;
}

const char* MSG_NewsHost::getNameAndPort()
{
	if (!m_nameAndPort) {
		if (m_port != (HG23837 NEWS_PORT)) {
			m_nameAndPort = PR_smprintf("%s:%ld", m_hostname, long(m_port));
		} else {
			m_nameAndPort = XP_STRDUP(m_hostname);
		}
	}
	return m_nameAndPort;
}

const char* MSG_NewsHost::getFullUIName()
{
	if (!m_fullUIName) {
		if (HG65276) return getNameAndPort();
		m_fullUIName = PR_smprintf("%s%s", getNameAndPort(),
								   HG38467);
	}
	return m_fullUIName;
}

int32
MSG_NewsHost::RememberLine(char* line)
{
	char* new_data;
	if (m_optionLines) {
		new_data =
			(char *) XP_REALLOC(m_optionLines,
								XP_STRLEN(m_optionLines)
								+ XP_STRLEN(line) + 4);
	} else {
		new_data = (char *) XP_ALLOC(XP_STRLEN(line) + 3);
	}
	if (!new_data) return MK_OUT_OF_MEMORY;
	XP_STRCPY(new_data, line);
	XP_STRCAT(new_data, LINEBREAK);

	m_optionLines = new_data;

	return 0;

}


int32
MSG_NewsHost::ProcessLine_s(char* line, uint32 line_size, void* closure)
{
	return ((MSG_NewsHost*) closure)->ProcessLine(line, line_size);
}


int32
MSG_NewsHost::ProcessLine(char* line, uint32 line_size)
{
	/* guard against blank line lossage */
	if (line[0] == '#' || line[0] == CR || line[0] == LF) return 0;

	line[line_size] = 0;

	if ((line[0] == 'o' || line[0] == 'O') &&
		!strncasecomp (line, "options", 7)) {
		return RememberLine(line);
	}

	char *s;
	char *end = line + line_size;
	static msg_NewsArtSet *set;
	
	for (s = line; s < end; s++)
		if (*s == ':' || *s == '!')
			break;
	
	if (*s == 0) {
		/* What is this? Well, don't just throw it away... */
		return RememberLine(line);
	}

	set = msg_NewsArtSet::Create(s + 1, this);
	if (!set) return MK_OUT_OF_MEMORY;

	XP_Bool subscribed = (*s == ':');
	*s = '\0';

	if (strlen(line) == 0)
	{
		delete set;
		return 0;
	}

	MSG_FolderInfoNews* info;

	if (subscribed && IsCategoryContainer(line))
	{
		info = new MSG_FolderInfoCategoryContainer(line, set, subscribed,
												   this,
												   m_hostinfo->GetDepth() + 1);
		msg_GroupRecord* group = FindOrCreateGroup(line);
		// Go add all of our categories to the newsrc.
		AssureAllDescendentsLoaded(group);
		msg_GroupRecord* end = group->GetSiblingOrAncestorSibling();
		msg_GroupRecord* child;
		for (child = group->GetNextAlphabetic() ;
			 child != end ;
			 child = child->GetNextAlphabetic()) {
			XP_ASSERT(child);
			if (!child) break;
			char* fullname = child->GetFullName();
			if (!fullname) break;
			MSG_FolderInfoNews* info = FindGroup(fullname);
			if (!info) {	// autosubscribe, if we haven't seen this one.
				char* groupLine = PR_smprintf("%s:", fullname);
				if (groupLine) {
					ProcessLine(groupLine, XP_STRLEN(groupLine));
					XP_FREE(groupLine);
				}
			}
			delete [] fullname;
		}
	}
	else
		info = new MSG_FolderInfoNews(line, set, subscribed, this,
							   m_hostinfo->GetDepth() + 1);

	if (!info) return MK_OUT_OF_MEMORY;

	// for now, you can't subscribe to category by itself.
	if (! info->IsCategory())
	{
		XPPtrArray* infolist = (XPPtrArray*) m_hostinfo->GetSubFolders();
		infolist->Add(info);
	}

	m_groups->Add(info);

	// prime the folder info from the folder cache while it's still around.
	// Except this might disable the update of new counts - check it out...
	m_master->InitFolderFromCache (info);

	return 0;
}


int MSG_NewsHost::LoadNewsrc(MSG_FolderInfo* hostinfo)
{
	char *ibuffer = 0;
	uint32 ibuffer_size = 0;
	uint32 ibuffer_fp = 0;

	m_hostinfo = hostinfo;
	XP_ASSERT(m_hostinfo);
	if (!m_hostinfo) return -1;

	int status = 0;

	FREEIF(m_optionLines);

	if (!m_groups) {
		int size = 2048;
		char* buffer;
		buffer = new char[size];
		if (!buffer) return MK_OUT_OF_MEMORY;

		m_groups = new MSG_FolderArray();
		if (!m_groups) {
			delete [] buffer;
			return MK_OUT_OF_MEMORY;
		}
		XP_File fid = XP_FileOpen(GetNewsrcFileName(),
								  HG84636 xpNewsRC,
								  XP_FILE_READ_BIN);
		if (fid) {
			do {
				status = XP_FileRead(buffer, size, fid);
				if (status > 0) {
					msg_LineBuffer(buffer, status,
								   &ibuffer, &ibuffer_size, &ibuffer_fp,
								   FALSE,
#ifdef XP_OS2
								   (int32 (_Optlink*) (char*,uint32,void*))
#endif
								   MSG_NewsHost::ProcessLine_s, this);
				}
			} while (status > 0);
			XP_FileClose(fid);
		}
		if (status == 0 && ibuffer_fp > 0) {
			status = ProcessLine_s(ibuffer, ibuffer_fp, this);
			ibuffer_fp = 0;
		}

		delete [] buffer;
		FREEIF(ibuffer);

	}

	// build up the category tree for each category container so that roll-up 
	// of counts will work before category containers are opened.
	for (int32 i = 0; i < m_groups->GetSize(); i++) {
		MSG_FolderInfoNews *info = (MSG_FolderInfoNews *) m_groups->GetAt(i);
		if (info->GetType() == FOLDER_CATEGORYCONTAINER) {
			const char* groupname = info->GetNewsgroupName();
			msg_GroupRecord* group = m_groupTree->FindDescendant(groupname);
			XP_ASSERT(group);
			if (group) {
				MSG_FolderInfoCategoryContainer *catContainer =
					(MSG_FolderInfoCategoryContainer *) info;

				info->SetFlag(MSG_FOLDER_FLAG_ELIDED | MSG_FOLDER_FLAG_DIRECTORY);

				catContainer->BuildCategoryTree(catContainer, groupname, group,
					2, m_master);
			}
		}
	}
	return 0;
}


int
MSG_NewsHost::WriteNewsrc()
{
	XP_ASSERT(m_groups);
	if (!m_groups) return -1;

	XP_ASSERT(m_dirty);
	// Just to be sure.  It's safest to go ahead and write it out anyway,
	// even if we do somehow get called without the dirty bit set.

	XP_File fid = XP_FileOpen(GetNewsrcFileName(), xpTemporaryNewsRC,
							  XP_FILE_WRITE_BIN);
	if (!fid) return MK_UNABLE_TO_OPEN_NEWSRC;
	
	int status = 0;

#ifdef XP_UNIX
	/* Clone the permissions of the "real" newsrc file into the temp file,
	   so that when we rename the finished temp file to the real file, the
	   preferences don't appear to change. */
	{
        XP_StatStruct st;
        if (XP_Stat (GetNewsrcFileName(), &st,
					 HG48362 xpNewsRC) == 0)
			/* Ignore errors; if it fails, bummer. */

		/* SCO doesn't define fchmod at all.  no big deal.
		   AIX3, however, defines it to take a char* as the first parameter.  The
		   man page sez it takes an int, though.  ... */
#if defined( SCO_SV ) || defined ( AIXV3 )
	  {
	    		char *really_tmp_file = WH_FileName(GetNewsrcFileName(), xpTemporaryNewsRC);

			chmod  (really_tmp_file, (st.st_mode | S_IRUSR | S_IWUSR));

			XP_FREE(really_tmp_file);
	  }
#else
			fchmod (fileno(fid), (st.st_mode | S_IRUSR | S_IWUSR));
#endif
	}
#endif /* XP_UNIX */

	if (m_optionLines) {
		status = XP_FileWrite(m_optionLines, XP_STRLEN(m_optionLines), fid);
		if (status < int(XP_STRLEN(m_optionLines))) {
			status = MK_MIME_ERROR_WRITING_FILE;
		}
	}

	int n = m_groups->GetSize();
	for (int i=0 ; i<n && status >= 0 ; i++) {
		MSG_FolderInfoNews* info = (MSG_FolderInfoNews*) ((*m_groups)[i]);
		// GetNewsFolderInfo will get root category for cat container.
		char* str = info->GetNewsFolderInfo()->GetSet()->Output();
		if (!str) {
			status = MK_OUT_OF_MEMORY;
			break;
		}
		char* line = PR_smprintf("%s%s %s" LINEBREAK, info->GetNewsgroupName(),
								 info->IsSubscribed() ? ":" : "!", str);
		if (!line) {
			delete [] str;
			status = MK_OUT_OF_MEMORY;
			break;
		}
		int length = XP_STRLEN(line);
		status = XP_FileWrite(line, length, fid);
		if (status < length) {
			status = MK_MIME_ERROR_WRITING_FILE;
		}
		delete [] str;
		XP_FREE(line);
	}
	  
	XP_FileClose(fid);
  
	if (status >= 0) {
		if (XP_FileRename(GetNewsrcFileName(), xpTemporaryNewsRC,
						  GetNewsrcFileName(), 
						  HG72626 xpNewsRC) < 0) {
			status = MK_MIME_ERROR_WRITING_FILE;
		}
	}

	if (status < 0) {
		XP_FileRemove(GetNewsrcFileName(), xpTemporaryNewsRC);
		return status;
	}
	m_dirty = FALSE;
	if (m_writetimer) {
		FE_ClearTimeout(m_writetimer);
		m_writetimer = NULL;
	}

	if (m_groupTreeDirty) SaveHostInfo();

	return status;
}


int
MSG_NewsHost::WriteIfDirty()
{
	if (m_dirty) return WriteNewsrc();
	return 0;
}


void
MSG_NewsHost::MarkDirty()
{
	m_dirty = TRUE;

	if (!m_writetimer)
		m_writetimer = FE_SetTimeout((TimeoutCallbackFunction)MSG_NewsHost::WriteTimer, this,
								 5L * 60L * 1000L); // Hard-coded -- 5 minutes.
  }

void
MSG_NewsHost::WriteTimer(void* closure)
{
	MSG_NewsHost* host = (MSG_NewsHost*) closure;
	host->m_writetimer = NULL;
	if (host->WriteNewsrc() < 0) {
		// ###tw  Pop up error message???
		host->MarkDirty();		// Cause us to try again. Or is this bad? ###tw
	}
}


const char*
MSG_NewsHost::GetNewsrcFileName()
{
	return m_filename;
}


int
MSG_NewsHost::SetNewsrcFileName(const char* name)
{
	delete [] m_filename;
	m_filename = new char [XP_STRLEN(name) + 1];
	if (!m_filename) return MK_OUT_OF_MEMORY;
	XP_STRCPY(m_filename, name);

	m_hostinfofilename = PR_smprintf("%s/hostinfo.dat", GetDBDirName());
	if (!m_hostinfofilename) return MK_OUT_OF_MEMORY;

	XP_StatStruct st;
	if (XP_Stat (m_hostinfofilename, &st, xpXoverCache) == 0) {
		m_fileSize = st.st_size;
	}
	                               
	if (m_groupFile) {
		XP_FileClose(m_groupFile);
		m_groupFile = NULL;
	}

	OpenGroupFile();

	if (!m_groupFile) {
		// It does not exist.  Create an empty one.  But first, we go through
		// the whole directory and blow away anything that does not end in
		// ".snm".  This is to take care of people running B1 code, which
		// kept databases with such filenames.
		int lastcount = -1;
		int count = -1;
		do {
			XP_Dir dir = XP_OpenDir(GetDBDirName(), xpXoverCache);
			if (!dir) break;
			lastcount = count;
			count = 0;
			XP_DirEntryStruct *entry = NULL;
			for (entry = XP_ReadDir(dir); entry; entry = XP_ReadDir(dir)) {
				count++;
				const char* name = entry->d_name;
				int length = XP_STRLEN(name);
				if (name[0] == '.' || name[0] == '#' ||
					name[length - 1] == '~') continue;
				if (length < 4 ||
					XP_STRCASECMP(name + length - 4, ".snm") != 0) {
					char* tmp = PR_smprintf("%s/%s", GetDBDirName(), name);
					if (tmp) {
						XP_FileRemove(tmp, xpXoverCache);
						XP_FREE(tmp);
					}
				}
			}
			XP_CloseDir(dir);
		} while (lastcount != count); // Keep doing it over and over, until
									  // we get the same number of entries in
									  // the dir.  This is because I do not
									  // trust XP_ReadDir() to do the right
									  // thing if I delete files out from
									  // under it.

		// OK, now create the new empty hostinfo file.
		OpenGroupFile(XP_FILE_WRITE_BIN);
		XP_ASSERT(m_groupFile);
		if (!m_groupFile)
			return -1;
		OpenGroupFile();
		XP_ASSERT(m_groupFile);
		if (!m_groupFile)
			return -1;

		m_groupTreeDirty = 2;
	}

	m_blockSize = 10240;			// ###tw  This really ought to be the most
								// efficient file reading size for the current
								// operating system.

	m_block = NULL;
	while (!m_block && (m_blockSize >= 512))
	{
		m_block = new char[m_blockSize + 1];
		if (!m_block)
			m_blockSize /= 2;
	}
	if (!m_block)
		return MK_OUT_OF_MEMORY;

	m_groupTree = msg_GroupRecord::Create(NULL, NULL, 0, 0, 0);
	if (!m_groupTree)
		return MK_OUT_OF_MEMORY;

	return ReadInitialPart();
}


int
MSG_NewsHost::ReadInitialPart()
{
	OpenGroupFile();
	if (!m_groupFile) return -1;

	int32 length = XP_FileRead(m_block, m_blockSize, m_groupFile);
	if (length < 0) length = 0;
	m_block[length] = '\0';
	int32 version = 0;

	char* ptr;
	char* endptr = NULL;
	for (ptr = m_block ; *ptr ; ptr = endptr + 1) {
		while (*ptr == CR || *ptr == LF) ptr++;
		endptr = XP_STRCHR(ptr, LINEBREAK_START);
		if (!endptr) break;
		*endptr = '\0';
		if (ptr[0] == '#' || ptr[0] == '\0') {
			continue;			// Skip blank lines and comments.
		}
		if (XP_STRCMP(ptr, "begingroups") == 0) {
			m_fileStart = endptr - m_block;
			break;
		}
		char* ptr2 = XP_STRCHR(ptr, '=');
		if (!ptr2) continue;
		*ptr2++ = '\0';
		if (XP_STRCMP(ptr, "lastgroupdate") == 0) {
			m_lastgroupdate = strtol(ptr2, NULL, 16);
		} else if (XP_STRCMP(ptr, "firstnewdate") == 0) {
			m_firstnewdate = strtol(ptr2, NULL, 16);
		} else if (XP_STRCMP(ptr, "uniqueid") == 0) {
			m_uniqueId = strtol(ptr2, NULL, 16);
		} else if (XP_STRCMP(ptr, "pushauth") == 0) {
			m_pushAuth = strtol(ptr2, NULL, 16);
		} else if (XP_STRCMP(ptr, "version") == 0) {
			version = strtol(ptr2, NULL, 16);
		}
	}
	m_block[0] = '\0';
	if (version != 1) {
		// The file got clobbered or damaged somehow.  Throw it away.
#ifdef DEBUG_bienvenu
		if (length > 0)
			XP_ASSERT(FALSE);	// this really shouldn't happen, right?
#endif
		OpenGroupFile(XP_FILE_WRITE_BIN);
		XP_ASSERT(m_groupFile);
		if (!m_groupFile) return -1;
		OpenGroupFile();
		XP_ASSERT(m_groupFile);
		if (!m_groupFile) return -1;

		m_groupTreeDirty = 2;
		m_fileStart = 0;
		m_fileSize = 0;
	}
	m_groupTree->SetFileOffset(m_fileStart);
	return 0;
}

int
MSG_NewsHost::CreateFileHeader()
{
	PR_snprintf(m_block, m_blockSize,
				"# Netscape newshost information file." LINEBREAK
				"# This is a generated file!  Do not edit." LINEBREAK
				"" LINEBREAK
				"version=1" LINEBREAK
				"newsrcname=%s" LINEBREAK
				"lastgroupdate=%08lx" LINEBREAK
				"firstnewdate=%08lx" LINEBREAK
				"uniqueid=%08lx" LINEBREAK
				"pushauth=%1x" LINEBREAK
				"" LINEBREAK
				"begingroups",
				m_filename, (long) m_lastgroupdate, (long) m_firstnewdate, (long) m_uniqueId,
				m_pushAuth);
	return XP_STRLEN(m_block);

}

int
MSG_NewsHost::SaveHostInfo()
{                   
	int status = 0;
	msg_GroupRecord* grec;
	XP_File in = NULL;
	XP_File out = NULL;
	char* blockcomma = NULL;
	char* ptrcomma = NULL;
	char* filename = NULL;
	int length = CreateFileHeader();
	XP_ASSERT(length < m_blockSize - 50);
	if (m_inhaled || length != m_fileStart) {
		m_groupTreeDirty = 2;
	}
	if (m_groupTreeDirty < 0) {
		// Uh, somehow we set the sign bit, probably some routine returned an
		// error status.  This will probably never happen.  But if it does,
		// we should just make sure to write things out in the most paranoid
		// fashion.
		m_groupTreeDirty = 2;
	}
	if (m_groupTreeDirty < 2) {
		// Only bits and pieces are dirty.  Write them out without writing the
		// whole file.
		OpenGroupFile();
		if (!m_groupFile) {
            status = MK_UNABLE_TO_OPEN_NEWSRC;
            goto FAIL;
		}
		XP_FileSeek(m_groupFile, 0, SEEK_SET);
		XP_FileWrite(m_block, length, m_groupFile);
		char* ptr = NULL;
		for (grec = m_groupTree->GetChildren();
			 grec;
			 grec = grec->GetNextAlphabetic()) {
			if (grec->IsDirty()) {
				if (grec->GetFileOffset() < m_fileStart) {
					m_groupTreeDirty = 2;
					break;
				}
				ptr = grec->GetSaveString();
				if (!ptr) {
					status = MK_OUT_OF_MEMORY;
					goto FAIL;
				}
				length = XP_STRLEN(ptr);
				if (length >= m_blockSize) {
					char* tmp = new char [length + 512];
					if (!tmp) {
						status = MK_OUT_OF_MEMORY;
						goto FAIL;
					}
					delete [] m_block;
					m_block = tmp;
					m_blockSize = length + 512;
				}
				// Read the old data, and make sure the old line was the
				// same length as the new one.  If not, set the dirty flag
				// to 2, so we end up writing the whole file, below.
				XP_FileSeek(m_groupFile, grec->GetFileOffset(), SEEK_SET);
				int l = XP_FileRead(m_block, length, m_groupFile);
				if (l != length || m_block[length - 1] != ptr[length - 1]) {
					m_groupTreeDirty = 2;
					break;
				}
				m_block[l] = '\0';
				char* p1 = XP_STRCHR(ptr, LINEBREAK_START);
				char* p2 = XP_STRCHR(m_block, LINEBREAK_START);
				XP_ASSERT(p1);
				if (!p1 || !p2 || (p1 - ptr) != (p2 - m_block)) {
					m_groupTreeDirty = 2;
					break;
				}
				XP_ASSERT(grec->GetFileOffset() > 100);	// header is at least 100 bytes long
				XP_FileSeek(m_groupFile, grec->GetFileOffset(), SEEK_SET);
				XP_FileWrite(ptr, XP_STRLEN(ptr), m_groupFile);
				XP_FREE(ptr);
				ptr = NULL;
			}
		}
		FREEIF(ptr);
	}
	if (m_groupTreeDirty >= 2) {
		// We need to rewrite the whole file.
		filename = PR_smprintf("%s/tempinfo", GetDBDirName());
		if (!filename) return MK_OUT_OF_MEMORY;
		int32 position = 0;

		if (m_groupFile)
		{
			XP_FileClose(m_groupFile);
			m_groupFile = NULL;
		}

		out = XP_FileOpen(filename, xpXoverCache, XP_FILE_WRITE_BIN);
		if (!out) return MK_MIME_ERROR_WRITING_FILE;

		length = CreateFileHeader();	// someone probably has hosed m_block - so regen
		status = XP_FileWrite(m_block, length, out);
		if (status < 0) goto FAIL;
		position += length;
		m_fileStart = position;
		m_groupTree->SetFileOffset(m_fileStart);
		status = XP_FileWrite(LINEBREAK, LINEBREAK_LEN, out);
		if (status < 0) goto FAIL;
		position += LINEBREAK_LEN;

		blockcomma = NULL;
		if (!m_inhaled) {
			in = XP_FileOpen(m_hostinfofilename, xpXoverCache, XP_FILE_READ);
			if (!in) {
				status = MK_MIME_ERROR_WRITING_FILE;
				goto FAIL;
			}
			do {
				m_block[0] = '\0';
				XP_FileReadLine(m_block, m_blockSize, in);
			} while (m_block[0] &&
					 XP_STRNCMP(m_block, "begingroups", 11) != 0);
			m_block[0] = '\0';
			XP_FileReadLine(m_block, m_blockSize, in);
			blockcomma = XP_STRCHR(m_block, ',');
			if (blockcomma) *blockcomma = '\0';
		}
		for (grec = m_groupTree->GetChildren();
			 grec;
			 grec = grec->GetNextAlphabetic()) {
			char* ptr = grec->GetSaveString();
			if (!ptr) {
				status = MK_OUT_OF_MEMORY;
				goto FAIL;
			}
			ptrcomma = XP_STRCHR(ptr, ',');
			XP_ASSERT(ptrcomma);
			if (!ptrcomma) {
				status = -1;
				goto FAIL;
			}
			*ptrcomma = '\0';
			while (blockcomma &&
				   msg_GroupRecord::GroupNameCompare(m_block, ptr) < 0) {
				*blockcomma = ',';
				XP_StripLine(m_block);
				length = XP_STRLEN(m_block);
				status = XP_FileWrite(m_block, length, out);
				if (status < 0) goto FAIL;
				position += length;
				status = XP_FileWrite(LINEBREAK, LINEBREAK_LEN, out);
				if (status < 0) goto FAIL;
				position += LINEBREAK_LEN;
				m_block[0] = '\0';
				XP_FileReadLine(m_block, m_blockSize, in);
				blockcomma = XP_STRCHR(m_block, ',');
				if (blockcomma) *blockcomma = '\0';
			}
			if (blockcomma && XP_STRCMP(m_block, ptr) == 0) {
				m_block[0] = '\0';
				XP_FileReadLine(m_block, m_blockSize, in);
				blockcomma = XP_STRCHR(m_block, ',');
				if (blockcomma) *blockcomma = '\0';
			}
			grec->SetFileOffset(position);
			*ptrcomma = ',';
			length = XP_STRLEN(ptr);
			// if the group doesn't exist on the server and has no children,
			// don't write it out.
			if (!grec->DoesNotExistOnServer()  || grec->GetNumKids() > 0)
			{
				status = XP_FileWrite(ptr, length, out);
				position += length;
			}
			else
				status = 0;
			XP_FREE(ptr);
			if (status < 0) goto FAIL;
		}
		if (blockcomma) {
			*blockcomma = ',';
			do {
				XP_StripLine(m_block);
				length = XP_STRLEN(m_block);
				status = XP_FileWrite(m_block, length, out);
				if (status < 0) goto FAIL;
				position += length;
				status = XP_FileWrite(LINEBREAK, LINEBREAK_LEN, out);
				if (status < 0) goto FAIL;
				position += LINEBREAK_LEN;
				m_block[0] = '\0';
			} while (XP_FileReadLine(m_block, m_blockSize, in) && *m_block);
		}
		if (in) {
			XP_FileClose(in);
			in = NULL;
		}
		XP_FileClose(out);
		out = NULL;
		XP_FileRename(filename, xpXoverCache,
					  m_hostinfofilename, xpXoverCache);
		m_fileSize = position;
	}
	m_groupTreeDirty = 0;

FAIL:
	if (in) XP_FileClose(in);
	if (out) XP_FileClose(out);
	if (filename) XP_FREE(filename);
	m_block[0] = '\0';
	return status;
}


struct InhaleState {
	msg_GroupRecord* tree;
	int32 position;
	msg_GroupRecord* onlyIfChild;
	msg_GroupRecord* lastInhaled;
	char lastfullname[512];
};


int32
MSG_NewsHost::InhaleLine(char* line, uint32 length, void* closure)
{
	int status = 0;
	InhaleState* state = (InhaleState*) closure;
	int32 position = state->position;
	state->position += length;
	char* lastdot;
	char* endptr = line + length;
	char* comma;
	for (comma = line ; comma < endptr ; comma++) {
		if (*comma == ',') break;
	}
	if (comma >= endptr) return 0;
	*comma = '\0';
	lastdot = XP_STRRCHR(line, '.');
	msg_GroupRecord* parent;
	msg_GroupRecord* child;
	if (lastdot) {
		*lastdot = '\0';
		// Try to find the parent of this line.  Very often, it will be
		// the last line we read, or some ancestor of that line.
		parent = NULL;
		if (state->lastInhaled && XP_STRNCMP(line, state->lastfullname,
											 lastdot - line) == 0) {
			char c = state->lastfullname[lastdot - line];
			if (c == '\0') parent = state->lastInhaled;
			else if (c == '.') {
				parent = state->lastInhaled;
				char* ptr = state->lastfullname + (lastdot - line);
				XP_ASSERT(parent);
				while (parent && ptr) {
					parent = parent->GetParent();
					XP_ASSERT(parent);
					ptr = XP_STRCHR(ptr + 1, '.');
				}
			}
		}


		if (!parent) parent = state->tree->FindDescendant(line);
		*lastdot = '.';
		XP_ASSERT(parent);
		if (!parent) {
			status = -1;
			goto DONE;
		}
	} else {
		parent = state->tree;
		lastdot = line - 1;
	}
	child = parent->FindDescendant(lastdot + 1);
	*comma = ',';
	if (child) {
		// It's already in memory.
		child->SetFileOffset(position);
	} else {
		child = msg_GroupRecord::Create(parent, line, endptr - line, position);
		if (!child) {
			status = MK_OUT_OF_MEMORY;
			goto DONE;
		}
	}
	if (state->onlyIfChild) {
		msg_GroupRecord* tmp;
		for (tmp = child; tmp ; tmp = tmp->GetParent()) {
			if (tmp == state->onlyIfChild) break;
		}
		if (tmp == NULL) status = -2;	// Indicates we're done.
	}
	XP_ASSERT(comma - line < sizeof(state->lastfullname));
	if (comma - line < sizeof(state->lastfullname)) {
		XP_STRNCPY_SAFE(state->lastfullname, line, comma - line + 1);
		state->lastfullname[comma - line] = '\0';
		state->lastInhaled = child;
	}
DONE:
	if (comma) *comma = ',';
	return status;
}


int
MSG_NewsHost::Inhale(XP_Bool force)
{
	if (m_groupTreeDirty) SaveHostInfo();
	if (force) {
		while (m_groupTree->GetChildren()) {
			delete m_groupTree->GetChildren();
		}
		m_inhaled = FALSE;
	}
	XP_ASSERT(!m_inhaled);
	if (m_inhaled) return -1;
	int status = 0;
	OpenGroupFile();
	if (!m_groupFile) return -1;
	XP_FileSeek(m_groupFile, 0, SEEK_SET);
	status = ReadInitialPart();
	if (status < 0) return status;
	XP_FileSeek(m_groupFile, m_fileStart, SEEK_SET);
	InhaleState state;
	state.tree = m_groupTree;
	state.position = m_fileStart;
	state.onlyIfChild = NULL;
	state.lastInhaled = NULL;
	char* buffer = NULL;
	uint32 buffer_size = 0;
	uint32 buffer_fp = 0;
	do {
		status = XP_FileRead(m_block, m_blockSize, m_groupFile);
		if (status <= 0) break;
		status = msg_LineBuffer(m_block, status,
								&buffer, &buffer_size, &buffer_fp,
								FALSE, 
#ifdef XP_OS2
								(int32 (_Optlink*) (char*,uint32,void*))
#endif
								MSG_NewsHost::InhaleLine, &state);
	} while (status >= 0);
	if (status >= 0 && buffer_fp > 0) {
		status = InhaleLine(buffer, buffer_fp, &state);
	}
	FREEIF(buffer);
	m_block[0] = '\0';
	if (status >= 0) m_inhaled = TRUE;
	return status;
}


int
MSG_NewsHost::Exhale()
{
	XP_ASSERT(m_inhaled);
	if (!m_inhaled) return -1;
	int status = SaveHostInfo();
	while (m_groupTree->GetChildren()) {
		delete m_groupTree->GetChildren();
	}
	m_inhaled = FALSE;
	return status;
}


int
MSG_NewsHost::EmptyInhale()
{
	XP_ASSERT(!m_inhaled);
	if (m_inhaled) return -1;
	while (m_groupTree->GetChildren()) {
		delete m_groupTree->GetChildren();
	}
	m_inhaled = TRUE;
	return 0;
}



MSG_FolderInfoNews*
MSG_NewsHost::FindGroup(const char* name)
{
	if (m_groups == NULL) return NULL;
	int n = m_groups->GetSize();
	for (int i=0 ; i<n ; i++) {
		MSG_FolderInfoNews* info = (MSG_FolderInfoNews*) (*m_groups)[i];
		if (XP_STRCMP(info->GetNewsgroupName(), name) == 0) {
			return info;
		}
	}
	return NULL;
}

MSG_FolderInfoNews *MSG_NewsHost::AddGroup(const char *groupName)
{
	MSG_FolderInfoNews *newsInfo = NULL;
	MSG_FolderInfoCategoryContainer *categoryContainer = NULL;
	char* containerName = NULL;
	XP_Bool needpaneupdate = FALSE;

	msg_GroupRecord* group = FindOrCreateGroup(groupName);
	if (!group) goto DONE;	// Out of memory.

	if (!group->IsCategoryContainer() && group->IsCategory()) {
		msg_GroupRecord *container = group->GetCategoryContainer();
		XP_ASSERT(container);
		if (!container) goto DONE;
		containerName = container->GetFullName();
		if (!containerName) goto DONE; // Out of memory.
		newsInfo = FindGroup(containerName);
		categoryContainer = (MSG_FolderInfoCategoryContainer *) newsInfo;
		// if we're not subscribed to container, do that instead.
		if (!newsInfo || !newsInfo->IsSubscribed())	
		{
			groupName = containerName;
			group = FindOrCreateGroup(groupName);
		}
	}

	// no need to diddle folder pane for new categories.
	needpaneupdate = m_hostinfo && !group->IsCategory();

	newsInfo = FindGroup(groupName);
	if (newsInfo) {	// seems to be already added
		if (!newsInfo->IsSubscribed()) {
			newsInfo->Subscribe(TRUE);
			XPPtrArray* infolist = (XPPtrArray*) m_hostinfo->GetSubFolders();
			// don't add it if it's already in the list!
			if (infolist->FindIndex(0, newsInfo) == -1)
				infolist->Add(newsInfo);
		} else {
			goto DONE;
		}
	} else {
		char* groupLine = PR_smprintf("%s:", groupName);
		if (!groupLine) {
			goto DONE;			// Out of memory.
		}
	
		// this will add and auto-subscribe - OK, a cheap hack.
		if (ProcessLine(groupLine, XP_STRLEN(groupLine)) == 0) {
			// groups are added at end so look there first...
			newsInfo = (MSG_FolderInfoNews *)
				m_groups->GetAt(m_groups->GetSize() - 1);
			if (XP_STRCMP(newsInfo->GetNewsgroupName(), groupName)) {
				newsInfo = FindGroup(groupName);
				XP_ASSERT(newsInfo);
			}
		}
		XP_FREE(groupLine);
	}
	XP_ASSERT(newsInfo);
	if (!newsInfo) goto DONE;

	if (group->IsCategoryContainer()) {
		// Go add all of our categories to the newsrc.
		AssureAllDescendentsLoaded(group);
		msg_GroupRecord* end = group->GetSiblingOrAncestorSibling();
		msg_GroupRecord* child;
		for (child = group->GetNextAlphabetic() ;
			 child != end ;
			 child = child->GetNextAlphabetic()) {
			XP_ASSERT(child);
			if (!child) break;
			char* fullname = child->GetFullName();
			if (!fullname) break;
			MSG_FolderInfoNews* info = FindGroup(fullname);
			if (info) {
				if (!info->IsSubscribed()) {
					info->Subscribe(TRUE);
				}
			} else {
				char* groupLine = PR_smprintf("%s:", fullname);
				if (groupLine) {
					ProcessLine(groupLine, XP_STRLEN(groupLine));
					XP_FREE(groupLine);
				}
			}
			delete [] fullname;
		}
		if (newsInfo->GetType() != FOLDER_CATEGORYCONTAINER)
		{
			newsInfo = SwitchNewsToCategoryContainer(newsInfo);
		}
		XP_ASSERT(newsInfo->GetType() == FOLDER_CATEGORYCONTAINER);
		if (newsInfo->GetType() == FOLDER_CATEGORYCONTAINER) {
			newsInfo->SetFlag(MSG_FOLDER_FLAG_ELIDED);
			MSG_FolderInfoCategoryContainer *catContainer =
				(MSG_FolderInfoCategoryContainer *) newsInfo;
			catContainer->BuildCategoryTree(catContainer, groupName, group,
											2, m_master);
		}
	}
	else if (group->IsCategory() && categoryContainer)
	{
		categoryContainer->AddToCategoryTree(categoryContainer, groupName, group, m_master);
	}

	if (needpaneupdate && newsInfo)
		m_master->BroadcastFolderAdded (newsInfo);

	MarkDirty();

DONE:

	if (containerName) delete [] containerName;
	return newsInfo;
}

MSG_FolderInfoNews *MSG_NewsHost::SwitchNewsToCategoryContainer(MSG_FolderInfoNews *newsInfo)
{
	int groupIndex = m_groups->FindIndex(0, newsInfo);
	if (groupIndex != -1)
	{
		MSG_FolderInfoCategoryContainer *newCatCont = newsInfo->CloneIntoCategoryContainer();
		// slip the category container where the newsInfo was.
		m_groups->SetAt(groupIndex, newCatCont);
		XPPtrArray* infoList = (XPPtrArray*) m_hostinfo->GetSubFolders();
		// replace in folder pane server list as well.
		groupIndex = infoList->FindIndex(0, newsInfo);
		if (groupIndex != -1)
			infoList->SetAt(groupIndex, newCatCont);
		newsInfo = newCatCont;
	}
	return newsInfo;
}

MSG_FolderInfoNews *MSG_NewsHost::SwitchCategoryContainerToNews(MSG_FolderInfoCategoryContainer *catContainerInfo)
{
	MSG_FolderInfoNews *retInfo = NULL;

	int groupIndex = m_groups->FindIndex(0, catContainerInfo);
	if (groupIndex != -1)
	{
		MSG_FolderInfoNews *rootCategory = catContainerInfo->GetRootCategory();
		// slip the root category container where the category container was.
		m_groups->SetAt(groupIndex, rootCategory);
		XPPtrArray* infoList = (XPPtrArray*) m_hostinfo->GetSubFolders();
		// replace in folder pane server list as well.
		groupIndex = infoList->FindIndex(0, catContainerInfo);
		if (groupIndex != -1)
			infoList->SetAt(groupIndex, rootCategory);
		retInfo = rootCategory;
		// this effectively leaks the category container, but I don't think that's a problem
	}
	return retInfo;
}

void MSG_NewsHost::RemoveGroup (MSG_FolderInfoNews *newsInfo)
{
	if (newsInfo && newsInfo->IsSubscribed()) 
	{
		newsInfo->Subscribe(FALSE);
		m_master->BroadcastFolderDeleted (newsInfo);
		XPPtrArray* infolist = (XPPtrArray*) m_hostinfo->GetSubFolders();
		infolist->Remove(newsInfo);
	}
}

void
MSG_NewsHost::RemoveGroup(const char* groupName)
{
	MSG_FolderInfoNews *newsInfo = NULL;

	newsInfo = FindGroup(groupName);
	RemoveGroup (newsInfo);
}


#ifdef DEBUG_terry
// #define XP_WIN16
#endif

const char*
MSG_NewsHost::GetDBDirName()
{
	if (!m_filename) return NULL;
	if (!m_dbfilename) {
#if defined(XP_WIN16) || defined(XP_OS2)
		const int MAX_HOST_NAME_LEN = 8;
#elif defined(XP_MAC)
		const int MAX_HOST_NAME_LEN = 25;
#else
		const int MAX_HOST_NAME_LEN = 55;
#endif
		// Given a hostname, use either that hostname, if it fits on our
		// filesystem, or a hashified version of it, if the hostname is too
		// long to fit.
		char hashedname[MAX_HOST_NAME_LEN + 1];
		XP_Bool needshash = XP_STRLEN(m_filename) > MAX_HOST_NAME_LEN;
#if defined(XP_WIN16)  || defined(XP_OS2)
		if (!needshash) {
			needshash = XP_STRCHR(m_filename, '.') != NULL ||
				XP_STRCHR(m_filename, ':') != NULL;
		}
#endif

		XP_STRNCPY_SAFE(hashedname, m_filename, MAX_HOST_NAME_LEN + 1);
		if (needshash) {
			PR_snprintf(hashedname + MAX_HOST_NAME_LEN - 8, 9, "%08lx",
						(unsigned long) XP_StringHash2(m_filename));
		}
		m_dbfilename = new char [XP_STRLEN(hashedname) + 15];
		XP_STRCPY(m_dbfilename, HG38376 "host-");
		XP_STRCAT(m_dbfilename, hashedname);
		char* ptr = XP_STRCHR(m_dbfilename, ':');
		if (ptr) *ptr = '.'; // WinFE doesn't like colons in filenames.
		XP_MakeDirectory(m_dbfilename, xpXoverCache);
	}
	return m_dbfilename;
}

#ifdef DEBUG_terry
// #undef XP_WIN16
#endif

int32
MSG_NewsHost::GetNumGroupsNeedingCounts()
{
	if (!m_groups) return 0;
	int num = m_groups->GetSize();
	int32 result = 0;
	for (int i=0 ; i<num ; i++) {
		MSG_FolderInfoNews* info = (MSG_FolderInfoNews*) ((*m_groups)[i]);
		if (info->GetWantNewTotals() && info->IsSubscribed()) {
			result++;
		}
	}
	return result;
}


char*
MSG_NewsHost::GetFirstGroupNeedingCounts()
{
	if (!m_groups) return NULL;
	int num = m_groups->GetSize();
	for (int i=0 ; i<num ; i++) {
		MSG_FolderInfoNews* info = (MSG_FolderInfoNews*) ((*m_groups)[i]);
		if (info->GetWantNewTotals() && info->IsSubscribed()) {
			info->SetWantNewTotals(FALSE);
			return XP_STRDUP(info->GetNewsgroupName());
		}
	}
	return NULL;
}


char*
MSG_NewsHost::GetFirstGroupNeedingExtraInfo()
{
	msg_GroupRecord* grec;
	
	for (grec = m_groupTree->GetChildren();	 grec; grec = grec->GetNextAlphabetic()) {
		if (grec && grec->NeedsExtraInfo()) {
			char *fullName = grec->GetFullName();
			char *ret = XP_STRDUP(fullName);
			delete [] fullName;
			return ret;
		}
	}
	return NULL;
}



void
MSG_NewsHost::SetWantNewTotals(XP_Bool value)
{
	if (!m_groups) return;
	int n = m_groups->GetSize();
	for (int i=0 ; i<n ; i++) {
		MSG_FolderInfoNews* info = (MSG_FolderInfoNews*) ((*m_groups)[i]);
		info->SetWantNewTotals(value);
	}
}



XP_Bool MSG_NewsHost::NeedsExtension (const char * /*extension*/)
{
	//###phil need to flesh this out
	XP_Bool needed = m_supportsExtensions;
	return needed;
}

void MSG_NewsHost::AddExtension (const char *ext)
{
	if (!QueryExtension(ext))
	{
		char *ourExt = XP_STRDUP (ext);
		if (ourExt)
			m_supportedExtensions.Add(ourExt);
	}
}

XP_Bool MSG_NewsHost::QueryExtension (const char *ext)
{
	for (int i = 0; i < m_supportedExtensions.GetSize(); i++)
		if (!XP_STRCMP(ext, (char*) m_supportedExtensions.GetAt(i)))
			return TRUE;
	return FALSE;
}

void MSG_NewsHost::AddSearchableGroup (const char *group)
{
	if (!QuerySearchableGroup(group))
	{
		char *ourGroup = XP_STRDUP (group);
		if (ourGroup)
		{
			// strip off character set spec 
			char *space = XP_STRCHR(ourGroup, ' ');
			if (space)
				*space = '\0';

			m_searchableGroups.Add(ourGroup);

			space++; // walk over to the start of the charsets
			// Add the group -> charset association.
			XP_Puthash(m_searchableGroupCharsets, ourGroup, space);
		}
	}
}

XP_Bool MSG_NewsHost::QuerySearchableGroup (const char *group)
{
	for (int i = 0; i < m_searchableGroups.GetSize(); i++)
	{
		const char *searchableGroup = (const char*) m_searchableGroups.GetAt(i);
		char *starInSearchableGroup = NULL;

		if (!XP_STRCMP(searchableGroup, "*"))
			return TRUE; // everything is searchable
		else if (NULL != (starInSearchableGroup = XP_STRCHR(searchableGroup, '*')))
		{
			if (!XP_STRNCASECMP(group, searchableGroup, XP_STRLEN(searchableGroup)-2))
				return TRUE; // this group is in a searchable hierarchy
		}
		else if (!XP_STRCASECMP(group, searchableGroup))
			return TRUE; // this group is individually searchable
	}
	return FALSE;
}

// ### mwelch This should have been merged into one routine with QuerySearchableGroup,
//            but with two interfaces.
const char *
MSG_NewsHost::QuerySearchableGroupCharsets(const char *group)
{
	// Very similar to the above, but this time we look up charsets.
	const char *result = NULL;
	XP_Bool gotGroup = FALSE;
	const char *searchableGroup = NULL;

	for (int i = 0; (i < m_searchableGroups.GetSize()) && (!gotGroup); i++)
	{
		searchableGroup = (const char*) m_searchableGroups.GetAt(i);
		char *starInSearchableGroup = NULL;

		if (!XP_STRCMP(searchableGroup, "*"))
			gotGroup = TRUE; // everything is searchable
		else if (NULL != (starInSearchableGroup = XP_STRCHR(searchableGroup, '*')))
		{
			if (!XP_STRNCASECMP(group, searchableGroup, XP_STRLEN(searchableGroup)-2))
				gotGroup = TRUE; // this group is in a searchable hierarchy
		}
		else if (!XP_STRCASECMP(group, searchableGroup))
			gotGroup = TRUE; // this group is individually searchable
	}

    if (gotGroup)
	{
		// Look up the searchable group for its supported charsets
		result = (const char *) XP_Gethash(m_searchableGroupCharsets, searchableGroup, NULL);
	}

	return result;
}

void MSG_NewsHost::AddSearchableHeader (const char *header)
{
	if (!QuerySearchableHeader(header))
	{
		char *ourHeader = XP_STRDUP(header);
		if (ourHeader)
			m_searchableHeaders.Add(ourHeader);
	}
}

XP_Bool MSG_NewsHost::QuerySearchableHeader(const char *header)
{
	for (int i = 0; i < m_searchableHeaders.GetSize(); i++)
		if (!XP_STRNCASECMP(header, (char*) m_searchableHeaders.GetAt(i), XP_STRLEN(header)))
			return TRUE;
	return FALSE;
}

void MSG_NewsHost::AddPropertyForGet (const char *property, const char *value)
{
	char *tmp = NULL;
	
	tmp = XP_STRDUP(property);
	if (tmp)
		m_propertiesForGet.Add (tmp);

	tmp = XP_STRDUP(value);
	if (tmp)
		m_valuesForGet.Add (tmp);
}

const char *MSG_NewsHost::QueryPropertyForGet (const char *property)
{
	for (int i = 0; i < m_propertiesForGet.GetSize(); i++)
		if (!XP_STRCASECMP(property, (const char *) m_propertiesForGet.GetAt(i)))
			return (const char *) m_valuesForGet.GetAt(i);
	return NULL;
}


void MSG_NewsHost::SetPushAuth(XP_Bool value)
{
	if (m_pushAuth != value) 
	{
		m_pushAuth = value;
		m_groupTreeDirty |= 1;
	}
}



const char*
MSG_NewsHost::GetURLBase()
{
	if (!m_urlbase) {
		m_urlbase = PR_smprintf("%s://%s", HG38262 "news",
								getNameAndPort());
	}
	return m_urlbase;
}




int MSG_NewsHost::RemoveHost()
{
	if (m_groupFile) {
		XP_FileClose(m_groupFile);
		m_groupFile = NULL;
	}
	m_master->GetFolderTree()->RemoveSubFolder(m_hostinfo);
	m_master->GetHostTable()->RemoveEntry(this);
	m_dirty = 0;
	if (m_writetimer) {
		FE_ClearTimeout(m_writetimer);
		m_writetimer = NULL;
	}
	XP_FileRemove(GetNewsrcFileName(), HG37252 xpNewsRC);


	// Now go do our best to remove the entire newshost directory and all of
	// its contents.  I would love to use XP_RemoveDirectoryRecursive(), but
	// selmer seems to have only implemented it on WinFE.  Ahem.
	int lastcount = -1;
	int count = -1;
	do {
		XP_Dir dir = XP_OpenDir(GetDBDirName(), xpXoverCache);
		if (!dir) break;
		lastcount = count;
		count = 0;
		XP_DirEntryStruct *entry = NULL;
		for (entry = XP_ReadDir(dir); entry; entry = XP_ReadDir(dir)) {
			count++;
			const char* name = entry->d_name;
			if (name[0] == '.') continue;
			char* tmp = PR_smprintf("%s/%s", GetDBDirName(), name);
			if (tmp) {
				XP_FileRemove(tmp, xpXoverCache);
				XP_FREE(tmp);
			}
		}
		XP_CloseDir(dir);
	} while (lastcount != count); // Keep doing it over and over, until
								  // we get the same number of entries in
								  // the dir.  This is because I do not
								  // trust XP_ReadDir() to do the right
								  // thing if I delete files out from
	// under it.

	XP_RemoveDirectory(GetDBDirName(), xpXoverCache);

	// Tell netlib to close any connections we might have to 
	// this news host.
	NET_OnNewsHostDeleted (m_hostname);

	m_master->BroadcastFolderDeleted (m_hostinfo);

	delete this;
	return 0;
}



char*
MSG_NewsHost::GetPrettyName(const char* groupname)
{
	msg_GroupRecord* group = FindOrCreateGroup(groupname);
	if (group) {
		const char* orig = group->GetPrettyName();
		if (orig) {
			char* result = new char [XP_STRLEN(orig) + 1];
			if (result) {
				XP_STRCPY(result, orig);
				return result;
			}
		}
	}
	return NULL;
}


int
MSG_NewsHost::SetPrettyName(const char* groupname, const char* prettyname)
{
	msg_GroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return MK_OUT_OF_MEMORY;
	int status = group->SetPrettyName(prettyname);
	if (status > 0)
	{
		MSG_FolderInfoNews *newsFolder = FindGroup(groupname);
		// make news folder forget prettyname so it will query again
		if (newsFolder)	
			newsFolder->ClearPrettyName();
	}
	m_groupTreeDirty |= status;
	return status;
}


time_t
MSG_NewsHost::GetAddTime(const char* groupname)
{
	msg_GroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return 0;
	return group->GetAddTime();
}


int32
MSG_NewsHost::GetUniqueID(const char* groupname)
{
	msg_GroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return 0;
	return group->GetUniqueID();
}


XP_Bool
MSG_NewsHost::IsCategory(const char* groupname)
{
	msg_GroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return FALSE;
	return group->IsCategory();
}


XP_Bool
MSG_NewsHost::IsCategoryContainer(const char* groupname)
{
	msg_GroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return FALSE;
	return group->IsCategoryContainer();
}

int
MSG_NewsHost::SetIsCategoryContainer(const char* groupname, XP_Bool value)
{
	msg_GroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return MK_OUT_OF_MEMORY;
	int status = group->SetIsCategoryContainer(value);
	m_groupTreeDirty |= status;
	if (status > 0)
	{
		MSG_FolderInfoNews *newsFolder = FindGroup(groupname);
		// make news folder have correct category container flag
		if (newsFolder)	
		{
			if (value)
			{
				newsFolder->SetFlag(MSG_FOLDER_FLAG_CAT_CONTAINER);
				SwitchNewsToCategoryContainer(newsFolder);
			}
			else
			{
				if (newsFolder->GetType() == FOLDER_CATEGORYCONTAINER)
				{
					MSG_FolderInfoCategoryContainer *catCont = (MSG_FolderInfoCategoryContainer *) newsFolder;
					newsFolder = SwitchCategoryContainerToNews(catCont);
				}
				if (newsFolder)
				{
					newsFolder->ClearFlag(MSG_FOLDER_FLAG_CAT_CONTAINER);
					newsFolder->ClearFlag(MSG_FOLDER_FLAG_CATEGORY);
				}
			}
		}
	}
	return status;
}

int 
MSG_NewsHost::SetGroupNeedsExtraInfo(const char *groupname, XP_Bool value)
{
	msg_GroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return MK_OUT_OF_MEMORY;
	int status = group->SetNeedsExtraInfo(value);
	m_groupTreeDirty |= status;
	return status;
}


char*
MSG_NewsHost::GetCategoryContainer(const char* groupname)
{
	msg_GroupRecord* group = FindOrCreateGroup(groupname);
	if (group) {
		group = group->GetCategoryContainer();
		if (group) return group->GetFullName();
	}
	return NULL;
}

MSG_FolderInfoNews *
MSG_NewsHost::GetCategoryContainerFolderInfo(const char *groupname)
{
	MSG_FolderInfoNews	*ret = NULL;
	// because GetCategoryContainer returns NULL for a category container...
	msg_GroupRecord *group = FindOrCreateGroup(groupname);
	if (group->IsCategoryContainer())
		return FindGroup(groupname);

	char *categoryContainerName = GetCategoryContainer(groupname);
	if (categoryContainerName)
	{
		ret = FindGroup(categoryContainerName);
		delete [] categoryContainerName;
	}
	return ret;
}


XP_Bool
MSG_NewsHost::IsProfile(const char* groupname)
{
	msg_GroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return FALSE;
	return group->IsProfile();
}


int
MSG_NewsHost::SetIsProfile(const char* groupname, XP_Bool value)
{
	msg_GroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return MK_OUT_OF_MEMORY;
	int status = group->SetIsProfile(value);
	m_groupTreeDirty |= status;
	return status;
}



XP_Bool
MSG_NewsHost::IsGroup(const char* groupname)
{
	msg_GroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return FALSE;
	return group->IsGroup();
}


int
MSG_NewsHost::SetIsGroup(const char* groupname, XP_Bool value)
{
	msg_GroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return MK_OUT_OF_MEMORY;
	int status = group->SetIsGroup(value);
	m_groupTreeDirty |= status;
	return status;
}


XP_Bool
MSG_NewsHost::IsHTMLOk(const char* groupname)
{
	msg_GroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return FALSE;
	if (group->IsHTMLOKGroup()) return TRUE;
	for ( ; group ; group = group->GetParent()) {
		if (group->IsHTMLOKTree()) return TRUE;
	}
	return FALSE;
}


XP_Bool
MSG_NewsHost::IsHTMLOKGroup(const char* groupname)
{
	msg_GroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return FALSE;
	return group->IsHTMLOKGroup();
}


int
MSG_NewsHost::SetIsHTMLOKGroup(const char* groupname, XP_Bool value)
{
	msg_GroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return MK_OUT_OF_MEMORY;
	int status = group->SetIsHTMLOKGroup(value);
	m_groupTreeDirty |= status;
	return status;
}


XP_Bool
MSG_NewsHost::IsHTMLOKTree(const char* groupname)
{
	msg_GroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return FALSE;
	return group->IsHTMLOKTree();
}


int
MSG_NewsHost::SetIsHTMLOKTree(const char* groupname, XP_Bool value)
{
	msg_GroupRecord* group = FindOrCreateGroup(groupname);
	if (!group) return MK_OUT_OF_MEMORY;
	int status = group->SetIsHTMLOKTree(value);
	m_groupTreeDirty |= status;
	return status;
}





msg_GroupRecord*
MSG_NewsHost::FindGroupInBlock(msg_GroupRecord* parent,
							   const char* groupname,
							   int32* comp)
{
	char* ptr;
	char* ptr2;
	char* tmp;
RESTART:
	ptr = m_block;
	*comp = 0;
	for (;;) {
		ptr = XP_STRCHR(ptr, LINEBREAK_START);
		ptr2 = ptr ? XP_STRCHR(ptr, ',') : 0;
		if (!ptr2) {
			if (*comp != 0) return NULL;
			// Yikes.  The whole string, and we can't even find a legitimate
			// beginning of a real line.  Grow the buffer until we can.

			if (m_fileSize - m_fileStart <= m_blockSize) {
				// Oh, well, maybe the file is just empty.
				*comp = 0;
				return NULL;
			}

			if (int32(XP_STRLEN(m_block)) < m_blockSize) goto RELOAD;
			goto GROWBUFFER;
		}
		while (*ptr == CR || *ptr == LF) ptr++;
		*ptr2 = '\0';
		int32 c = msg_GroupRecord::GroupNameCompare(groupname, ptr);
		*ptr2 = ',';
		if (c < 0) {
			if (*comp > 0) {
				// This group isn't in the file, since earlier compares said
				// "look later" and this one says "look earlier".
				*comp = 0;
				return NULL;
			}
			*comp = c;
			return NULL;		// This group is somewhere before this chunk.
		}
		*comp = c;
		if (c == 0) {
			// Hey look, we found it.
			int32 offset = m_blockStart + (ptr - m_block);
			ptr2 = XP_STRCHR(ptr2, LINEBREAK_START);
			if (!ptr2) {
				// We found the right place, but the rest of
				// the line is off the end of the buffer.
				if (offset > m_blockStart + LINEBREAK_LEN) {
					m_blockStart = offset - LINEBREAK_LEN;
					goto RELOAD;
				}
				// We couldn't find it, even though we're at the beginning
				// of the buffer.  This means that our buffer is too small.

				goto GROWBUFFER;
			}
			return msg_GroupRecord::Create(parent, ptr, -1, offset);
		}
	}
	/* NOTREACHED */
	return NULL;

GROWBUFFER:
			
	tmp = new char [m_blockSize + 512 + 1];
	if (!tmp) return NULL;
	delete [] m_block;
	m_block = tmp;
	m_blockSize += 512;
	
RELOAD:
	if (m_blockStart + m_blockSize > m_fileSize) {
		m_blockStart = m_fileSize - m_blockSize;
		if (m_blockStart < m_fileStart) m_blockStart = m_fileStart;
	}

	OpenGroupFile();
	if (!m_groupFile) return NULL;
	XP_FileSeek(m_groupFile, m_blockStart, SEEK_SET);
	int length = XP_FileRead(m_block, m_blockSize, m_groupFile);
	if (length < 0) length = 0;
	m_block[length] = '\0';
	goto RESTART;
}


msg_GroupRecord*
MSG_NewsHost::LoadSingleEntry(msg_GroupRecord* parent, const char* groupname,
							  int32 min, int32 max)
{
	OpenGroupFile();
	if (!m_groupFile) return NULL;

#ifdef DEBUG
	if (parent != m_groupTree) {
		char* pname = parent->GetFullName();
		if (pname) {
			XP_ASSERT(XP_STRNCMP(pname, groupname, XP_STRLEN(pname)) == 0);
			delete [] pname;
			pname = NULL;
		}
	}
#endif

	if (min < m_fileStart) min = m_fileStart;
	if (max < m_fileStart || max > m_fileSize) max = m_fileSize;

	msg_GroupRecord* result = NULL;
	int32 comp = 1;

	// First, check if we happen to already have the line for this group in
	// memory.
	if (m_block[0]) {
		result = FindGroupInBlock(parent, groupname, &comp);
	}

	while (!result && comp != 0 && min < max) {
		int32 mid = (min + max) / 2;
		m_blockStart = mid - LINEBREAK_LEN;
		XP_FileSeek(m_groupFile, m_blockStart, SEEK_SET);
		int length = XP_FileRead(m_block, m_blockSize, m_groupFile);
		if (length < 0) length = 0;
		m_block[length] = '\0';

		result = FindGroupInBlock(parent, groupname, &comp);

		if (comp > 0) {
			min = mid + 1;
		} else {
			max = mid;
		}
	}
	return result;
}


msg_GroupRecord*
MSG_NewsHost::FindOrCreateGroup(const char* groupname,
								int* statusOfMakingGroup)
{
	char buf[256];
	
	msg_GroupRecord* parent = m_groupTree;
	const char* start = groupname;
	XP_ASSERT(start && *start);
	if (!start || !*start) return NULL;

	XP_ASSERT(*start != '.');	// groupnames can't start with ".".
	if (*start == '.') return NULL;
	while (*start) {
		if (*start == '.') start++;
		const char* end = XP_STRCHR(start, '.');
		if (!end) end = start + XP_STRLEN(start);
		int length = end - start;
		XP_ASSERT(length > 0);	// groupnames can't contain ".." or end in
								// a ".".
		if (length <= 0) return NULL;
		XP_ASSERT(length < sizeof(buf));
		if (length >= sizeof(buf)) return NULL;
		XP_STRNCPY_SAFE(buf, start, length + 1);
		buf[length] = '\0';
		msg_GroupRecord* prev = parent;
		msg_GroupRecord* ptr;
		int comp = 0;  // Initializing to zero.
		for (ptr = parent->GetChildren() ; ptr ; ptr = ptr->GetSibling()) {
			comp = msg_GroupRecord::GroupNameCompare(ptr->GetPartName(), buf);
			if (comp >= 0) break;
			prev = ptr;
		}
		if (ptr == NULL || comp != 0) {
			// We don't have this one in memory.  See if we can load it in.
			if (!m_inhaled) {
				if (ptr == NULL) {
					ptr = parent->GetSiblingOrAncestorSibling();
				}
				length = end - groupname;
				char* tmp = new char[length + 1];
				if (!tmp) return NULL;
				XP_STRNCPY_SAFE(tmp, groupname, length + 1);
				tmp[length] = '\0';
				ptr = LoadSingleEntry(parent, tmp,
									  prev->GetFileOffset(),
									  ptr ? ptr->GetFileOffset() : m_fileSize);
				delete [] tmp;
				tmp = NULL;
			} else {
				ptr = NULL;
			}
			if (!ptr) {
				m_groupTreeDirty = 2;
				ptr = msg_GroupRecord::Create(parent, buf, time(0),
											  m_uniqueId++, 0);
				if (!ptr) return NULL;
			}
		}
		parent = ptr;
		start = end;
	}
	int status = parent->SetIsGroup(TRUE);
	m_groupTreeDirty |= status;
	if (statusOfMakingGroup) *statusOfMakingGroup = status;
	return parent;
}


int
MSG_NewsHost::NoticeNewGroup(const char* groupname)
{
	int status = 0;
	msg_GroupRecord* group = FindOrCreateGroup(groupname, &status);
	if (!group) return MK_OUT_OF_MEMORY;
	return status;
}


int
MSG_NewsHost::AssureAllDescendentsLoaded(msg_GroupRecord* group)
{
	int status = 0;
	XP_ASSERT(group);
	if (!group) return -1;
	if (group->IsDescendentsLoaded()) return 0;
	m_blockStart = group->GetFileOffset();
	XP_ASSERT(group->GetFileOffset() > 0);
	if (group->GetFileOffset() == 0) return -1;
	InhaleState state;
	state.tree = m_groupTree;
	state.position = m_blockStart;
	state.onlyIfChild = group;
	state.lastInhaled = NULL;
	OpenGroupFile();
	XP_ASSERT(m_groupFile);
	if (!m_groupFile) return -1;
	XP_FileSeek(m_groupFile, group->GetFileOffset(), SEEK_SET);
	char* buffer = NULL;
	uint32 buffer_size = 0;
	uint32 buffer_fp = 0;
	do {
		status = XP_FileRead(m_block, m_blockSize, m_groupFile);
		if (status <= 0) break;
		status = msg_LineBuffer(m_block, status,
								&buffer, &buffer_size, &buffer_fp,
								FALSE, 
#ifdef XP_OS2
								(int32 (_Optlink*) (char*,uint32,void*))
#endif
								MSG_NewsHost::InhaleLine, &state);
	} while (status >= 0);
	if (status >= 0 && buffer_fp > 0) {
		status = InhaleLine(buffer, buffer_fp, &state);
	}
	if (status == -2) status = 0; // Special status meaning we ran out of
								  // lines that are children of the given
								  // group.
	
	FREEIF(buffer);
	m_block[0] = '\0';

	if (status >= 0) group->SetIsDescendentsLoaded(TRUE);
	return status;
}

void MSG_NewsHost::GroupNotFound(const char *groupName, XP_Bool opening)
{
	// if no group command has succeeded, don't blow away categories.
	// The server might be hanging...
	if (!opening && !m_groupSucceeded)
		return;

	msg_GroupRecord* group = FindOrCreateGroup(groupName);
	if (group && (group->IsCategory() || opening))
	{
		MSG_FolderInfoNews *newsInfo = FindGroup(groupName);
		group->SetDoesNotExistOnServer(TRUE);
		m_groupTreeDirty |= 2;	// deleting a group has to force a rewrite anyway
		if (group->IsCategory())
		{
			MSG_FolderInfoNews *catCont = GetCategoryContainerFolderInfo(groupName);
			if (catCont)
			{
				MSG_FolderInfo *parentCategory = catCont->FindParentOf(newsInfo);
				if (parentCategory)
					parentCategory->RemoveSubFolder(newsInfo);
			}
		}
		else if (newsInfo)
			m_master->BroadcastFolderDeleted (newsInfo);

		if (newsInfo)
		{
			XPPtrArray* infolist = (XPPtrArray*) m_hostinfo->GetSubFolders();
			infolist->Remove(newsInfo);
			m_groups->Remove(newsInfo);
			m_dirty = TRUE;
		}
	}
}


int MSG_NewsHost::ReorderGroup (MSG_FolderInfoNews *groupToMove, MSG_FolderInfoNews *groupToMoveBefore, int32 *newIdx)
{
	// Do the list maintenance to reorder a newsgroup. The news host has a list, and
	// so does the FolderInfo which represents the host in the hierarchy tree

	int err = MK_MSG_CANT_MOVE_FOLDER;
	XPPtrArray *infoList = m_hostinfo->GetSubFolders();

	if (groupToMove && groupToMoveBefore && infoList)
	{
		if (m_groups->Remove (groupToMove)) 
		{
			// Not necessary to remove from infoList here because the folderPane does that (urk)

			// Unsubscribed groups are in the lists, but not in the view
			MSG_FolderInfo *group = NULL;
			int idxInView, idxInData;
			int idxInHostInfo = 0;
			XP_Bool	foundIdxInHostInfo = FALSE;

			for (idxInData = 0, idxInView = -1; idxInData < m_groups->GetSize(); idxInData++)
			{
				group = m_groups->GetAt (idxInData);
				MSG_FolderInfo *groupInHostInfo = (MSG_FolderInfo *) infoList->GetAt(idxInHostInfo);
				if (group->CanBeInFolderPane())
					idxInView++;
				if (group == groupToMoveBefore)
					break;
				if (groupInHostInfo == groupToMoveBefore)
					foundIdxInHostInfo = TRUE;
				else if (!foundIdxInHostInfo)
					idxInHostInfo++;

			} 

			if (idxInView != -1) 
			{
				m_groups->InsertAt (idxInData, groupToMove); // the index has to be the same, right?
				infoList->InsertAt (idxInHostInfo, groupToMove);

				MarkDirty (); // Make sure the newsrc gets written out in the new order
				*newIdx = idxInView; // Tell the folder pane where the new item belongs

				err = 0;
			}
		}
	}
	return err;
}

