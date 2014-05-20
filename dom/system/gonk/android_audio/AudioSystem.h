/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_AUDIOSYSTEM_H_
#define ANDROID_AUDIOSYSTEM_H_

#include <utils/RefBase.h>
#include <utils/threads.h>
#include "IAudioFlinger.h"

#ifndef VANILLA_ANDROID
/* request to open a direct output with get_output() (by opposition to
 * sharing an output with other AudioTracks)
 */
typedef enum {
    AUDIO_POLICY_OUTPUT_FLAG_INDIRECT = 0x0,
    AUDIO_POLICY_OUTPUT_FLAG_DIRECT = 0x1
} audio_policy_output_flags_t;

/* device categories used for audio_policy->set_force_use() */
typedef enum {
    AUDIO_POLICY_FORCE_NONE,
    AUDIO_POLICY_FORCE_SPEAKER,
    AUDIO_POLICY_FORCE_HEADPHONES,
    AUDIO_POLICY_FORCE_BT_SCO,
    AUDIO_POLICY_FORCE_BT_A2DP,
    AUDIO_POLICY_FORCE_WIRED_ACCESSORY,
    AUDIO_POLICY_FORCE_BT_CAR_DOCK,
    AUDIO_POLICY_FORCE_BT_DESK_DOCK,

#ifdef VANILLA_ANDROID
    AUDIO_POLICY_FORCE_ANALOG_DOCK,
    AUDIO_POLICY_FORCE_DIGITAL_DOCK,
#endif

    AUDIO_POLICY_FORCE_CFG_CNT,
    AUDIO_POLICY_FORCE_CFG_MAX = AUDIO_POLICY_FORCE_CFG_CNT - 1,

    AUDIO_POLICY_FORCE_DEFAULT = AUDIO_POLICY_FORCE_NONE,
} audio_policy_forced_cfg_t;

/* usages used for audio_policy->set_force_use() */
typedef enum {
    AUDIO_POLICY_FORCE_FOR_COMMUNICATION,
    AUDIO_POLICY_FORCE_FOR_MEDIA,
    AUDIO_POLICY_FORCE_FOR_RECORD,
    AUDIO_POLICY_FORCE_FOR_DOCK,

    AUDIO_POLICY_FORCE_USE_CNT,
    AUDIO_POLICY_FORCE_USE_MAX = AUDIO_POLICY_FORCE_USE_CNT - 1,
} audio_policy_force_use_t;

typedef enum {
    AUDIO_STREAM_DEFAULT          = -1,
    AUDIO_STREAM_VOICE_CALL       = 0,
    AUDIO_STREAM_SYSTEM           = 1,
    AUDIO_STREAM_RING             = 2,
    AUDIO_STREAM_MUSIC            = 3,
    AUDIO_STREAM_ALARM            = 4,
    AUDIO_STREAM_NOTIFICATION     = 5,
    AUDIO_STREAM_BLUETOOTH_SCO    = 6,
    AUDIO_STREAM_ENFORCED_AUDIBLE = 7, /* Sounds that cannot be muted by user and must be routed to speaker */
    AUDIO_STREAM_DTMF             = 8,
    AUDIO_STREAM_TTS              = 9,
    AUDIO_STREAM_FM               = 10,

    AUDIO_STREAM_CNT,
    AUDIO_STREAM_MAX              = AUDIO_STREAM_CNT - 1,
} audio_stream_type_t;

/* PCM sub formats */
typedef enum {
    AUDIO_FORMAT_PCM_SUB_16_BIT          = 0x1, /* DO NOT CHANGE - PCM signed 16 bits */
    AUDIO_FORMAT_PCM_SUB_8_BIT           = 0x2, /* DO NOT CHANGE - PCM unsigned 8 bits */
    AUDIO_FORMAT_PCM_SUB_32_BIT          = 0x3, /* PCM signed .31 fixed point */
    AUDIO_FORMAT_PCM_SUB_8_24_BIT        = 0x4, /* PCM signed 7.24 fixed point */
} audio_format_pcm_sub_fmt_t;

/* Audio format consists in a main format field (upper 8 bits) and a sub format
 * field (lower 24 bits).
 *
 * The main format indicates the main codec type. The sub format field
 * indicates options and parameters for each format. The sub format is mainly
 * used for record to indicate for instance the requested bitrate or profile.
 * It can also be used for certain formats to give informations not present in
 * the encoded audio stream (e.g. octet alignement for AMR).
 */
typedef enum {
    AUDIO_FORMAT_INVALID             = 0xFFFFFFFFUL,
    AUDIO_FORMAT_DEFAULT             = 0,
    AUDIO_FORMAT_PCM                 = 0x00000000UL, /* DO NOT CHANGE */
    AUDIO_FORMAT_MP3                 = 0x01000000UL,
    AUDIO_FORMAT_AMR_NB              = 0x02000000UL,
    AUDIO_FORMAT_AMR_WB              = 0x03000000UL,
    AUDIO_FORMAT_AAC                 = 0x04000000UL,
    AUDIO_FORMAT_HE_AAC_V1           = 0x05000000UL,
    AUDIO_FORMAT_HE_AAC_V2           = 0x06000000UL,
    AUDIO_FORMAT_VORBIS              = 0x07000000UL,
    AUDIO_FORMAT_MAIN_MASK           = 0xFF000000UL,
    AUDIO_FORMAT_SUB_MASK            = 0x00FFFFFFUL,

    /* Aliases */
    AUDIO_FORMAT_PCM_16_BIT          = (AUDIO_FORMAT_PCM |
                                        AUDIO_FORMAT_PCM_SUB_16_BIT),
    AUDIO_FORMAT_PCM_8_BIT           = (AUDIO_FORMAT_PCM |
                                        AUDIO_FORMAT_PCM_SUB_8_BIT),
    AUDIO_FORMAT_PCM_32_BIT          = (AUDIO_FORMAT_PCM |
                                        AUDIO_FORMAT_PCM_SUB_32_BIT),
    AUDIO_FORMAT_PCM_8_24_BIT        = (AUDIO_FORMAT_PCM |
                                        AUDIO_FORMAT_PCM_SUB_8_24_BIT),
} audio_format_t;

