/* 
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
 * Contributor(s): Ashutosh Kulkarni <ashuk@eng.sun.com>
 *                 Ed Burns <edburns@acm.org>
 *
 */

package org.mozilla.webclient;

/**

 * This interface contains methods that pertain to the specific
 * underlying browser implemantation type.  These methods are the union
 * of all such methods for underlying browser implementations.  For
 * example, the ICE browser needs methods setICEProps(), getStormBase,
 * getViewPort().  In the mozilla implementation, these methods do
 * nothing.

 */

public interface BrowserControlICE extends BrowserControl
{

/**
 *  These 3 methods, setICEProps, getStormBase and getViewPort
 *  are for Webclient embedding the ICE browser. For Gecko or
 *  IE, these methods are to be ignored
 *
 */

public abstract void setICEProps(Object base, Object viewport);

public abstract Object getStormBase();

public abstract Object getViewPort();

} // end of interface BrowserControlICE
