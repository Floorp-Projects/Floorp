/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsCalTestStrings_h___
#define nsCalTestStrings_h___

#define ZULUCOMMAND_HELP "Valid JS commands are:\r\n\r\n" \
                         "  zulucommand(\"help\",\"help\");    Get even more help on help\r\n" \
                         "  zulucommand(\"help\",\"panel\");  Get help on specifying methods for panels\r\n"

#define ZULUCOMMAND_HELP_PANEL "***panel syntax is:***\r\n\r\n" \
                               "  zulucommand(\"panel\",\"name\",\"method\",\"param1\",...,\"paramN\");\r\n\r\n" \
                               "  where: \r\n" \
                               "    name:      XML Name of the panel\r\n" \
                               "    method:   Method to be invoked on panel\r\n" \
                               "    paramX:   Arbitrary list of parameters to be passed to method\r\n\r\n" \
                               "***Available Methods include:***\r\n" \
                               "  \"getbackgroundcolor\"             - Get the background color for named canvas\r\n" \
                               "  \"setbackgroundcolor\",\"#RRGGBB\" - Set the background color to named canvas\r\n" \

#endif //nsCalTestStrings_h___

