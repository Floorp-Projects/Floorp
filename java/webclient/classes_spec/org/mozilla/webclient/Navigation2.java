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
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *               Ann Sunhachawee
 *               Brian Satterfield <bsatterf@atl.lmco.com>
 *               Anthony Sizer <sizera@yahoo.com>
 */

package org.mozilla.webclient;

import java.io.InputStream;
import java.util.Properties;

public interface Navigation2 extends Navigation
{

/**

 * @param absoluteUrl, must be absolute

 * @param target null at the moment

 * @param postData must end with a \r\n

 * @param postHeaders Each header, including the last, must be of the
 * form "Name: value\r\n".  The last header must have an additional
 * "\r\n".

 */
public void post(String  absoluteUrl,
                 String  target,
                 String  postData,
                 String  postHeaders);

public void loadURLBlocking(String absoluteURL);

public void loadFromStreamBlocking(InputStream stream, String uri,
                                   String contentType, int contentLength,
                                   Properties loadInfo);


} 
// end of interface Navigation2