typedef enum {
    /* output channels */
    AUDIO_CHANNEL_OUT_FRONT_LEFT            = 0x1,
    AUDIO_CHANNEL_OUT_FRONT_RIGHT           = 0x2,
    AUDIO_CHANNEL_OUT_FRONT_CENTER          = 0x4,
    AUDIO_CHANNEL_OUT_LOW_FREQUENCY         = 0x8,
    AUDIO_CHANNEL_OUT_BACK_LEFT             = 0x10,
    AUDIO_CHANNEL_OUT_BACK_RIGHT            = 0x20,
    AUDIO_CHANNEL_OUT_FRONT_LEFT_OF_CENTER  = 0x40,
    AUDIO_CHANNEL_OUT_FRONT_RIGHT_OF_CENTER = 0x80,
    AUDIO_CHANNEL_OUT_BACK_CENTER           = 0x100,
    AUDIO_CHANNEL_OUT_SIDE_LEFT             = 0x200,
    AUDIO_CHANNEL_OUT_SIDE_RIGHT            = 0x400,
    AUDIO_CHANNEL_OUT_TOP_CENTER            = 0x800,
    AUDIO_CHANNEL_OUT_TOP_FRONT_LEFT        = 0x1000,
    AUDIO_CHANNEL_OUT_TOP_FRONT_CENTER      = 0x2000,
    AUDIO_CHANNEL_OUT_TOP_FRONT_RIGHT       = 0x4000,
    AUDIO_CHANNEL_OUT_TOP_BACK_LEFT         = 0x8000,
    AUDIO_CHANNEL_OUT_TOP_BACK_CENTER       = 0x10000,
    AUDIO_CHANNEL_OUT_TOP_BACK_RIGHT        = 0x20000,

    AUDIO_CHANNEL_OUT_MONO     = AUDIO_CHANNEL_OUT_FRONT_LEFT,
    AUDIO_CHANNEL_OUT_STEREO   = (AUDIO_CHANNEL_OUT_FRONT_LEFT |
                                  AUDIO_CHANNEL_OUT_FRONT_RIGHT),
    AUDIO_CHANNEL_OUT_QUAD     = (AUDIO_CHANNEL_OUT_FRONT_LEFT |
                                  AUDIO_CHANNEL_OUT_FRONT_RIGHT |
                                  AUDIO_CHANNEL_OUT_BACK_LEFT |
                                  AUDIO_CHANNEL_OUT_BACK_RIGHT),
    AUDIO_CHANNEL_OUT_SURROUND = (AUDIO_CHANNEL_OUT_FRONT_LEFT |
                                  AUDIO_CHANNEL_OUT_FRONT_RIGHT |
                                  AUDIO_CHANNEL_OUT_FRONT_CENTER |
                                  AUDIO_CHANNEL_OUT_BACK_CENTER),
    AUDIO_CHANNEL_OUT_5POINT1  = (AUDIO_CHANNEL_OUT_FRONT_LEFT |
                                  AUDIO_CHANNEL_OUT_FRONT_RIGHT |
                                  AUDIO_CHANNEL_OUT_FRONT_CENTER |
                                  AUDIO_CHANNEL_OUT_LOW_FREQUENCY |
                                  AUDIO_CHANNEL_OUT_BACK_LEFT |
                                  AUDIO_CHANNEL_OUT_BACK_RIGHT),
    // matches the correct AudioFormat.CHANNEL_OUT_7POINT1_SURROUND definition for 7.1
    AUDIO_CHANNEL_OUT_7POINT1  = (AUDIO_CHANNEL_OUT_FRONT_LEFT |
                                  AUDIO_CHANNEL_OUT_FRONT_RIGHT |
                                  AUDIO_CHANNEL_OUT_FRONT_CENTER |
                                  AUDIO_CHANNEL_OUT_LOW_FREQUENCY |
                                  AUDIO_CHANNEL_OUT_BACK_LEFT |
                                  AUDIO_CHANNEL_OUT_BACK_RIGHT |
                                  AUDIO_CHANNEL_OUT_SIDE_LEFT |
                                  AUDIO_CHANNEL_OUT_SIDE_RIGHT),
    AUDIO_CHANNEL_OUT_ALL      = (AUDIO_CHANNEL_OUT_FRONT_LEFT |
                                  AUDIO_CHANNEL_OUT_FRONT_RIGHT |
                                  AUDIO_CHANNEL_OUT_FRONT_CENTER |
                                  AUDIO_CHANNEL_OUT_LOW_FREQUENCY |
                                  AUDIO_CHANNEL_OUT_BACK_LEFT |
                                  AUDIO_CHANNEL_OUT_BACK_RIGHT |
                                  AUDIO_CHANNEL_OUT_FRONT_LEFT_OF_CENTER |
                                  AUDIO_CHANNEL_OUT_FRONT_RIGHT_OF_CENTER |
                                  AUDIO_CHANNEL_OUT_BACK_CENTER|
                                  AUDIO_CHANNEL_OUT_SIDE_LEFT|
                                  AUDIO_CHANNEL_OUT_SIDE_RIGHT|
                                  AUDIO_CHANNEL_OUT_TOP_CENTER|
                                  AUDIO_CHANNEL_OUT_TOP_FRONT_LEFT|
                                  AUDIO_CHANNEL_OUT_TOP_FRONT_CENTER|
                                  AUDIO_CHANNEL_OUT_TOP_FRONT_RIGHT|
                                  AUDIO_CHANNEL_OUT_TOP_BACK_LEFT|
                                  AUDIO_CHANNEL_OUT_TOP_BACK_CENTER|
                                  AUDIO_CHANNEL_OUT_TOP_BACK_RIGHT),

    /* input channels */
    AUDIO_CHANNEL_IN_LEFT            = 0x4,
    AUDIO_CHANNEL_IN_RIGHT           = 0x8,
    AUDIO_CHANNEL_IN_FRONT           = 0x10,
    AUDIO_CHANNEL_IN_BACK            = 0x20,
    AUDIO_CHANNEL_IN_LEFT_PROCESSED  = 0x40,
    AUDIO_CHANNEL_IN_RIGHT_PROCESSED = 0x80,
    AUDIO_CHANNEL_IN_FRONT_PROCESSED = 0x100,
    AUDIO_CHANNEL_IN_BACK_PROCESSED  = 0x200,
    AUDIO_CHANNEL_IN_PRESSURE        = 0x400,
    AUDIO_CHANNEL_IN_X_AXIS          = 0x800,
    AUDIO_CHANNEL_IN_Y_AXIS          = 0x1000,
    AUDIO_CHANNEL_IN_Z_AXIS          = 0x2000,
    AUDIO_CHANNEL_IN_VOICE_UPLINK    = 0x4000,
    AUDIO_CHANNEL_IN_VOICE_DNLINK    = 0x8000,

    AUDIO_CHANNEL_IN_MONO   = AUDIO_CHANNEL_IN_FRONT,
    AUDIO_CHANNEL_IN_STEREO = (AUDIO_CHANNEL_IN_LEFT | AUDIO_CHANNEL_IN_RIGHT),
    AUDIO_CHANNEL_IN_ALL    = (AUDIO_CHANNEL_IN_LEFT |
                               AUDIO_CHANNEL_IN_RIGHT |
                               AUDIO_CHANNEL_IN_FRONT |
                               AUDIO_CHANNEL_IN_BACK|
                               AUDIO_CHANNEL_IN_LEFT_PROCESSED |
                               AUDIO_CHANNEL_IN_RIGHT_PROCESSED |
                               AUDIO_CHANNEL_IN_FRONT_PROCESSED |
                               AUDIO_CHANNEL_IN_BACK_PROCESSED|
                               AUDIO_CHANNEL_IN_PRESSURE |
                               AUDIO_CHANNEL_IN_X_AXIS |
                               AUDIO_CHANNEL_IN_Y_AXIS |
                               AUDIO_CHANNEL_IN_Z_AXIS |
                               AUDIO_CHANNEL_IN_VOICE_UPLINK |
                               AUDIO_CHANNEL_IN_VOICE_DNLINK),
} audio_channels_t;

