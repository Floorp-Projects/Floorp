/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <mozilla/Hal.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <mozilla/dom/battery/Constants.h>
#include "nsAutoRef.h"

/*
 * Helper that manages the destruction of glib objects as soon as they leave
 * the current scope.
 *
 * We are specializing nsAutoRef class.
 */

template <>
class nsAutoRefTraits<DBusGProxy> : public nsPointerRefTraits<DBusGProxy>
{
public:
  static void Release(DBusGProxy* ptr) { g_object_unref(ptr); }
};

template <>
class nsAutoRefTraits<GHashTable> : public nsPointerRefTraits<GHashTable>
{
public:
  static void Release(GHashTable* ptr) { g_hash_table_unref(ptr); }
};

using namespace mozilla::dom::battery;

namespace mozilla {
namespace hal_impl {

/**
 * This is the declaration of UPowerClient class. This class is listening and
 * communicating to upower daemon through DBus.
 * There is no header file because this class shouldn't be public.
 */
class UPowerClient
{
public:
  static UPowerClient* GetInstance();

  void BeginListening();
  void StopListening();

  double GetLevel();
  bool   IsCharging();
  double GetRemainingTime();

  ~UPowerClient();

private:
  UPowerClient();

  enum States {
    eState_Unknown = 0,
    eState_Charging,
    eState_Discharging,
    eState_Empty,
    eState_FullyCharged,
    eState_PendingCharge,
    eState_PendingDischarge
  };

  /**
   * Update the currently tracked device.
   * @return whether everything went ok.
   */
  void UpdateTrackedDevice();

  /**
   * Returns a hash table with the properties of aDevice.
   * Note: the caller has to unref the hash table.
   */
  GHashTable* GetDeviceProperties(const gchar* aDevice);

  /**
   * Using the device properties (aHashTable), this method updates the member
   * variable storing the values we care about.
   */
  void UpdateSavedInfo(GHashTable* aHashTable);

  /**
   * Callback used by 'DeviceChanged' signal.
   */
  static void DeviceChanged(DBusGProxy* aProxy, const gchar* aObjectPath,
                            UPowerClient* aListener);

  /**
   * Callback called when mDBusConnection gets a signal.
   */
  static DBusHandlerResult ConnectionSignalFilter(DBusConnection* aConnection,
                                                  DBusMessage* aMessage,
                                                  void* aData);

  // The DBus connection object.
  DBusGConnection* mDBusConnection;

  // The DBus proxy object to upower.
  DBusGProxy* mUPowerProxy;

  // The path of the tracked device.
  gchar* mTrackedDevice;

  double mLevel;
  bool mCharging;
  double mRemainingTime;

  static UPowerClient* sInstance;

  static const guint sDeviceTypeBattery = 2;
  static const guint64 kUPowerUnknownRemainingTime = 0;
};

/*
 * Implementation of mozilla::hal_impl::EnableBatteryNotifications,
 *                   mozilla::hal_impl::DisableBatteryNotifications,
 *               and mozilla::hal_impl::GetCurrentBatteryInformation.
 */

void
EnableBatteryNotifications()
{
  UPowerClient::GetInstance()->BeginListening();
}

void
DisableBatteryNotifications()
{
  UPowerClient::GetInstance()->StopListening();
}

void
GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo)
{
  UPowerClient* upowerClient = UPowerClient::GetInstance();

  aBatteryInfo->level() = upowerClient->GetLevel();
  aBatteryInfo->charging() = upowerClient->IsCharging();
  aBatteryInfo->remainingTime() = upowerClient->GetRemainingTime();
}

/*
 * Following is the implementation of UPowerClient.
 */

UPowerClient* UPowerClient::sInstance = nsnull;

/* static */ UPowerClient*
UPowerClient::GetInstance()
{
  if (!sInstance) {
    sInstance = new UPowerClient();
  }

  return sInstance;
}

UPowerClient::UPowerClient()
  : mDBusConnection(nsnull)
  , mUPowerProxy(nsnull)
  , mTrackedDevice(nsnull)
  , mLevel(kDefaultLevel)
  , mCharging(kDefaultCharging)
  , mRemainingTime(kDefaultRemainingTime)
{
}

UPowerClient::~UPowerClient()
{
  NS_ASSERTION(!mDBusConnection && !mUPowerProxy && !mTrackedDevice,
               "The observers have not been correctly removed! "
               "(StopListening should have been called)");
}

void
UPowerClient::BeginListening()
{
  GError* error = nsnull;
  mDBusConnection = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);

