/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is Basic Socket Library
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. Copyright (C) 1999
 * New Dimenstions Consulting, Inc. All Rights Reserved.
 *
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 */

#include <stdio.h>

#include "prerror.h"
#include "prmem.h"
#include "prlog.h"

#include "bspubtd.h"
#include "bserror.h"

const char *bs_errormsg_map[BSERR_MAP_SIZE] = {
    "Undefined Error",
    "Out of memory",
    "Parameter mismatch",
    "Property not supported",
    "Connect timed out",
    "Property is Read-Only",
    "Invalid State"
};

void
BS_ReportError (bsint err)
{    

    fprintf (stderr, "BS Error (%i): %s\n", err, bs_errormsg_map[err]);
#ifdef DEBUG
    if (err == BSERR_OUT_OF_MEMORY)
        PR_ASSERT (PR_FALSE);
#endif
    
}

void
BS_ReportPRError (bsint err)
{
    const char *name;
    const char *msg;

    name = PR_ErrorToName (err);
    msg = PR_ErrorToString (err, 0);
    fprintf (stderr, "NSPR Error (%i/%s): %s\n", err, name, msg);

}

void
BS_old_ReportPRError (bsint err)
{    
    char *msg;
    bsuint c;

    c = PR_GetErrorTextLength();
    if (!c)
    {
        fprintf (stderr, "Unknown NSPR Error (%i)\n", err);
        return;
    }

    msg = PR_Malloc (c * (sizeof (bschar *) + 1));
    
    if (!msg)
    {
        fprintf (stderr, "Out of Memory while reporting NSPR error %i\n", err);
        return;
    }
    PR_GetErrorText (msg);
    
    fprintf (stderr, "NSPR Error (%i): %s\n", err, msg);
    
    PR_Free (msg);

}
