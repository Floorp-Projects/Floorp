/* 
 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s): 
*/

package org.mozilla.dom.tests;

import java.applet.Applet;
import org.w3c.dom.Document;
import org.mozilla.dom.DOMAccessor;
import org.mozilla.dom.util.DOMTreeDumper;
import org.mozilla.dom.util.GenericDocLoadListener;
import org.mozilla.dom.test.TestLoader;

public class TestRunnerApplet extends Applet {

    private final String name = "TestRunnerApplet";

    public void init()
    {
        DOMAccessor.addDocumentLoadListener(new TestRunnerListener(name));
	System.out.println(name + " init...");
    }

    private void runTests(Document doc) {
	Object obj = (Object) doc;
	TestLoader tl = new TestLoader(obj, 0);
	if (tl != null) {
	    Object retobj = tl.loadTest();
	}
    }

    class TestRunnerListener extends GenericDocLoadListener {
	public TestRunnerListener(String name) {
	    super(name);
	}

	public void endDocumentLoad(String url, int status, Document doc) {
	    runTests(doc);
	}
    }
}
