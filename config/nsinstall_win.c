/*
 * The nsinstall command for Win32
 *
 * Our gmake makefiles use the nsinstall command to create the
 * object directories or installing headers and libs. This code was originally
 * taken from shmsdos.c
 */

#include <direct.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <windows.h>
#pragma hdrstop

/*
 * sh_FileFcn --
 *
 * A function that operates on a file.  The pathname is either
 * absolute or relative to the current directory, and contains
 * no wildcard characters such as * and ?.   Additional arguments
 * can be passed to the function via the arg pointer.
 */

typedef BOOL (*sh_FileFcn)(
        wchar_t *pathName,
        WIN32_FIND_DATA *fileData,
        void *arg);

static int shellCp (wchar_t **pArgv); 
static int shellNsinstall (wchar_t **pArgv);
static int shellMkdir (wchar_t **pArgv); 
static BOOL sh_EnumerateFiles(const wchar_t *pattern, const wchar_t *where,
        sh_FileFcn fileFcn, void *arg, int *nFiles);
static const char *sh_GetLastErrorMessage(void);
static BOOL sh_DoCopy(wchar_t *srcFileName, DWORD srcFileAttributes,
        wchar_t *dstFileName, DWORD dstFileAttributes,
        int force, int recursive);

#define LONGPATH_PREFIX L"\\\\?\\"
#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))
#define STR_LEN(a) (ARRAY_LEN(a) - 1)

/* changes all forward slashes in token to backslashes */
void changeForwardSlashesToBackSlashes ( wchar_t *arg )
{
    if ( arg == NULL )
        return;

    while ( *arg ) {
        if ( *arg == '/' )
            *arg = '\\';
        arg++;
    }
}

int wmain(int argc, wchar_t *argv[ ])
{
    return shellNsinstall ( argv + 1 );
}

static int
shellNsinstall (wchar_t **pArgv)
{
    int retVal = 0;     /* exit status */
    int dirOnly = 0;    /* 1 if and only if -D is specified */
    wchar_t **pSrc;
    wchar_t **pDst;

    /*
     * Process the command-line options.  We ignore the
     * options except for -D.  Some options, such as -m,
     * are followed by an argument.  We need to skip the
     * argument too.
     */
    while ( *pArgv && **pArgv == '-' ) {
        wchar_t c = (*pArgv)[1];  /* The char after '-' */

        if ( c == 'D' ) {
            dirOnly = 1;
        } else if ( c == 'm' ) {
            pArgv++;  /* skip the next argument */
        }
        pArgv++;
    }

    if ( !dirOnly ) {
        /* There are files to install.  Get source files */
        if ( *pArgv ) {
            pSrc = pArgv++;
        } else {
            fprintf( stderr, "nsinstall: not enough arguments\n");
            return 3;
        }
    }

    /* Get to last token to find destination directory */
    if ( *pArgv ) {
        pDst = pArgv++;
        if ( dirOnly && *pArgv ) {
            fprintf( stderr, "nsinstall: too many arguments with -D\n");
            return 3;
        }
    } else {
        fprintf( stderr, "nsinstall: not enough arguments\n");
        return 3;
    }
    while ( *pArgv ) 
        pDst = pArgv++;

    retVal = shellMkdir ( pDst );
    if ( retVal )
        return retVal;
    if ( !dirOnly )
        retVal = shellCp ( pSrc );
    return retVal;
}

