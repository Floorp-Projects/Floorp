Version 1.4.2

MoreFiles

A collection of File Manager and related routines

by Jim Luther, Apple Macintosh Developer Technical Support Emeritus
with significant code contributions by Nitin Ganatra, Apple Macintosh Developer Technical Support Emeritus
Copyright © 1992-1996 Apple Computer, Inc.
Portions copyright © 1995 Jim Luther
All rights reserved.

You may incorporate this sample code into your applications without restriction, though the sample code has been provided "AS IS" and the responsibility for its operation is 100% yours.  However, what you are not permitted to do is to redistribute the source as "DSC Sample Code" after having made changes. If you're going to re-distribute the source, we require that you make it clear in the source that the code was descended from Apple Sample Code, but that you've made changes.


What is MoreFiles?

MoreFiles is a collection of high-level routines written over the last couple of years to answer File Manager questions developers have sent to Apple Developer Technical Support and to answer questions on various online services and the internet. The routines have been tested (but not stress-tested), documented, code-reviewed, and used in both my own programs and in many commercial products.


Important Note

These routines are meant to be used from an application environment. In particular, some routines use static variables which require an A5 world and almost all routines make calls that are unsafe at interrupt time (i.e., synchronous File Manager calls and Memory Manager calls). If you plan to use these routines from stand-alone code modules, you may need to make some modifications to the code that uses static variables. Don't even think about using most of these routines from code that executes at interrupt time.

Note: If you need to use the routines in FSpCompat in stand-alone 68K code, set GENERATENODATA to 1 in FSpCompat.c so globals (static variables) are not used.


Build Environments

MoreFiles is built using the latest release C development environments I have access to. In particular, MoreFiles v1.4.2 was built using:

