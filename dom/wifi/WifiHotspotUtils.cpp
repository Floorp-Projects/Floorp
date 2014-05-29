/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WifiHotspotUtils.h"
#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <cutils/properties.h>

#include "prinit.h"
#include "mozilla/Assertions.h"
#include "nsDebug.h"
#include "nsPrintfCString.h"

static void* sWifiHotspotUtilsLib;
static PRCallOnceType sInitWifiHotspotUtilsLib;
// Socket pair used to exit from a blocking read.
static struct wpa_ctrl* ctrl_conn;
static const char *ctrl_iface_dir = "/data/misc/wifi/hostapd";
static char *ctrl_ifname = nullptr;

DEFINE_DLFUNC(wpa_ctrl_open, struct wpa_ctrl*, const char*)
DEFINE_DLFUNC(wpa_ctrl_close, void, struct wpa_ctrl*)
DEFINE_DLFUNC(wpa_ctrl_attach, int32_t, struct wpa_ctrl*)
DEFINE_DLFUNC(wpa_ctrl_detach, int32_t, struct wpa_ctrl*)
DEFINE_DLFUNC(wpa_ctrl_request, int32_t, struct wpa_ctrl*,
              const char*, size_t cmd_len, char *reply,
              size_t *reply_len, void (*msg_cb)(char *msg, size_t len))


static PRStatus
InitWifiHotspotUtilsLib()
{
  sWifiHotspotUtilsLib = dlopen("/system/lib/libwpa_client.so", RTLD_LAZY);
  // We might fail to open the hardware lib. That's OK.
  return PR_SUCCESS;
}

static void*
GetWifiHotspotLibHandle()
{
  PR_CallOnce(&sInitWifiHotspotUtilsLib, InitWifiHotspotUtilsLib);
  return sWifiHotspotUtilsLib;
}

struct wpa_ctrl *
WifiHotspotUtils::openConnection(const char *ifname)
{
  if (!ifname) {
    return nullptr;
  }

  USE_DLFUNC(wpa_ctrl_open)
  ctrl_conn = wpa_ctrl_open(nsPrintfCString("%s/%s", ctrl_iface_dir, ifname).get());
  return ctrl_conn;
}

int32_t
WifiHotspotUtils::sendCommand(struct wpa_ctrl *ctrl, const char *cmd,
                              char *reply, size_t *reply_len)
{
  int32_t ret;

  if (!ctrl_conn) {
    NS_WARNING(nsPrintfCString("Not connected to hostapd - \"%s\" command dropped.\n", cmd).get());
    return -1;
  }

  USE_DLFUNC(wpa_ctrl_request)
  ret = wpa_ctrl_request(ctrl, cmd, strlen(cmd), reply, reply_len, nullptr);
  if (ret == -2) {
    NS_WARNING(nsPrintfCString("'%s' command timed out.\n", cmd).get());
    return -2;
  } else if (ret < 0 || strncmp(reply, "FAIL", 4) == 0) {
    return -1;
  }

  // Make the reply printable.
  reply[*reply_len] = '\0';
  if (strncmp(cmd, "STA-FIRST", 9) == 0 ||
      strncmp(cmd, "STA-NEXT", 8) == 0) {
    char *pos = reply;

    while (*pos && *pos != '\n')
      pos++;
    *pos = '\0';
  }

  return 0;
}


// static
void*
WifiHotspotUtils::GetSharedLibrary()
{
  void* wpaClientLib = GetWifiHotspotLibHandle();
  if (!wpaClientLib) {
    NS_WARNING("No /system/lib/libwpa_client.so");
  }
  return wpaClientLib;
}

int32_t WifiHotspotUtils::do_wifi_connect_to_hostapd()
{
  struct dirent *dent;

  DIR *dir = opendir(ctrl_iface_dir);
  if (dir) {
    while ((dent = readdir(dir))) {
      if (strcmp(dent->d_name, ".") == 0 ||
          strcmp(dent->d_name, "..") == 0) {
        continue;
      }
      ctrl_ifname = strdup(dent->d_name);
      break;
    }
    closedir(dir);
  }

  ctrl_conn = openConnection(ctrl_ifname);
  if (!ctrl_conn) {
    NS_WARNING(nsPrintfCString("Unable to open connection to hostapd on \"%s\": %s",
               ctrl_ifname, strerror(errno)).get());
    return -1;
  }

  USE_DLFUNC(wpa_ctrl_attach)
  if (wpa_ctrl_attach(ctrl_conn) != 0) {
    USE_DLFUNC(wpa_ctrl_close)
    wpa_ctrl_close(ctrl_conn);
    ctrl_conn = nullptr;
    return -1;
  }

  return 0;
}

int32_t WifiHotspotUtils::do_wifi_close_hostapd_connection()
{
  if (!ctrl_conn) {
    NS_WARNING("Invalid ctrl_conn.");
    return -1;
  }

  USE_DLFUNC(wpa_ctrl_detach)
  if (wpa_ctrl_detach(ctrl_conn) < 0) {
    NS_WARNING("Failed to detach wpa_ctrl.");
  }

  USE_DLFUNC(wpa_ctrl_close)
  wpa_ctrl_close(ctrl_conn);
  ctrl_conn = nullptr;
  return 0;
}

int32_t WifiHotspotUtils::do_wifi_hostapd_command(const char *command,
                                                  char *reply,
                                                  size_t *reply_len)
{
  return sendCommand(ctrl_conn, command, reply, reply_len);
}

int32_t WifiHotspotUtils::do_wifi_hostapd_get_stations()
{
  char addr[32], cmd[64];
  int stations = 0;
  size_t addrLen = sizeof(addr);

  if (sendCommand(ctrl_conn, "STA-FIRST", addr, &addrLen)) {
    return 0;
  }
  stations++;

  sprintf(cmd, "STA-NEXT %s", addr);
  while (sendCommand(ctrl_conn, cmd, addr, &addrLen) == 0) {
    stations++;
    sprintf(cmd, "STA-NEXT %s", addr);
  }

  return stations;
}
