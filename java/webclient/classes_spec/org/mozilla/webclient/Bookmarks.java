/* -*- Mode: java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s):  Ed Burns <edburns@acm.org>
 */

package org.mozilla.webclient;

import javax.swing.tree.TreeModel;

/**
 * <p>Provide an abstraction around the bookmarks facility provided by
 * the underlying browser.</p>
 */

public interface Bookmarks {
    
    /**
     * <p>Add the argument <code>bookmark</code> as a child of the
     * argument <code>mayBeNullParent</code>.  If
     * <code>mayBeNullParent</code>is null, the <code>bookmark</code> is
     * added as a child of the root node.</p>
     *
     * @param mayBeNullParent if non-<code>null</code> the parent of
     * this bookmark.  The parent must return <code>true</code> from
     * {@link BookmarkEntry#isFolder}.
     */
    
    public void addBookmark(BookmarkEntry mayBeNullParent, 
			    BookmarkEntry bookmark);

    /**
     * <p>Return a <code>TreeModel</code> representation of the
     * bookmarks for the current profile.</p>
     */
    
    public TreeModel getBookmarks() throws IllegalStateException;

    /**
     * <p>Remove the argument bookmark from its current position in the
     * tree.</p>
     */
            
    public void removeBookmark(BookmarkEntry bookmark);

    /**
     * <p>Create a new, un-attached {@link BookmarkEntry} instance
     * suitable for passing to {@link #addBookmark}.</p>
     */

    public BookmarkEntry newBookmarkEntry(String url);

    /**
     * <p>Create a new, un-attached {@link BookmarkEntry} instance
     * suitable for passing to {@link #addBookmark} that is a
     * folder for containing other <code>BookmarkEntry</code> instances.</p>
     */

    public BookmarkEntry newBookmarkFolder(String name);
            
} 
// end of interface Bookmarks

