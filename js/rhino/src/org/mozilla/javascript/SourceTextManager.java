/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * John Bandhauer
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
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
