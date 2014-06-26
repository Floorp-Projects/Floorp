/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Abstraction on top of the network support from libnetutils that we
 * use to set up network connections.
 */

#ifndef WifiHotspotUtils_h
#define WifiHotspotUtils_h

// Forward declaration.
struct wpa_ctrl;

class WifiHotspotUtils
{
public:
  static void* GetSharedLibrary();

  int32_t do_wifi_connect_to_hostapd();
  int32_t do_wifi_close_hostapd_connection();
  int32_t do_wifi_hostapd_command(const char *command,
                                  char *reply,
                                  size_t *reply_len);
  int32_t do_wifi_hostapd_get_stations();

private:
  struct wpa_ctrl * openConnection(const char *ifname);
  int32_t sendCommand(struct wpa_ctrl *ctrl, const char *cmd,
                      char *reply, size_t *reply_len);
};

// Defines a function type with the right arguments and return type.
#define DEFINE_DLFUNC(name, ret, args...) typedef ret (*FUNC##name)(args);

// Set up a dlsymed function ready to use.
#define USE_DLFUNC(name)                                                      \
  FUNC##name name = (FUNC##name) dlsym(GetSharedLibrary(), #name);            \
  if (!name) {                                                                \
    MOZ_CRASH("Symbol not found in shared library : " #name);                 \
  }

#endif // WifiHotspotUtils_h
