/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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

