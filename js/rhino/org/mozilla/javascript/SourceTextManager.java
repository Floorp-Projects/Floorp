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

// DEBUG API class

package org.mozilla.javascript;

import java.util.Enumeration;

/**
 * This interface supports managing incrementally updated source text.
 *
 * @see org.mozilla.javascript.SourceTextItem
 * @see org.mozilla.javascript.Context#setSourceTextManager
 * @author John Bandhauer
 */

public interface SourceTextManager
{
    /**
     * Create a new item.
     * <p>
     * A new item is always created. If an item with this name already exists, 
     * then that preexisting iten is is set as invalid. 
     *
     * @param name item name - in most embedings this will be a URL or filename
     * @return new item
     */
    public SourceTextItem newItem(String name);

    /**
     * Get an existing item.
     * <p>
     * If an item with this name already exists, then return it. Otherwise, 
     * return null.
     *
     * @param name item name - in most embedings this will be a URL or filename
     * @return existing item (or null if none)
     */
    public SourceTextItem getItem(String name);

    /**
     * Get all items.
     * <p>
     * Takes a snapshot of the list of all items and returns an Enumeration.
     *
     * @return snapshot Enumeration of all items
     */
    public Enumeration     getAllItems();
}    
