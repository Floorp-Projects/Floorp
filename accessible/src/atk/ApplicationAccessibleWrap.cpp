/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ApplicationAccessibleWrap.h"

#include "nsCOMPtr.h"
#include "nsMai.h"
#include "prlink.h"
#include "prenv.h"
#include "nsIGConfService.h"
#include "nsIServiceManager.h"
#include "nsAutoPtr.h"
#include "nsAccessibilityService.h"
#include "AtkSocketAccessible.h"

#include <gtk/gtk.h>
#include <atk/atk.h>
#ifdef MOZ_ENABLE_DBUS
#include <dbus/dbus.h>
#endif

using namespace mozilla;
using namespace mozilla::a11y;

typedef GType (* AtkGetTypeType) (void);
GType g_atk_hyperlink_impl_type = G_TYPE_INVALID;
static bool sATKChecked = false;
static PRLibrary *sATKLib = nullptr;
static const char sATKLibName[] = "libatk-1.0.so.0";
static const char sATKHyperlinkImplGetTypeSymbol[] =
  "atk_hyperlink_impl_get_type";

static bool sToplevel_event_hook_added = false;
static gulong sToplevel_show_hook = 0;
static gulong sToplevel_hide_hook = 0;

G_BEGIN_DECLS
typedef void (*GnomeAccessibilityInit) (void);
typedef void (*GnomeAccessibilityShutdown) (void);
G_END_DECLS

struct GnomeAccessibilityModule
{
    const char *libName;
    PRLibrary *lib;
    const char *initName;
    GnomeAccessibilityInit init;
    const char *shutdownName;
    GnomeAccessibilityShutdown shutdown;
};

static GnomeAccessibilityModule sAtkBridge = {
#ifdef AIX
    "libatk-bridge.a(libatk-bridge.so.0)", NULL,
#else
    "libatk-bridge.so", NULL,
#endif
    "gnome_accessibility_module_init", NULL,
    "gnome_accessibility_module_shutdown", NULL
};

static GnomeAccessibilityModule sGail = {
    "libgail.so", NULL,
    "gnome_accessibility_module_init", NULL,
    "gnome_accessibility_module_shutdown", NULL
};

static nsresult LoadGtkModule(GnomeAccessibilityModule& aModule);

static gboolean toplevel_event_watcher(GSignalInvocationHint*, guint,
                                       const GValue*, gpointer);

// ApplicationAccessibleWrap

ApplicationAccessibleWrap::ApplicationAccessibleWrap():
  ApplicationAccessible()
{
  if (ShouldA11yBeEnabled()) {
      // Load and initialize gail library.
      nsresult rv = LoadGtkModule(sGail);
      if (NS_SUCCEEDED(rv))
          (*sGail.init)();
  }
}

// XXX we can't do this in ApplicationAccessibleWrap's constructor because
// a11y::ApplicationAcc() will return null then which breaks atk's attempt to
// get the application's root accessible during initialization.  this needs to
// be defined here because LoadGtkModule() and the library info is static.  See
// bug 817133.
void
nsAccessNodeWrap::InitAccessibility()
{
  if (!ShouldA11yBeEnabled())
    return;

  // Initialize the MAI Utility class, it will overwrite gail_util.
  g_type_class_unref(g_type_class_ref(mai_util_get_type()));

  // Init atk-bridge now
  PR_SetEnv("NO_AT_BRIDGE=0");
  nsresult rv = LoadGtkModule(sAtkBridge);
  if (NS_SUCCEEDED(rv)) {
    (*sAtkBridge.init)();
  }

  if (!sToplevel_event_hook_added) {
    sToplevel_event_hook_added = true;
    sToplevel_show_hook =
      g_signal_add_emission_hook(g_signal_lookup("show", GTK_TYPE_WINDOW),
                                 0, toplevel_event_watcher,
                                 reinterpret_cast<gpointer>(nsIAccessibleEvent::EVENT_SHOW),
                                 NULL);
    sToplevel_hide_hook =
      g_signal_add_emission_hook(g_signal_lookup("hide", GTK_TYPE_WINDOW), 0,
                                 toplevel_event_watcher,
                                 reinterpret_cast<gpointer>(nsIAccessibleEvent::EVENT_HIDE),
                                 NULL);
  }
}