#if ANDROID_VERSION >= 17
typedef enum {
    AUDIO_MODE_INVALID          = -2,
    AUDIO_MODE_CURRENT          = -1,
    AUDIO_MODE_NORMAL           = 0,
    AUDIO_MODE_RINGTONE         = 1,
    AUDIO_MODE_IN_CALL          = 2,
    AUDIO_MODE_IN_COMMUNICATION = 3,

    AUDIO_MODE_CNT,
    AUDIO_MODE_MAX              = AUDIO_MODE_CNT - 1,
} audio_mode_t;
#endif
#endif

#if ANDROID_VERSION < 17
typedef enum {
    /* output devices */      
    AUDIO_DEVICE_OUT_EARPIECE                  = 0x1,
    AUDIO_DEVICE_OUT_SPEAKER                   = 0x2,
    AUDIO_DEVICE_OUT_WIRED_HEADSET             = 0x4,
    AUDIO_DEVICE_OUT_WIRED_HEADPHONE           = 0x8,
    AUDIO_DEVICE_OUT_BLUETOOTH_SCO             = 0x10,
    AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET     = 0x20,
    AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT      = 0x40,
    AUDIO_DEVICE_OUT_BLUETOOTH_A2DP            = 0x80,
    AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES = 0x100,
    AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER    = 0x200,
    AUDIO_DEVICE_OUT_AUX_DIGITAL               = 0x400,
    AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET         = 0x800,
    AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET         = 0x1000,
    AUDIO_DEVICE_OUT_FM                        = 0x2000,
    AUDIO_DEVICE_OUT_ANC_HEADSET               = 0x4000,
    AUDIO_DEVICE_OUT_ANC_HEADPHONE             = 0x8000,
    AUDIO_DEVICE_OUT_FM_TX                     = 0x10000,
    AUDIO_DEVICE_OUT_DIRECTOUTPUT              = 0x20000,
    AUDIO_DEVICE_OUT_PROXY                     = 0x40000,
    AUDIO_DEVICE_OUT_DEFAULT                   = 0x80000,
    AUDIO_DEVICE_OUT_ALL      = (AUDIO_DEVICE_OUT_EARPIECE |
                                 AUDIO_DEVICE_OUT_SPEAKER |
                                 AUDIO_DEVICE_OUT_WIRED_HEADSET |
                                 AUDIO_DEVICE_OUT_WIRED_HEADPHONE |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_SCO |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_A2DP |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER |
                                 AUDIO_DEVICE_OUT_AUX_DIGITAL |
                                 AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET |
                                 AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET |
                                 AUDIO_DEVICE_OUT_FM |
                                 AUDIO_DEVICE_OUT_ANC_HEADSET |
                                 AUDIO_DEVICE_OUT_ANC_HEADPHONE |
                                 AUDIO_DEVICE_OUT_FM_TX |
                                 AUDIO_DEVICE_OUT_DIRECTOUTPUT |
                                 AUDIO_DEVICE_OUT_PROXY |
                                 AUDIO_DEVICE_OUT_DEFAULT),
    AUDIO_DEVICE_OUT_ALL_A2DP = (AUDIO_DEVICE_OUT_BLUETOOTH_A2DP |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER),
    AUDIO_DEVICE_OUT_ALL_SCO  = (AUDIO_DEVICE_OUT_BLUETOOTH_SCO |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT),
    /* input devices */
    AUDIO_DEVICE_IN_COMMUNICATION         = 0x100000,
    AUDIO_DEVICE_IN_AMBIENT               = 0x200000,
    AUDIO_DEVICE_IN_BUILTIN_MIC           = 0x400000,
    AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET = 0x800000,
    AUDIO_DEVICE_IN_WIRED_HEADSET         = 0x1000000,
    AUDIO_DEVICE_IN_AUX_DIGITAL           = 0x2000000,
    AUDIO_DEVICE_IN_VOICE_CALL            = 0x4000000,
    AUDIO_DEVICE_IN_BACK_MIC              = 0x8000000,
    AUDIO_DEVICE_IN_ANC_HEADSET           = 0x10000000,
    AUDIO_DEVICE_IN_FM_RX                 = 0x20000000,
    AUDIO_DEVICE_IN_FM_RX_A2DP            = 0x40000000,
    AUDIO_DEVICE_IN_DEFAULT               = 0x80000000,
    
    AUDIO_DEVICE_IN_ALL     = (AUDIO_DEVICE_IN_COMMUNICATION |
                               AUDIO_DEVICE_IN_AMBIENT |
                               AUDIO_DEVICE_IN_BUILTIN_MIC |
                               AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET |
                               AUDIO_DEVICE_IN_WIRED_HEADSET |
                               AUDIO_DEVICE_IN_AUX_DIGITAL |
                               AUDIO_DEVICE_IN_VOICE_CALL |
                               AUDIO_DEVICE_IN_BACK_MIC |
                               AUDIO_DEVICE_IN_ANC_HEADSET |
                               AUDIO_DEVICE_IN_FM_RX |
                               AUDIO_DEVICE_IN_FM_RX_A2DP |
                               AUDIO_DEVICE_IN_DEFAULT),
    AUDIO_DEVICE_IN_ALL_SCO = AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET,
} audio_devices_t;
#else
enum {
    AUDIO_DEVICE_NONE                          = 0x0,
    /* reserved bits */
    AUDIO_DEVICE_BIT_IN                        = 0x80000000,
    AUDIO_DEVICE_BIT_DEFAULT                   = 0x40000000,
    /* output devices */
    AUDIO_DEVICE_OUT_EARPIECE                  = 0x1,
    AUDIO_DEVICE_OUT_SPEAKER                   = 0x2,
    AUDIO_DEVICE_OUT_WIRED_HEADSET             = 0x4,
    AUDIO_DEVICE_OUT_WIRED_HEADPHONE           = 0x8,
    AUDIO_DEVICE_OUT_BLUETOOTH_SCO             = 0x10,
    AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET     = 0x20,
    AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT      = 0x40,
    AUDIO_DEVICE_OUT_BLUETOOTH_A2DP            = 0x80,
    AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES = 0x100,
    AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER    = 0x200,
    AUDIO_DEVICE_OUT_AUX_DIGITAL               = 0x400,
    AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET         = 0x800,
    AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET         = 0x1000,
    AUDIO_DEVICE_OUT_USB_ACCESSORY             = 0x2000,
    AUDIO_DEVICE_OUT_USB_DEVICE                = 0x4000,
    AUDIO_DEVICE_OUT_REMOTE_SUBMIX             = 0x8000,
    AUDIO_DEVICE_OUT_ANC_HEADSET               = 0x10000,
    AUDIO_DEVICE_OUT_ANC_HEADPHONE             = 0x20000,
    AUDIO_DEVICE_OUT_PROXY                     = 0x40000,
    AUDIO_DEVICE_OUT_FM                        = 0x80000,
    AUDIO_DEVICE_OUT_FM_TX                     = 0x100000,
    AUDIO_DEVICE_OUT_DEFAULT                   = AUDIO_DEVICE_BIT_DEFAULT,
    AUDIO_DEVICE_OUT_ALL      = (AUDIO_DEVICE_OUT_EARPIECE |
                                 AUDIO_DEVICE_OUT_SPEAKER |
                                 AUDIO_DEVICE_OUT_WIRED_HEADSET |
                                 AUDIO_DEVICE_OUT_WIRED_HEADPHONE |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_SCO |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_A2DP |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER |
                                 AUDIO_DEVICE_OUT_AUX_DIGITAL |
                                 AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET |
                                 AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET |
                                 AUDIO_DEVICE_OUT_USB_ACCESSORY |
                                 AUDIO_DEVICE_OUT_USB_DEVICE |
                                 AUDIO_DEVICE_OUT_REMOTE_SUBMIX |
                                 AUDIO_DEVICE_OUT_ANC_HEADSET |
                                 AUDIO_DEVICE_OUT_ANC_HEADPHONE |
                                 AUDIO_DEVICE_OUT_PROXY |
                                 AUDIO_DEVICE_OUT_FM |
                                 AUDIO_DEVICE_OUT_FM_TX |
                                 AUDIO_DEVICE_OUT_DEFAULT),
    AUDIO_DEVICE_OUT_ALL_A2DP = (AUDIO_DEVICE_OUT_BLUETOOTH_A2DP |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER),
    AUDIO_DEVICE_OUT_ALL_SCO  = (AUDIO_DEVICE_OUT_BLUETOOTH_SCO |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET |
                                 AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT),
    AUDIO_DEVICE_OUT_ALL_USB  = (AUDIO_DEVICE_OUT_USB_ACCESSORY |
                                 AUDIO_DEVICE_OUT_USB_DEVICE),

