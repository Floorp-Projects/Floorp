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
 * Contributor(s):  Daniel Park <daepark@apmindsf.com>
 *                  Kyle Yuan <kyle.yuan@sun.com>
 */

package org.mozilla.webclient;

/**
 * <p>Extended current page functionality.</p>
 */

public interface CurrentPage2 extends CurrentPage {

    /**
     * <p>Return a {@link Selection} instance for the current selection.
     * This is the only way to obtain a <code>Selection</code>
     * instance.</p>
     */

    public Selection getSelection();

    /**
     * <p>Take the argument {@link Selection} and highlight it in the
     * current page.</p>
     *
     * @param selection the selection, obtained from {@link
     * #getSelection} to be highlighted.
     */
    
    public void highlightSelection(Selection selection);
    
    /**
     * <p>Make it so the current page has nothing selected.</p>
     */

    public void clearAllSelections();

    /**
     * <p>Pop up a native print dialog to allow the user to print the
     * current page.</p>
     */
    
    public void print();

    /**
     * <p>Turn the browser into print preview mode depending on the
     * value of the argument <code>preview</code>.</p>
     *
     * @param preview if true, turn the browser into print preview mode.
     * If false, turn the browser to normal view mode.
     */
    
    public void printPreview(boolean preview);

    /**
     * <p>Enhanced version of {@link CurrentPage#findInPage} that allows
     * the caller to discover the result of the find.</p>
     */
    
    public boolean find(String stringToFind, boolean forward, boolean matchCase);

    /**
     * <p>Enhanced version of {@link CurrentPage#findNextInPage} that allows
     * the caller to discover the result of the find.</p>
     */

    public boolean findNext();
    
    /**
     * <p>Like {@link CurrentPage#copyCurrentSelectionToSystemClipboard} but
     * puts the html of the selection on the clipboard, instead of the plain text
     * </p>
     */
    public void copyCurrentSelectionHtmlToSystemClipboard();
}
// end of interface CurrentPage2
