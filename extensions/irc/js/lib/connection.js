/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 **HERE
 *
 * The contents of this file are Copyright 1999 Robert G. Ginda,
 * rginda@ndcico.com.  This program may be freely copied and modified for
 * non-commercial use, provided everything between HERE and THERE appears
 * in the copy.
 * If you have questions, comments or suggestions direct them to
 * rginda@ndcico.com.
 *
 **THERE
 *
 * depends on utils.js, and the connection-*.js implementations.
 * 
 * loads an appropriate connection implementation, or dies trying.
 *
 */

function connection_init(libPath)
{
    
    if (jsenv.HAS_XPCOM)
        load (libPath + "connection-xpcom.js");
    else if (jsenv.HAS_RHINO)
        load (libPath + "connection-rhino.js");
    else
    {
        dd ("No connection object for this platform.");
        return false;
    }

    return true;

}