  if (!mDBusConnection) {
    g_printerr("Failed to open connection to bus: %s\n", error->message);
    g_error_free(error);
    return;
  }

  DBusConnection* dbusConnection =
    dbus_g_connection_get_connection(mDBusConnection);

  // Make sure we do not exit the entire program if DBus connection get lost.
  dbus_connection_set_exit_on_disconnect(dbusConnection, false);

  // Listening to signals the DBus connection is going to get so we will know
  // when it is lost and we will be able to disconnect cleanly.
  dbus_connection_add_filter(dbusConnection, ConnectionSignalFilter, this,
                             nsnull);

  mUPowerProxy = dbus_g_proxy_new_for_name(mDBusConnection,
                                           "org.freedesktop.UPower",
                                           "/org/freedesktop/UPower",
                                           "org.freedesktop.UPower");

  UpdateTrackedDevice();

  /*
   * TODO: we should probably listen to DeviceAdded and DeviceRemoved signals.
   * If we do that, we would have to disconnect from those in StopListening.
   * It's not yet implemented because it requires testing hot plugging and
   * removal of a battery.
   */
  dbus_g_proxy_add_signal(mUPowerProxy, "DeviceChanged", G_TYPE_STRING,
                          G_TYPE_INVALID);
  dbus_g_proxy_connect_signal(mUPowerProxy, "DeviceChanged",
                              G_CALLBACK (DeviceChanged), this, nsnull);
}

void
UPowerClient::StopListening()
{
  // If mDBusConnection isn't initialized, that means we are not really listening.
  if (!mDBusConnection) {
    return;
  }

  dbus_connection_remove_filter(
      dbus_g_connection_get_connection(mDBusConnection),
      ConnectionSignalFilter, this);

  dbus_g_proxy_disconnect_signal(mUPowerProxy, "DeviceChanged",
                                 G_CALLBACK (DeviceChanged), this);

  g_free(mTrackedDevice);
  mTrackedDevice = nsnull;

  g_object_unref(mUPowerProxy);
  mUPowerProxy = nsnull;

  dbus_g_connection_unref(mDBusConnection);
  mDBusConnection = nsnull;

  // We should now show the default values, not the latest we got.
  mLevel = kDefaultLevel;
  mCharging = kDefaultCharging;
  mRemainingTime = kDefaultRemainingTime;
}

void
UPowerClient::UpdateTrackedDevice()
{
  GType typeGPtrArray = dbus_g_type_get_collection("GPtrArray",
                                                   DBUS_TYPE_G_OBJECT_PATH);
  GPtrArray* devices = nsnull;
  GError* error = nsnull;

  // If that fails, that likely means upower isn't installed.
  if (!dbus_g_proxy_call(mUPowerProxy, "EnumerateDevices", &error, G_TYPE_INVALID,
                         typeGPtrArray, &devices, G_TYPE_INVALID)) {
    g_printerr ("Error: %s\n", error->message);

    mTrackedDevice = nsnull;
    g_error_free(error);
    return;
  }

  /*
   * We are looking for the first device that is a battery.
   * TODO: we could try to combine more than one battery.
   */
  for (guint i=0; i<devices->len; ++i) {
    gchar* devicePath = static_cast<gchar*>(g_ptr_array_index(devices, i));
    nsAutoRef<GHashTable> hashTable(GetDeviceProperties(devicePath));

    if (g_value_get_uint(static_cast<const GValue*>(g_hash_table_lookup(hashTable, "Type"))) == sDeviceTypeBattery) {
      UpdateSavedInfo(hashTable);
      mTrackedDevice = devicePath;
      break;
    }

    g_free(devicePath);
  }

#if GLIB_MAJOR_VERSION >= 2 && GLIB_MINOR_VERSION >= 22
    g_ptr_array_unref(devices);
#else
    g_ptr_array_free(devices, true);
#endif
}

