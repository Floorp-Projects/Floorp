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

#ifndef __ExtensionGenerator_H
#define __ExtensionGenerator_H

/*  Author:
 *      Garrett Arch Blythe
 *      blythe@netscape.com
 */

/*--------------------------------------------------------------------------**
** Flags passed into the EXT_* functions.                                   **
**                                                                          **
** EXT_PRESERVE_EXT        The generated extension must be based on the     **
**                             provided file/url name.                      **
** EXT_PRESERVE_MIME       The generated extension must be based on the     **
**                             provided mime type.                          **
** EXT_SHELL_EXECUTABLE    The generated extension must be ShellExecutable. **
** EXT_NO_PERIOD           The generated extension will not contain a       **
**                             leading period.                              **
** EXT_DOT_THREE           Limit generated extension to 3 characters for    **
**                             16 bit operating system compatibility.       **
** EXT_EXECUTABLE_IDENTITY Only has meaning when used in conjunction with   **
**                             EXT_SHELL_EXECUTABLE.  When specified, it    **
**                             will allow EXT_SHELL_EXECUTABLE flag to      **
**                             succeed when the extension generated itself  **
**                             is executable (.exe, .bat, .lnk, etc.).      **
**                             This behavior is off by default.             **
**--------------------------------------------------------------------------*/
#define EXT_PRESERVE_EXT        0x00000001UL
#define EXT_PRESERVE_MIME       0x00000002UL
#define EXT_SHELL_EXECUTABLE    0x00000004UL
#define EXT_NO_PERIOD           0x00000008UL
#define EXT_DOT_THREE           0x00000010UL
#define EXT_EXECUTABLE_IDENTITY 0x00000020UL

/*-----------------------------------------------------------------------**
** EXT_Invent      Autogenerate an extension from provided criteria.     **
**                 Uses a complex approach, attempting to find the       **
**                     best possible extension under any given           **
**                     circumstance when an extension is abosulutely     **
**                     needed.                                           **
**                 This function mainly calls EXT_Generate will certain  **
**                     combinations of flags in a certain priority.      **
**                     READS:  Heuristic, don't muck with this casually. **
**                                                                       **
** Returns size_t  0 on failure or no extension available.               **
**                 Number of bytes written to pOutExtension on success.  **
**                 If pOutExtension is NULL, then number of bytes needed **
**                     not including terminating NULL character.         **
**                                                                       **
** pOutExtension   Returned extension once generated.  Can be NULL if    **
**                     querying size of buffer needed.                   **
** stExtBufSize    Size in bytes of pOutExtension.  Ignored if           **
**                     pOutExtension is NULL.                            **
** dwFlags         Combination of flags indicating which algorithm to    **
**                     use to generate the extension.  The only flags    **
**                     currently allowed are EXT_NO_PERIOD and           **
**                     EXT_DOT_THREE.                                    **
** pOrigName       File, URL, or .EXT which we are deriving the          **
**                     extension from.  Can be NULL.                     **
** pMimeType       Mime type which we are deriving the extension from.   **
**                     Can be NULL.                                      **
**-----------------------------------------------------------------------*/
size_t EXT_Invent(char *pOutExtension, size_t stExtBufSize, DWORD dwFlags, const char *pOrigName, const char *pMimeType);

/*-----------------------------------------------------------------------**
** EXT_Generate    Autogenerate an extension from provided criteria.     **
**                                                                       **
** Returns size_t  0 on failure or no extension available.               **
**                 Number of bytes written to pOutExtension on success.  **
**                 If pOutExtension is NULL, then number of bytes needed **
**                     not including terminating NULL character.         **
**                                                                       **
** pOutExtension   Returned extension once generated.  Can be NULL if    **
**                     querying size of buffer needed.                   **
** stExtBufSize    Size in bytes of pOutExtension.  Ignored if           **
**                     pOutExtension is NULL.                            **
** dwFlags         Combination of flags indicating which algorithm to    **
**                     use to generate the extension.  All flags are     **
**                      allowed.                                         **
** pOrigName       File, URL, or .EXT which we are deriving the          **
**                     extension from.  Can be NULL.                     **
** pMimeType       Mime type which we are deriving the extension from.   **
**                     Can be NULL.                                      **
**-----------------------------------------------------------------------*/
size_t EXT_Generate(char *pOutExtension, size_t stExtBufSize, DWORD dwFlags, const char *pOrigName, const char *pMimeType);

#endif // __ExtensionGenerator_H