    /* input devices */
    AUDIO_DEVICE_IN_COMMUNICATION         = AUDIO_DEVICE_BIT_IN | 0x1,
    AUDIO_DEVICE_IN_AMBIENT               = AUDIO_DEVICE_BIT_IN | 0x2,
    AUDIO_DEVICE_IN_BUILTIN_MIC           = AUDIO_DEVICE_BIT_IN | 0x4,
    AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET = AUDIO_DEVICE_BIT_IN | 0x8,
    AUDIO_DEVICE_IN_WIRED_HEADSET         = AUDIO_DEVICE_BIT_IN | 0x10,
    AUDIO_DEVICE_IN_AUX_DIGITAL           = AUDIO_DEVICE_BIT_IN | 0x20,
    AUDIO_DEVICE_IN_VOICE_CALL            = AUDIO_DEVICE_BIT_IN | 0x40,
    AUDIO_DEVICE_IN_BACK_MIC              = AUDIO_DEVICE_BIT_IN | 0x80,
    AUDIO_DEVICE_IN_REMOTE_SUBMIX         = AUDIO_DEVICE_BIT_IN | 0x100,
    AUDIO_DEVICE_IN_ANLG_DOCK_HEADSET     = AUDIO_DEVICE_BIT_IN | 0x200,
    AUDIO_DEVICE_IN_DGTL_DOCK_HEADSET     = AUDIO_DEVICE_BIT_IN | 0x400,
    AUDIO_DEVICE_IN_USB_ACCESSORY         = AUDIO_DEVICE_BIT_IN | 0x800,
    AUDIO_DEVICE_IN_USB_DEVICE            = AUDIO_DEVICE_BIT_IN | 0x1000,
    AUDIO_DEVICE_IN_ANC_HEADSET           = AUDIO_DEVICE_BIT_IN | 0x2000,
    AUDIO_DEVICE_IN_PROXY                 = AUDIO_DEVICE_BIT_IN | 0x4000,
    AUDIO_DEVICE_IN_FM_RX                 = AUDIO_DEVICE_BIT_IN | 0x8000,
    AUDIO_DEVICE_IN_FM_RX_A2DP            = AUDIO_DEVICE_BIT_IN | 0x10000,
    AUDIO_DEVICE_IN_DEFAULT               = AUDIO_DEVICE_BIT_IN | AUDIO_DEVICE_BIT_DEFAULT,

