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
#include <string.h>

#include "prtypes.h"
#include "prmem.h"
#include "prprf.h"
#include "prnetdb.h"

#include "bspubtd.h"
#include "bserror.h"

PR_BEGIN_EXTERN_C

PRBool
bs_util_is_ip (char *str)
{
    bsuint length;
    bsuint decimals = 0;
    int i;
    
    length = strlen (str);

    for (i = 0; i < length; i++)
    {
        switch (str[i])
        {
            case '.':
                decimals++;
                if (decimals > 3)
                    return PR_FALSE;
                break;

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                break;

            default:
                /* bad character, bail */
                return PR_FALSE;
        }
    }

    return PR_TRUE;
    
}

PRStatus
bs_util_resolve_host (const char *hostname, PRNetAddr *na)
{
    PRStatus  rv;
    bschar    buf[PR_NETDB_BUF_SIZE];
    PRHostEnt he;

    rv = PR_GetHostByName (hostname, buf, PR_NETDB_BUF_SIZE, &he);
    if (PR_FAILURE == rv)
        return rv;

    return PR_EnumerateHostEnt (0, &he, 0, na);
    
}

bschar *
bs_util_linebuffer (bschar *newline, bschar **buffer, PRBool flush)
{
    if (!newline)
        return NULL;

    *buffer = PR_sprintf_append(*buffer, "%s", newline);

    if (!buffer)
    {
        BS_ReportError (BSERR_OUT_OF_MEMORY);
        return NULL;
    }
    
    if ((newline[strlen(newline) - 1] == '\n') || flush)
        return *buffer;
    else
        return NULL;
    
}

/*
 * turns a buffer full of |delim| delimeted lines into
 * an array of \00 delimeted strings.  Make sure to
 * free the return value AND the longbuf when you're done with them.
 */
char **
bs_util_delimbuffer_to_array (char *longbuf, int *lines, char delim)
{
    int buflen;
    int i, line;
    char **lineary;

    *lines = 0;
    buflen = strlen (longbuf);

    for (i=0; i<buflen; i++) 
        if (longbuf[i] == delim)
            (*lines)++;

    if (longbuf[buflen - 1] != delim)
        (*lines)++;

    lineary = (char **) PR_Malloc ((*lines) * sizeof (char *));
    if (!lineary)
        return NULL;

    i=0;
    line=0;

    while (i<buflen)
    {
        lineary[line++] = &longbuf[i];
        
        while ((longbuf[i] != delim) && (i < buflen)) i++;
        
        if (i < buflen) longbuf[i] = '\00';
        i++;
    }
    
    return lineary;
    
}

PR_END_EXTERN_C
