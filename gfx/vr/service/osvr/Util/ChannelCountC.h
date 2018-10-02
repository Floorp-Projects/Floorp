/** @file
    @brief Header

    Must be c-safe!

    @date 2015

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

/*
// Copyright 2015 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#ifndef INCLUDED_ChannelCountC_h_GUID_CF7E5EE7_28B0_4B99_E823_DD701904B5D1
#define INCLUDED_ChannelCountC_h_GUID_CF7E5EE7_28B0_4B99_E823_DD701904B5D1

/* Internal Includes */
#include <osvr/Util/StdInt.h>
#include <osvr/Util/APIBaseC.h>

/* Library/third-party includes */
/* none */

/* Standard includes */
/* none */

OSVR_EXTERN_C_BEGIN

/** @addtogroup PluginKit
@{
*/

/** @brief The integer type specifying a number of channels/sensors or a
channel/sensor index.
*/
typedef uint32_t OSVR_ChannelCount;

/** @} */

OSVR_EXTERN_C_END

#endif
