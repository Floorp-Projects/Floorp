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
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 */

package org.mozilla.webclient.test;

/*
 * EmbeddedMozilla.java
 */

import org.mozilla.webclient.*;
import org.mozilla.util.Assert;

/**
 *

 * This is a test application for using the BrowserControl.

 *
 * @version $Id: EmbeddedMozilla.java,v 1.6 2001/05/23 22:26:52 edburns%acm.org Exp $
 * 
 * @see	org.mozilla.webclient.BrowserControlFactory

 */

public interface EmbeddedMozilla 
{

public void CreateEMWindow();
public void DestroyEMWindow(int winNumber);

}

// EOF