static int
shellMkdir (wchar_t **pArgv) 
{
    int retVal = 0; /* assume valid return */
    wchar_t *arg;
    wchar_t *pArg;
    wchar_t path[_MAX_PATH];
    wchar_t tmpPath[_MAX_PATH];
    wchar_t *pTmpPath = tmpPath;

    /* All the options are simply ignored in this implementation */
    while ( *pArgv && **pArgv == '-' ) {
        if ( (*pArgv)[1] == 'm' ) {
            pArgv++;  /* skip the next argument (mode) */
        }
        pArgv++;
    }

    while ( *pArgv ) {
        arg = *pArgv;
        changeForwardSlashesToBackSlashes ( arg );
        pArg = arg;
        pTmpPath = tmpPath;
        while ( 1 ) {
            /* create part of path */
            while ( *pArg ) {
                *pTmpPath++ = *pArg++;
                if ( *pArg == '\\' )
                    break;
            }
            *pTmpPath = '\0';

            /* check if directory already exists */
            _wgetcwd ( path, _MAX_PATH );
            if ( _wchdir ( tmpPath ) == -1 &&
                 _wmkdir ( tmpPath ) == -1 && // might have hit EEXIST
                 _wchdir ( tmpPath ) == -1) { // so try again
                char buf[2048];
                _snprintf(buf, 2048, "Could not create the directory: %S",
                          tmpPath);
                perror ( buf );
                retVal = 3;
                break;
            } else {
                // get back to the cwd
                _wchdir ( path );
            }
            if ( *pArg == '\0' )      /* complete path? */
                break;
            /* loop for next directory */
        }

        pArgv++;
    }
    return retVal;
}

static const char *
sh_GetLastErrorMessage()
{
    static char buf[128];

    FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  /* default language */
            buf,
            sizeof(buf),
            NULL
    );
    return buf;
}

/*
 * struct sh_FileData --
 *
 * A pointer to the sh_FileData structure is passed into sh_RecordFileData,
 * which will fill in the fields.
 */

struct sh_FileData {
    wchar_t pathName[_MAX_PATH];
    DWORD dwFileAttributes;
};

/*
 * sh_RecordFileData --
 *
 * Record the pathname and attributes of the file in
 * the sh_FileData structure pointed to by arg.
 *
 * Always return TRUE (successful completion).
 *
 * This function is intended to be passed into sh_EnumerateFiles
 * to see if a certain pattern expands to exactly one file/directory,
 * and if so, record its pathname and attributes.
 */

static BOOL
sh_RecordFileData(wchar_t *pathName, WIN32_FIND_DATA *findData, void *arg)
{
    struct sh_FileData *fData = (struct sh_FileData *) arg;

    wcscpy(fData->pathName, pathName);
    fData->dwFileAttributes = findData->dwFileAttributes;
    return TRUE;
}

static BOOL
sh_DoCopy(wchar_t *srcFileName,
          DWORD srcFileAttributes,
          wchar_t *dstFileName,
          DWORD dstFileAttributes,
          int force,
          int recursive
)
{
    if (dstFileAttributes != 0xFFFFFFFF) {
        if ((dstFileAttributes & FILE_ATTRIBUTE_READONLY) && force) {
            dstFileAttributes &= ~FILE_ATTRIBUTE_READONLY;
            SetFileAttributes(dstFileName, dstFileAttributes);
        }
    }

    if (srcFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        fprintf(stderr, "nsinstall: %ls is a directory\n",
                srcFileName);
        return FALSE;
    } else {
        DWORD r;
        wchar_t longSrc[1004] = LONGPATH_PREFIX;
        wchar_t longDst[1004] = LONGPATH_PREFIX;
        r = GetFullPathName(srcFileName, 1000, longSrc + STR_LEN(LONGPATH_PREFIX), NULL);
        if (!r) {
            fprintf(stderr, "nsinstall: couldn't get full path of %ls: %s\n",
                    srcFileName, sh_GetLastErrorMessage());
            return FALSE;
        }
        r = GetFullPathName(dstFileName, 1000, longDst + ARRAY_LEN(LONGPATH_PREFIX) - 1, NULL);
        if (!r) {
            fprintf(stderr, "nsinstall: couldn't get full path of %ls: %s\n",
                    dstFileName, sh_GetLastErrorMessage());
            return FALSE;
        }

        if (!CopyFile(longSrc, longDst, FALSE)) {
            fprintf(stderr, "nsinstall: cannot copy %ls to %ls: %s\n",
                    srcFileName, dstFileName, sh_GetLastErrorMessage());
            return FALSE;
        }
    }
    return TRUE;
}

/*
 * struct sh_CpCmdArg --
 *
 * A pointer to the sh_CpCmdArg structure is passed into sh_CpFileCmd.
 * The sh_CpCmdArg contains information about the cp command, and
 * provide a buffer for constructing the destination file name.
 */

