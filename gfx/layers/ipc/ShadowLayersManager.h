/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ShadowLayersManager_h
#define mozilla_layers_ShadowLayersManager_h

namespace mozilla {
namespace layers {

class TargetConfig;
class LayerTransactionParent;
class AsyncCompositionManager;
class APZTestData;

class ShadowLayersManager
{
public:
    virtual void ShadowLayersUpdated(LayerTransactionParent* aLayerTree,
                                     const uint64_t& aTransactionId,
                                     const TargetConfig& aTargetConfig,
                                     bool aIsFirstPaint,
                                     bool aScheduleComposite,
                                     uint32_t aPaintSequenceNumber) = 0;

    virtual AsyncCompositionManager* GetCompositionManager(LayerTransactionParent* aLayerTree) { return nullptr; }

    virtual void ForceComposite(LayerTransactionParent* aLayerTree) { }
    virtual bool SetTestSampleTime(LayerTransactionParent* aLayerTree,
                                   const TimeStamp& aTime) { return true; }
    virtual void LeaveTestMode(LayerTransactionParent* aLayerTree) { }
    virtual void GetAPZTestData(const LayerTransactionParent* aLayerTree,
                                APZTestData* aOutData) { }
};

} // layers
} // mozilla

#endif // mozilla_layers_ShadowLayersManager_h
