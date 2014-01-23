/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WifiUtils.h"
#include <dlfcn.h>
#include <errno.h>
#include "prinit.h"
#include "js/CharacterEncoding.h"
#include "NetUtils.h"

using namespace mozilla::dom;

#define BUFFER_SIZE        4096
#define COMMAND_SIZE       256
#define PROPERTY_VALUE_MAX 80

// Intentionally not trying to dlclose() this handle. That's playing
// Russian roulette with security bugs.
static void* sWifiLib;
static PRCallOnceType sInitWifiLib;

static PRStatus
InitWifiLib()
{
  sWifiLib = dlopen("/system/lib/libhardware_legacy.so", RTLD_LAZY);
  // We might fail to open the hardware lib. That's OK.
  return PR_SUCCESS;
}

static void*
GetSharedLibrary()
{
  PR_CallOnce(&sInitWifiLib, InitWifiLib);
  return sWifiLib;
}

// This is the same algorithm as in InflateUTF8StringToBuffer with Copy and
// while ignoring invalids.
// https://mxr.mozilla.org/mozilla-central/source/js/src/vm/CharacterEncoding.cpp#231

static const uint32_t REPLACE_UTF8 = 0xFFFD;

void LossyConvertUTF8toUTF16(const char* aInput, uint32_t aLength, nsAString& aOut)
{
  JS::UTF8Chars src(aInput, aLength);

  char16_t dst[aLength]; // Allocating for worst case.

  // First, count how many jschars need to be in the inflated string.
  // |i| is the index into |src|, and |j| is the the index into |dst|.
  size_t srclen = src.length();
  uint32_t j = 0;
  for (uint32_t i = 0; i < srclen; i++, j++) {
    uint32_t v = uint32_t(src[i]);
    if (!(v & 0x80)) {
      // ASCII code unit.  Simple copy.
      dst[j] = char16_t(v);
    } else {
      // Non-ASCII code unit.  Determine its length in bytes (n).
      uint32_t n = 1;
      while (v & (0x80 >> n))
        n++;

  #define INVALID(report, arg, n2)                          \
      do {                                                  \
        n = n2;                                             \
        goto invalidMultiByteCodeUnit;                      \
      } while (0)

      // Check the leading byte.
      if (n < 2 || n > 4)
        INVALID(ReportInvalidCharacter, i, 1);

      // Check that |src| is large enough to hold an n-byte code unit.
      if (i + n > srclen)
        INVALID(ReportBufferTooSmall, /* dummy = */ 0, 1);

      // Check the second byte.  From Unicode Standard v6.2, Table 3-7
      // Well-Formed UTF-8 Byte Sequences.
      if ((v == 0xE0 && ((uint8_t)src[i + 1] & 0xE0) != 0xA0) ||  // E0 A0~BF
        (v == 0xED && ((uint8_t)src[i + 1] & 0xE0) != 0x80) ||  // ED 80~9F
        (v == 0xF0 && ((uint8_t)src[i + 1] & 0xF0) == 0x80) ||  // F0 90~BF
        (v == 0xF4 && ((uint8_t)src[i + 1] & 0xF0) != 0x80))    // F4 80~8F
      {
        INVALID(ReportInvalidCharacter, i, 1);
      }

      // Check the continuation bytes.
      for (uint32_t m = 1; m < n; m++)
        if ((src[i + m] & 0xC0) != 0x80)
          INVALID(ReportInvalidCharacter, i, m);

      // Determine the code unit's length in jschars and act accordingly.
      v = JS::Utf8ToOneUcs4Char((uint8_t *)&src[i], n);
      if (v < 0x10000) {
        // The n-byte UTF8 code unit will fit in a single jschar.
        dst[j] = jschar(v);
      } else {
        v -= 0x10000;
        if (v <= 0xFFFFF) {
          // The n-byte UTF8 code unit will fit in two jschars.
          dst[j] = jschar((v >> 10) + 0xD800);
          j++;
          dst[j] = jschar((v & 0x3FF) + 0xDC00);
        } else {
          // The n-byte UTF8 code unit won't fit in two jschars.
          INVALID(ReportTooBigCharacter, v, 1);
        }
      }

    invalidMultiByteCodeUnit:
      // Move i to the last byte of the multi-byte code unit;  the loop
      // header will do the final i++ to move to the start of the next
      // code unit.
      i += n - 1;
    }
  }

  dst[j] = 0;
  aOut = dst;
}

