/* 
The contents of this file are subject to the Mozilla Public License
Version 1.0 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
the License for the specific language governing rights and limitations
under the License.

The Initial Developer of the Original Code is Sun Microsystems,
Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
Inc. All Rights Reserved. 
*/

package org.mozilla.dom;

import org.w3c.dom.Document;

public interface DocumentLoadListener {

    public void startURLLoad(String url, String contentType, Document doc);
    public void endURLLoad(String url, int status, Document doc);
    public void progressURLLoad(String url, int progress, int progressMax, 
				Document doc);
    public void statusURLLoad(String url, String message, Document doc);

    public void startDocumentLoad(String url);
    public void endDocumentLoad(String url, int status, Document doc);
}