/* static */ void
UPowerClient::DeviceChanged(DBusGProxy* aProxy, const gchar* aObjectPath, UPowerClient* aListener)
{
#if GLIB_MAJOR_VERSION >= 2 && GLIB_MINOR_VERSION >= 16
  if (g_strcmp0(aObjectPath, aListener->mTrackedDevice)) {
#else
  if (g_ascii_strcasecmp(aObjectPath, aListener->mTrackedDevice)) {
#endif
    return;
  }

  nsAutoRef<GHashTable> hashTable(aListener->GetDeviceProperties(aObjectPath));
  aListener->UpdateSavedInfo(hashTable);

  hal::NotifyBatteryChange(hal::BatteryInformation(aListener->mLevel,
                                                   aListener->mCharging,
                                                   aListener->mRemainingTime));
}

/* static */ DBusHandlerResult
UPowerClient::ConnectionSignalFilter(DBusConnection* aConnection,
                                     DBusMessage* aMessage, void* aData)
{
  if (dbus_message_is_signal(aMessage, DBUS_INTERFACE_LOCAL, "Disconnected")) {
    static_cast<UPowerClient*>(aData)->StopListening();
    // We do not return DBUS_HANDLER_RESULT_HANDLED here because the connection
    // might be shared and some other filters might want to do something.
  }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

GHashTable*
UPowerClient::GetDeviceProperties(const gchar* aDevice)
{
  nsAutoRef<DBusGProxy> proxy(dbus_g_proxy_new_for_name(mDBusConnection,
                                                        "org.freedesktop.UPower",
                                                        aDevice,
                                                        "org.freedesktop.DBus.Properties"));

  GError* error = nsnull;
  GHashTable* hashTable = nsnull;
  GType typeGHashTable = dbus_g_type_get_map("GHashTable", G_TYPE_STRING,
                                            G_TYPE_VALUE);
  if (!dbus_g_proxy_call(proxy, "GetAll", &error, G_TYPE_STRING,
                         "org.freedesktop.UPower.Device", G_TYPE_INVALID,
                         typeGHashTable, &hashTable, G_TYPE_INVALID)) {
    g_printerr("Error: %s\n", error->message);
    g_error_free(error);
    return nsnull;
  }

  return hashTable;
}

void
UPowerClient::UpdateSavedInfo(GHashTable* aHashTable)
{
  bool isFull = false;

  /*
   * State values are confusing...
   * First of all, after looking at upower sources (0.9.13), it seems that
   * PendingDischarge and PendingCharge are not used.
   * In addition, FullyCharged and Empty states are not clear because we do not
   * know if the battery is actually charging or not. Those values come directly
   * from sysfs (in the Linux kernel) which have four states: "Empty", "Full",
   * "Charging" and "Discharging". In sysfs, "Empty" and "Full" are also only
   * related to the level, not to the charging state.
   * In this code, we are going to assume that Full means charging and Empty
   * means discharging because if that is not the case, the state should not
   * last a long time (actually, it should disappear at the following update).
   * It might be even very hard to see real cases where the state is Empty and
   * the battery is charging or the state is Full and the battery is discharging
   * given that plugging/unplugging the battery should have an impact on the
   * level.
   */
  switch (g_value_get_uint(static_cast<const GValue*>(g_hash_table_lookup(aHashTable, "State")))) {
    case eState_Unknown:
      mCharging = kDefaultCharging;
      break;
    case eState_FullyCharged:
      isFull = true;
    case eState_Charging:
    case eState_PendingCharge:
      mCharging = true;
      break;
    case eState_Discharging:
    case eState_Empty:
    case eState_PendingDischarge:
      mCharging = false;
      break;
  }

  /*
   * The battery level might be very close to 100% (like 99.xxxx%) without
   * increasing. It seems that upower sets the battery state as 'full' in that
   * case so we should trust it and not even try to get the value.
   */
  if (isFull) {
    mLevel = 1.0;
  } else {
    mLevel = g_value_get_double(static_cast<const GValue*>(g_hash_table_lookup(aHashTable, "Percentage")))*0.01;
  }

  if (isFull) {
    mRemainingTime = 0;
  } else {
    mRemainingTime = mCharging ? g_value_get_int64(static_cast<const GValue*>(g_hash_table_lookup(aHashTable, "TimeToFull")))
                               : g_value_get_int64(static_cast<const GValue*>(g_hash_table_lookup(aHashTable, "TimeToEmpty")));

    if (mRemainingTime == kUPowerUnknownRemainingTime) {
      mRemainingTime = kUnknownRemainingTime;
    }
  }
}

double
UPowerClient::GetLevel()
{
  return mLevel;
}

bool
UPowerClient::IsCharging()
{
  return mCharging;
}

double
UPowerClient::GetRemainingTime()
{
  return mRemainingTime;
}

} // namespace hal_impl
} // namespace mozilla
