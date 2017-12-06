// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IONetworkInterface.h>
#include <stddef.h>
#include <stdint.h>

#include <vector>
#include <string>
// Note: The original machine_id_mac.cc code is in namespace rlz_lib below.
// It depends on some external files, which would bring in a log of Chromium
// code if imported as well.
// Instead only the necessary code has been extracted from the relevant files,
// and further combined and reduced to limit the maintenance burden.

// [Extracted from base/logging.h]
#define DCHECK assert

namespace base {

// [Extracted from base/mac/scoped_typeref.h and base/mac/scoped_cftyperef.h]
template<typename T>
class ScopedCFTypeRef {
 public:
  typedef T element_type;

  explicit ScopedCFTypeRef(T object)
      : object_(object) {
  }

  ScopedCFTypeRef(const ScopedCFTypeRef<T>& that) = delete;
  ScopedCFTypeRef(ScopedCFTypeRef<T>&& that) = delete;

  ~ScopedCFTypeRef() {
    if (object_)
      CFRelease(object_);
  }

  ScopedCFTypeRef& operator=(const ScopedCFTypeRef<T>& that) = delete;
  ScopedCFTypeRef& operator=(ScopedCFTypeRef<T>&& that) = delete;

  operator T() const {
    return object_;
  }

  // ScopedCFTypeRef<>::release() is like scoped_ptr<>::release.  It is NOT
  // a wrapper for CFRelease().
  T release() {
    T temp = object_;
    object_ = NULL;
    return temp;
  }

 private:
  T object_;
};

namespace mac {

// [Extracted from base/mac/scoped_ioobject.h]
// Just like ScopedCFTypeRef but for io_object_t and subclasses.
template<typename IOT>
class ScopedIOObject {
 public:
  typedef IOT element_type;

  explicit ScopedIOObject(IOT object = IO_OBJECT_NULL)
      : object_(object) {
  }

  ~ScopedIOObject() {
    if (object_)
      IOObjectRelease(object_);
  }

  ScopedIOObject(const ScopedIOObject&) = delete;
  void operator=(const ScopedIOObject&) = delete;

  void reset(IOT object = IO_OBJECT_NULL) {
    if (object_)
      IOObjectRelease(object_);
    object_ = object;
  }

  operator IOT() const {
    return object_;
  }

 private:
  IOT object_;
};

// [Extracted from base/mac/foundation_util.h]
template<typename T>
T CFCast(const CFTypeRef& cf_val);

template<>
CFDataRef
CFCast<CFDataRef>(const CFTypeRef& cf_val) {
  if (cf_val == NULL) {
    return NULL;
  }
  if (CFGetTypeID(cf_val) == CFDataGetTypeID()) {
    return (CFDataRef)(cf_val);
  }
  return NULL;
}

template<>
CFStringRef
CFCast<CFStringRef>(const CFTypeRef& cf_val) {
  if (cf_val == NULL) {
    return NULL;
  }
  if (CFGetTypeID(cf_val) == CFStringGetTypeID()) {
    return (CFStringRef)(cf_val);
  }
  return NULL;
}

}  // namespace mac

// [Extracted from base/strings/sys_string_conversions_mac.mm]
static const CFStringEncoding kNarrowStringEncoding = kCFStringEncodingUTF8;

template<typename StringType>
static StringType CFStringToSTLStringWithEncodingT(CFStringRef cfstring,
                                                   CFStringEncoding encoding) {
  CFIndex length = CFStringGetLength(cfstring);
  if (length == 0)
    return StringType();

  CFRange whole_string = CFRangeMake(0, length);
  CFIndex out_size;
  CFIndex converted = CFStringGetBytes(cfstring,
                                       whole_string,
                                       encoding,
                                       0,      // lossByte
                                       false,  // isExternalRepresentation
                                       NULL,   // buffer
                                       0,      // maxBufLen
                                       &out_size);
  if (converted == 0 || out_size == 0)
    return StringType();

  // out_size is the number of UInt8-sized units needed in the destination.
  // A buffer allocated as UInt8 units might not be properly aligned to
  // contain elements of StringType::value_type.  Use a container for the
  // proper value_type, and convert out_size by figuring the number of
  // value_type elements per UInt8.  Leave room for a NUL terminator.
  typename StringType::size_type elements =
      out_size * sizeof(UInt8) / sizeof(typename StringType::value_type) + 1;

  std::vector<typename StringType::value_type> out_buffer(elements);
  converted = CFStringGetBytes(cfstring,
                               whole_string,
                               encoding,
                               0,      // lossByte
                               false,  // isExternalRepresentation
                               reinterpret_cast<UInt8*>(&out_buffer[0]),
                               out_size,
                               NULL);  // usedBufLen
  if (converted == 0)
    return StringType();

  out_buffer[elements - 1] = '\0';
  return StringType(&out_buffer[0], elements - 1);
}

std::string SysCFStringRefToUTF8(CFStringRef ref)
{
  return CFStringToSTLStringWithEncodingT<std::string>(ref,
                                                       kNarrowStringEncoding);
}

} // namespace base