    AUDIO_DEVICE_IN_ALL     = (AUDIO_DEVICE_IN_COMMUNICATION |
                               AUDIO_DEVICE_IN_AMBIENT |
                               AUDIO_DEVICE_IN_BUILTIN_MIC |
                               AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET |
                               AUDIO_DEVICE_IN_WIRED_HEADSET |
                               AUDIO_DEVICE_IN_AUX_DIGITAL |
                               AUDIO_DEVICE_IN_VOICE_CALL |
                               AUDIO_DEVICE_IN_BACK_MIC |
                               AUDIO_DEVICE_IN_REMOTE_SUBMIX |
                               AUDIO_DEVICE_IN_ANLG_DOCK_HEADSET |
                               AUDIO_DEVICE_IN_DGTL_DOCK_HEADSET |
                               AUDIO_DEVICE_IN_USB_ACCESSORY |
                               AUDIO_DEVICE_IN_USB_DEVICE |
                               AUDIO_DEVICE_IN_ANC_HEADSET |
                               AUDIO_DEVICE_IN_FM_RX |
                               AUDIO_DEVICE_IN_FM_RX_A2DP |
                               AUDIO_DEVICE_IN_PROXY |
                               AUDIO_DEVICE_IN_DEFAULT),
    AUDIO_DEVICE_IN_ALL_SCO = AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET,
};

typedef uint32_t audio_devices_t;
#endif

/* device connection states used for audio_policy->set_device_connection_state()
 *  */
typedef enum {
    AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE,
    AUDIO_POLICY_DEVICE_STATE_AVAILABLE,
    
    AUDIO_POLICY_DEVICE_STATE_CNT,
    AUDIO_POLICY_DEVICE_STATE_MAX = AUDIO_POLICY_DEVICE_STATE_CNT - 1,
} audio_policy_dev_state_t;

namespace android {

typedef void (*audio_error_callback)(status_t err);
typedef int audio_io_handle_t;

class IAudioPolicyService;
class String8;

class AudioSystem
{
public:

    enum stream_type {
        DEFAULT          =-1,
        VOICE_CALL       = 0,
        SYSTEM           = 1,
        RING             = 2,
        MUSIC            = 3,
        ALARM            = 4,
        NOTIFICATION     = 5,
        BLUETOOTH_SCO    = 6,
        ENFORCED_AUDIBLE = 7, // Sounds that cannot be muted by user and must be routed to speaker
        DTMF             = 8,
        TTS              = 9,
        FM               = 10,
        NUM_STREAM_TYPES
    };

    // Audio sub formats (see AudioSystem::audio_format).
    enum pcm_sub_format {
        PCM_SUB_16_BIT          = 0x1, // must be 1 for backward compatibility
        PCM_SUB_8_BIT           = 0x2, // must be 2 for backward compatibility
    };

    // MP3 sub format field definition : can use 11 LSBs in the same way as MP3 frame header to specify
    // bit rate, stereo mode, version...
    enum mp3_sub_format {
        //TODO
    };

    // AMR NB/WB sub format field definition: specify frame block interleaving, bandwidth efficient or octet aligned,
    // encoding mode for recording...
    enum amr_sub_format {
        //TODO
    };

    // AAC sub format field definition: specify profile or bitrate for recording...
    enum aac_sub_format {
        //TODO
    };

    // VORBIS sub format field definition: specify quality for recording...
    enum vorbis_sub_format {
        //TODO
    };

    // Audio format consists in a main format field (upper 8 bits) and a sub format field (lower 24 bits).
    // The main format indicates the main codec type. The sub format field indicates options and parameters
    // for each format. The sub format is mainly used for record to indicate for instance the requested bitrate
    // or profile. It can also be used for certain formats to give informations not present in the encoded
    // audio stream (e.g. octet alignement for AMR).
    enum audio_format {
        INVALID_FORMAT      = -1,
        FORMAT_DEFAULT      = 0,
        PCM                 = 0x00000000, // must be 0 for backward compatibility
        MP3                 = 0x01000000,
        AMR_NB              = 0x02000000,
        AMR_WB              = 0x03000000,
        AAC                 = 0x04000000,
        HE_AAC_V1           = 0x05000000,
        HE_AAC_V2           = 0x06000000,
        VORBIS              = 0x07000000,
        EVRC                = 0x08000000,
        QCELP               = 0x09000000,
        VOIP_PCM_INPUT      = 0x0A000000,
        MAIN_FORMAT_MASK    = 0xFF000000,
        SUB_FORMAT_MASK     = 0x00FFFFFF,
        // Aliases
        PCM_16_BIT          = (PCM|PCM_SUB_16_BIT),
        PCM_8_BIT          = (PCM|PCM_SUB_8_BIT)
    };


    // Channel mask definitions must be kept in sync with JAVA values in /media/java/android/media/AudioFormat.java
    enum audio_channels {
        // output channels
        CHANNEL_OUT_FRONT_LEFT = 0x4,
        CHANNEL_OUT_FRONT_RIGHT = 0x8,
        CHANNEL_OUT_FRONT_CENTER = 0x10,
        CHANNEL_OUT_LOW_FREQUENCY = 0x20,
        CHANNEL_OUT_BACK_LEFT = 0x40,
        CHANNEL_OUT_BACK_RIGHT = 0x80,
        CHANNEL_OUT_FRONT_LEFT_OF_CENTER = 0x100,
        CHANNEL_OUT_FRONT_RIGHT_OF_CENTER = 0x200,
        CHANNEL_OUT_BACK_CENTER = 0x400,
        CHANNEL_OUT_MONO = CHANNEL_OUT_FRONT_LEFT,
        CHANNEL_OUT_STEREO = (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT),
        CHANNEL_OUT_QUAD = (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT |
                CHANNEL_OUT_BACK_LEFT | CHANNEL_OUT_BACK_RIGHT),
        CHANNEL_OUT_SURROUND = (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT |
                CHANNEL_OUT_FRONT_CENTER | CHANNEL_OUT_BACK_CENTER),
        CHANNEL_OUT_5POINT1 = (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT |
                CHANNEL_OUT_FRONT_CENTER | CHANNEL_OUT_LOW_FREQUENCY | CHANNEL_OUT_BACK_LEFT | CHANNEL_OUT_BACK_RIGHT),
        CHANNEL_OUT_7POINT1 = (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT |
                CHANNEL_OUT_FRONT_CENTER | CHANNEL_OUT_LOW_FREQUENCY | CHANNEL_OUT_BACK_LEFT | CHANNEL_OUT_BACK_RIGHT |
                CHANNEL_OUT_FRONT_LEFT_OF_CENTER | CHANNEL_OUT_FRONT_RIGHT_OF_CENTER),
        CHANNEL_OUT_ALL = (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT |
                CHANNEL_OUT_FRONT_CENTER | CHANNEL_OUT_LOW_FREQUENCY | CHANNEL_OUT_BACK_LEFT | CHANNEL_OUT_BACK_RIGHT |
                CHANNEL_OUT_FRONT_LEFT_OF_CENTER | CHANNEL_OUT_FRONT_RIGHT_OF_CENTER | CHANNEL_OUT_BACK_CENTER),

