/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Abstraction on top of the wifi support from libhardware_legacy that we
 * use to talk to the wpa_supplicant.
 */

#ifndef WifiUtils_h
#define WifiUtils_h

#include "nsString.h"
#include "nsAutoPtr.h"
#include "mozilla/dom/WifiOptionsBinding.h"
#include "WifiHotspotUtils.h"

// Needed to add a copy constructor to WifiCommandOptions.
struct CommandOptions
{
public:
  CommandOptions(const mozilla::dom::WifiCommandOptions& aOther) {

#define COPY_OPT_FIELD(prop, defaultValue)            \
    if (aOther.prop.WasPassed()) {                    \
      prop = aOther.prop.Value();                     \
    } else {                                          \
      prop = defaultValue;                            \
    }

#define COPY_FIELD(prop) prop = aOther.prop;
    COPY_FIELD(mId)
    COPY_FIELD(mCmd)
    COPY_OPT_FIELD(mRequest, EmptyString())

#undef COPY_OPT_FIELD
#undef COPY_FIELD
  }

  // All the fields, not Optional<> anymore to get copy constructors.
  nsString mCmd;
  int32_t mId;
  nsString mRequest;
};

// Abstract class that exposes libhardware_legacy functions we need for
// wifi management.
// We use the ICS signatures here since they are likely more future-proof.
class WpaSupplicantImpl
{
public:
  // Suppress warning from nsAutoPtr
  virtual ~WpaSupplicantImpl() {}

  virtual int32_t
  do_wifi_wait_for_event(const char *iface, char *buf, size_t len) = 0; // KK == ICS != JB

  virtual int32_t
  do_wifi_command(const char* iface, const char* cmd, char* buff, size_t* len) = 0; // KK == ICS != JB

  virtual int32_t
  do_wifi_load_driver() = 0;

  virtual int32_t
  do_wifi_unload_driver() = 0;

  virtual int32_t
  do_wifi_start_supplicant(int32_t) = 0; // ICS != JB == KK

  virtual int32_t
  do_wifi_stop_supplicant(int32_t) = 0; //ICS != JB == KK

  virtual int32_t
  do_wifi_connect_to_supplicant(const char* iface) = 0; // KK == ICS != JB

  virtual void
  do_wifi_close_supplicant_connection(const char* iface) = 0; // KK == ICS != JB
};

// Concrete class to use to access the wpa supplicant.
class WpaSupplicant MOZ_FINAL
{
public:
  WpaSupplicant();

  // Use nsCString as the type of aInterface to guarantee it's
  // null-terminated so that we can pass it to c API without
  // conversion
  void WaitForEvent(nsAString& aEvent, const nsCString& aInterface);
  bool ExecuteCommand(CommandOptions aOptions,
                      mozilla::dom::WifiResultOptions& result,
                      const nsCString& aInterface);

private:
  nsAutoPtr<WpaSupplicantImpl> mImpl;
  nsAutoPtr<WifiHotspotUtils> mWifiHotspotUtils;

  uint32_t mSdkVersion;

protected:
  void CheckBuffer(char* buffer, int32_t length, nsAString& aEvent);
  uint32_t MakeMask(uint32_t len);
};

#endif // WifiUtils_h
