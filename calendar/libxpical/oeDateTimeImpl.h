/* 
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
 * The Original Code is OEone Corporation.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by OEone Corporation are Copyright (C) 2001
 * OEone Corporation. All Rights Reserved.
 *
 * Contributor(s): Mostafa Hosseini (mostafah@oeone.com)
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
 * 
*/

/* Header file for oeDateTimeImpl.cpp containing its CID and CONTRACTID.*/

#ifndef __OE_DATETIMEIMPL_H__
#define __OE_DATETIMEIMPL_H__

#include "oeIICal.h"
extern "C" {
    #include "ical.h"
    int   icaltimezone_get_vtimezone_properties (icaltimezone  *zone,
						    icalcomponent *component);
}

extern icaltimezone *currenttimezone;

#define OE_DATETIME_CID \
{ 0x78b5b255, 0x7450, 0x47c0, { 0xba, 0x16, 0x0a, 0x6e, 0x7e, 0x80, 0x6e, 0x5d } }

#define OE_DATETIME_CONTRACTID "@mozilla.org/oedatetime;1"

class oeDateTimeImpl : public oeIDateTime
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_OEIDATETIME
  oeDateTimeImpl();
  virtual ~oeDateTimeImpl();
  void AdjustToWeekday( short weekday );
  int CompareDate( oeDateTimeImpl *anotherdt );
  void SetTzID(const char *aNewVal);
  /* additional members */
  struct icaltimetype m_datetime;
private:
  char *m_tzid;
};

#endif
