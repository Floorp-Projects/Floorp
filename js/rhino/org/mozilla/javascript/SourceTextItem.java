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

/**
 * This interface supports managing incrementally updated source text items.
 * <p>
 * Items have immutable names. They can be valid or invalid. They can 
 * accumulate text, but existing text can not be overwritten. They can be 
 * marked as 'complete', 'aborted', etc. They can be cleared of all text. 
 * See status flags for details.
 *
 * @see org.mozilla.javascript.SourceTextManager
 * @author John Bandhauer
 */

public interface SourceTextItem
{

    /**
     * Possible status values...
     */

    /**
     * Item just created, no text added yet.
     */
    public static final int INITED    = 0;

    /**
     * Item has some text, likely that more will be added.
     */
    public static final int PARTIAL   = 1;

    /**
     * Item has all the text it is going to get.
     */
    public static final int COMPLETED = 2;

    /**
     * User aborted loading of text, some text may be in item.
     */
    public static final int ABORTED   = 3;

    /**
     * Loading of text failed, some text may be in item.
     */
    public static final int FAILED    = 4;

    /**
     * Whatever text was in item has been cleared by a consumer.
     */
    public static final int CLEARED   = 5;

    /**
     * Item has be marked as invalid and has no useful information.
     */
    public static final int INVALID   = 6;

    /**
     * Append some text.
     * <p>
     * This only succeeds if status is INITED or PARTIAL.
     *
     * @param text String to append
     * @return true if succeeded, false if failed
     */
    public boolean append(String text);

    /**
     * Append a char.
     * <p>
     * This only succeeds if status is INITED or PARTIAL.
     *
     * @param c char to append
     * @return true if succeeded, false if failed
     */
    public boolean append(char c);

    /**
     * Append a char from a char[].
     * <p>
     * This only succeeds if status is INITED or PARTIAL.
     *
     * @param buf char[] from which to append
     * @param offset offset into char[] from which to append
     * @param count count of chars to append
     * @return true if succeeded, false if failed
     */
    public boolean append(char[] buf, int offset, int count);

    /**
     * Set status to COMPLETED.
     * <p>
     * meaning: all the text is there, it is complete, no problems.
     * This only succeeds if status is INITED or PARTIAL.
     *
     * @return true if succeeded, false if failed
     */
    public boolean complete();

    /**
     * Set status to ABORTED.
     * <p>
     * meaning: some text might be there, but user aborted text loading.
     * This only succeeds if status is INITED or PARTIAL.
     *
     * @return true if succeeded, false if failed
     */
    public boolean abort();

    /**
     * Set status to FAILED.
     * <p>
     * meaning: some text might be there, but loading failed.
     * This only succeeds if status is INITED or PARTIAL.
     *
     * @return true if succeeded, false if failed
     */
    public boolean fail();

    /**
     * Clear the text and set status to CLEARED.
     * <p>
     * meaning: consumer of the text has what he wants, leave this 
     * as an emptly placeholder.
     * This succeeds unless status is INVALID.
     *
     * @return true if succeeded, false if failed
     */
    public boolean clear();

    /**
     * Clear the text and set status to INVALID.
     * <p>
     * meaning: This item is not to be used, likely the SourceTextManager 
     * has been asked to create a new item (with potentially different 
     * text) in its place.
     * This succeeds unless status is INVALID.
     *
     * @return true if succeeded, false if failed
     */
    public boolean invalidate();

    /**
     * Get the Current Text.
     *
     * @return the text, null if INVALID
     */
    public String  getText();

    /**
     * Get the name.
     *
     * @return the name (immutable).
     */
    public String  getName();

    /**
     * Get the status.
     *
     * @return the current status
     */
    public int     getStatus();

    /**
     * Get the validity status.
     *
     * @return true if item is valid, false if not
     */
    public boolean isValid();

    /**
     * Get a counter representing the modification count of the item.
     * <p>
     * Any consumer of the item may look at this value and store it at one
     * point in time and then later look at the value again. If the 
     * value has increased, then the consumer can know that the item has 
     * been modified in some way and can then take the appropriate action.
     * If the count has not changed from one point in time to another, 
     * then the item is guarenteed to not have changed in any way.
     *
     * NOTE: this value is not guaranteed to start at 0;
     *
     * @return the alter count
     */
    public int     getAlterCount();
}
