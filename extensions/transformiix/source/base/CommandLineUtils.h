/*
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
 * The Original Code is TransforMiiX XSLT processor.
 * 
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * $Id: CommandLineUtils.h,v 1.2 1999/11/15 07:12:39 nisheeth%netscape.com Exp $
 */

#include "baseutils.h"
#include "StringList.h"
#include "NamedMap.h"

#ifndef MITRE_COMMANDLINEUTILS_H
#define MITRE_COMMANDLINEUTILS_H

/**
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.2 $ $Date: 1999/11/15 07:12:39 $
**/
class CommandLineUtils {

public:
   static void getOptions
        (NamedMap& options, int argc, char** argv, StringList& flags);

};

#endif
