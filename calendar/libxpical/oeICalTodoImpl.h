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
 * The Original Code is Mozilla Calendar Code.
 *
 * The Initial Developer of the Original Code is
 * Chris Charabaruk <ccharabaruk@meldstar.com>.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Chris Charabaruk <ccharabaruk@meldstar.com>
 *                 Mostafa Hosseini <mostafah@oeone.com>
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
 
 /* Header file for oeICalTodoImpl.cpp containing its CID and CONTRACTID.*/

#ifndef _OEICALTODOIMPL_H_
#define _OEICALTODOIMPL_H_

#include "oeIICal.h"
#include "oeDateTimeImpl.h"
#include "nsISimpleEnumerator.h"
#include "nsISupportsPrimitives.h"
#include "nsSupportsPrimitives.h"

extern "C" {
    #include "ical.h"
}

#define OE_ICALTODO_CID { 0x0c06905a, 0x1dd2, 0x11b2, { 0xba, 0x61, 0xc9, 0x5d, 0x84, 0x0b, 0x01, 0xef } }

#define OE_ICALTODO_CONTRACTID "@mozilla.org/icaltodo;1"

/* oeICalTodo Header file */
class oeICalTodoImpl : public oeIICalTodo 
{
public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_OEIICALEVENT(mEvent->)
  NS_DECL_OEIICALTODO

  oeICalTodoImpl();
  virtual ~oeICalTodoImpl();
  /* additional members */
  bool ParseIcalComponent( icalcomponent *vcalendar );
  icalcomponent *AsIcalComponent();
  NS_IMETHOD Clone(oeIICalTodo **_retval);
  bool matchId( const char *id );
  oeICalEventImpl *GetBaseEvent();
private:
    int m_percent;
    oeDateTimeImpl *m_completed;
    oeICalEventImpl *mEvent;
};

#endif