struct sh_CpCmdArg {
    int force;                /* -f option, ok to overwrite an existing
                               * read-only destination file */
    int recursive;            /* -r or -R option, recursively copy
                               * directories. Note: this field is not used
                               * by nsinstall and should always be 0. */
    wchar_t *dstFileName;        /* a buffer for constructing the destination
                               * file name */
    wchar_t *dstFileNameMarker;  /* points to where in the dstFileName buffer
                               * we should write the file component of the
                               * destination file */
};

/*
 * sh_CpFileCmd --
 *
 * Copy a file to the destination directory
 * 
 * This function is intended to be passed into sh_EnumerateFiles to
 * copy all the files specified by the pattern to the destination
 * directory.
 *
 * Return TRUE if the file is successfully copied, and FALSE otherwise.
 */

static BOOL
sh_CpFileCmd(wchar_t *pathName, WIN32_FIND_DATA *findData, void *cpArg)
{
    BOOL retVal = TRUE;
    struct sh_CpCmdArg *arg = (struct sh_CpCmdArg *) cpArg;

    wcscpy(arg->dstFileNameMarker, findData->cFileName);
    return sh_DoCopy(pathName, findData->dwFileAttributes,
            arg->dstFileName, GetFileAttributes(arg->dstFileName),
            arg->force, arg->recursive);
}

static int
shellCp (wchar_t **pArgv) 
{
    int retVal = 0;
    wchar_t **pSrc;
    wchar_t **pDst;
    struct sh_CpCmdArg arg;
    struct sh_FileData dstData;
    int dstIsDir = 0;
    int n;

    arg.force = 0;
    arg.recursive = 0;
    arg.dstFileName = dstData.pathName;
    arg.dstFileNameMarker = 0;

    while (*pArgv && **pArgv == '-') {
        wchar_t *p = *pArgv;

        while (*(++p)) {
            if (*p == 'f') {
                arg.force = 1;
            }
        }
        pArgv++;
    }

    /* the first source file */
    if (*pArgv) {
        pSrc = pArgv++;
    } else {
        fprintf(stderr, "nsinstall: not enough arguments\n");
        return 3;
    }

    /* get to the last token to find destination */
    if (*pArgv) {
        pDst = pArgv++;
    } else {
        fprintf(stderr, "nsinstall: not enough arguments\n");
        return 3;
    }
    while (*pArgv) {
        pDst = pArgv++;
    }

    /*
     * The destination pattern must unambiguously expand to exactly
     * one file or directory.
     */

    changeForwardSlashesToBackSlashes(*pDst);
    sh_EnumerateFiles(*pDst, *pDst, sh_RecordFileData, &dstData, &n);
    assert(n >= 0);
    if (n == 1) {
        /*
         * Is the destination a file or directory?
         */

        if (dstData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            dstIsDir = 1;
        }
    } else if (n > 1) {
        fprintf(stderr, "nsinstall: %ls: ambiguous destination file "
                "or directory\n", *pDst);
        return 3;
    } else {
        /*
         * n == 0, meaning that destination file or directory does
         * not exist.  In this case the destination file directory
         * name must be fully specified.
         */

        wchar_t *p;

        for (p = *pDst; *p; p++) {
            if (*p == '*' || *p == '?') {
                fprintf(stderr, "nsinstall: %ls: No such file or directory\n",
                        *pDst);
                return 3;
            }
        }

        /*
         * Do not include the trailing \, if any, unless it is a root
         * directory (\ or X:\).
         */

        if (p > *pDst && p[-1] == '\\' && p != *pDst + 1 && p[-2] != ':') {
            p[-1] = '\0';
        }
        wcscpy(dstData.pathName, *pDst);
        dstData.dwFileAttributes = 0xFFFFFFFF;
    }

    /*
     * If there are two or more source files, the destination has
     * to be a directory.
     */

    if (pDst - pSrc > 1 && !dstIsDir) {
        fprintf(stderr, "nsinstall: cannot copy more than"
                " one file to the same destination file\n");
        return 3;
    }

    if (dstIsDir) {
        arg.dstFileNameMarker = arg.dstFileName + wcslen(arg.dstFileName);

        /*
         * Now arg.dstFileNameMarker is pointing to the null byte at the
         * end of string.  We want to make sure that there is a \ at the
         * end of string, and arg.dstFileNameMarker should point right
         * after that \. 
         */

        if (arg.dstFileNameMarker[-1] != '\\') {
            *(arg.dstFileNameMarker++) = '\\';
        }
    }
    
    if (!dstIsDir) {
        struct sh_FileData srcData;

        assert(pDst - pSrc == 1);
        changeForwardSlashesToBackSlashes(*pSrc);
        sh_EnumerateFiles(*pSrc, *pSrc, sh_RecordFileData, &srcData, &n);
        if (n == 0) {
            fprintf(stderr, "nsinstall: %ls: No such file or directory\n",
                    *pSrc);
            retVal = 3;
        } else if (n > 1) {
            fprintf(stderr, "nsinstall: cannot copy more than one file or "
                    "directory to the same destination\n");
            retVal = 3;
        } else {
            assert(n == 1);
            if (sh_DoCopy(srcData.pathName, srcData.dwFileAttributes,
                    dstData.pathName, dstData.dwFileAttributes,
                    arg.force, arg.recursive) == FALSE) {
                retVal = 3;
            }
        }
        return retVal;
    }

    for ( ; *pSrc != *pDst; pSrc++) {
        BOOL rv;

        changeForwardSlashesToBackSlashes(*pSrc);
        rv = sh_EnumerateFiles(*pSrc, *pSrc, sh_CpFileCmd, &arg, &n);
        if (rv == FALSE) {
            retVal = 3;
        } else {
            if (n == 0) {
                fprintf(stderr, "nsinstall: %ls: No such file or directory\n",
                        *pSrc);
                retVal = 3;
            }
        }
    }

    return retVal;
}