// Helper to check we have loaded the hardware shared library.
#define CHECK_HWLIB(ret)                                                      \
  void* hwLib = GetSharedLibrary();                                           \
  if (!hwLib) {                                                               \
    NS_WARNING("No /system/lib/libhardware_legacy.so");                       \
    return ret;                                                               \
  }

#define DEFAULT_IMPL(name, ret, args...) \
  DEFINE_DLFUNC(name, ret, args...)      \
  ret do_##name(args) {                  \
    USE_DLFUNC(name)                     \
    return name(args);                   \
  }

// ICS implementation.
class ICSWpaSupplicantImpl : public WpaSupplicantImpl
{
public:
  DEFAULT_IMPL(wifi_load_driver, int32_t, )
  DEFAULT_IMPL(wifi_unload_driver, int32_t, )

  DEFINE_DLFUNC(wifi_wait_for_event, int32_t, char*, size_t)
  int32_t do_wifi_wait_for_event(const char *iface, char *buf, size_t len) {
    USE_DLFUNC(wifi_wait_for_event)
    return wifi_wait_for_event(buf, len);
  }

  DEFINE_DLFUNC(wifi_command, int32_t, const char*, char*, size_t*)
  int32_t do_wifi_command(const char* iface, const char* cmd, char* buf, size_t* len) {
    USE_DLFUNC(wifi_command)
    return wifi_command(cmd, buf, len);
  }

  DEFINE_DLFUNC(wifi_start_supplicant, int32_t, )
  int32_t do_wifi_start_supplicant(int32_t) {
    USE_DLFUNC(wifi_start_supplicant)
    return wifi_start_supplicant();
  }

  DEFINE_DLFUNC(wifi_stop_supplicant, int32_t)
  int32_t do_wifi_stop_supplicant(int32_t) {
    USE_DLFUNC(wifi_stop_supplicant)
    return wifi_stop_supplicant();
  }

  DEFINE_DLFUNC(wifi_connect_to_supplicant, int32_t, )
  int32_t do_wifi_connect_to_supplicant(const char* iface) {
    USE_DLFUNC(wifi_connect_to_supplicant)
    return wifi_connect_to_supplicant();
  }

  DEFINE_DLFUNC(wifi_close_supplicant_connection, void, )
  void do_wifi_close_supplicant_connection(const char* iface) {
    USE_DLFUNC(wifi_close_supplicant_connection)
    return wifi_close_supplicant_connection();
  }
};

// JB implementation.
// We only redefine the methods that have a different signature than on ICS.
class JBWpaSupplicantImpl : public ICSWpaSupplicantImpl
{
public:
  DEFINE_DLFUNC(wifi_wait_for_event, int32_t, const char*, char*, size_t)
  int32_t do_wifi_wait_for_event(const char* iface, char* buf, size_t len) {
    USE_DLFUNC(wifi_wait_for_event)
    return wifi_wait_for_event(iface, buf, len);
  }

  DEFINE_DLFUNC(wifi_command, int32_t, const char*, const char*, char*, size_t*)
  int32_t do_wifi_command(const char* iface, const char* cmd, char* buf, size_t* len) {
    USE_DLFUNC(wifi_command)
    return wifi_command(iface, cmd, buf, len);
  }

  DEFINE_DLFUNC(wifi_start_supplicant, int32_t, int32_t)
  int32_t do_wifi_start_supplicant(int32_t arg) {
    USE_DLFUNC(wifi_start_supplicant)
    return wifi_start_supplicant(arg);
  }

  DEFINE_DLFUNC(wifi_stop_supplicant, int32_t, int32_t)
  int32_t do_wifi_stop_supplicant(int32_t arg) {
    USE_DLFUNC(wifi_stop_supplicant)
    return wifi_stop_supplicant(arg);
  }

  DEFINE_DLFUNC(wifi_connect_to_supplicant, int32_t, const char*)
  int32_t do_wifi_connect_to_supplicant(const char* iface) {
    USE_DLFUNC(wifi_connect_to_supplicant)
    return wifi_connect_to_supplicant(iface);
  }

  DEFINE_DLFUNC(wifi_close_supplicant_connection, void, const char*)
  void do_wifi_close_supplicant_connection(const char* iface) {
    USE_DLFUNC(wifi_close_supplicant_connection)
    wifi_close_supplicant_connection(iface);
  }
};

