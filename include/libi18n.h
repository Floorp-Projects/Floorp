/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


/* libi18n.h */


#ifndef INTL_LIBI18N_H
#define INTL_LIBI18N_H

#include "xp_core.h"
#include "csid.h"

XP_BEGIN_PROTOS

/**
 * Converts a piece of text from one charset to another.
 *
 * This function does not do charset ID auto-detection. The caller must pass
 * the from/to charset IDs. This function does not keep state. Don't use it to
 * convert a stream of data. Only use this when you want to convert a string,
 * and you have no way to hold on to the converter object.
 *
 * If the string gets converted in place (use the input buffer), then this
 * function returns NULL.
 *
 * @param fromcsid    Specifies the charset ID to convert from
 * @param tocsid      Specifies the charset ID to convert to
 * @param pSrc        Specifies the input text
 * @param block_size  Specifies the number of bytes in the input text
 * @return The converted text, null terminated, or NULL if converted in place
 * @see INTL_CallCharCodeConverter
 */
PUBLIC unsigned char *INTL_ConvertLineWithoutAutoDetect(
    int16 fromcsid,
    int16 tocsid,
    unsigned char *pSrc,
    uint32 block_size
);

/**
 * Returns the window charset ID corresponding to the given document charset ID.
 *
 * This function searches a built-in table to find the first entry that
 * matches the given document charset ID. If no such entry is found, it
 * returns CS_FE_ASCII.
 *
 * @param csid  Specifies the document charset ID
 * @return The corresponding window charset ID
 */
PUBLIC int16 INTL_DocToWinCharSetID(
    int16 csid
);

/*@}*/
/*=======================================================*/
/**@name CharSetID and Charset Name Mapping */
/*@{*/
/**
 * Returns the preferred MIME charset name corresponding to the given
 * charset ID.
 *
 * Charset names are registered by IANA (Internet Assigned Numbers Authority).
 * The current charset name database can be found at:
 *
 * <A HREF=ftp://ftp.isi.edu/in-notes/iana/assignments/character-sets>
 *         ftp://ftp.isi.edu/in-notes/iana/assignments/character-sets</A>.
 *
 * This function returns the charset name for the given Character Set ID
 * which in most cases corresponds to the "(preferred MIME name)" registered
 * with IANA.  This function may return private names not found in the
 * registered. Private names start with "x-". See INTL_CharSetNameToID for 
 * information about charset IDs.
 *
 * @param charSetID            Specifies the charset ID
 * @param charset_return  Returns the corresponding charset name, max 128 bytes
 * @see INTL_CharSetNameToID
 */
PUBLIC void INTL_CharSetIDToName(
    int16 charSetID,
    char *charset_return
);

/**
 * Returns the charset ID corresponding to the given charset name.
 *
 * The charset ID is a private 16-bit integer, described in
 * ns/include/csid.h. If the given charset is unknown, CS_UNKNOWN is returned.
 * If the given charset is NULL, CS_DEFAULT is returned. Charset names are not
 * case-sensitive. See INTL_CharSetIDToName for a description of charset names.
 *
 * @param charset  Specifies the charset name
 * @return the corresponding charset ID
 * @see INTL_CharSetIDToName
 */
PUBLIC int16 INTL_CharSetNameToID(
    char *charset
);

/**
 * Returns a pointer to the preferred MIME charset name corresponding 
 * to the given charset ID.
 *
 * This function is similar to INTL_CharSetIDToName. It returns a pointer to
 * the charset name. See INTL_CharSetIDToName for other details.
 *
 * @param charSetID  Specifies the charset ID
 * @return The corresponding charset name
 * @see INTL_CharSetIDToName
 */
PUBLIC unsigned char *INTL_CsidToCharsetNamePt(
    int16 charSetID
);

XP_END_PROTOS


#endif /* INTL_LIBI18N_H */



