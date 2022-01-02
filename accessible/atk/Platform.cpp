/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Platform.h"

#include "nsIAccessibleEvent.h"
#include "nsIGSettingsService.h"
#include "nsMai.h"
#include "nsServiceManagerUtils.h"
#include "AtkSocketAccessible.h"
#include "prenv.h"
#include "prlink.h"

#ifdef MOZ_ENABLE_DBUS
#  include <dbus/dbus.h>
#endif
#include <gtk/gtk.h>

using namespace mozilla;
using namespace mozilla::a11y;

int atkMajorVersion = 1, atkMinorVersion = 12, atkMicroVersion = 0;

GType (*gAtkTableCellGetTypeFunc)();

extern "C" {
typedef GType (*AtkGetTypeType)(void);
typedef void (*AtkBridgeAdaptorInit)(int*, char**[]);
}

static PRLibrary* sATKLib = nullptr;
static const char sATKLibName[] = "libatk-1.0.so.0";
static const char sATKHyperlinkImplGetTypeSymbol[] =
    "atk_hyperlink_impl_get_type";

gboolean toplevel_event_watcher(GSignalInvocationHint*, guint, const GValue*,
                                gpointer);
static bool sToplevel_event_hook_added = false;
static gulong sToplevel_show_hook = 0;
static gulong sToplevel_hide_hook = 0;

GType g_atk_hyperlink_impl_type = G_TYPE_INVALID;

struct AtkBridgeModule {
  const char* libName;
  PRLibrary* lib;
  const char* initName;
  AtkBridgeAdaptorInit init;
};

static AtkBridgeModule sAtkBridge = {"libatk-bridge-2.0.so.0", nullptr,
                                     "atk_bridge_adaptor_init", nullptr};

