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

import java.util.Properties;
import javax.swing.tree.MutableTreeNode;

/**
 * <p>The <code>TreeModel</code> returned by {@link
 * Bookmarks#getBookmarks} is composed of instances of this class.</p>
 *
 * <p>New instances of this class may only be created with {@link
 * Bookmarks#newBookmarkEntry}.  Instances may be added to and removed
 * from the bookmarks tree by calling {@link Bookmarks#addBookmark} and
 * {@link Bookmarks#removeBookmark} respectively.  A special entry that is a
 * folder may be created with {@link Bookmarks#newBookmarkFolder}.</p>
 *
 * <p>The meta-data for the bookmark is contained in a
 * <code>Properties</code></p> instance returned from {@link
 * #getProperties}.  The constants for this class signify the valid keys
 * for this <code>Properties</code> table.
 *
 */ 

public interface BookmarkEntry extends MutableTreeNode {
    
    /**
     * <p>The date on which this bookmark entry was added.</p>
     */
    
    public final static String ADD_DATE = "AddDate";

    /**
     * <p>The date on which this bookmark entry was last modified.</p>
     */

    public final static String LAST_MODIFIED_DATE = "LastModifiedDate";

    /**
     * <p>The date on which this bookmark entry was last visited by the
     * browser.</p>
     */

    public final static String LAST_VISIT_DATE = "LastVisitDate";

    /**
     * <p>The name of this bookmark entry.</p>
     */

    public final static String NAME = "Name";

    /**
     * <p>The URL of this bookmark entry.</p>
     */

    public final static String URL = "URL";

    /**
     * <p>The optional user given description of this bookmark
     * entry.</p>
     */

    public final static String DESCRIPTION = "Description";

    /**
     * <p>If this bookmark entry is a folder, the value of this property
     * is the string "<code>true</code>" without the quotes.</p>
     */

    public final static String IS_FOLDER = "IsFolder";

    /**
     * <p>Returns the <code>Properties</code> for this instance.  Valid
     * keys are given by the constants of this interface.  Other keys
     * may exist depending on the implementation.</p>
     */
    
    public Properties getProperties();

    /**
     * <p>Returns <code>true</code> if and only if this instance is a
     * bookmark folder.</p>
     */

    public boolean isFolder();
} 
// end of interface BookmarkEntry