namespace rlz_lib {

namespace {

// See http://developer.apple.com/library/mac/#technotes/tn1103/_index.html

// The caller is responsible for freeing |matching_services|.
bool FindEthernetInterfaces(io_iterator_t* matching_services) {
  base::ScopedCFTypeRef<CFMutableDictionaryRef> matching_dict(
      IOServiceMatching(kIOEthernetInterfaceClass));
  if (!matching_dict)
    return false;

  base::ScopedCFTypeRef<CFMutableDictionaryRef> primary_interface(
      CFDictionaryCreateMutable(kCFAllocatorDefault,
                                0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  if (!primary_interface)
    return false;

  CFDictionarySetValue(
      primary_interface, CFSTR(kIOPrimaryInterface), kCFBooleanTrue);
  CFDictionarySetValue(
      matching_dict, CFSTR(kIOPropertyMatchKey), primary_interface);

  kern_return_t kern_result = IOServiceGetMatchingServices(
      kIOMasterPortDefault, matching_dict.release(), matching_services);

  return kern_result == KERN_SUCCESS;
}

bool GetMACAddressFromIterator(io_iterator_t primary_interface_iterator,
                               uint8_t* buffer, size_t buffer_size) {
  if (buffer_size < kIOEthernetAddressSize)
    return false;

  bool success = false;

  bzero(buffer, buffer_size);
  base::mac::ScopedIOObject<io_object_t> primary_interface;
  while (primary_interface.reset(IOIteratorNext(primary_interface_iterator)),
         primary_interface) {
    io_object_t primary_interface_parent;
    kern_return_t kern_result = IORegistryEntryGetParentEntry(
        primary_interface, kIOServicePlane, &primary_interface_parent);
    base::mac::ScopedIOObject<io_object_t> primary_interface_parent_deleter(
        primary_interface_parent);
    success = kern_result == KERN_SUCCESS;

    if (!success)
      continue;

    base::ScopedCFTypeRef<CFTypeRef> mac_data(
        IORegistryEntryCreateCFProperty(primary_interface_parent,
                                        CFSTR(kIOMACAddress),
                                        kCFAllocatorDefault,
                                        0));
    CFDataRef mac_data_data = base::mac::CFCast<CFDataRef>(mac_data);
    if (mac_data_data) {
      CFDataGetBytes(
          mac_data_data, CFRangeMake(0, kIOEthernetAddressSize), buffer);
    }
  }

  return success;
}

bool GetMacAddress(unsigned char* buffer, size_t size) {
  io_iterator_t primary_interface_iterator;
  if (!FindEthernetInterfaces(&primary_interface_iterator))
    return false;
  bool result = GetMACAddressFromIterator(
      primary_interface_iterator, buffer, size);
  IOObjectRelease(primary_interface_iterator);
  return result;
}

CFStringRef CopySerialNumber() {
  base::mac::ScopedIOObject<io_service_t> expert_device(
      IOServiceGetMatchingService(kIOMasterPortDefault,
          IOServiceMatching("IOPlatformExpertDevice")));
  if (!expert_device)
    return NULL;

  base::ScopedCFTypeRef<CFTypeRef> serial_number(
      IORegistryEntryCreateCFProperty(expert_device,
                                      CFSTR(kIOPlatformSerialNumberKey),
                                      kCFAllocatorDefault,
                                      0));
  CFStringRef serial_number_cfstring =
      base::mac::CFCast<CFStringRef>(serial_number.release());
  if (!serial_number_cfstring)
    return NULL;

  return serial_number_cfstring;
}

}  // namespace

bool GetRawMachineId(std::vector<uint8_t>* data, int* more_data) {
  uint8_t mac_address[kIOEthernetAddressSize];

  std::string id;
  if (GetMacAddress(mac_address, sizeof(mac_address))) {
    id += "mac:";
    static const char hex[] =
      { '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    for (int i = 0; i < kIOEthernetAddressSize; ++i) {
      uint8_t byte = mac_address[i];
      id += hex[byte >> 4];
      id += hex[byte & 0xF];
    }
  }

  // A MAC address is enough to uniquely identify a machine, but it's only 6
  // bytes, 3 of which are manufacturer-determined. To make brute-forcing the
  // SHA1 of this harder, also append the system's serial number.
  CFStringRef serial = CopySerialNumber();
  if (serial) {
    if (!id.empty()) {
      id += ' ';
    }
    id += "serial:";
    id += base::SysCFStringRefToUTF8(serial);
    CFRelease(serial);
  }

  // Get the contents of the string 'id' as a bunch of bytes.
  data->assign(&id[0], &id[id.size()]);

  // On windows, this is set to the volume id. Since it's not scrambled before
  // being sent, just set it to 1.
  *more_data = 1;
  return true;
}

}  // namespace rlz_lib
