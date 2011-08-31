/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Maemo code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brad Lassey <blassey@mozilla.com>
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

#if (MOZ_PLATFORM_MAEMO == 5)
#include <dbus/dbus.h>
#include <mce/dbus-names.h>
#endif
#if defined(MOZ_ENABLE_QTMOBILITY)
#include <QFeedbackEffect>
using namespace QtMobility;
#endif

#include "nsHapticFeedback.h"

NS_IMPL_ISUPPORTS1(nsHapticFeedback, nsIHapticFeedback)

NS_IMETHODIMP
nsHapticFeedback::PerformSimpleAction(PRInt32 aType)
{
#if (MOZ_PLATFORM_MAEMO == 5)
    DBusError err;
    dbus_error_init(&err);

    DBusConnection  *connection;
    connection = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err)) {
        dbus_error_free(&err);
        return NS_ERROR_FAILURE;
    }
    if (nsnull == connection) {
        return NS_ERROR_FAILURE;
    }

    dbus_connection_set_exit_on_disconnect(connection,false);

    DBusMessage* msg =
        dbus_message_new_method_call(MCE_SERVICE, MCE_REQUEST_PATH,
                                     MCE_REQUEST_IF, MCE_ACTIVATE_VIBRATOR_PATTERN);

    if (!msg) {
        return NS_ERROR_FAILURE;
    }

    dbus_message_set_no_reply(msg, PR_TRUE);

    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);
    const char* pattern = "PatternTouchscreen";
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &pattern);

    if (dbus_connection_send(connection, msg, NULL)) {
        dbus_connection_flush(connection);
        dbus_message_unref(msg);
    } else {
        dbus_message_unref(msg);
        return NS_ERROR_FAILURE;
    }
#elif defined(MOZ_ENABLE_QTMOBILITY)
    if (aType == ShortPress)
        QFeedbackEffect::playThemeEffect(QFeedbackEffect::ThemeBasicButton);
    if (aType == LongPress)
        QFeedbackEffect::playThemeEffect(QFeedbackEffect::ThemeLongPress);
#endif

    return NS_OK;
}
