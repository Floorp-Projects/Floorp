/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


/* intl_csi.h */

#ifndef INTL_CSI_H
#define INTL_CSI_H
/*
Using the i18n Character-Set-Information (CSI) accessor functions:
1) include the header file

  #include "intl_csi.h"

2) get the i18n CSI object (generally from MWContext)

  INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(context);

3) access (read/set) the data element

  int16 doc_csid = INTL_GetCSIDocCSID(csi);
  INTL_SetCSIDocCSID(csi, new_doc_csid);

  int16 win_csid = INTL_GetCSIWinCSID(csi);
  INTL_SetCSIWinCSID(csi, new_win_csid);

  char *mime_name = INTL_GetCSIMimeCharset(csi);
  INTL_SetCSIMimeCharset(csi, new_mime_charset);

  int16 relayout_flag = INTL_GetCSIRelayoutFlag(csi);
  INTL_SetCSIRelayoutFlag(csi, new_relayout_flag);

*/

XP_BEGIN_PROTOS

#include "ntypes.h"
#include "libi18n.h"

/**
 * Cookie for INTL_CSIInfo.
 * 
 * This is a Magic Cookie to validate the pointer to INTL_CSIInfo in MWContext.
 * 
 */
#define INTL_TAG  0x494E544C  

/**@name Character Set Information (CSI) */
/*@{*/
/**
 * Allocate a new charset info object, and clear it with zeroes.
 *
 * @return The new charset info object
 * @see INTL_CSIInitialize, INTL_CSIDestroy
 */
PUBLIC INTL_CharSetInfo INTL_CSICreate(void);

/**
 * Frees the given charset info object.
 *
 * @param obj  Specifies the charset info object
 * @see INTL_CSICreate
 */
PUBLIC void INTL_CSIDestroy(
    INTL_CharSetInfo obj
);

/**
 * Sets all the charset info object fields to initial values.
 *
 * The override, HTTP, HTML META, document and window charset IDs are all set
 * to CS_DEFAULT. The MIME charset is set to NULL. The relayout flag is set
 * to METACHARSET_NONE.
 *
 * @param obj  Specifies the charset info object
 * @see INTL_CSIInitialize
 */
PUBLIC void INTL_CSIReset(
    INTL_CharSetInfo obj
);

/**
 * Initializes the charset info object.
 *
 * <UL>
 * <LI>
 * If the given is_metacharset_reload argument is FALSE, INTL_CSIReset is
 * called, passing the given charset info object.
 *
 * <LI>
 * If the given is_metacharset_reload argument is TRUE, the charset info
 * object's relayout flag is set to METACHARSET_RELAYOUTDONE.
 *
 * <LI>
 * Otherwise, if the override charset ID is previously set in this object, 
 * it is set in the document charset ID field.
 *
 * <LI>
 * Otherwise, if the given HTTP charset is known, it is set in the HTTP
 * charset field.
 *
 * <LI>
 * Otherwise, the document charset ID field is set to the given
 * defaultDocCharSetID, unless the type is mail/news, in which case CS_DEFAULT
 * is used. This is because the META charset in mail/news is sometimes wrong.
 *
 * <LI>
 * Finally, the window charset ID is set, based on the document charset ID.
 * </UL>
 *
 * @param obj  Specifies the charset info object
 * @param is_metacharset_reload TRUE if it is currently reloading because 
 *                              the layout code found HTML META charset.
 *                              FALSE otherwise.
 * @param http_charset Specifies the charset name if it is presented in 
 *                     HTTP Content-Type header
 * @param type Specifies the context type
 * @param defaultDocCharSetID Specifies the default document charset ID.
 * @see
 */
PUBLIC void INTL_CSIInitialize(
    INTL_CharSetInfo obj,
    XP_Bool is_metacharset_reload, 
    char *http_charset,
    int type,
    uint16 defaultDocCharSetID
);

/**
 * Sets HTML META charset info in the given charset info object.
 *
 * <UL>
 * <LI>
 * If the given charset is unknown, this function returns. 
 * 
 * <LI>
 * If the given context type is mail or news, this function returns, 
 * since mail/news sometimes has wrong HTML META charsets.
 *
 * <LI>
 * If the relayout flag is set to something other than METACHARSET_NONE, this
 * function returns, to avoid setting the META charset more than once.
 *
 * <LI>
 * Otherwise, the HTML META charset field is set, and the relayout flag is
 * set to METACHARSET_HASCHARSET. 
 * If the previous document charset was known,
 * and was different from the new META charset, the relayout flag is set to
 * METACHARSET_REQUESTRELAYOUT. 
 * The window charset ID is also checked against the new one. 
 * If they are different, the relayout flag is set to
 * METACHARSET_REQUESTRELAYOUT.
 * </UL>
 *
 * @param obj  Specifies the charset info object
 * @param charset_tag  Specifies the HTML META charset
 * @param type         Specifies the context type
 * @see INTL_GetCSIMetaDocCSID, INTL_GetCSIRelayoutFlag
 */
PUBLIC void INTL_CSIReportMetaCharsetTag(
    INTL_CharSetInfo obj,
    char *charset_tag,
    int type
);

/**
 * Returns the context's charset info object.
 *
 * @param context  Specifies the context
 * @return The context's charset info object
 * @see INTL_CSICreate
 */
PUBLIC INTL_CharSetInfo LO_GetDocumentCharacterSetInfo(
    MWContext *context
);

