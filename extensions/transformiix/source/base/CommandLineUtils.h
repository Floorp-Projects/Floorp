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
 * $Id: CommandLineUtils.h,v 1.4 2001/04/08 14:38:03 peterv%netscape.com Exp $
 */

#ifndef TRANSFRMX_COMMANDLINEUTILS_H
#define TRANSFRMX_COMMANDLINEUTILS_H

#include "StringList.h"
#include "NamedMap.h"

/**
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.4 $ $Date: 2001/04/08 14:38:03 $
**/
class CommandLineUtils {

public:
   static void getOptions
        (NamedMap& options, int argc, char** argv, StringList& flags);

};

#endif
