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

/*

 * W3C® IPR SOFTWARE NOTICE

 * Copyright © 1994-2000 World Wide Web Consortium, (Massachusetts
 * Institute of Technology, Institut National de Recherche en
 * Informatique et en Automatique, Keio University). All Rights
 * Reserved. http://www.w3.org/Consortium/Legal/

 * This W3C work (including software, documents, or other related items) is
 * being provided by the copyright holders under the following
 * license. By obtaining, using and/or copying this work, you (the
 * licensee) agree that you have read, understood, and will comply with
 * the following terms and conditions:

 * Permission to use, copy, and modify this software and its documentation,
 * with or without modification, for any purpose and without fee or
 * royalty is hereby granted, provided that you include the following on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make:

 * The full text of this NOTICE in a location viewable to users of the
 * redistributed or derivative work.

 * Any pre-existing intellectual property disclaimers, notices, or terms
 * and conditions. If none exist, a short notice of the following form
 * (hypertext is preferred, text is permitted) should be used within the
 * body of any redistributed or derivative code: "Copyright ©
 * [$date-of-software] World Wide Web Consortium, (Massachusetts
 * Institute of Technology, Institut National de Recherche en
 * Informatique et en Automatique, Keio University). All Rights
 * Reserved. http://www.w3.org/Consortium/Legal/"

 * Notice of any changes or modifications to the W3C files, including
 * the date changes were made. (We recommend you provide URIs to the
 * location from which the code is derived.)

 * THIS SOFTWARE AND DOCUMENTATION IS PROVIDED "AS IS," AND COPYRIGHT
 * HOLDERS MAKE NO REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO, WARRANTIES OF MERCHANTABILITY OR
 * FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR
 * DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS, COPYRIGHTS,
 * TRADEMARKS OR OTHER RIGHTS.

 * COPYRIGHT HOLDERS WILL NOT BE LIABLE FOR ANY DIRECT, INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF ANY USE OF THE
 * SOFTWARE OR DOCUMENTATION.

 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders.

 */

package org.mozilla.webclient;

import java.util.Properties;
import org.w3c.dom.Document;

public interface CurrentPage
{
public void copyCurrentSelectionToSystemClipboard();
            
public void findInPage(String stringToFind, boolean forward, boolean matchCase);
            
public void findNextInPage(boolean forward);
            
public String getCurrentURL();
            
public Document getDOM();

public Properties getPageInfo();
            
public String getSource();
 
public byte [] getSourceBytes(boolean viewMode);
            
public void resetFind();
            
public void selectAll();
} 
// end of interface CurrentPage