ApplicationAccessibleWrap::~ApplicationAccessibleWrap()
{
  AccessibleWrap::ShutdownAtkObject();
}

static gboolean
toplevel_event_watcher(GSignalInvocationHint* ihint,
                       guint                  n_param_values,
                       const GValue*          param_values,
                       gpointer               data)
{
  static GQuark sQuark_gecko_acc_obj = 0;

  if (!sQuark_gecko_acc_obj)
    sQuark_gecko_acc_obj = g_quark_from_static_string("GeckoAccObj");

  if (nsAccessibilityService::IsShutdown())
    return TRUE;

  GObject* object = reinterpret_cast<GObject*>(g_value_get_object(param_values));
  if (!GTK_IS_WINDOW(object))
    return TRUE;

  AtkObject* child = gtk_widget_get_accessible(GTK_WIDGET(object));

  // GTK native dialog
  if (!IS_MAI_OBJECT(child) &&
      (atk_object_get_role(child) == ATK_ROLE_DIALOG)) {

    if (data == reinterpret_cast<gpointer>(nsIAccessibleEvent::EVENT_SHOW)) {

      // Attach the dialog accessible to app accessible tree
      Accessible* windowAcc = GetAccService()->AddNativeRootAccessible(child);
      g_object_set_qdata(G_OBJECT(child), sQuark_gecko_acc_obj,
                         reinterpret_cast<gpointer>(windowAcc));

    } else {

      // Deattach the dialog accessible
      Accessible* windowAcc =
        reinterpret_cast<Accessible*>
                        (g_object_get_qdata(G_OBJECT(child), sQuark_gecko_acc_obj));
      if (windowAcc) {
        GetAccService()->RemoveNativeRootAccessible(windowAcc);
        g_object_set_qdata(G_OBJECT(child), sQuark_gecko_acc_obj, NULL);
      }

    }
  }

  return TRUE;
}

void
ApplicationAccessibleWrap::Unload()
{
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
        // if (sAtkBridge.shutdown)
        //     (*sAtkBridge.shutdown)();
        // PR_UnloadLibrary(sAtkBridge.lib);
        sAtkBridge.lib = NULL;
        sAtkBridge.init = NULL;
        sAtkBridge.shutdown = NULL;
    }
    if (sGail.lib) {
        // Do not shutdown gail because
        // 1) Maybe it's not init-ed by us. e.g. GtkEmbed
        // 2) We need it to avoid assert in spi_atk_tidy_windows
        // if (sGail.shutdown)
        //   (*sGail.shutdown)();
        // PR_UnloadLibrary(sGail.lib);
        sGail.lib = NULL;
        sGail.init = NULL;
        sGail.shutdown = NULL;
    }
    // if (sATKLib) {
    //     PR_UnloadLibrary(sATKLib);
    //     sATKLib = nullptr;
    // }
}

ENameValueFlag
ApplicationAccessibleWrap::Name(nsString& aName)
{
  // ATK doesn't provide a way to obtain an application name (for example,
  // Firefox or Thunderbird) like IA2 does. Thus let's return an application
  // name as accessible name that was used to get a branding name (for example,
  // Minefield aka nightly Firefox or Daily aka nightly Thunderbird).
  GetAppName(aName);
  return eNameOK;
}

NS_IMETHODIMP
ApplicationAccessibleWrap::GetNativeInterface(void** aOutAccessible)
{
    *aOutAccessible = nullptr;

    if (!mAtkObject) {
        mAtkObject =
            reinterpret_cast<AtkObject *>
                            (g_object_new(MAI_TYPE_ATK_OBJECT, NULL));
        NS_ENSURE_TRUE(mAtkObject, NS_ERROR_OUT_OF_MEMORY);

        atk_object_initialize(mAtkObject, this);
        mAtkObject->role = ATK_ROLE_INVALID;
        mAtkObject->layer = ATK_LAYER_INVALID;
    }

    *aOutAccessible = mAtkObject;
    return NS_OK;
}

struct AtkRootAccessibleAddedEvent {
  AtkObject *app_accessible;
  AtkObject *root_accessible;
  uint32_t index;
};