// KK implementation.
// We only redefine the methods that have a different signature than on ICS.
class KKWpaSupplicantImpl : public ICSWpaSupplicantImpl
{
public:
  DEFINE_DLFUNC(wifi_start_supplicant, int32_t, int32_t)
  int32_t do_wifi_start_supplicant(int32_t arg) {
    USE_DLFUNC(wifi_start_supplicant)
    return wifi_start_supplicant(arg);
  }

  DEFINE_DLFUNC(wifi_stop_supplicant, int32_t, int32_t)
  int32_t do_wifi_stop_supplicant(int32_t arg) {
    USE_DLFUNC(wifi_stop_supplicant)
    return wifi_stop_supplicant(arg);
  }

  DEFINE_DLFUNC(wifi_command, int32_t, const char*, char*, size_t*)
  int32_t do_wifi_command(const char* iface, const char* cmd, char* buf, size_t* len) {
    char command[COMMAND_SIZE];
    if (!strcmp(iface, "p2p0")) {
      // Commands for p2p0 interface don't need prefix
      snprintf(command, COMMAND_SIZE, "%s", cmd);
    }
    else {
      snprintf(command, COMMAND_SIZE, "IFNAME=%s %s", iface, cmd);
    }
    USE_DLFUNC(wifi_command)
    return wifi_command(command, buf, len);
  }
};

// Concrete class to use to access the wpa supplicant.
WpaSupplicant::WpaSupplicant()
{
  if (NetUtils::SdkVersion() < 16) {
    mImpl = new ICSWpaSupplicantImpl();
  } else if (NetUtils::SdkVersion() < 19) {
    mImpl = new JBWpaSupplicantImpl();
  } else {
    mImpl = new KKWpaSupplicantImpl();
  }
  mNetUtils = new NetUtils();
};

void WpaSupplicant::WaitForEvent(nsAString& aEvent, const nsCString& aInterface)
{
  CHECK_HWLIB()

  char buffer[BUFFER_SIZE];
  int32_t ret = mImpl->do_wifi_wait_for_event(aInterface.get(), buffer, BUFFER_SIZE);
  CheckBuffer(buffer, ret, aEvent);
}

#define GET_CHAR(prop) NS_ConvertUTF16toUTF8(aOptions.prop).get()

/**
 * Make a subnet mask.
 */
uint32_t WpaSupplicant::MakeMask(uint32_t len) {
  uint32_t mask = 0;
  for (uint32_t i = 0; i < len; ++i) {
    mask |= (0x80000000 >> i);
  }
  return ntohl(mask);
}

