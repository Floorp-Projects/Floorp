/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

public interface BookmarkEntry extends MutableTreeNode
{

/** 

 * Property names

 */

public final static String ADD_DATE = "AddDate";
public final static String LAST_MODIFIED_DATE = "LastModifiedDate";
public final static String LAST_VISIT_DATE = "LastVisitDate";
public final static String NAME = "Name";
public final static String URL = "URL";
public final static String DESCRIPTION = "Description";
public final static String IS_FOLDER = "IsFolder";

public Properties getProperties();

public boolean isFolder();


} 
// end of interface BookmarkEntry