¥  Universal Interfaces 2.1.1 in ÒMPW LatestÓ on MPW Pro #19
¥  Latest MPW from E.T.O./MPW Pro #18 (with Universal Interfaces 2.1.1 in ÒMPW LatestÓ on MPW Pro #19)
¥  THINK C 7.0.4/Symantec C++ 8.0.4 (with Universal Interfaces 2.1.1 in ÒMPW LatestÓ on MPW Pro #19)
¥  Metrowerks CodeWarrior 8.0 (MW C/C++ 68K 1.4 and MW C/C++ PPC 1.4 with Universal Interfaces 2.1.1 in ÒMPW LatestÓ on MPW Pro #19)

Note: If you are not using these versions, you might have to make some minor changes to compile or link with MoreFiles. I've tried hard to write C code that compiles with no warnings using all current development environments.

The Pascal interfaces to MoreFiles are maintained, but have not been tested with all available Pascal development environments.


Files in the MoreFile Package

MoreFiles.c - the Òglue codeÓ for high-level and FSSpec style routines that were never implemented or added to the standard interface for one reason or another. If you're uncomfortable filling in parameter blocks, you'll find the code in this file very useful.

MoreFiles.h and MoreFiles.p - the documentation and header files for calling the routines included in MoreFiles.c.

Sharing.h and Sharing.p - the inline Òglue codeÓ for calling the AppleShare server routines, PBShare, PBUnshare, and PBGetUGEntry. These routines have been added to Files.h in the Universal Interfaces so they aren't used by MoreFiles. However, I've left them here in case you're still using the old interfaces.

MoreFilesExtras.c - a collection of useful File Manager utility routines.
 
MoreFilesExtras.h and MoreFilesExtras.p - the documentation and header files for calling the routines included in MoreFilesExtras.c. I recommend that you browse through the header files - you'll probably find a routine that you've always wanted.

MoreDesktopMgr.c - a collection of useful high-level Desktop Manager routines. If the Desktop Manager isn't available, the "Desktop" file is used for 'read' operations.

MoreDesktopMgr.h and MoreDesktopMgr.p - the documentation and header files for calling the routines included in MoreDesktopMgr.c.

FileCopy.c - a robust, general purpose file copy routine that does everything the right way. Copying a file on the Macintosh isn't as simple as you'd think if you want to handle all cases (i.e., file server drop boxes, preflighting disk space, etc.). This routine does it all and is fast, too.

FileCopy.h and FileCopy.p - the documentation and header files for calling the routines included in FileCopy.c.

DirectoryCopy.c - a general purpose recursive directory copy routine with a hook for your error handling routine and a hook for filtering what you want or don't want copied.

DirectoryCopy.h and DirectoryCopy.p - the documentation and header files for calling the routines included in DirectoryCopy.c.

FSpCompat.c - the Òglue codeÓ for FSSpec style routines that will work under System 6 as much as possible. If you still need to support System 6, you'll find the code in this file very useful.

FSpCompat.h and FSpCompat.p -  the documentation and header files for calling the routines included in FSpCompat.c.  

Search.c - IndexedSearch and PBCatSearchSyncCompat. IndexedSearch performs an indexed search in and below the specified directory using the same parameters as is passed to PBCatSearch. PBCatSearchSyncCompat uses PBCatSearch (if available) or IndexedSearch (if PBCatSearch is not available) to search a volume. Also included are a couple of examples of how to call PBCatSearch: one that searches by creator and file type, and one that searches by file name.

Search.h and Search.p - the documentation and header files for calling the routines included in Search.c.  

FullPath.c - Routines for dealing with full pathnamesÉ if you really must use them. Read the warning at the top of the interface files for reasons why you shouldn't use them most of the time.

FullPath.h and FullPath.p - the documentation and header files for calling the routines included in FullPath.c.  

IterateDirectory.c - Routines that perform a recursive iteration (scan) of a specified directory and call your IterateFilterProc function once	for each file and directory found. IterateDirectory lets you decide how deep you want the function to dive into the directory tree - anything from 1 level or all levels. This routine is very useful when you want to do something to each item in a directory.

IterateDirectory.h and IterateDirectory.p - the documentation and header files for calling the routines included in IterateDirectory.c.

MoreFiles.68K.¹ - the THINK C project file you can use to build a 68K THINK library.

MoreFiles.PPC.¹ - the Symantec C++ project file you can use to build a PowerPC Symantec library.

MoreFiles.68K.µ - the Metrowerks C project file you can use to build a 68K CodeWarrior library.

MoreFiles.PPC.µ - the Metrowerks C project file you can use to build a PowerPC CodeWarrior library.

BuildMoreFiles Symantec - the AppleScript  used to compile and build the THINK and Symantec versions of the libraries.

BuildMoreFiles Metrowerks - the AppleScript  used to compile and build the Metrowerks CodeWarrior versions of the libraries.

BuildMoreFiles and MoreFilesLib.make - the MPW script and make file used to compile and build the MPW versions of the libraries.

FindFolderGlue.o - a library containing the FindFolder compatibility glue. This library is needed only by THINK Pascal users. Read ÒFindFolderGlue Read MeÓ for details.

UpperString.p - a UNIT that maps UpperString to UprString for THINK Pascal users.

Note: All 68K libraries are built with SystemSevenOrLater false so that all compatibility code is included.


How to use MoreFiles

You can compile the code, put it in a library, and link it into your programs. You can cut and paste it into your programs. You can use it as example code. Feel free to rip it off and modify it in whatever ways you find work best for you.

All exported routines use Pascal calling conventions so this code can be used from either C or Pascal. (Note: Pascal calling conventions can be turned off starting with MoreFiles 1.4.2.)

You'll also notice that all routines that make other File Manager calls return an OSErr result. I always check for error results and you should too. (Well, there's a few places where I purposely ignore errors, but they're commented in the source.)


Documentation

The documentation for the routines can be found in the C header files. There, you'll find a short description of each call and a complete listing of all input and output parameters. I've also included a list of possible result codes for each routine. For example, here's the documentation for one of the routines, GetFileLocation.

    /*****************************************************************************/
    
    pascal  OSErr   GetFileLocation(short refNum,
                                    short *vRefNum,
                                    long *dirID,
                                    StringPtr fileName);
    /*  ¦ Get the location of an open file.
        The GetFileLocation function gets the location (volume reference number,
        directory ID, and fileName) of an open file.
        
        refNum      input:  The file reference number of an open file.
        vRefNum     output: The volume reference number.
        dirID       output: The parent directory ID.
        fileName    input:  Points to a buffer (minimum Str63) where the
                            filename is to be returned or must be nil.
                    output:	The filename.
 	
        Result Codes
            noErr        0  No error
            nsvErr     -35  Specified volume doesnÕt exist
            fnOpnErr   -38  File not open
            rfNumErr   -51  Reference number specifies nonexistent
                            access path
        __________
	    
        See also:   FSpGetFileLocation
    */
    
    /*****************************************************************************/

Some routines have very few comments in their code because they simply make a single File Manager call. For those routines, the routine description is the comment. However, you'll find that more complicated routines have plenty of comments to clarify what and why the code is doing something. If something isn't clear, take a look at Inside Macintosh: Files for further reference.


Release Notes

v1.0  09/14/93

¥  First release for Developer CD
________________________________________

v1.1  01/22/94

¥  MoreFiles is now built with the Universal Interfaces.
¥  Gave DirectoryCopy.p and MoreFilesExtras.p the correct UNIT names.
¥  Built MoreFiles.lib with THINK C and MoreFilesLib.o with MPW C.
¥  Changed the way the GetFilenameFromPathname works function (I made it work correctly for its intended purpose).
¥  Added GetObjectLocation function to MoreFilesExtras.
¥  Moved local constants from FileCopy.h to FileCopy.c.
¥  Moved local constants from DirectoryCopy.h to DirectoryCopy.c.
¥  Added Sharing.p. to go along with Sharing.h, but then didn't need either because the Universal Interfaces now include PBShare, PBUnshare, and PBGetUGEntry in Files.h.
¥  Added standard disclaimer to all source files.
¥  Fixed bug in FSpMoveRename function (it called HCopyFile instead of HMoveRename).
¥  Added HMoveRenameCompat and FSpMoveRenameCompat functions to MoreFilesExtras.
¥  Changed HOpenAware and HOpenRFAware functions in MoreFilesExtras to work around a bug in Foreign File Access (FFA) based foreign file systems (ISO 9960, High Sierra, Audio, Photo CD Access, etc.). FFA based file systems incorrectly return a wPrErr result to the OpenDeny calls instead of a paramErr result.
¥  Added UnmountAndEject function to MoreFilesExtras.
¥  The ChangeCreatorType and ChangeFDFlags functions in MoreFilesExtras no longer try (and fail) to use BumpDate on fsRtParID.
¥  Added DeleteDirectoryContents and DeleteDirectory functions to MoreFilesExtras.
¥  Removed DTS.Lib StringUtils files. The only function needed by MoreFiles was pcpy, so I dropped it into the MoreFiles sources as a static function rather than force you to include all of StringUtils.
¥  Fixed conditional #include in MoreFilesExtras.c.
¥  In MoreFilesExtras.h, changed macros for checking volume attribute bits to shift the 1L constant rather than the vMAttrib field in the GetVolParmsInfoBuffer. This produces tighter code.
¥  Added FSpCompat.c, FSpCompat.h, and FSpCompat.p. The FSpCompat functions let you use the System 7 FSSpec calls under System 6 (the functions use the same calling conventions as the System 7 FSSpec calls but have a modified name (for example, FSpCreate is FSpCreateCompat).
¥  Ran all sources through as many C compilers as possible and made minor modifications to all sources in an attempt to get rid of most warnings. There are still a couple left to fix, but I'm up against the deadline for the next Developer CD, so they'll have to waitÉ
________________________________________

v1.1.1  02/01/94

¥ Added FindFolderGlue.o and its Read Me file to the Libraries folder for THINK Pascal users.
________________________________________

v1.2  07/20/94

New Routines:
¥  Added GetDInfo, FSpGetDInfo, SetDInfo, FSpSetDInfo, CheckVolLock, CheckObjectLock, and FSpCheckObjectLock to MoreFilesExtras.
¥  Added MoreDesktopMgr. In the process, I moved GetComment, SetComment, and CopyComment over from MoreFilesExtras and changed their names (added a DT).  My apologies to anyone this API change screws up. I think the new and improved code makes up for the hassles.
¥  Added Search. Ever want to search a volume with PBCatSearch and it isn't available? Then you'll like this code.

Enhancements to existing routines:
¥  Made changes for powerc.
¥  THINK C project is for version 7.0.3.
¥  Reformatted code to meet new DTS standards for sample code.
¥  Added code to FileCopy to open and copy only file forks that contain data. By doing this, we won't create a data or resource fork for the copied file unless it's really needed (some foreign file systems such as the ProDOS File System and Macintosh PC Exchange have to create additional disk structures to support resource forks).
¥  Added code to FileCopy to copy the file attributes to the destination only if the destination is an AppleShare drop box. That way, if the file has the bundle bit set and if FileCopy is modified (by you - read the comments in the source before modification) to give up time to other processes during a copy, any Finder with access to the file won't try to read bundle information from a partially copied resource fork.
¥  DirectoryCopy no longer calls your CopyErrProc with a copyDirCommentOp failed operation code. There's really nothing wrong with comments not getting copied and DirectoryCopy does give it its best effort.
¥  Modified OpenAware and OpenRFAware functions to check for locked volume if write access is requested.
¥  Rewrote GetObjectLocation so that it calls PBGetCatInfo fewer times (worst case is two calls). That makes the code a little harder to read, but on slow volumes (for example, AppleShare volumes over AppleTalk Remote Access), fewer calls *is* better.
¥  Replaced the static pcpy functions with calls to BlockMoveData (which is BlockMove if you don't have System Update 3.0, System 7.5, or later).
¥  The routines in MoreDesktopMgr (used to be in MoreFilesExtras) can get APPL mappings, icons, and comments from the ÒDesktopÓ file if the Desktop Manager isn't supported by a volume.
¥  NameFileSearch and CreatorTypeFileSearch in MoreFilesExtras now call PBCatSearchSyncCompat in Search so that they can search on all volumes.
¥  Tweaked all headers so that they can be read by a future "documentation viewer" project.

Bugs fixed:
¥  Changed DTCopyComment to use Str255 instead of unsigned char commentBuffer[199]. PhotoCD's wrongly return up to 255 characters instead of 200 characters and that was trashing memory.
¥  Changed FSMakeFSSpecCompat to fix Macintosh PC Exchange 1.0.x bug.
¥  Fixed a couple of bugs in GetFilenameFromPathname.
¥  Fixed ChangeFDFlags and ChangeCreatorType to get the real vRefNum before calling BumpDate. BumpDate was ÒbumpingÓ the wrong object in some cases.
¥  Fixed GetParentID so that it gets the right parent ID in all cases (HFS has a weird bug that I work around).
¥  Cleared ioFVersNum field for all PBHOpenDF, PBHOpenRF, PBHOpen, PBHCreate, PBHDelete, PBHGetFInfo, PBHSetFInfo, PBHSetFLock, PBHRstFLock, and PBHRename calls. Not needed for HFS volumes, but some of those old MFS volumes might still be around.
¥  Changed the macro name hasLocalList to hasLocalWList to match bit name in Files.h.
________________________________________

v1.2.1  08/09/94

New Routine:
¥  Added HGetVInfo to MoreFilesExtras. Like GetVInfo, this routine returns the volume name, volume reference number, and free bytes on the volume; HGetVInfo also returns the total number of bytes on the volume, and both freeBytes and totalBytes are unsigned longs so this routine returns accurate information when used on volumes larger than 2 Gigabytes.

Bugs fixed:
¥  Changed PBCatSearchSyncCompat to only check the bHasCatSearch bit once instead of twice in a row.
¥  Changed CheckForMatches to pass EqualString cPB->hFileInfo.ioNamePtr instead of the uninitialized variable itemName. Now, searching using fsSBFullName works.
¥  Changed PreflightFileCopySpace to use unsigned variables in space calculations.
¥  Changed PreflightDirectoryCopySpace to use unsigned variables in space calculations. Now, I'm 4 GB clean.
________________________________________

v1.2.2  11/01/94

Bugs fixed (all in Search.c):
¥  Changed IndexedSearch to correctly handle non-fnfErr error results. IndexedSearch now just backs out a level when a non-fnfErr error result is returned.
¥  Changed IndexedSearch to correctly check for parent directory changes when return to a parent directory.
¥  Changed IsSubString to work correctly.
________________________________________

v1.3  04/17/95

New Routines:
¥  Added FullPath which includes the routines GetFullPath, FSpGetFullPath, LocationFromFullPath, and FSpLocationFromFullPath. These routines work with full pathnames longer than 255 characters.  Read the warning at the top of the interface files for reasons why you shouldn't use them most of the time.
¥  Added GetFileSize and FSpGetFileSize functions to MoreFilesExtras.
¥  Added FSWriteVerify function to MoreFilesExtras.
¥  Added FindDrive function to MoreFilesExtras.
¥  Added GetDriverName to MoreFilesExtras.

Bugs fixed:
¥  CopyFork used srcPB to reset the destination fork (which reset the source file's mark again). It now uses dstPB.
¥  On 68000 processor based Macintosh systems (Macintosh Plus, SE, etc.), PBGetCatInfo doesn't accept a pointer to a zero-length string as its ioNamePtr input parameter. If you pass an empty string, PBGetCatInfo returns bdNamErr (-37). So, all MoreFiles code (changes made to MoreFilesExtras, MoreDesktopMgr and FileCopy) that calls PBGetCatInfo with ioFDirIndex==0 now checks for that condition and sets ioNamePtr to NULL if a zero-length string would have been input.

Other changes:
¥  Made various minor changes to remove static global data from 68K code whenever possible.
¥  Added the conditional GENERATENODATA to FSpCompat.c. If GENERATENODATA is false, the code will be faster, but will use a few static globals. If GENERATENODATA is true, the code will be slightly slower, but won't use any static globals. You may find it useful to set GENERATENODATA to true if you want to use the FSpCompat code in stand-alone/non-CFM code.
¥  Changed all code to use new routine names (i.e., DisposePtr instead of DisposPtr, UpperString instead of UprString, etc.).
¥  MoreFiles uses the 2.0 Universal Headers. All references to old header files have been removed.
¥  Moved NameFileSearch and CreatorTypeFileSearch from MoreFilesExtras to Search. That puts all of the PBCatSearch related code in one place and removed the only static global data in MoreFilesExtras.
¥  Moved #includes that aren't needed in the header files, but are needed for the source code, into the source files.
¥  Removed THINK C libraries from libraries folder. The MPW ".o" libraries should work everywhere since most development environments can use or convert them in this format.
¥  Dropped MoreFiles Reference - I'll look into supplying a QuickViewª database in the future.
________________________________________

v1.3.1  06/17/95

New Routine:
¥  Added GetDiskBlocks function to MoreFilesExtras.

Bugs fixed:
¥  Changed work-around for problems that affected PBGetCatInfo when ioNamePtr = NULL or ioNamePtr = "\p" because of a problem in old versions of File Sharing. This change affected the following routines: (in FileCopy) GetDestinationDirInfo, (in MoreDesktopMgr) GetCommentID, (in MoreFilesExtras) GetDInfo, SetDInfo, GetDirID, GetDirName, GetParentID, GetObjectLocation, CheckObjectLock, BumpDate, ChangeFDFlags, CopyFileMgrAttributes, (in Search) GetDirModDate.
¥  The static function GetDirModDate in Search was only returning the low word of the modification date. There was only a slight chance that this would cause a problem since IndexedSearch uses the modification date only to detect if something in the directory changed.
¥  In FileCopy, I was calling CopyFileMgrAttributes and passing it a pointer to the copyName string pointer instead of just passing copyName.
¥  Changed GetDirID so that it really returns the parent directory if the specified object is a file.
¥  FullPath wasn't in MoreFilesLib.o library files.
¥  HasFSpExchangeFilesCompatibilityFix and HasFSpCreateScriptSupportFix weren't static in FSpCompat.

Other changes:
¥  Changed Share, Unshare, and GetUGEntry to use (void *) in parameter block type cast. That lets it compile without warnings using both the old and new Files.h header files.
¥  Changed all cases of "spec: FSSpec" in the Pascal interfaces to "{CONST}VAR spec: FSSpec" so they will work with PowerPC Pascal compilers.
¥ MoreFiles Reference is still included.
________________________________________

v1.4  12/21/95

New Routines:
¥  Added directory scanning functions, IterateDirectory and FSpIterateDirectory in a new files IterateDirectory.c, IterateDirectory.h and IterateDirectory.p. Check these routines out. I think you'll find them useful.
¥  Added SetDefault and RestoreDefault to MoreFileExtras. These routines let you set the default directory for those times when you're stuck calling standard C I/O or a library that takes only a filename.
¥  Added XGetVInfo to MoreFilesExtras. Like GetVInfo, this routine returns the volume name, volume reference number, and free bytes on the volume; like HGetVInfo, XGetVInfo also returns the total number of bytes on the volume. Both freeBytes and totalBytes are UnsignedWide values so this routine can report volume size information for volumes up to 2 terabytes. XGetVInfo's glue code calls through to HGetVInfo if needed to provide backwards compatibility.

Bugs fixed:
¥  Fixed bug where FileCopy could pass NULL to CopyFileMgrAttributes if PBHCopyFile was used.
¥  Added 68K alignment to MoreDesktopMgr.c structs.
¥  Added 68K alignment to MoreFilesExtras.p records.

Other changes:
¥  Conditionalized FSpCompat.c with SystemSevenOrLater and SystemSevenFiveOrLater so the FSSpec compatibility code is only included when needed. It makes the code a little harder to read, but it removes 2K-3K of code and the overhead of additional calls when SystemSevenOrLater and SystemSevenFiveOrLater are true. See ConditionalMacros.h for an explanation of SystemSevenOrLater and SystemSevenFiveOrLater.
¥  Moved the code to create a full pathname from GetFullPath to FSpGetFullPath. This accomplished two goals: (1) when FSpGetFullPath is used, the input FSSpec isn't passed to FSMakeFSSpecCompat (it was already an FSSpec) and (2) the code is now smaller. While I was in the code, I changed a couple of calls from Munger to PtrToHand.
¥  Changed GetDirID to GetDirectoryID so that MoreFiles could be used by MacApp programs (There's a MacApp method named GetDirID).
¥  Changed DirIDFromFSSpec to FSpGetDirectoryID to be consistent in naming conventions.
¥  Added macros wrapped with "#if OLDROUTINENAMES É #endif" so GetDirID and DirIDFromFSSpec will still work with your old code.
¥  Changed alignment #if's to use PRAGMA_ALIGN_SUPPORTED instead of GENERATINGPOWERPC (will they ever stop changing?).
¥  Changed all occurances of filler2 to ioACUser to match the change in Universal Interfaces 2.1.
¥  Added type-casts from "void *" to "Ptr" in various places to get rid of compiler warnings.
¥  Added CallCopyErrProc to DirectoryCopy.h and DirectoryCopy.c (it just looks better that way).
¥  Added the "__cplusplus" conditional code around all .h header files so they'll work with C++.
¥  Built libraries with Metrowerks, THINK C, and MPW so everyone can link.

________________________________________

v1.4.1  1/6/96

Bugs fixed:
¥  Fixed the conditionalized code FSpCreateCompat.
¥  Fixed the conditionalized code FSpExchangeFilesCompat.
¥  Fixed the conditionalized code FSpCreateResFileCompat.

Other changes:
¥  Changed PBStatus(&pb, false) to PBStatusSync(&pb) in GetDiskBlocks.
________________________________________

v1.4.2  3/25/96

New Routines:
¥  Added FSpResolveFileIDRef to MoreFiles.
¥  Added GetIOACUser and FSpGetIOACUser to MoreFilesExtras. These routines let you get a directory's access privileges for the current user.
¥  Added bit masks, macros, and function for testing ioACUser values to MoreFilesExtras.h and MoreFilesExtras.p.
¥  Added GetVolumeInfoNoName to MoreFilesExtras to put common calls to PBHGetVInfo in one place. Functions that call GetVolumeInfoNoName are: (in DirectoryCopy.c) PreflightDirectoryCopySpace, (in FileCopy.c) PreflightFileCopySpace, (in MoreFilesExtras.c) DetermineVRefNum, CheckVolLock, FindDrive, UnmountAndEject, (in Search.c) CheckVol.
¥  Added GetCatInfoNoName to MoreFilesExtras to put common calls to PBGetCatInfo in one place. Functions that call GetCatInfoNoName are: (in FileCopy.c) GetDestinationDirInfo, (in MoreDesktopMgr.c) GetCommentID, (in MoreFilesExtras.c) GetDInfo, GetDirectoryID, CheckObjectLock.
¥  Added TruncPString to MoreFilesExtras. This lets you shorten a Pascal string without breaking the string in the middle of a multi-byte character.
¥  Added FilteredDirectoryCopy and FSpFilteredDirectoryCopy to DirectoryCopy. FilteredDirectoryCopy and FSpFilteredDirectoryCopy work just like DirectoryCopy and FSpDirectoryCopy only they both take an optional CopyFilterProc parameter which can point to routine you supply. The CopyFilterProc lets your code decide what files or directories are copied to the destination. DirectoryCopy and FSpDirectoryCopy now call through to FilteredDirectoryCopy with a NULL CopyFilterProc.

Bugs fixed:
¥  Fixed minor bug in GetDiskBlocks where driveQElementPtr->dQRefNum was checked when driveQElementPtr could be NULL.
¥  DirectoryCopy didn't handle error conditions correctly. In some cases, DirectoryCopy would return noErr when there was a problem and in other cases, the CopyErrProc wasn't called and the function immediately failed.
¥  The result of DirectoryCopy's CopyErrProc was documented incorrectly.

Other changes and improvements:
¥  Added result codes to function descriptions in the C header files (these probably aren't a perfect list of possible errors, but they should catch most of the results you'll ever see).
¥  Removed most of the function descriptions in Pascal interface files since they haven't been completely in sync with the C headers for some time and I don't have time to keep the documentation in both places up to date.
¥  Rewrote HMoveRenameCompat so it doesn't use the Temporary Items folder.
¥  Added parameter checking to OnLine so that it doesn't allow the volIndex parameter to be less than or equal to 0.
¥  Added parameter checking to GetDirItems so that it doesn't allow the itemIndex parameter to be less than or equal to 0.
¥  FSpExchangeFilesCompat now returns diffVolErr (instead of paramErr)	if the source and the destination are on different volumes.
¥  Changed GetDirName's name parameter to Str31 and added parameter checking so that it doesn't allow a NULL name parameter.
¥  Forced errors returned by MoreDesktopMgr routines to be closer to what would be expected if the low-level Desktop Manager calls were used.
¥  Added conditionalized changes from Fabrizio Oddone so that Pascal calling conventions can be easily disabled. Disabling Pascal calling conventions reduces the code size slightly and allows C compilers to optimize parameter passing. NOTE: If you disable Pascal calling conventions, you'll have to remove the "pascal" keyword from all of the MoreFiles callbacks you've defined in your code.
¥  Changed DirectoryCopy so that you can copy the source directory's content to a disk's root directory.
¥  Added a build script and a make file for MPW libraries.
¥  Added a build script for Metrowerks CodeWarrior libraries.
¥  Added a build script for Symantec THINK Project Manager and Symantec Project Manager libraries.
¥  Renamed the Symantec and Metrowerks project files.
¥  Changed MoreFile's directory structure so that C headers, Pascal interfaces, and the source code aren't in the main directory.

Thanks to Fabrizio Oddone for supplying the conditionalized changes that optionally remove Pascal calling conventions. Thanks to Byron Han for beating the bugs out of DirectoryCopy and for suggesting and prototyping the changes needed for the "copy to root directory" option and the FilteredDirectoryCopy routine in DirectoryCopy.
________________________________________

Thanks again (you know who you are) for the bug reports and suggestions that helped improve MoreFiles since the last version(s). If you find any more bugs or have any comments, please let us know at:

AppleLink: DEVFEEDBACK

Internet: devfeedback@AppleLink.Apple.com

Please put "Attn: MoreFiles, DTS/ADS group" in the message title.
