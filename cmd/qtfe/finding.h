/* $Id: finding.h,v 1.1 1998/09/25 18:01:33 ramiro%netscape.com Exp $
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

#ifndef _FINDING_H
#define _FINDING_H

#include <structs.h>
#include <ntypes.h>
class QWidget;

#define FE_FIND_FOUND			0
#define FE_FIND_NOTFOUND		1
#define FE_FIND_CANCEL			2
#define FE_FIND_HEADER_FOUND		3
#define FE_FIND_HEADER_NOTFOUND		4
#define FE_FIND_NOSTRING		5


typedef struct fe_FindData
{
    MWContext *context;
    MWContext *context_to_find;	/* used for which frame cell to find in. */
    bool find_in_headers;
    bool case_sensitive_p, backward_p;
    QString string;
    QWidget* shell;
    LO_Element *start_element, *end_element;
    int32 start_pos, end_pos;

    fe_FindData()
	{
	    context_to_find = 0;
	    find_in_headers = false;
	    case_sensitive_p = false;
	    backward_p = false;
	    start_element = end_element = 0;
	    start_pos = end_pos = 0;
	}
} fe_FindData;

#endif
