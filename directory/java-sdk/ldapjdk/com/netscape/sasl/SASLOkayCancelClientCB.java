/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
package com.netscape.sasl;

/**
 * Present information to a user and return an indication that an operation
 * is to proceed or to be cancelled.
 */
public interface SASLOkayCancelClientCB extends SASLClientCB {

    /**
     * Presents the user with the supplied textual information. The return
     * value is true to continue operations, false to abort. This may be
     * implemented with OK and CANCEL buttons in a dialog. If okText and/or
     * cancelText are non-null and not empty, they may be used to label buttons.
     * 
     * @return true to continue operations, false to abort.
     */
    public boolean promptOkayCancel(String prompt, String okText, 
        String cancelText);
}

