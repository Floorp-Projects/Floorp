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

#ifndef _XPGETSTR_H_
#define _XPGETSTR_H_
#include "prtypes.h"

XP_BEGIN_PROTOS

/**@name Platform Independent String Resources */
/*@{*/
/**
 * Get a cross platform string resource by ID.
 *
 * This function makes localization easier for cross platform strings.
 * The cross platfrom string resources are defined in allxpstr.h.
 * You should use XP_GetString when:
 * <OL>
 * <LI>Any human readable string that not in front-end
 * <LI>With the exception of HTML strings (use XP_GetStringForHTML)
 * <LI>The translator/localizer will then translate the string defined
 * in allxpstr.h and show the translated version to user.
 * </OL>
 * The caller should make a copy of the returned string if it needs to use
 * it for a while. The same memory buffer will be used to store 
 * another string the next time this function is called. The caller 
 * does not need to free the memory of the returned string.
 * @param id	Specifies the string resource ID
 * @return		Localized (translated) string 
 * @see XP_GetStringForHTML
 * @see INTL_ResourceCharSet
 */
PUBLIC char *XP_GetString(int id);

/**
 * Get a cross platform HTML string resource by ID.
 *
 * This function makes localization easier for cross platform strings used
 * for generating HTML.  The cross platfrom string resources are defined in
 * allxpstr.h.  You should use XP_GetStringForHTML when:
 * <OL>
 * <LI>Human readable string not defined in front-end
 * <LI>The code generates HTML page and will go into HTML window
 * <LI>Only use this when the message will be generated into HTML.
 * <LI>Only use this if you can access to the winCharSetID.
 * <LI>This is needed because half of the text is generated from resource
 * (in resource charset) and half of the text is coming from the net
 * (in winCharSetID charset). When we meet this kind of mixing charset 
 * condition. We use this function instead of XP_GetString().
 * </OL>
 *
 * The code checks the current CharSetID in the resource and the
 * CharSetID of the data from the net.  If they are equal, it returns the
 * string defined in the resource, otherwise, it will return the English
 * version. So a French client can display French if the data from the
 * net is in the CharSetID of French and it will use half English and half 
 * Japanese if the French client receives Japanese data from the net.
 *
 * The caller should make a copy of the returned string if it needs to use
 * it for a while. The same memory buffer will be used to store 
 * another string the next time this function get called. The caller 
 * does not need to free the memory of the returned string.
 *  
 * @param id			Specifies the string resource ID
 * @param winCharSetID	Specifies the winCharSetID of the HTML 
 * @param english		Specifies the English string
 * @return				Localized (translated) string if the winCharSetID
 *						matches the CharSetID of the resource. Otherwise it
 *						returns the English message or the English string 
 * @see XP_GetStringForHTML
 * @see INTL_ResourceCharSet
 */
PUBLIC char *XP_GetStringForHTML(
    int id, 
    int16 winCharSetID, 
    char* english
);
/*@}*/

XP_END_PROTOS

#endif
