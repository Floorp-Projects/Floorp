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

/**
 * <p>Get information about and perform operations on the page currently
 * being shown in the {@link BrowserControlCanvas} for the {@link
 * BrowserControl} instance from which this <code>CurrentPage</code>
 * instance was obtained.</p>
 */

public interface CurrentPage {

    /**
     * <p>Copy the current selection to the system clipboard so its
     * contents can be obtained using the standard Java
     * <code>Toolkit.getDefaultToolkit().getSystemClipboard()</code>
     * method.  The selection that is copied may be made either by the
     * user, or programmatically via the {@link #selectAll} or {@link
     * CurrentPage2#highlightSelection} methods. </p>
     */ 
    public void copyCurrentSelectionToSystemClipboard();

    /**
     * <p>Search for the argument <code>stringToFind</code> in the
     * current page, highlighting it and scrolling the view to show
     * it.</p>
     *
     * @param stringToFind the search string
     *
     * @param forward if <code>true</code>, search forward from the
     * previous hit
     *
     * @param matchCase if <code>true</code>, the case must match in
     * order to be considered a hit.
     *
     * @deprecated this method has been replaced by {@link
     * CurrentPage2#find}.
     */
            
    public void findInPage(String stringToFind, boolean forward, boolean matchCase);

    /**
     * <p>Find the next occurrence of the String found with {@link
     * #findInPage}.</p>
     * 
     * @deprecated this method has been replaced by {@link
     * CurrentPage2#findNext}.
     */

    public void findNextInPage();

    /**
     * <p>Return the <code>URL</code> of the document currently
     * showing.</p>
     */
            
    public String getCurrentURL();

    /**
     * <p>Return a W3C DOM <code>Document</code> of the document currently
     * showing.</p>
     */
    
    public Document getDOM();

    /**
     * <p><b>Unimplemented</b> Return meta-information for the current
     * page.</p>
     */

    public Properties getPageInfo();
            
    /**
     * <p>Return the source for the current page as a String.</p>
     */ 
    public String getSource();
 
    /**
     * <p>Return the source for the current page as a <code>byte</code>
     * array.</p>
     */ 
    public byte [] getSourceBytes();

    /**
     * <p>Reset the find so that the next find starts from the beginning
     * or end of the page.</p>
     */
            
    public void resetFind();
            
    /**
     * <p>Tell the underlying browser to select all text in the current
     * page.</p>
     */

    public void selectAll();
} 
// end of interface CurrentPage