static nsresult LoadGtkModule(AtkBridgeModule& aModule) {
  NS_ENSURE_ARG(aModule.libName);

  if (!(aModule.lib = PR_LoadLibrary(aModule.libName))) {
    return NS_ERROR_FAILURE;
  }

  // we have loaded the library, try to get the function ptrs
  if (!(aModule.init = (AtkBridgeAdaptorInit)PR_FindFunctionSymbol(
            aModule.lib, aModule.initName))) {
    // fail, :(
    PR_UnloadLibrary(aModule.lib);
    aModule.lib = nullptr;
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

void a11y::PlatformInit() {
  if (!ShouldA11yBeEnabled()) return;

  sATKLib = PR_LoadLibrary(sATKLibName);
  if (!sATKLib) return;

  AtkGetTypeType pfn_atk_hyperlink_impl_get_type =
      (AtkGetTypeType)PR_FindFunctionSymbol(sATKLib,
                                            sATKHyperlinkImplGetTypeSymbol);
  if (pfn_atk_hyperlink_impl_get_type) {
    g_atk_hyperlink_impl_type = pfn_atk_hyperlink_impl_get_type();
  }

  AtkGetTypeType pfn_atk_socket_get_type =
      (AtkGetTypeType)PR_FindFunctionSymbol(
          sATKLib, AtkSocketAccessible::sATKSocketGetTypeSymbol);
  if (pfn_atk_socket_get_type) {
    AtkSocketAccessible::g_atk_socket_type = pfn_atk_socket_get_type();
    AtkSocketAccessible::g_atk_socket_embed =
        (AtkSocketEmbedType)PR_FindFunctionSymbol(
            sATKLib, AtkSocketAccessible ::sATKSocketEmbedSymbol);
    AtkSocketAccessible::gCanEmbed =
        AtkSocketAccessible::g_atk_socket_type != G_TYPE_INVALID &&
        AtkSocketAccessible::g_atk_socket_embed;
  }

  gAtkTableCellGetTypeFunc =
      (GType(*)())PR_FindFunctionSymbol(sATKLib, "atk_table_cell_get_type");

  const char* (*atkGetVersion)() =
      (const char* (*)())PR_FindFunctionSymbol(sATKLib, "atk_get_version");
  if (atkGetVersion) {
    const char* version = atkGetVersion();
    if (version) {
      char* endPtr = nullptr;
      atkMajorVersion = strtol(version, &endPtr, 10);
      if (atkMajorVersion != 0L) {
        atkMinorVersion = strtol(endPtr + 1, &endPtr, 10);
        if (atkMinorVersion != 0L) {
          atkMicroVersion = strtol(endPtr + 1, &endPtr, 10);
        }
      }
    }
  }

  // Initialize the MAI Utility class, it will overwrite gail_util.
  g_type_class_unref(g_type_class_ref(mai_util_get_type()));

  // Init atk-bridge now
  PR_SetEnv("NO_AT_BRIDGE=0");
  nsresult rv = LoadGtkModule(sAtkBridge);
  if (NS_SUCCEEDED(rv)) {
    (*sAtkBridge.init)(nullptr, nullptr);
  }

  if (!sToplevel_event_hook_added) {
    sToplevel_event_hook_added = true;
    sToplevel_show_hook = g_signal_add_emission_hook(
        g_signal_lookup("show", GTK_TYPE_WINDOW), 0, toplevel_event_watcher,
        reinterpret_cast<gpointer>(nsIAccessibleEvent::EVENT_SHOW), nullptr);
    sToplevel_hide_hook = g_signal_add_emission_hook(
        g_signal_lookup("hide", GTK_TYPE_WINDOW), 0, toplevel_event_watcher,
        reinterpret_cast<gpointer>(nsIAccessibleEvent::EVENT_HIDE), nullptr);
  }
}

void a11y::PlatformShutdown() {
  if (sToplevel_event_hook_added) {
    sToplevel_event_hook_added = false;
    g_signal_remove_emission_hook(g_signal_lookup("show", GTK_TYPE_WINDOW),
                                  sToplevel_show_hook);
    g_signal_remove_emission_hook(g_signal_lookup("hide", GTK_TYPE_WINDOW),
                                  sToplevel_hide_hook);
  }

  if (sAtkBridge.lib) {
    // Do not shutdown/unload atk-bridge,
    // an exit function registered will take care of it
    // PR_UnloadLibrary(sAtkBridge.lib);
    sAtkBridge.lib = nullptr;
    sAtkBridge.init = nullptr;
  }
  // if (sATKLib) {
  //     PR_UnloadLibrary(sATKLib);
  //     sATKLib = nullptr;
  // }
}

static const char sAccEnv[] = "GNOME_ACCESSIBILITY";
#ifdef MOZ_ENABLE_DBUS
static DBusPendingCall* sPendingCall = nullptr;
#endif

void a11y::PreInit() {
#ifdef MOZ_ENABLE_DBUS
  static bool sChecked = FALSE;
  if (sChecked) return;

  sChecked = TRUE;

  // dbus is only checked if GNOME_ACCESSIBILITY is unset
  // also make sure that a session bus address is available to prevent dbus from
  // starting a new one.  Dbus confuses the test harness when it creates a new
  // process (see bug 693343)
  if (PR_GetEnv(sAccEnv) || !PR_GetEnv("DBUS_SESSION_BUS_ADDRESS")) return;

  DBusConnection* bus = dbus_bus_get(DBUS_BUS_SESSION, nullptr);
  if (!bus) return;

  dbus_connection_set_exit_on_disconnect(bus, FALSE);

  static const char* iface = "org.a11y.Status";
  static const char* member = "IsEnabled";
  DBusMessage* message;
  message =
      dbus_message_new_method_call("org.a11y.Bus", "/org/a11y/bus",
                                   "org.freedesktop.DBus.Properties", "Get");
  if (!message) goto dbus_done;

  dbus_message_append_args(message, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING,
                           &member, DBUS_TYPE_INVALID);
  dbus_connection_send_with_reply(bus, message, &sPendingCall, 1000);
  dbus_message_unref(message);

dbus_done:
  dbus_connection_unref(bus);
#endif
}

bool a11y::ShouldA11yBeEnabled() {
  static bool sChecked = false, sShouldEnable = false;
  if (sChecked) return sShouldEnable;

  sChecked = true;

  EPlatformDisabledState disabledState = PlatformDisabledState();
  if (disabledState == ePlatformIsDisabled) return sShouldEnable = false;

  // check if accessibility enabled/disabled by environment variable
  const char* envValue = PR_GetEnv(sAccEnv);
  if (envValue) return sShouldEnable = !!atoi(envValue);

#ifdef MOZ_ENABLE_DBUS
  PreInit();
  bool dbusSuccess = false;
  DBusMessage* reply = nullptr;
  if (!sPendingCall) goto dbus_done;

  dbus_pending_call_block(sPendingCall);
  reply = dbus_pending_call_steal_reply(sPendingCall);
  dbus_pending_call_unref(sPendingCall);
  sPendingCall = nullptr;
  if (!reply ||
      dbus_message_get_type(reply) != DBUS_MESSAGE_TYPE_METHOD_RETURN ||
      strcmp(dbus_message_get_signature(reply), DBUS_TYPE_VARIANT_AS_STRING)) {
    goto dbus_done;
  }

  DBusMessageIter iter, iter_variant, iter_struct;
  dbus_bool_t dResult;
  dbus_message_iter_init(reply, &iter);
  dbus_message_iter_recurse(&iter, &iter_variant);
  switch (dbus_message_iter_get_arg_type(&iter_variant)) {
    case DBUS_TYPE_STRUCT:
      // at-spi2-core 2.2.0-2.2.1 had a bug where it returned a struct
      dbus_message_iter_recurse(&iter_variant, &iter_struct);
      if (dbus_message_iter_get_arg_type(&iter_struct) == DBUS_TYPE_BOOLEAN) {
        dbus_message_iter_get_basic(&iter_struct, &dResult);
        sShouldEnable = dResult;
        dbusSuccess = true;
      }

      break;
    case DBUS_TYPE_BOOLEAN:
      dbus_message_iter_get_basic(&iter_variant, &dResult);
      sShouldEnable = dResult;
      dbusSuccess = true;
      break;
    default:
      break;
  }

dbus_done:
  if (reply) dbus_message_unref(reply);

  if (dbusSuccess) return sShouldEnable;
#endif

// check GSettings
#define GSETINGS_A11Y_INTERFACE "org.gnome.desktop.interface"
#define GSETINGS_A11Y_KEY "toolkit-accessibility"
  nsCOMPtr<nsIGSettingsService> gsettings =
      do_GetService(NS_GSETTINGSSERVICE_CONTRACTID);
  nsCOMPtr<nsIGSettingsCollection> a11y_settings;

  if (gsettings) {
    gsettings->GetCollectionForSchema(nsLiteralCString(GSETINGS_A11Y_INTERFACE),
                                      getter_AddRefs(a11y_settings));
    if (a11y_settings) {
      a11y_settings->GetBoolean(nsLiteralCString(GSETINGS_A11Y_KEY),
                                &sShouldEnable);
    }
  }

  return sShouldEnable;
}
