/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 */

package grendel.composition;

import java.util.Vector;
import javax.swing.event.*;
import netscape.orion.misc.*;

public class TestDataSource2 extends ATC_SourceAdapter {
    String[] stringTable = {
            "Bob Schenefelt",
            "Tom Jones",
            "Tim Smith",
            "John Simon",
            "Jim",
            "Jimmy",
            "Les",
            "Lester",
            "Lesters@netscape.com",
            "Lester Schueler"
    };

    private Vector mSearchResults;      //holds search results. (see search)
    private int mCurrentIndex = 0;      //current index if search results.

    public TestDataSource2 () {
    }

    /**
     * get the next match from the results vector.
     */
    public String getNext () {
        if (mSearchResults.size() > 0) {
            mCurrentIndex = (mCurrentIndex + 1) % mSearchResults.size();
            return (String) mSearchResults.elementAt(mCurrentIndex);
        }

        return null;
    }

    /**
     * get the previous match from the results vector.
     */
    public String getPrevious () {
        if (mSearchResults.size() > 0) {
            mCurrentIndex = mCurrentIndex + mSearchResults.size() - 1;
            mCurrentIndex %= mSearchResults.size();
            return (String) mSearchResults.elementAt(mCurrentIndex);
        }

        return null;
    }

    /**
     * Gives notification the document has changed so perform a new search.
     */
    public void documentChanged(ATC_Document doc) {
        //get the text from the document to search on.
         String textToSearchFor = getDocumentText(doc);

        //find all matches for the string.
        findAllMatches (textToSearchFor);

        //Inform the document of how many strings were found.
        doc.setQueryResults(mSearchResults.size());
    }

    /**
     *  Find all partial matches for a search string.
     *  Store the result in the vector mSearchResults.
     */
    private void findAllMatches (String subString) {
        int subStringLength = subString.length();

        //store search results in s vector for later
        mSearchResults = new Vector ();
        mCurrentIndex = 0;

        //don't match emtpy substrings.
        if ((null == subString) || (subString.equals ("")))
            return;

        for (int i = 0; i < stringTable.length; i++) {
            if ((stringTable[i].length() >= subStringLength)  &&
                (stringTable[i].substring (0, subStringLength).equalsIgnoreCase (subString))) {

                //matching substring
                mSearchResults.addElement (stringTable[i].substring (subStringLength));
            }
        }

        return;
    }
}
