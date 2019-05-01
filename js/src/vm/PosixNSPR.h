/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_PosixNSPR_h
#define vm_PosixNSPR_h

#ifdef JS_POSIX_NSPR

#  include "jspubtd.h"

int32_t PR_FileDesc2NativeHandle(PRFileDesc* fd);

enum PRFileType { PR_FILE_FILE = 1, PR_FILE_DIRECTORY = 2, PR_FILE_OTHER = 3 };

struct PRFileInfo {
  PRFileType type;
  int32_t size;
  int64_t creationTime;
  int64_t modifyTime;
};

typedef enum { PR_FAILURE = -1, PR_SUCCESS = 0 } PRStatus;

PRStatus PR_GetOpenFileInfo(PRFileDesc* fd, PRFileInfo* info);

enum PRSeekWhence { PR_SEEK_SET = 0, PR_SEEK_CUR = 1, PR_SEEK_END = 2 };

int32_t PR_Seek(PRFileDesc* fd, int32_t offset, PRSeekWhence whence);

enum PRFileMapProtect {
  PR_PROT_READONLY,
  PR_PROT_READWRITE,
  PR_PROT_WRITECOPY
};

struct PRFileMap;

PRFileMap* PR_CreateFileMap(PRFileDesc* fd, int64_t size,
                            PRFileMapProtect prot);

void* PR_MemMap(PRFileMap* fmap, int64_t offset, uint32_t len);

PRStatus PR_MemUnmap(void* addr, uint32_t len);

PRStatus PR_CloseFileMap(PRFileMap* fmap);

#endif /* JS_POSIX_NSPR */

#endif /* vm_PosixNSPR_h */