        // input channels
        CHANNEL_IN_LEFT = 0x4,
        CHANNEL_IN_RIGHT = 0x8,
        CHANNEL_IN_FRONT = 0x10,
        CHANNEL_IN_BACK = 0x20,
        CHANNEL_IN_LEFT_PROCESSED = 0x40,
        CHANNEL_IN_RIGHT_PROCESSED = 0x80,
        CHANNEL_IN_FRONT_PROCESSED = 0x100,
        CHANNEL_IN_BACK_PROCESSED = 0x200,
        CHANNEL_IN_PRESSURE = 0x400,
        CHANNEL_IN_X_AXIS = 0x800,
        CHANNEL_IN_Y_AXIS = 0x1000,
        CHANNEL_IN_Z_AXIS = 0x2000,
        CHANNEL_IN_VOICE_UPLINK = 0x4000,
        CHANNEL_IN_VOICE_DNLINK = 0x8000,
        CHANNEL_IN_MONO = CHANNEL_IN_FRONT,
        CHANNEL_IN_STEREO = (CHANNEL_IN_LEFT | CHANNEL_IN_RIGHT),
        CHANNEL_IN_ALL = (CHANNEL_IN_LEFT | CHANNEL_IN_RIGHT | CHANNEL_IN_FRONT | CHANNEL_IN_BACK|
                CHANNEL_IN_LEFT_PROCESSED | CHANNEL_IN_RIGHT_PROCESSED | CHANNEL_IN_FRONT_PROCESSED | CHANNEL_IN_BACK_PROCESSED|
                CHANNEL_IN_PRESSURE | CHANNEL_IN_X_AXIS | CHANNEL_IN_Y_AXIS | CHANNEL_IN_Z_AXIS |
                CHANNEL_IN_VOICE_UPLINK | CHANNEL_IN_VOICE_DNLINK)
    };

    enum audio_mode {
        MODE_INVALID = -2,
        MODE_CURRENT = -1,
        MODE_NORMAL = 0,
        MODE_RINGTONE,
        MODE_IN_CALL,
        MODE_IN_COMMUNICATION,
        NUM_MODES  // not a valid entry, denotes end-of-list
    };

    enum audio_in_acoustics {
        AGC_ENABLE    = 0x0001,
        AGC_DISABLE   = 0,
        NS_ENABLE     = 0x0002,
        NS_DISABLE    = 0,
        TX_IIR_ENABLE = 0x0004,
        TX_DISABLE    = 0
    };

    // special audio session values
    enum audio_sessions {
        SESSION_OUTPUT_STAGE = -1, // session for effects attached to a particular output stream
                                   // (value must be less than 0)
        SESSION_OUTPUT_MIX = 0,    // session for effects applied to output mix. These effects can
                                   // be moved by audio policy manager to another output stream
                                   // (value must be 0)
    };

    /* These are static methods to control the system-wide AudioFlinger
     * only privileged processes can have access to them
     */

    // mute/unmute microphone
    static status_t muteMicrophone(bool state);
    static status_t isMicrophoneMuted(bool *state);

    // set/get master volume
    static status_t setMasterVolume(float value);
    static status_t getMasterVolume(float* volume);
    // mute/unmute audio outputs
    static status_t setMasterMute(bool mute);
    static status_t getMasterMute(bool* mute);

    // set/get stream volume on specified output
    static status_t setStreamVolume(int stream, float value, int output);
    static status_t getStreamVolume(int stream, float* volume, int output);

    // mute/unmute stream
    static status_t setStreamMute(int stream, bool mute);
    static status_t getStreamMute(int stream, bool* mute);

    // set audio mode in audio hardware (see AudioSystem::audio_mode)
    static status_t setMode(int mode);

    // returns true in *state if tracks are active on the specified stream
    static status_t isStreamActive(int stream, bool *state);

    // set/get audio hardware parameters. The function accepts a list of parameters
    // key value pairs in the form: key1=value1;key2=value2;...
    // Some keys are reserved for standard parameters (See AudioParameter class).
    static status_t setParameters(audio_io_handle_t ioHandle, const String8& keyValuePairs);
    static String8  getParameters(audio_io_handle_t ioHandle, const String8& keys);

    static void setErrorCallback(audio_error_callback cb);

    // helper function to obtain AudioFlinger service handle
    static const sp<IAudioFlinger>& get_audio_flinger();

    static float linearToLog(int volume);
    static int logToLinear(float volume);

    static status_t getOutputSamplingRate(int* samplingRate, int stream = DEFAULT);
    static status_t getOutputFrameCount(int* frameCount, int stream = DEFAULT);
    static status_t getOutputLatency(uint32_t* latency, int stream = DEFAULT);

    static bool routedToA2dpOutput(int streamType);

    static status_t getInputBufferSize(uint32_t sampleRate, int format, int channelCount,
        size_t* buffSize);

    static status_t setVoiceVolume(float volume);

    // return the number of audio frames written by AudioFlinger to audio HAL and
    // audio dsp to DAC since the output on which the specificed stream is playing
    // has exited standby.
    // returned status (from utils/Errors.h) can be:
    // - NO_ERROR: successful operation, halFrames and dspFrames point to valid data
    // - INVALID_OPERATION: Not supported on current hardware platform
    // - BAD_VALUE: invalid parameter
    // NOTE: this feature is not supported on all hardware platforms and it is
    // necessary to check returned status before using the returned values.
    static status_t getRenderPosition(uint32_t *halFrames, uint32_t *dspFrames, int stream = DEFAULT);

    static unsigned int  getInputFramesLost(audio_io_handle_t ioHandle);

    static int newAudioSessionId();
    //
    // AudioPolicyService interface
    //