/**
 * Returns the document charset ID of the given charset info object.
 *
 * @param obj  Specifies the charset info object
 * @return The document charset ID
 * @see INTL_SetCSIDocCSID
 */
PUBLIC int16 INTL_GetCSIDocCSID(
    INTL_CharSetInfo obj
);

/**
 * Sets the document charset ID field of the given charset info object.
 *
 * The document charset ID field is only set if the higher precedence fields
 * (override, HTTP and META) are all set to CS_DEFAULT.
 *
 * @param obj         Specifies the charset info object
 * @param docCharSetID  Specifies the document charset ID
 * @see INTL_GetCSIDocCSID
 */
PUBLIC void INTL_SetCSIDocCSID(
    INTL_CharSetInfo obj,
    int16 docCharSetID
);

/**
 * Returns the override document charset ID of the given charset info object.
 *
 * @param obj  Specifies the charset info object
 * @return The override document charset ID
 * @see INTL_SetCSIOverrideDocCSID
 */
PUBLIC int16 INTL_GetCSIOverrideDocCSID(
    INTL_CharSetInfo obj
);

/**
 * Sets the override document charset ID of the given charset info object.
 *
 * @param obj         Specifies the charset info object
 * @param overrideDocCharSetID  Specifies the override document charset ID
 * @see INTL_GetCSIOverrideDocCSID
 */
PUBLIC void INTL_SetCSIOverrideDocCSID(
    INTL_CharSetInfo obj,
    int16 overrideDocCharSetID
);

/**
 * Returns the HTML META document charset ID of the given charset info object.
 *
 * @param obj  Specifies the charset info object
 * @return The HTML META document charset ID
 * @see INTL_SetCSIMetaDocCSID
 */
PUBLIC int16 INTL_GetCSIMetaDocCSID(
    INTL_CharSetInfo obj
);

/**
 * Sets the HTML META document charset ID of the given charset info object.
 *
 * The HTML META document charset ID field is only set if the higher precedence
 * fields (override and HTTP) are all set to CS_DEFAULT.
 *
 * @param obj         Specifies the charset info object
 * @param metaCharSetID  Specifies the HTML META document charset ID
 * @see INTL_GetCSIMetaDocCSID
 */
PUBLIC void INTL_SetCSIMetaDocCSID(
    INTL_CharSetInfo obj,
    int16 metaCharSetID
);

/**
 * Returns the HTTP document charset ID of the given charset info object.
 *
 * @param obj  Specifies the charset info object
 * @return The HTTP document charset ID
 * @see INTL_SetCSIHTTPDocCSID
 */
PUBLIC int16 INTL_GetCSIHTTPDocCSID(
    INTL_CharSetInfo obj
);

/**
 * Sets the HTTP document charset ID of the given charset info object.
 *
 * The HTTP document charset ID field is only set if the higher precedence
 * field (override) is set to CS_DEFAULT.
 *
 * @param obj         Specifies the charset info object
 * @param httpDocCharSetID  Specifies the HTTP document charset ID
 * @see INTL_GetCSIHTTPDocCSID
 */
PUBLIC void INTL_SetCSIHTTPDocCSID(
    INTL_CharSetInfo obj,
    int16 httpDocCharSetID
);

/**
 * Returns the window charset ID of the given charset info object.
 *
 * @param obj  Specifies the charset info object
 * @return The window charset ID
 * @see INTL_SetCSIWinCSID
 */
PUBLIC int16 INTL_GetCSIWinCSID(
    INTL_CharSetInfo obj
);

/**
 * Sets the window charset ID field of the given charset info object.
 *
 * @param obj         Specifies the charset info object
 * @param winCharSetID  Specifies the window charset ID
 * @see INTL_GetCSIWinCSID
 */
PUBLIC void INTL_SetCSIWinCSID(
    INTL_CharSetInfo obj,
    int16 winCharSetID
);

/**
 * Returns the MIME charset field of the given charset info object.
 *
 * @param obj  Specifies the charset info object
 * @return The MIME charset
 * @see INTL_SetCSIMimeCharset
 */
PUBLIC char *INTL_GetCSIMimeCharset(
    INTL_CharSetInfo obj
);

/**
 * Sets the MIME charset field of the given charset info object.
 *
 * If the charset info object already contains a pointer to a MIME charset,
 * that charset is freed. Then the given charset is copied, and the copy is
 * converted to lower case. The copy is then set in the MIME charset field.
 *
 * @param obj         Specifies the charset info object
 * @param mime_charset  Specifies the MIME charset
 * @see INTL_GetCSIMimeCharset
 */
PUBLIC void INTL_SetCSIMimeCharset(
    INTL_CharSetInfo obj,
    char *mime_charset
);

/**
 * Returns the relayout field of the given charset info object.
 *
 * @param obj  Specifies the charset info object
 * @return The relayout field
 * @see INTL_SetCSIRelayoutFlag
 */
PUBLIC int16 INTL_GetCSIRelayoutFlag(
    INTL_CharSetInfo obj
);

/**
 * Sets the relayout field of the given charset info object.
 *
 * @param obj         Specifies the charset info object
 * @param relayout  Specifies the relayout field
 * @see INTL_GetCSIRelayoutFlag
 */
PUBLIC void INTL_SetCSIRelayoutFlag(
    INTL_CharSetInfo obj,
    int16 relayout
);
/*@}*/

XP_END_PROTOS

#endif /* INTL_CSI_H */
