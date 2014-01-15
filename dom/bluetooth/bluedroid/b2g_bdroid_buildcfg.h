/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef B2G_BDROID_BUILDCFG_H
#define B2G_BDROID_BUILDCFG_H

/**
 * This header defines B2G common bluedroid build configuration.
 *
 * This header is included by
 *   $(BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR)/bdroid_buildcfg.h,
 * which applies external configuration onto bluedroid.
 */

/******************************************************************************
**
** HSP, HFP
**
******************************************************************************/
/* AG feature masks */
#define BTIF_HF_FEATURES   ( BTA_AG_FEAT_3WAY | \
                             BTA_AG_FEAT_REJECT | \
                             BTA_AG_FEAT_ECS    | \
                             BTA_AG_FEAT_EXTERR)

/* CHLD values */
#define BTA_AG_CHLD_VAL    "(0,1,2)"

#endif /* B2G_BDROID_BUILDCFG_H */
