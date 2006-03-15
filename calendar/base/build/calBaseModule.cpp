/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
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

#include "nsIGenericFactory.h"
#include "nsIClassInfoImpl.h"

#include "calDateTime.h"
#include "calDuration.h"
#include "calICSService.h"
#include "calRecurrenceRule.h"
#include "calRecurrenceDate.h"
#include "calRecurrenceDateSet.h"

#include "calBaseCID.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(calDateTime)
NS_GENERIC_FACTORY_CONSTRUCTOR(calDuration)
NS_GENERIC_FACTORY_CONSTRUCTOR(calICSService)
NS_GENERIC_FACTORY_CONSTRUCTOR(calRecurrenceRule)
NS_GENERIC_FACTORY_CONSTRUCTOR(calRecurrenceDate)
NS_GENERIC_FACTORY_CONSTRUCTOR(calRecurrenceDateSet)

NS_DECL_CLASSINFO(calDateTime)
NS_DECL_CLASSINFO(calDuration)
NS_DECL_CLASSINFO(calICSService)
NS_DECL_CLASSINFO(calRecurrenceRule)
NS_DECL_CLASSINFO(calRecurrenceDate)
NS_DECL_CLASSINFO(calRecurrenceDateSet)

static const nsModuleComponentInfo components[] =
{
    { "Calendar DateTime Object",
      CAL_DATETIME_CID,
      CAL_DATETIME_CONTRACTID,
      calDateTimeConstructor,
      NULL,
      NULL,
      NULL,
      NS_CI_INTERFACE_GETTER_NAME(calDateTime),
      NULL,
      &NS_CLASSINFO_NAME(calDateTime)
    },
    { "Calendar Duration Object",
      CAL_DURATION_CID,
      CAL_DURATION_CONTRACTID,
      calDurationConstructor,
      NULL,
      NULL,
      NULL,
      NS_CI_INTERFACE_GETTER_NAME(calDuration),
      NULL,
      &NS_CLASSINFO_NAME(calDuration)
    },
    { "ICS parser/serializer",
      CAL_ICSSERVICE_CID,
      CAL_ICSSERVICE_CONTRACTID,
      calICSServiceConstructor,
      NULL,
      NULL,
      NULL,
      NS_CI_INTERFACE_GETTER_NAME(calICSService),
      NULL,
      &NS_CLASSINFO_NAME(calICSService)
    },
    { "Calendar Recurrence Rule",
      CAL_RECURRENCERULE_CID,
      CAL_RECURRENCERULE_CONTRACTID,
      calRecurrenceRuleConstructor,
      NULL,
      NULL,
      NULL,
      NS_CI_INTERFACE_GETTER_NAME(calRecurrenceRule),
      NULL,
      &NS_CLASSINFO_NAME(calRecurrenceRule)
    },
    { "Calendar Recurrence Date",
      CAL_RECURRENCEDATE_CID,
      CAL_RECURRENCEDATE_CONTRACTID,
      calRecurrenceDateConstructor,
      NULL,
      NULL,
      NULL,
      NS_CI_INTERFACE_GETTER_NAME(calRecurrenceDate),
      NULL,
      &NS_CLASSINFO_NAME(calRecurrenceDate)
    },
    { "Calendar Recurrence Date Set",
      CAL_RECURRENCEDATESET_CID,
      CAL_RECURRENCEDATESET_CONTRACTID,
      calRecurrenceDateSetConstructor,
      NULL,
      NULL,
      NULL,
      NS_CI_INTERFACE_GETTER_NAME(calRecurrenceDateSet),
      NULL,
      &NS_CLASSINFO_NAME(calRecurrenceDateSet)
    }
};

NS_IMPL_NSGETMODULE(calBaseModule, components)
