/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_hal_Types_h
#define mozilla_hal_Types_h

#include "IPCMessageUtils.h"

namespace mozilla {
namespace hal {

/**
 * These are defined by libhardware, specifically, hardware/libhardware/include/hardware/lights.h
 * in the gonk subsystem.
 * If these change and are exposed to JS, make sure nsIHal.idl is updated as well.
 */
enum LightType {
  eHalLightID_Backlight     = 0,
  eHalLightID_Keyboard      = 1,
  eHalLightID_Buttons       = 2,
  eHalLightID_Battery       = 3,
  eHalLightID_Notifications = 4,
  eHalLightID_Attention     = 5,
  eHalLightID_Bluetooth     = 6,
  eHalLightID_Wifi          = 7,
  eHalLightID_Count         = 8  // This should stay at the end
};
enum LightMode {
  eHalLightMode_User   = 0,  // brightness is managed by user setting
  eHalLightMode_Sensor = 1,  // brightness is managed by a light sensor
  eHalLightMode_Count
};
enum FlashMode {
  eHalLightFlash_None     = 0,
  eHalLightFlash_Timed    = 1,  // timed flashing.  Use flashOnMS and flashOffMS for timing
  eHalLightFlash_Hardware = 2,  // hardware assisted flashing
  eHalLightFlash_Count
};

enum ShutdownMode {
  eHalShutdownMode_Unknown  = -1,
  eHalShutdownMode_PowerOff = 0,
  eHalShutdownMode_Reboot   = 1,
  eHalShutdownMode_Restart  = 2,
  eHalShutdownMode_Count    = 3
};

class SwitchEvent;

enum SwitchDevice {
  SWITCH_DEVICE_UNKNOWN = -1,
  SWITCH_HEADPHONES,
  SWITCH_USB,
  NUM_SWITCH_DEVICE
};

enum SwitchState {
  SWITCH_STATE_UNKNOWN = -1,
  SWITCH_STATE_ON,
  SWITCH_STATE_OFF,
  SWITCH_STATE_HEADSET,          // Headphone with microphone
  SWITCH_STATE_HEADPHONE,        // without microphone
  NUM_SWITCH_STATE
};

typedef Observer<SwitchEvent> SwitchObserver;

enum ProcessPriority {
  PROCESS_PRIORITY_BACKGROUND,
  PROCESS_PRIORITY_BACKGROUND_HOMESCREEN,
  PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE,
  // Any priority greater than or equal to FOREGROUND is considered
  // "foreground" for the purposes of priority testing, for example
  // CurrentProcessIsForeground().
  PROCESS_PRIORITY_FOREGROUND,
  PROCESS_PRIORITY_MASTER,
  NUM_PROCESS_PRIORITY
};

/**
 * Used by ModifyWakeLock
 */
enum WakeLockControl {
  WAKE_LOCK_REMOVE_ONE = -1,
  WAKE_LOCK_NO_CHANGE  = 0,
  WAKE_LOCK_ADD_ONE    = 1,
  NUM_WAKE_LOCK
};

class FMRadioOperationInformation;

enum FMRadioOperation {
  FM_RADIO_OPERATION_UNKNOWN = -1,
  FM_RADIO_OPERATION_ENABLE,
  FM_RADIO_OPERATION_DISABLE,
  FM_RADIO_OPERATION_SEEK,
  NUM_FM_RADIO_OPERATION
};

enum FMRadioOperationStatus {
  FM_RADIO_OPERATION_STATUS_UNKNOWN = -1,
  FM_RADIO_OPERATION_STATUS_SUCCESS,
  FM_RADIO_OPERATION_STATUS_FAIL,
  NUM_FM_RADIO_OPERATION_STATUS
};

enum FMRadioSeekDirection {
  FM_RADIO_SEEK_DIRECTION_UNKNOWN = -1,
  FM_RADIO_SEEK_DIRECTION_UP,
  FM_RADIO_SEEK_DIRECTION_DOWN,
  NUM_FM_RADIO_SEEK_DIRECTION
};

enum FMRadioCountry {
  FM_RADIO_COUNTRY_UNKNOWN = -1,
  FM_RADIO_COUNTRY_US,  //USA
  FM_RADIO_COUNTRY_EU,
  FM_RADIO_COUNTRY_JP_STANDARD,
  FM_RADIO_COUNTRY_JP_WIDE,
  FM_RADIO_COUNTRY_DE,  //Germany
  FM_RADIO_COUNTRY_AW,  //Aruba
  FM_RADIO_COUNTRY_AU,  //Australlia
  FM_RADIO_COUNTRY_BS,  //Bahamas
  FM_RADIO_COUNTRY_BD,  //Bangladesh
  FM_RADIO_COUNTRY_CY,  //Cyprus
  FM_RADIO_COUNTRY_VA,  //Vatican
  FM_RADIO_COUNTRY_CO,  //Colombia
  FM_RADIO_COUNTRY_KR,  //Korea
  FM_RADIO_COUNTRY_DK,  //Denmark
  FM_RADIO_COUNTRY_EC,  //Ecuador
  FM_RADIO_COUNTRY_ES,  //Spain
  FM_RADIO_COUNTRY_FI,  //Finland
  FM_RADIO_COUNTRY_FR,  //France
  FM_RADIO_COUNTRY_GM,  //Gambia
  FM_RADIO_COUNTRY_HU,  //Hungary
  FM_RADIO_COUNTRY_IN,  //India
  FM_RADIO_COUNTRY_IR,  //Iran
  FM_RADIO_COUNTRY_IT,  //Italy
  FM_RADIO_COUNTRY_KW,  //Kuwait
  FM_RADIO_COUNTRY_LT,  //Lithuania
  FM_RADIO_COUNTRY_ML,  //Mali
  FM_RADIO_COUNTRY_MA,  //Morocco
  FM_RADIO_COUNTRY_NO,  //Norway
  FM_RADIO_COUNTRY_NZ,  //New Zealand
  FM_RADIO_COUNTRY_OM,  //Oman
  FM_RADIO_COUNTRY_PG,  //Papua New Guinea
  FM_RADIO_COUNTRY_NL,  //Netherlands
  FM_RADIO_COUNTRY_QA,  //Qatar
  FM_RADIO_COUNTRY_CZ,  //Czech Republic
  FM_RADIO_COUNTRY_UK,  //United Kingdom of Great Britain and Northern Ireland
  FM_RADIO_COUNTRY_RW,  //Rwandese Republic
  FM_RADIO_COUNTRY_SN,  //Senegal
  FM_RADIO_COUNTRY_SG,  //Singapore
  FM_RADIO_COUNTRY_SI,  //Slovenia
  FM_RADIO_COUNTRY_ZA,  //South Africa
  FM_RADIO_COUNTRY_SE,  //Sweden
  FM_RADIO_COUNTRY_CH,  //Switzerland
  FM_RADIO_COUNTRY_TW,  //Taiwan
  FM_RADIO_COUNTRY_TR,  //Turkey
  FM_RADIO_COUNTRY_UA,  //Ukraine
  FM_RADIO_COUNTRY_USER_DEFINED,
  NUM_FM_RADIO_COUNTRY
};

typedef Observer<FMRadioOperationInformation> FMRadioObserver;
} // namespace hal
} // namespace mozilla