gboolean fireRootAccessibleAddedCB(gpointer data)
{
    AtkRootAccessibleAddedEvent* eventData = (AtkRootAccessibleAddedEvent*)data;
    g_signal_emit_by_name(eventData->app_accessible, "children_changed::add",
                          eventData->index, eventData->root_accessible, NULL);
    g_object_unref(eventData->app_accessible);
    g_object_unref(eventData->root_accessible);
    free(data);

    return FALSE;
}

bool
ApplicationAccessibleWrap::AppendChild(Accessible* aChild)
{
  if (!ApplicationAccessible::AppendChild(aChild))
    return false;

  AtkObject* atkAccessible = AccessibleWrap::GetAtkObject(aChild);
  atk_object_set_parent(atkAccessible, mAtkObject);

    uint32_t count = mChildren.Length();

    // Emit children_changed::add in a timeout
    // to make sure aRootAccWrap is fully initialized.
    AtkRootAccessibleAddedEvent* eventData = (AtkRootAccessibleAddedEvent*)
      malloc(sizeof(AtkRootAccessibleAddedEvent));
    if (eventData) {
      eventData->app_accessible = mAtkObject;
      eventData->root_accessible = atkAccessible;
      eventData->index = count -1;
      g_object_ref(mAtkObject);
      g_object_ref(atkAccessible);
      g_timeout_add(0, fireRootAccessibleAddedCB, eventData);
    }

    return true;
}

bool
ApplicationAccessibleWrap::RemoveChild(Accessible* aChild)
{
  int32_t index = aChild->IndexInParent();

  AtkObject* atkAccessible = AccessibleWrap::GetAtkObject(aChild);
  atk_object_set_parent(atkAccessible, NULL);
  g_signal_emit_by_name(mAtkObject, "children_changed::remove", index,
                        atkAccessible, NULL);

  return ApplicationAccessible::RemoveChild(aChild);
}

void
ApplicationAccessibleWrap::PreCreate()
{
    if (!sATKChecked) {
        sATKLib = PR_LoadLibrary(sATKLibName);
        if (sATKLib) {
            AtkGetTypeType pfn_atk_hyperlink_impl_get_type = (AtkGetTypeType) PR_FindFunctionSymbol(sATKLib, sATKHyperlinkImplGetTypeSymbol);
            if (pfn_atk_hyperlink_impl_get_type)
                g_atk_hyperlink_impl_type = pfn_atk_hyperlink_impl_get_type();

            AtkGetTypeType pfn_atk_socket_get_type;
            pfn_atk_socket_get_type = (AtkGetTypeType)
                                      PR_FindFunctionSymbol(sATKLib,
                                                            AtkSocketAccessible::sATKSocketGetTypeSymbol);
            if (pfn_atk_socket_get_type) {
                AtkSocketAccessible::g_atk_socket_type =
                  pfn_atk_socket_get_type();
                AtkSocketAccessible::g_atk_socket_embed = (AtkSocketEmbedType)
                  PR_FindFunctionSymbol(sATKLib,
                                        AtkSocketAccessible
                                          ::sATKSocketEmbedSymbol);
            AtkSocketAccessible::gCanEmbed =
              AtkSocketAccessible::g_atk_socket_type != G_TYPE_INVALID &&
              AtkSocketAccessible::g_atk_socket_embed;
            }
        }
        sATKChecked = true;
    }
}