    enum audio_devices {
        // output devices
        DEVICE_OUT_EARPIECE = 0x1,
        DEVICE_OUT_SPEAKER = 0x2,
        DEVICE_OUT_WIRED_HEADSET = 0x4,
        DEVICE_OUT_WIRED_HEADPHONE = 0x8,
        DEVICE_OUT_BLUETOOTH_SCO = 0x10,
        DEVICE_OUT_BLUETOOTH_SCO_HEADSET = 0x20,
        DEVICE_OUT_BLUETOOTH_SCO_CARKIT = 0x40,
        DEVICE_OUT_BLUETOOTH_A2DP = 0x80,
        DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES = 0x100,
        DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER = 0x200,
        DEVICE_OUT_AUX_DIGITAL = 0x400,
        DEVICE_OUT_DEFAULT = 0x8000,
        DEVICE_OUT_ALL = (DEVICE_OUT_EARPIECE | DEVICE_OUT_SPEAKER | DEVICE_OUT_WIRED_HEADSET |
                DEVICE_OUT_WIRED_HEADPHONE | DEVICE_OUT_BLUETOOTH_SCO | DEVICE_OUT_BLUETOOTH_SCO_HEADSET |
                DEVICE_OUT_BLUETOOTH_SCO_CARKIT | DEVICE_OUT_BLUETOOTH_A2DP | DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES |
                DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER | DEVICE_OUT_AUX_DIGITAL | DEVICE_OUT_DEFAULT),
        DEVICE_OUT_ALL_A2DP = (DEVICE_OUT_BLUETOOTH_A2DP | DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES |
                DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER),

        // input devices
        DEVICE_IN_COMMUNICATION = 0x10000,
        DEVICE_IN_AMBIENT = 0x20000,
        DEVICE_IN_BUILTIN_MIC = 0x40000,
        DEVICE_IN_BLUETOOTH_SCO_HEADSET = 0x80000,
        DEVICE_IN_WIRED_HEADSET = 0x100000,
        DEVICE_IN_AUX_DIGITAL = 0x200000,
        DEVICE_IN_VOICE_CALL = 0x400000,
        DEVICE_IN_BACK_MIC = 0x800000,
        DEVICE_IN_DEFAULT = 0x80000000,

        DEVICE_IN_ALL = (DEVICE_IN_COMMUNICATION | DEVICE_IN_AMBIENT | DEVICE_IN_BUILTIN_MIC |
                DEVICE_IN_BLUETOOTH_SCO_HEADSET | DEVICE_IN_WIRED_HEADSET | DEVICE_IN_AUX_DIGITAL |
                DEVICE_IN_VOICE_CALL | DEVICE_IN_BACK_MIC | DEVICE_IN_DEFAULT)
    };

    // device connection states used for setDeviceConnectionState()
    enum device_connection_state {
        DEVICE_STATE_UNAVAILABLE,
        DEVICE_STATE_AVAILABLE,
        NUM_DEVICE_STATES
    };

    // request to open a direct output with getOutput() (by opposition to sharing an output with other AudioTracks)
    enum output_flags {
        OUTPUT_FLAG_INDIRECT = 0x0,
        OUTPUT_FLAG_DIRECT = 0x1
    };

    // device categories used for setForceUse()
    enum forced_config {
        FORCE_NONE,
        FORCE_SPEAKER,
        FORCE_HEADPHONES,
        FORCE_BT_SCO,
        FORCE_BT_A2DP,
        FORCE_WIRED_ACCESSORY,
        FORCE_BT_CAR_DOCK,
        FORCE_BT_DESK_DOCK,
        NUM_FORCE_CONFIG,
        FORCE_DEFAULT = FORCE_NONE
    };

    // usages used for setForceUse()
    enum force_use {
        FOR_COMMUNICATION,
        FOR_MEDIA,
        FOR_RECORD,
        FOR_DOCK,
        NUM_FORCE_USE
    };

    // types of io configuration change events received with ioConfigChanged()
    enum io_config_event {
        OUTPUT_OPENED,
        OUTPUT_CLOSED,
        OUTPUT_CONFIG_CHANGED,
        INPUT_OPENED,
        INPUT_CLOSED,
        INPUT_CONFIG_CHANGED,
        STREAM_CONFIG_CHANGED,
        NUM_CONFIG_EVENTS
    };

    // audio output descritor used to cache output configurations in client process to avoid frequent calls
    // through IAudioFlinger
    class OutputDescriptor {
    public:
        OutputDescriptor()
        : samplingRate(0), format(0), channels(0), frameCount(0), latency(0)  {}

        uint32_t samplingRate;
        int32_t format;
        int32_t channels;
        size_t frameCount;
        uint32_t latency;
    };

    //
    // IAudioPolicyService interface (see AudioPolicyInterface for method descriptions)
    //
    static status_t setDeviceConnectionState(audio_devices device, device_connection_state state, const char *device_address);
    static device_connection_state getDeviceConnectionState(audio_devices device, const char *device_address);
    static status_t setPhoneState(int state);
#if ANDROID_VERSION >= 17
    static status_t setPhoneState(audio_mode_t state);
#endif
    static status_t setRingerMode(uint32_t mode, uint32_t mask);
#ifdef VANILLA_ANDROID
    static status_t setForceUse(force_use usage, forced_config config);
    static forced_config getForceUse(force_use usage);
    static audio_io_handle_t getOutput(stream_type stream,
                                        uint32_t samplingRate = 0,
                                        uint32_t format = FORMAT_DEFAULT,
                                        uint32_t channels = CHANNEL_OUT_STEREO,
                                        output_flags flags = OUTPUT_FLAG_INDIRECT);
    static status_t setDeviceConnectionState(audio_devices_t device, audio_policy_dev_state_t state, const char *device_address);
    static status_t setFmVolume(float volume);
    static audio_policy_dev_state_t getDeviceConnectionState(audio_devices_t device, const char *device_address);
#else
    static status_t setForceUse(force_use usage, forced_config config) __attribute__((weak));
    static forced_config getForceUse(force_use usage) __attribute__((weak));
    static audio_io_handle_t getOutput(stream_type stream,
                                        uint32_t samplingRate = 0,
                                        uint32_t format = FORMAT_DEFAULT,
                                        uint32_t channels = CHANNEL_OUT_STEREO,
                                        output_flags flags = OUTPUT_FLAG_INDIRECT) __attribute__((weak));

