/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

    dbus_message_set_no_reply(msg, true);

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