static nsresult
LoadGtkModule(GnomeAccessibilityModule& aModule)
{
    NS_ENSURE_ARG(aModule.libName);

    if (!(aModule.lib = PR_LoadLibrary(aModule.libName))) {
        //try to load the module with "gtk-2.0/modules" appended
        char *curLibPath = PR_GetLibraryPath();
        nsAutoCString libPath(curLibPath);
#if defined(LINUX) && defined(__x86_64__)
        libPath.Append(":/usr/lib64:/usr/lib");
#else
        libPath.Append(":/usr/lib");
#endif
        PR_FreeLibraryName(curLibPath);

        int16_t loc1 = 0, loc2 = 0;
        int16_t subLen = 0;
        while (loc2 >= 0) {
            loc2 = libPath.FindChar(':', loc1);
            if (loc2 < 0)
                subLen = libPath.Length() - loc1;
            else
                subLen = loc2 - loc1;
            nsAutoCString sub(Substring(libPath, loc1, subLen));
            sub.Append("/gtk-2.0/modules/");
            sub.Append(aModule.libName);
            aModule.lib = PR_LoadLibrary(sub.get());
            if (aModule.lib)
                break;

            loc1 = loc2+1;
        }
        if (!aModule.lib)
            return NS_ERROR_FAILURE;
    }

    //we have loaded the library, try to get the function ptrs
    if (!(aModule.init = PR_FindFunctionSymbol(aModule.lib,
                                               aModule.initName)) ||
        !(aModule.shutdown = PR_FindFunctionSymbol(aModule.lib,
                                                   aModule.shutdownName))) {

        //fail, :(
        PR_UnloadLibrary(aModule.lib);
        aModule.lib = NULL;
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

namespace mozilla {
namespace a11y {

  static const char sAccEnv [] = "GNOME_ACCESSIBILITY";
#ifdef MOZ_ENABLE_DBUS
static DBusPendingCall *sPendingCall = nullptr;
#endif

void
PreInit()
{
#ifdef MOZ_ENABLE_DBUS
  static bool sChecked = FALSE;
  if (sChecked)
    return;

  sChecked = TRUE;

  // dbus is only checked if GNOME_ACCESSIBILITY is unset
  // also make sure that a session bus address is available to prevent dbus from
  // starting a new one.  Dbus confuses the test harness when it creates a new
  // process (see bug 693343)
  if (PR_GetEnv(sAccEnv) || !PR_GetEnv("DBUS_SESSION_BUS_ADDRESS"))
    return;

  DBusConnection* bus = dbus_bus_get(DBUS_BUS_SESSION, nullptr);
  if (!bus)
    return;

  dbus_connection_set_exit_on_disconnect(bus, FALSE);

  static const char* iface = "org.a11y.Status";
  static const char* member = "IsEnabled";
  DBusMessage *message;
  message = dbus_message_new_method_call("org.a11y.Bus", "/org/a11y/bus",
                                         "org.freedesktop.DBus.Properties",
                                         "Get");
  if (!message)
    goto dbus_done;

  dbus_message_append_args(message, DBUS_TYPE_STRING, &iface,
                           DBUS_TYPE_STRING, &member, DBUS_TYPE_INVALID);
  dbus_connection_send_with_reply(bus, message, &sPendingCall, 1000);
  dbus_message_unref(message);

dbus_done:
  dbus_connection_unref(bus);
#endif
}

bool
ShouldA11yBeEnabled()
{
  static bool sChecked = false, sShouldEnable = false;
  if (sChecked)
    return sShouldEnable;

  sChecked = true;

  EPlatformDisabledState disabledState = PlatformDisabledState();
  if (disabledState == ePlatformIsDisabled)
    return sShouldEnable = false;

  // check if accessibility enabled/disabled by environment variable
  const char* envValue = PR_GetEnv(sAccEnv);
  if (envValue)
    return sShouldEnable = !!atoi(envValue);

#ifdef MOZ_ENABLE_DBUS
  PreInit();
  bool dbusSuccess = false;
  DBusMessage *reply = nullptr;
  if (!sPendingCall)
    goto dbus_done;

  dbus_pending_call_block(sPendingCall);
  reply = dbus_pending_call_steal_reply(sPendingCall);
  dbus_pending_call_unref(sPendingCall);
  sPendingCall = nullptr;
  if (!reply ||
      dbus_message_get_type(reply) != DBUS_MESSAGE_TYPE_METHOD_RETURN ||
      strcmp(dbus_message_get_signature (reply), DBUS_TYPE_VARIANT_AS_STRING))
    goto dbus_done;

  DBusMessageIter iter, iter_variant, iter_struct;
  dbus_bool_t dResult;
  dbus_message_iter_init(reply, &iter);
  dbus_message_iter_recurse (&iter, &iter_variant);
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
  if (reply)
    dbus_message_unref(reply);

  if (dbusSuccess)
    return sShouldEnable;
#endif

  //check gconf-2 setting
static const char sGconfAccessibilityKey[] =
    "/desktop/gnome/interface/accessibility";
  nsresult rv = NS_OK;
  nsCOMPtr<nsIGConfService> gconf =
    do_GetService(NS_GCONFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && gconf)
    gconf->GetBool(NS_LITERAL_CSTRING(sGconfAccessibilityKey), &sShouldEnable);

  return sShouldEnable;
}
} // namespace a11y
} // namespace mozilla