    static status_t setForceUse(audio_policy_force_use_t usage, audio_policy_forced_cfg_t config) __attribute__((weak));
    static audio_policy_forced_cfg_t getForceUse(audio_policy_force_use_t usage) __attribute__((weak));
    static audio_io_handle_t getOutput(audio_stream_type_t stream,
                                        uint32_t samplingRate = 0,
                                        uint32_t format = AUDIO_FORMAT_DEFAULT,
                                        uint32_t channels = AUDIO_CHANNEL_OUT_STEREO,
                                        audio_policy_output_flags_t flags = AUDIO_POLICY_OUTPUT_FLAG_INDIRECT) __attribute__((weak));
    static status_t setDeviceConnectionState(audio_devices_t device, audio_policy_dev_state_t state, const char *device_address) __attribute__((weak));
    static status_t setFmVolume(float volume) __attribute__((weak));
    static audio_policy_dev_state_t getDeviceConnectionState(audio_devices_t device, const char *device_address) __attribute__((weak));

#endif
    static status_t startOutput(audio_io_handle_t output,
                                AudioSystem::stream_type stream,
                                int session = 0);
    static status_t stopOutput(audio_io_handle_t output,
                               AudioSystem::stream_type stream,
                               int session = 0);
    static void releaseOutput(audio_io_handle_t output);
    static audio_io_handle_t getInput(int inputSource,
                                    uint32_t samplingRate = 0,
                                    uint32_t format = FORMAT_DEFAULT,
                                    uint32_t channels = CHANNEL_IN_MONO,
                                    audio_in_acoustics acoustics = (audio_in_acoustics)0);
    static status_t startInput(audio_io_handle_t input);
    static status_t stopInput(audio_io_handle_t input);
    static void releaseInput(audio_io_handle_t input);
    static status_t initStreamVolume(stream_type stream,
                                      int indexMin,
                                      int indexMax);
    static status_t initStreamVolume(audio_stream_type_t stream,
                                      int indexMin,
                                      int indexMax);
    static status_t setStreamVolumeIndex(stream_type stream, int index);
    static status_t setStreamVolumeIndex(audio_stream_type_t stream, int index);
#if ANDROID_VERSION >= 17
    static status_t setStreamVolumeIndex(audio_stream_type_t stream,
                                         int index,
                                         audio_devices_t device);
    static status_t getStreamVolumeIndex(audio_stream_type_t stream,
                                         int *index,
                                         audio_devices_t device);
#endif
    static status_t getStreamVolumeIndex(stream_type stream, int *index);
    static status_t getStreamVolumeIndex(audio_stream_type_t stream, int *index);

    static uint32_t getStrategyForStream(stream_type stream);

    static audio_io_handle_t getOutputForEffect(effect_descriptor_t *desc);
    static status_t registerEffect(effect_descriptor_t *desc,
                                    audio_io_handle_t output,
                                    uint32_t strategy,
                                    int session,
                                    int id);
    static status_t unregisterEffect(int id);

    static const sp<IAudioPolicyService>& get_audio_policy_service();

    // ----------------------------------------------------------------------------

    static uint32_t popCount(uint32_t u);
    static bool isOutputDevice(audio_devices device);
    static bool isInputDevice(audio_devices device);
    static bool isA2dpDevice(audio_devices device);
    static bool isBluetoothScoDevice(audio_devices device);
    static bool isSeperatedStream(stream_type stream);
    static bool isLowVisibility(stream_type stream);
    static bool isOutputChannel(uint32_t channel);
    static bool isInputChannel(uint32_t channel);
    static bool isValidFormat(uint32_t format);
    static bool isLinearPCM(uint32_t format);
    static bool isModeInCall();

private:

    class AudioFlingerClient: public IBinder::DeathRecipient, public BnAudioFlingerClient
    {
    public:
        AudioFlingerClient() {
        }

        // DeathRecipient
        virtual void binderDied(const wp<IBinder>& who);

        // IAudioFlingerClient

        // indicate a change in the configuration of an output or input: keeps the cached
        // values for output/input parameters upto date in client process
        virtual void ioConfigChanged(int event, int ioHandle, void *param2);
    };

    class AudioPolicyServiceClient: public IBinder::DeathRecipient
    {
    public:
        AudioPolicyServiceClient() {
        }

        // DeathRecipient
        virtual void binderDied(const wp<IBinder>& who);
    };

    static sp<AudioFlingerClient> gAudioFlingerClient;
    static sp<AudioPolicyServiceClient> gAudioPolicyServiceClient;
    friend class AudioFlingerClient;
    friend class AudioPolicyServiceClient;

    static Mutex gLock;
    static sp<IAudioFlinger> gAudioFlinger;
    static audio_error_callback gAudioErrorCallback;

    static size_t gInBuffSize;
    // previous parameters for recording buffer size queries
    static uint32_t gPrevInSamplingRate;
    static int gPrevInFormat;
    static int gPrevInChannelCount;

    static sp<IAudioPolicyService> gAudioPolicyService;

    // mapping between stream types and outputs
    static DefaultKeyedVector<int, audio_io_handle_t> gStreamOutputMap;
    // list of output descritor containing cached parameters (sampling rate, framecount, channel count...)
    static DefaultKeyedVector<audio_io_handle_t, OutputDescriptor *> gOutputs;
};

class AudioParameter {

public:
    AudioParameter() {}
    AudioParameter(const String8& keyValuePairs);
    virtual ~AudioParameter();

    // reserved parameter keys for changing standard parameters with setParameters() function.
    // Using these keys is mandatory for AudioFlinger to properly monitor audio output/input
    // configuration changes and act accordingly.
    //  keyRouting: to change audio routing, value is an int in AudioSystem::audio_devices
    //  keySamplingRate: to change sampling rate routing, value is an int
    //  keyFormat: to change audio format, value is an int in AudioSystem::audio_format
    //  keyChannels: to change audio channel configuration, value is an int in AudioSystem::audio_channels
    //  keyFrameCount: to change audio output frame count, value is an int
    //  keyInputSource: to change audio input source, value is an int in audio_source
    //     (defined in media/mediarecorder.h)
    static const char *keyRouting;
    static const char *keySamplingRate;
    static const char *keyFormat;
    static const char *keyChannels;
    static const char *keyFrameCount;
    static const char *keyInputSource;

    String8 toString();

    status_t add(const String8& key, const String8& value);
    status_t addInt(const String8& key, const int value);
    status_t addFloat(const String8& key, const float value);

    status_t remove(const String8& key);

    status_t get(const String8& key, String8& value);
    status_t getInt(const String8& key, int& value);
    status_t getFloat(const String8& key, float& value);
    status_t getAt(size_t index, String8& key, String8& value);

    size_t size() { return mParameters.size(); }

private:
    String8 mKeyValuePairs;
    KeyedVector <String8, String8> mParameters;
};

};  // namespace android

#endif  /*ANDROID_AUDIOSYSTEM_H_*/
