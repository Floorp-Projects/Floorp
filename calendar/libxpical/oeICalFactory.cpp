/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is OEone Calendar Code, released October 31st, 2001.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Mostafa Hosseini <mostafah@oeone.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Factory implementation of various calendar objects */

#include "oeICalImpl.h"
#include "oeICalContainerImpl.h"
#include "oeICalEventImpl.h"
#include "oeDateTimeImpl.h"

#include "nsIGenericFactory.h"



NS_GENERIC_FACTORY_CONSTRUCTOR(oeICalImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(oeICalContainerImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(oeICalEventImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(oeICalTodoImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(oeDateTimeImpl)

static const nsModuleComponentInfo pModuleInfo[] =
{
  { "ICal Service",
    OE_ICAL_CID,
    OE_ICAL_CONTRACTID,
    oeICalImplConstructor,
  },
  { "ICal Container",
    OE_ICALCONTAINER_CID,
    OE_ICALCONTAINER_CONTRACTID,
    oeICalContainerImplConstructor,
  },
  { "ICal Event",
    OE_ICALEVENT_CID,
    OE_ICALEVENT_CONTRACTID,
    oeICalEventImplConstructor,
  },
  { "ICal DateTime",
    OE_DATETIME_CID,
    OE_DATETIME_CONTRACTID,
    oeDateTimeImplConstructor,
  },
  { "ICal Todo",
    OE_ICALTODO_CID,
    OE_ICALTODO_CONTRACTID,
    oeICalTodoImplConstructor,
  }
};

NS_IMPL_NSGETMODULE( oeICalModule, pModuleInfo)