/*
 * sh_EnumerateFiles --
 *
 * Enumerate all the files in the specified pattern, which is a pathname
 * containing possibly wildcard characters such as * and ?.  fileFcn
 * is called on each file, passing the expanded file name, a pointer
 * to the file's WIN32_FILE_DATA, and the arg pointer.
 * 
 * It is assumed that there are no wildcard characters before the
 * character pointed to by 'where'.
 *
 * On return, *nFiles stores the number of files enumerated.  *nFiles is
 * set to this number whether sh_EnumerateFiles or 'fileFcn' succeeds
 * or not.
 *
 * Return TRUE if the files are successfully enumerated and all
 * 'fileFcn' invocations succeeded.  Return FALSE if something went
 * wrong.
 */

static BOOL sh_EnumerateFiles(
        const wchar_t *pattern,
        const wchar_t *where,
        sh_FileFcn fileFcn,
        void *arg,
        int *nFiles
        )
{
    WIN32_FIND_DATA fileData;
    HANDLE hSearch;
    const wchar_t *src;
    wchar_t *dst;
    wchar_t fileName[_MAX_PATH];
    wchar_t *fileNameMarker = fileName;
    wchar_t *oldFileNameMarker;
    BOOL hasWildcard = FALSE;
    BOOL retVal = TRUE;
    BOOL patternEndsInDotStar = FALSE;
    BOOL patternEndsInDot = FALSE;  /* a special case of
                                     * patternEndsInDotStar */
    int numDotsInPattern;
    int len;
    
    /*
     * Windows expands patterns ending in ".", ".*", ".**", etc.
     * differently from the glob expansion on Unix.  For example,
     * both "foo." and "foo.*" match "foo", and "*.*" matches
     * everything, including filenames with no dots.  So we need
     * to throw away extra files returned by the FindNextFile()
     * function.  We require that a matched filename have at least
     * the number of dots in the pattern.
     */
    len = wcslen(pattern);
    if (len >= 2) {
        /* Start from the end of pattern and go backward */
        const wchar_t *p = &pattern[len - 1];

        /* We can have zero or more *'s */
        while (p >= pattern && *p == '*') {
            p--;
        }
        if (p >= pattern && *p == '.') {
            patternEndsInDotStar = TRUE;
            if (p == &pattern[len - 1]) {
                patternEndsInDot = TRUE;
            }
            p--;
            numDotsInPattern = 1;
            while (p >= pattern && *p != '\\') {
                if (*p == '.') {
                    numDotsInPattern++;
                }
                p--;
            }
        }
    }

    *nFiles = 0;

    /*
     * Copy pattern to fileName, but only up to and not including
     * the first \ after the first wildcard letter.
     *
     * Make fileNameMarker point to one of the following:
     * - the start of fileName, if fileName does not contain any \.
     * - right after the \ before the first wildcard letter, if there is
     *   a wildcard character.
     * - right after the last \, if there is no wildcard character.
     */

    dst = fileName;
    src = pattern;
    while (src < where) {
        if (*src == '\\') {
            oldFileNameMarker = fileNameMarker;
            fileNameMarker = dst + 1;
        }
        *(dst++) = *(src++);
    }

    while (*src && *src != '*' && *src != '?') {
        if (*src == '\\') {
            oldFileNameMarker = fileNameMarker;
            fileNameMarker = dst + 1;
        }
        *(dst++) = *(src++);
    }

    if (*src) {
        /*
         * Must have seen the first wildcard letter
         */

        hasWildcard = TRUE;
        while (*src && *src != '\\') {
            *(dst++) = *(src++);
        }
    }
    
    /* Now src points to either null or \ */

    assert(*src == '\0' || *src == '\\');
    assert(hasWildcard || *src == '\0');
    *dst = '\0';

    /*
     * If the pattern does not contain any wildcard characters, then
     * we don't need to go the FindFirstFile route.
     */

    if (!hasWildcard) {
        /*
         * See if it is the root directory, \, or X:\.
         */

        assert(!wcscmp(fileName, pattern));
        assert(wcslen(fileName) >= 1);
        if (dst[-1] == '\\' && (dst == fileName + 1 || dst[-2] == ':')) {
            fileData.cFileName[0] = '\0';
        } else {
            /*
             * Do not include the trailing \, if any
             */

            if (dst[-1] == '\\') {
                assert(*fileNameMarker == '\0');
                dst[-1] = '\0';
                fileNameMarker = oldFileNameMarker;
            } 
            wcscpy(fileData.cFileName, fileNameMarker);
        }
        fileData.dwFileAttributes = GetFileAttributes(fileName);
        if (fileData.dwFileAttributes == 0xFFFFFFFF) {
            return TRUE;
        }
        *nFiles = 1;
        return (*fileFcn)(fileName, &fileData, arg);
    }

    hSearch = FindFirstFile(fileName, &fileData);
    if (hSearch == INVALID_HANDLE_VALUE) {
        return retVal;
    }

    do {
        if (!wcscmp(fileData.cFileName, L".")
                || !wcscmp(fileData.cFileName, L"..")) {
            /* 
             * Skip over . and ..
             */

            continue;
        }

        if (patternEndsInDotStar) {
            int nDots = 0;
            wchar_t *p = fileData.cFileName;
            while (*p) {
                if (*p == '.') {
                    nDots++;
                }
                p++;
            }
            /* Now p points to the null byte at the end of file name */
            if (patternEndsInDot && (p == fileData.cFileName
                    || p[-1] != '.')) {
                /*
                 * File name does not end in dot.  Skip this file.
                 * Note: windows file name probably cannot end in dot,
                 * but we do this check anyway.
                 */
                continue;
            }
            if (nDots < numDotsInPattern) {
                /*
                 * Not enough dots in file name.  Must be an extra
                 * file in matching .* pattern.  Skip this file.
                 */
                continue;
            }
        }

        wcscpy(fileNameMarker, fileData.cFileName);
        if (*src && *(src + 1)) {
            /*
             * More to go.  Recurse.
             */

            int n;

            assert(*src == '\\');
            where = fileName + wcslen(fileName);
            wcscat(fileName, src);
            sh_EnumerateFiles(fileName, where, fileFcn, arg, &n);
            *nFiles += n;
        } else {
            assert(wcschr(fileName, '*') == NULL);
            assert(wcschr(fileName, '?') == NULL);
            (*nFiles)++;
            if ((*fileFcn)(fileName, &fileData, arg) == FALSE) {
                retVal = FALSE;
            }
        }
    } while (FindNextFile(hSearch, &fileData));

    FindClose(hSearch);
    return retVal;
}