bool WpaSupplicant::ExecuteCommand(CommandOptions aOptions,
                                   WifiResultOptions& aResult,
                                   const nsCString& aInterface)
{
  CHECK_HWLIB(false)
  if (!mNetUtils->GetSharedLibrary()) {
    return false;
  }

  // Always correlate the opaque ids.
  aResult.mId = aOptions.mId;

  if (aOptions.mCmd.EqualsLiteral("command")) {
    size_t len = BUFFER_SIZE - 1;
    char buffer[BUFFER_SIZE];
    NS_ConvertUTF16toUTF8 request(aOptions.mRequest);
    aResult.mStatus = mImpl->do_wifi_command(aInterface.get(), request.get(), buffer, &len);
    nsString value;
    if (aResult.mStatus == 0) {
      if (buffer[len - 1] == '\n') { // remove trailing new lines.
        len--;
      }
      buffer[len] = '\0';
      CheckBuffer(buffer, len, value);
    }
    aResult.mReply = value;
  } else if (aOptions.mCmd.EqualsLiteral("close_supplicant_connection")) {
    mImpl->do_wifi_close_supplicant_connection(aInterface.get());
  } else if (aOptions.mCmd.EqualsLiteral("load_driver")) {
    aResult.mStatus = mImpl->do_wifi_load_driver();
  } else if (aOptions.mCmd.EqualsLiteral("unload_driver")) {
    aResult.mStatus = mImpl->do_wifi_unload_driver();
  } else if (aOptions.mCmd.EqualsLiteral("start_supplicant")) {
    aResult.mStatus = mImpl->do_wifi_start_supplicant(0);
  } else if (aOptions.mCmd.EqualsLiteral("stop_supplicant")) {
    aResult.mStatus = mImpl->do_wifi_stop_supplicant(0);
  } else if (aOptions.mCmd.EqualsLiteral("connect_to_supplicant")) {
    aResult.mStatus = mImpl->do_wifi_connect_to_supplicant(aInterface.get());
  } else if (aOptions.mCmd.EqualsLiteral("ifc_enable")) {
    aResult.mStatus = mNetUtils->do_ifc_enable(GET_CHAR(mIfname));
  } else if (aOptions.mCmd.EqualsLiteral("ifc_disable")) {
    aResult.mStatus = mNetUtils->do_ifc_disable(GET_CHAR(mIfname));
  } else if (aOptions.mCmd.EqualsLiteral("ifc_configure")) {
    aResult.mStatus = mNetUtils->do_ifc_configure(
      GET_CHAR(mIfname), aOptions.mIpaddr, aOptions.mMask,
      aOptions.mGateway, aOptions.mDns1, aOptions.mDns2
    );
  } else if (aOptions.mCmd.EqualsLiteral("ifc_reset_connections")) {
    aResult.mStatus = mNetUtils->do_ifc_reset_connections(
      GET_CHAR(mIfname), RESET_ALL_ADDRESSES
    );
  } else if (aOptions.mCmd.EqualsLiteral("dhcp_stop")) {
    aResult.mStatus = mNetUtils->do_dhcp_stop(GET_CHAR(mIfname));
  } else if (aOptions.mCmd.EqualsLiteral("dhcp_do_request")) {
    char ipaddr[PROPERTY_VALUE_MAX];
    char gateway[PROPERTY_VALUE_MAX];
    uint32_t prefixLength;
    char dns1[PROPERTY_VALUE_MAX];
    char dns2[PROPERTY_VALUE_MAX];
    char server[PROPERTY_VALUE_MAX];
    uint32_t lease;
    char vendorinfo[PROPERTY_VALUE_MAX];
    aResult.mStatus =
      mNetUtils->do_dhcp_do_request(GET_CHAR(mIfname),
                                    ipaddr,
                                    gateway,
                                    &prefixLength,
                                    dns1,
                                    dns2,
                                    server,
                                    &lease,
                                    vendorinfo);

    if (aResult.mStatus == -1) {
      // Early return since we failed.
      return true;
    }

    aResult.mIpaddr_str = NS_ConvertUTF8toUTF16(ipaddr);
    aResult.mGateway_str = NS_ConvertUTF8toUTF16(gateway);
    aResult.mDns1_str = NS_ConvertUTF8toUTF16(dns1);
    aResult.mDns2_str = NS_ConvertUTF8toUTF16(dns2);
    aResult.mServer_str = NS_ConvertUTF8toUTF16(server);
    aResult.mVendor_str = NS_ConvertUTF8toUTF16(vendorinfo);
    aResult.mLease = lease;
    aResult.mMask = MakeMask(prefixLength);

    uint32_t inet4; // only support IPv4 for now.

#define INET_PTON(var, field)                                                 \
  PR_BEGIN_MACRO                                                              \
    inet_pton(AF_INET, var, &inet4);                                          \
    aResult.field = inet4;                                                    \
  PR_END_MACRO

    INET_PTON(ipaddr, mIpaddr);
    INET_PTON(gateway, mGateway);

    if (dns1[0] != '\0') {
      INET_PTON(dns1, mDns1);
    }

    if (dns2[0] != '\0') {
      INET_PTON(dns2, mDns2);
    }

    INET_PTON(server, mServer);

    //aResult.mask_str = netHelpers.ipToString(obj.mask);
    char inet_str[64];
    if (inet_ntop(AF_INET, &aResult.mMask, inet_str, sizeof(inet_str))) {
      aResult.mMask_str = NS_ConvertUTF8toUTF16(inet_str);
    }

    uint32_t broadcast = (aResult.mIpaddr & aResult.mMask) + ~aResult.mMask;
    if (inet_ntop(AF_INET, &broadcast, inet_str, sizeof(inet_str))) {
      aResult.mBroadcast_str = NS_ConvertUTF8toUTF16(inet_str);
    }
  } else {
    NS_WARNING("WpaSupplicant::ExecuteCommand : Unknown command");
    printf_stderr("WpaSupplicant::ExecuteCommand : Unknown command: %s",
      NS_ConvertUTF16toUTF8(aOptions.mCmd).get());
    return false;
  }

  return true;
}

// Checks the buffer and do the utf processing.
void
WpaSupplicant::CheckBuffer(char* buffer, int32_t length,
                           nsAString& aEvent)
{
  if (length > 0 && length < BUFFER_SIZE) {
    buffer[length] = 0;
    LossyConvertUTF8toUTF16(buffer, length, aEvent);
  }
}
