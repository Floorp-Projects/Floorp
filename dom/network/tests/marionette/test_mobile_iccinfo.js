/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

let connection = navigator.mozMobileConnection;
ok(connection instanceof MozMobileConnection,
   "connection is instanceof " + connection.constructor);

// The emulator's hard coded iccid value.
// See it here {B2G_HOME}/external/qemu/telephony/sim_card.c#L299.
is(connection.iccInfo.iccid, 89014103211118510720);

// The emulator's hard coded mcc and mnc codes.
// See it here {B2G_HOME}/external/qemu/telephony/android_modem.c#L2465.
is(connection.iccInfo.mcc, 310);
is(connection.iccInfo.mnc, 260);

SpecialPowers.removePermission("mobileconnection", document);
finish();
