/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package org.mozilla.javascript.debug.util;

/**
 * Interface that is called on a given thread to respond to assertion failure.
 *
 * @see ER
 */

public interface AssertFailureHandler
{
    public static final int CONTINUE = 0;
    public static final int ABORT    = 1;
    public static final int DEBUG    = 3;

   /**
    * Called when assertion fails
    * @param msgRaw the raw message as passed to ER.T()
    * @param msgCooked the message generated in ER (includes msgRaw)
    * @param ob the Object (optionally) passed to ER.T(). MAY be null
    */
    public int assertFailed( String msgRaw, String msgCooked, Object ob );
}    