namespace IPC {

/**
 * Light type serializer.
 */
template <>
struct ParamTraits<mozilla::hal::LightType>
  : public EnumSerializer<mozilla::hal::LightType,
                          mozilla::hal::eHalLightID_Backlight,
                          mozilla::hal::eHalLightID_Count>
{};

/**
 * Light mode serializer.
 */
template <>
struct ParamTraits<mozilla::hal::LightMode>
  : public EnumSerializer<mozilla::hal::LightMode,
                          mozilla::hal::eHalLightMode_User,
                          mozilla::hal::eHalLightMode_Count>
{};

/**
 * Flash mode serializer.
 */
template <>
struct ParamTraits<mozilla::hal::FlashMode>
  : public EnumSerializer<mozilla::hal::FlashMode,
                          mozilla::hal::eHalLightFlash_None,
                          mozilla::hal::eHalLightFlash_Count>
{};

/**
 * Serializer for ShutdownMode.
 */
template <>
struct ParamTraits<mozilla::hal::ShutdownMode>
  : public EnumSerializer<mozilla::hal::ShutdownMode,
                          mozilla::hal::eHalShutdownMode_Unknown,
                          mozilla::hal::eHalShutdownMode_Count>
{};

/**
 * WakeLockControl serializer.
 */
template <>
struct ParamTraits<mozilla::hal::WakeLockControl>
  : public EnumSerializer<mozilla::hal::WakeLockControl,
                          mozilla::hal::WAKE_LOCK_REMOVE_ONE,
                          mozilla::hal::NUM_WAKE_LOCK>
{};

/**
 * Serializer for SwitchState
 */
template <>
struct ParamTraits<mozilla::hal::SwitchState>:
  public EnumSerializer<mozilla::hal::SwitchState,
                        mozilla::hal::SWITCH_STATE_UNKNOWN,
                        mozilla::hal::NUM_SWITCH_STATE> {
};

/**
 * Serializer for SwitchDevice
 */
template <>
struct ParamTraits<mozilla::hal::SwitchDevice>:
  public EnumSerializer<mozilla::hal::SwitchDevice,
                        mozilla::hal::SWITCH_DEVICE_UNKNOWN,
                        mozilla::hal::NUM_SWITCH_DEVICE> {
};

template <>
struct ParamTraits<mozilla::hal::ProcessPriority>:
  public EnumSerializer<mozilla::hal::ProcessPriority,
                        mozilla::hal::PROCESS_PRIORITY_BACKGROUND,
                        mozilla::hal::NUM_PROCESS_PRIORITY> {
};

/**
 * Serializer for FMRadioOperation
 */
template <>
struct ParamTraits<mozilla::hal::FMRadioOperation>:
  public EnumSerializer<mozilla::hal::FMRadioOperation,
                        mozilla::hal::FM_RADIO_OPERATION_UNKNOWN,
                        mozilla::hal::NUM_FM_RADIO_OPERATION>
{};

/**
 * Serializer for FMRadioOperationStatus
 */
template <>
struct ParamTraits<mozilla::hal::FMRadioOperationStatus>:
  public EnumSerializer<mozilla::hal::FMRadioOperationStatus,
                        mozilla::hal::FM_RADIO_OPERATION_STATUS_UNKNOWN,
                        mozilla::hal::NUM_FM_RADIO_OPERATION_STATUS>
{};

/**
 * Serializer for FMRadioSeekDirection
 */
template <>
struct ParamTraits<mozilla::hal::FMRadioSeekDirection>:
  public EnumSerializer<mozilla::hal::FMRadioSeekDirection,
                        mozilla::hal::FM_RADIO_SEEK_DIRECTION_UNKNOWN,
                        mozilla::hal::NUM_FM_RADIO_SEEK_DIRECTION>
{};

/**
 * Serializer for FMRadioCountry
 **/
template <>
struct ParamTraits<mozilla::hal::FMRadioCountry>:
  public EnumSerializer<mozilla::hal::FMRadioCountry,
                        mozilla::hal::FM_RADIO_COUNTRY_UNKNOWN,
                        mozilla::hal::NUM_FM_RADIO_COUNTRY>
{};
} // namespace IPC

#endif // mozilla_hal_Types_h
